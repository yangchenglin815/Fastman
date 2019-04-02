#include "zdmprivate.h"
#include <QDateTime>
#include <QStringList>
#include <QUuid>
#include <QEventLoop>
#include <QFileInfo>
#include <QTimer>
#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>

#include <stdlib.h>
#include "zdmhttp.h"
#include <curl/curl.h>
#include "zlog.h"

ZDMTask::ZDMTask(ZDM *dm) :
    ZDownloadTask(dm) {
    DBG("+ ZDMTask %p\n", this);
    status = STAT_PENDING;
    result = ERR_NOERROR;
    mtime = QDateTime::currentDateTime().toTime_t();
    progress = 0;
    size = 0;

    this->dm = dm;
    needsStop = false;
    nextStatus = STAT_PENDING;
    lastProgress = 0;

    threadPool = new ZDMThreadPool(this);
    file = NULL;

    connect(&speed_timer, SIGNAL(timeout()), SLOT(slot_calcSpeed()));
    connect(this, SIGNAL(signal_startThreads()), SLOT(slot_startThreads()));
    connect(this, SIGNAL(signal_stopSpeedTimer(qint64,int)), SLOT(slot_stopSpeedTimer(qint64,int)));
}

ZDMTask::~ZDMTask() {
    while(!parts.isEmpty()) {
        delete parts.takeFirst();
    }
    DBG("- ZDMTask %p\n", this);
}

bool ZDMTask::initParts() {
    // if failed last time, clear all progress
    if(status == ZDMTask::STAT_FAILED) {
        progress = 0;
        lastProgress = 0;
        foreach(ZDMPart *part, parts) {
            part->progress = 0;
        }
    }

    // fetch remote size
    char *uri = strdup(url.toEncoded().data());
    DBG("uri='%s'\n", uri);

    qint64 new_size = -1;
    long http_resp = -1;
    int r = ZDMHttp::getContentLength(uri, http_resp, new_size);
    free(uri);

    DBG("getContentLength http_code: %d, content_length: %d\n", http_resp, new_size);
    if(r != CURLE_OK) {
        result = ZDMTask::ERR_CONNECT;
        DBG("curl fetch size error, code %d\n", r);
        return false;
    }
    if(new_size != size) {
        DBG("size mismatch, re-split parts");
        progress = 0;
        lastProgress = 0;
        while(!parts.isEmpty()) {
            delete parts.takeFirst();
        }
    }
    size = new_size;

    // split parts
    if(parts.isEmpty()) {
        if(size < MIN_PART_BLKSIZE) {
            ZDMPart *part = new ZDMPart();
            parts.append(part);

            part->begin = 0;
            part->end = size-1;
            part->progress = 0;
            DBG("creating part %lld -> %lld\n", part->begin, part->end);
        } else {
            qint64 pos = 0;
            qint64 part_size = size/dm->maxParts -1;
            uint padding = size % dm->maxParts;

            while(pos < size) {
                ZDMPart *part = new ZDMPart();
                parts.append(part);

                part->begin = pos;
                part->progress = 0;
                if(pos == 0) {
                    pos += padding;
                    part->end = part_size + padding;
                } else {
                    part->end = pos + part_size;
                }

                DBG("creating part %lld -> %lld\n", part->begin, part->end);
                pos += part_size;
                pos ++;
            }
        }
    }
    return true;
}

bool ZDMTask::initFile() {
    QString tmpName = path + ".dm!";
    QFileInfo finfo(tmpName);
    if(finfo.exists() && finfo.size() == size) {
        DBG("%p using exist file\n", this);
        file = new QFile(tmpName);
        if(!file->open(QIODevice::ReadWrite)) {
            result = ZDMTask::ERR_WRITE_FILE;
            delete file;
            file = NULL;
            return false;
        }
    } else {
        DBG("%p create new file\n", this);
        QFile::remove(tmpName);
        progress = 0;
        lastProgress = 0;
        foreach(ZDMPart *part, parts) {
            part->progress = 0;
        }

        file = new QFile(tmpName);
        if(file->open(QIODevice::WriteOnly)) {
            qint64 left = size;
            char dummy[4096] = {0};
            int n;

            while(left > 0) {
                n = file->write(dummy, left > sizeof(dummy) ? sizeof(dummy) : left);
                if(n < 0) {
                    break;
                }

                file->flush();
                left -= n;
            }

            if(left > 0) {
                result = ZDMTask::ERR_WRITE_FILE;
                file->close();
                delete file;
                file = NULL;
                return false;
            }
        } else {
            result = ZDMTask::ERR_CREATE_FILE;
            delete file;
            file = NULL;
            return false;
        }
    }
    return true;
}

qint64 ZDMTask::writeFile(qint64 pos, const QByteArray& buffer) {
    if(file == NULL) {
        result = ZDMTask::ERR_WRITE_FILE;
        return -1;
    }
    DBG("write at %lld, size %d\n", pos, buffer.size());

    qint64 cur = 0;
    bool success = false;

    file_mutex.lock();
    if((success = file->seek(pos)) == true) {
        success = buffer.size() == file->write(buffer);
    }
    file_mutex.unlock();

    if(!success) {
        result = ZDMTask::ERR_WRITE_FILE;
    }
    return cur;
}

void ZDMTask::addProgress(qint64 n) {
    progress_mutex.lock();
    progress += n;
    int elapsed = progress_timer.elapsed();
    if(elapsed > 1000) {
        emit signal_progress(this);
        emit signal_saveConfig();
        progress_timer.restart();
    }
    progress_mutex.unlock();
}

void ZDMTask::slot_startThreads() {
    lastProgress = progress;
    speed_timer.start(1000);

    progress_timer.restart();
    DBG("slot_startThreads for %d parts\n", parts.size());
    foreach(ZDMPart *part, parts) {
        if(part->begin + part->progress > part->end) {
            DBG("skipped finished part\n");
            continue;
        }
        ZDMPartThread *t = new ZDMPartThread(this, dm, this, part);
        t->start();
    }
    if(threadPool->threadCount() == 0) {
        emit signal_notStartedThreads();
    }
}

void ZDMTask::slot_calcSpeed() {
    emit signal_speed(this, progress - lastProgress);
    lastProgress = progress;
}

void ZDMTask::slot_stopSpeedTimer(qint64 gap, int elapsed) {
    speed_timer.stop();

    double average_speed = gap/(elapsed/1000.0f);
    emit signal_speed(this, (int)average_speed);
}

void ZDMTask::run() {
    bool success = false;
    status = ZDMTask::STAT_DOWNLOADING;
    emit signal_status(this);

    qint64 start_progress = progress;
    QTime start_time;
    start_time.start();

    do {
        if(!initParts()) {
            break;
        }
        if(!initFile()) {
            break;
        }
        DBG("after init\n");

        QEventLoop e;
        connect(this, SIGNAL(signal_notStartedThreads()), &e, SLOT(quit()));
        connect(threadPool, SIGNAL(signalEmpty()), &e, SLOT(quit()));
        emit signal_startThreads();
        e.exec();

        DBG("progress %lld / %lld\n", progress, size);
        success = size > 0 && progress >= size;
    } while(0);
    emit signal_stopSpeedTimer(progress - start_progress, start_time.elapsed());

    if(needsStop) {
        status = nextStatus;
    } else if(dm->needsStop) {
        status = ZDMTask::STAT_PENDING;
    } else {
        if(success) {
            status = ZDMTask::STAT_FINISHED;
        } else {
            status = ZDMTask::STAT_FAILED;
        }
    }

    if(file != NULL) {
        file->close();
        if(success) {
            QFile::remove(path);
            file->rename(path);
            mtime = QFileInfo(path).lastModified().toTime_t();
        } else {
            mtime = QDateTime::currentDateTime().toTime_t();
        }
        delete file;
        file = NULL;
    } else {
        mtime = QDateTime::currentDateTime().toTime_t();
    }

    emit signal_status(this);
    dm->saveConfig();
    DBG("ZDMTask %p finished.\n", this);
}

ZDMTaskThread::ZDMTaskThread(ZDMTask *task, ZDM *dm) :
    QThread(dm) {
    dm->threadPool->addThread(this);
    this->dm = dm;
    this->task = task;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

ZDMTaskThread::~ZDMTaskThread() {
    dm->threadPool->removeThread(this);
}

void ZDMTaskThread::run() {
    task->run();
}

ZDMPartThread::ZDMPartThread(QObject *parent, ZDM *dm, ZDMTask *task, ZDMPart *part) :
    QThread(parent) {
    DBG("++ partThread %p\n", this);
    task->threadPool->addThread(this);

    this->dm = dm;
    this->task = task;
    this->part = part;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

ZDMPartThread::~ZDMPartThread() {
    task->threadPool->removeThread(this);
    DBG("-- partThread %p\n", this);
}

typedef struct WRITE_ARG {
    ZDMTask *task;
    ZDMPart *part;
} WRITE_ARG;

static size_t curl_write_func(const char *buf, size_t size, size_t nmemb, void *p) {
    WRITE_ARG *arg = (WRITE_ARG *)p;
    size_t n = size * nmemb;

    arg->part->buffer.append(buf, n);
    arg->task->addProgress(n);

    if(arg->part->buffer.size() >= MAX_BUFF_BLKSIZE) {
        arg->task->writeFile(arg->part->begin + arg->part->progress, arg->part->buffer);
        arg->part->progress += arg->part->buffer.size();
        arg->part->buffer.clear();
    }

    if(arg->task->needsStop || arg->task->dm->needsStop) {
        return 0;
    }
    return n;
}

void ZDMPartThread::run() {
    WRITE_ARG arg;
    arg.task = task;
    arg.part = part;
    char *uri = strdup(task->url.toEncoded().data());
    DBG("uri = '%s'\n", uri);	

    int r = ZDMHttp::getDownloadData(uri, part->begin + part->progress, part->end, curl_write_func, (void *)&arg);
    free(uri);
    if(part->buffer.size() > 0) {
        task->writeFile(part->begin + part->progress, part->buffer);
        part->progress += part->buffer.size();
        part->buffer.clear();
    }
	DBG("download progress = '%d'; begin = %d; end = %d. \n", part->progress,part->begin, part->end); 
    if(r == CURLE_OK && task->size < 0) {
        task->size = task->progress;
    }
}

ZDownloadTask::ZDownloadTask(QObject *parent) :
    QObject(parent) {

}

ZDownloadManager::ZDownloadManager(QObject *parent) :
    QObject(parent) {

}

bool ZDownloadManager::global_init() {
    return ZDMHttp::global_init();
}

ZDownloadManager *ZDownloadManager::newDownloadManager(const QString &prefix, QObject *parent) {
    ZDM *dm = new ZDM(prefix, parent);
    return dm;
}

void ZDownloadManager::delDownloadManager(ZDownloadManager *obj) {
    ZDM *dm = (ZDM*) obj;
    delete dm;
}

ZDM::ZDM(const QString &prefix, QObject *parent) :
    ZDownloadManager(parent) {
    DBG("+ ZDM %p\n", this);
    needsStop = false;
    threadPool = new ZDMThreadPool(this);

    dbPrefix = prefix + ".xml";

    maxTasks = DEF_MAX_TASKS;
    maxParts = DEF_MAX_PARTS;
    maxRetry = DEF_MAX_RETRY;

    connect(this, SIGNAL(signal_newTask(ZDMTask*)), SLOT(slot_newTask(ZDMTask*)));
    connect(this, SIGNAL(signal_checkAndStart()), SLOT(slot_checkAndStart()));
    connect(threadPool, SIGNAL(signalEmpty()), SIGNAL(signal_idle()));
}

ZDM::~ZDM() {
    while(!tasks.isEmpty()) {
        delete tasks.takeFirst();
    }
    DBG("- ZDM %p\n", this);
}

void ZDM::setMaxTasks(int max) {
    maxTasks = max;
}

void ZDM::setMaxParts(int max) {
    maxParts = max;
}

void ZDM::setMaxRetry(int max) {
    maxRetry = max;
}

void ZDM::startWork() {
    emit signal_checkAndStart();
}

void ZDM::stopWork() {
    needsStop = true;
    saveConfig();
}

int ZDM::getUnfinishedCount() {
    int ret = 0;
    tasks_mutex.lock();
    foreach(ZDMTask *t, tasks) {
        if(t->status != ZDMTask::STAT_FINISHED) {
            ret ++;
        }
    }
    tasks_mutex.unlock();
    return ret;
}

int ZDM::getRunningCount() {
    int ret = 0;
    tasks_mutex.lock();
    foreach(ZDMTask *t, tasks) {
        if(t->status == ZDMTask::STAT_DOWNLOADING) {
            ret ++;
        }
    }
    tasks_mutex.unlock();
    return ret;
}

ZDownloadTask *ZDM::addTask(const QString &url, const QString &path) {
    return addTask(QUuid::createUuid().toString(), url, path);
}

ZDownloadTask *ZDM::addTask(const QString& id, const QString &url, const QString &path) {
    ZDMTask *task = new ZDMTask(this);
    task->id = id;
    task->url = url;
    task->path = path;

    tasks_mutex.lock();
    tasks.append(task);
    tasks_mutex.unlock();

    emit signal_newTask(task);
    emit signal_checkAndStart();
    emit signal_status(task);
    return task;
}

ZDownloadTask *ZDM::getTask(const QString &id) {
    ZDownloadTask *ret = NULL;
    tasks_mutex.lock();
    foreach(ZDMTask *t, tasks) {
        if(t->id == id) {
            ret = (ZDownloadTask *)t;
            break;
        }
    }
    tasks_mutex.unlock();
    return ret;
}

QList<ZDownloadTask *> ZDM::getTasks() {
    QList<ZDownloadTask *> list;
    tasks_mutex.lock();
    foreach(ZDMTask *t, tasks) {
        list.append(t);
    }
    tasks_mutex.unlock();
    return list;
}

QList<ZDownloadTask *> ZDM::getTasks(const QStringList &ids) {
    QList<ZDownloadTask *> list;
    tasks_mutex.lock();
    foreach(const QString& id, ids) {
        foreach(ZDMTask *t, tasks) {
            if(t->id == id) {
                list.append(t);
                break;
            }
        }
    }
    tasks_mutex.unlock();
    return list;
}

void ZDM::startTask(ZDownloadTask *t) {
    ZDMTask *task = (ZDMTask *)t;
    if(task->status == ZDMTask::STAT_STOPPED
            || task->status == ZDMTask::STAT_FAILED) {
        task->status = ZDMTask::STAT_PENDING;
        emit signal_status(task);
    }
    emit signal_checkAndStart();
}

void ZDM::stopTask(ZDownloadTask *t) {
    ZDMTask *task = (ZDMTask *)t;
    task->needsStop = true;
    if(task->status == ZDMTask::STAT_DOWNLOADING) {
        task->nextStatus = ZDMTask::STAT_STOPPED;
    } else if(task->status == ZDMTask::STAT_PENDING) {
        task->status = ZDMTask::STAT_STOPPED;
        emit signal_status(task);
    }
}

void ZDM::removeTask(ZDownloadTask *t) {
    ZDMTask *task = (ZDMTask *)t;
    if(task->status != ZDMTask::STAT_DOWNLOADING) {
        tasks_mutex.lock();
        DBG("removed task '%s'\n", t->id.toLocal8Bit().data());
        tasks.removeOne(task);
        tasks_mutex.unlock();
        delete task;
    }
}

void ZDM::removeTask(const QString &id) {
    tasks_mutex.lock();
    for(int i=0; i<tasks.size(); i++) {
        if(tasks[i]->status != ZDMTask::STAT_DOWNLOADING && tasks[i]->id == id) {
            DBG("removed task '%s'\n", id.toLocal8Bit().data());
            delete tasks[i];
            tasks.removeAt(i);
            break;
        }
    }
    tasks_mutex.unlock();
}

void ZDM::removeTasks(const QStringList &ids) {
    tasks_mutex.lock();
    foreach(const QString& id, ids) {
        for(int i=0; i<tasks.size(); i++) {
            if(tasks[i]->status != ZDMTask::STAT_DOWNLOADING && tasks[i]->id == id) {
                DBG("removed task '%s'\n", id.toLocal8Bit().data());
                delete tasks[i];
                tasks.removeAt(i);
                break;
            }
        }
    }
    tasks_mutex.unlock();
}

void ZDM::slot_newTask(ZDMTask *t) {
    DBG("slot_newTask %p\n", t);
    connect(t, SIGNAL(signal_progress(ZDownloadTask*)), this, SIGNAL(signal_progress(ZDownloadTask*)));
    connect(t, SIGNAL(signal_speed(ZDownloadTask*,int)), this, SIGNAL(signal_speed(ZDownloadTask*,int)));
    connect(t, SIGNAL(signal_status(ZDownloadTask*)), this, SIGNAL(signal_status(ZDownloadTask*)));
    connect(t, SIGNAL(signal_saveConfig()), this, SLOT(slot_saveConfig()));
}

void ZDM::slot_saveConfig() {
    if(dbTimer.elapsed() > 2000) {
        dbTimer.restart();
        saveConfig();
    }
}

void ZDM::loadConfig() {
    DBG("load config\n");

    QFile file(dbPrefix);
    if(!file.open(QIODevice::ReadOnly)) {
        DBG("ERROR read config file\n");
        return;
    }

    QDomDocument doc;
    if(!doc.setContent(&file)) {
        DBG("ERROR parse config file\n");
        file.close();
        file.remove();
        return;
    }

    QDomNodeList nodes = doc.elementsByTagName("task");
    int count = nodes.count();
    tasks_mutex.lock();
    for(int i=0; i<count; i++) {
        QDomElement e = nodes.at(i).toElement();

        ZDMTask *task = new ZDMTask(this);
        task->id = e.attribute("id");
        task->url = QUrl::fromEncoded(e.attribute("url").toLocal8Bit());
        task->path = e.attribute("path");
        task->status = e.attribute("status").toInt();
        task->mtime = e.attribute("mtime").toUInt();
        task->progress = e.attribute("progress").toLongLong();
        task->size = e.attribute("size").toLongLong();

        if(task->status != ZDMTask::STAT_STOPPED) {
            task->status = ZDMTask::STAT_PENDING;
        }

        DBG("found exist task '%s'\n", task->id.toLocal8Bit().data());
        QDomNodeList subs = e.elementsByTagName("part");
        for(int j = 0; j < subs.count(); j++) {
            QDomElement sub = subs.at(j).toElement();
            ZDMPart *part = new ZDMPart();
            part->begin = sub.attribute("begin").toLongLong();
            part->end = sub.attribute("end").toLongLong();
            part->progress = sub.attribute("progress").toLongLong();
            task->parts.append(part);
            DBG("found exist part %lld-%lld, %lld\n", part->begin, part->end, part->progress);
        }

        emit signal_newTask(task);
        emit signal_status(task);
        tasks.append(task);
    }
    tasks_mutex.unlock();

    file.close();
}

void ZDM::saveConfig() {
    dbMutex.lock();
    DBG("save config\n");
    do {
        QFile file(dbPrefix);
        file.remove();
        if(!file.open(QIODevice::WriteOnly)) {
            DBG("ERROR create config file\n");
            break;
        }

        QTextStream out(&file);
        out.setCodec("UTF-8");

        QDomDocument doc;
        QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
        doc.appendChild(instruction);

        QDomElement root = doc.createElement("tasks");
        doc.appendChild(root);

        tasks_mutex.lock();
        foreach(ZDMTask *task, tasks) {
            if(task->status == ZDMTask::STAT_FINISHED) {
                DBG("skipped finished task %p\n", task);
                continue;
            }

            QDomElement e = doc.createElement("task");
            e.setAttribute("id", task->id);
            e.setAttribute("url", task->url.toEncoded().constData());
            e.setAttribute("path", task->path);
            e.setAttribute("status", task->status);
            e.setAttribute("mtime", task->mtime);
            e.setAttribute("progress", task->progress);
            e.setAttribute("size", task->size);

            foreach(ZDMPart *part, task->parts) {
                QDomElement sub = doc.createElement("part");
                sub.setAttribute("begin", part->begin);
                sub.setAttribute("end", part->end);
                sub.setAttribute("progress", part->progress);
                e.appendChild(sub);
            }
            root.appendChild(e);
        }
        tasks_mutex.unlock();

        doc.save(out, 4, QDomNode::EncodingFromTextStream);
        file.close();
    } while(0);
    dbMutex.unlock();
}

void ZDM::slot_checkAndStart() {
    int running_count = 0;
    if(needsStop) {
        return;
    }

    tasks_mutex.lock();
    // get running count
    foreach(ZDMTask *t, tasks) {
        if(t->status == ZDMTask::STAT_DOWNLOADING) {
            running_count ++;
        }
    }
    DBG("running count %d\n", running_count);

    // check and pause extra tasks
    if(running_count > maxTasks) {
        foreach(ZDMTask *t, tasks) {
            if(t->status == ZDMTask::STAT_DOWNLOADING) {
                DBG("pause task %p\n", t);
                t->needsStop = true;
                t->nextStatus = ZDMTask::STAT_PENDING;
                if(--running_count == maxTasks) {
                    break;
                }
            }
        }
    }

    // check and start more tasks
    if(running_count < maxTasks) {
        foreach(ZDMTask *t, tasks) {
            if(t->status == ZDMTask::STAT_PENDING) {
                DBG("start task %p\n", t);
                t->needsStop = false;
                t->status = ZDMTask::STAT_DOWNLOADING; // must apply status here, to avoid duplicate start
                ZDMTaskThread *thread = new ZDMTaskThread(t, this);
                connect(thread, SIGNAL(destroyed()), this, SIGNAL(signal_checkAndStart()));
                thread->start();

                DBG("running count now = %d, maxTasks = %d\n", running_count + 1, maxTasks);
                if(++running_count == maxTasks) {
                    DBG("break\n");
                    break;
                }
            }
        }
    }
    tasks_mutex.unlock();
}

ZDMThreadPool::ZDMThreadPool(QObject *parent) :
    QObject(parent){
    DBG("+ ZDMThreadPool %p\n", this);
}

ZDMThreadPool::~ZDMThreadPool() {
    DBG("- ZDMThreadPool %p\n", this);
}

void ZDMThreadPool::addThread(QThread *t) {
    mutex.lock();
    list.append(t);
    mutex.unlock();
}

void ZDMThreadPool::removeThread(QThread *t) {
    int size;
    mutex.lock();
    list.removeOne(t);
    size = list.size();
    mutex.unlock();
    if(size == 0) {
        emit signalEmpty();
    }
}

int ZDMThreadPool::threadCount() {
    int size;
    mutex.lock();
    size = list.size();
    mutex.unlock();
    return size;
}

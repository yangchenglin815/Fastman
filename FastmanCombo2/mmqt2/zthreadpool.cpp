#include "zthreadpool.h"
#include <stdio.h>

ZThreadPool::ZThreadPool(QObject *parent) :
    QObject(parent)
{

}

ZThreadPool::~ZThreadPool() {
    while(!tList.isEmpty()) {
        delete tList.takeFirst();
    }
}

void ZThreadPool::print() {
#ifdef CMD_DEBUG
    tMutex.lock();
    printf("/-- threadpool --\n");
    foreach(ThreadEntry *t, tList) {
        printf("Thread '%s' -> %p\n", t->name.toLocal8Bit().data(), t->p);
    }
    printf("\\-- threadpool --\n");
    tMutex.unlock();
#endif
}

int ZThreadPool::size() {
    int n = 0;
    tMutex.lock();
    n = tList.size();
    tMutex.unlock();
    return n;
}

void ZThreadPool::addThread(QThread *p, const QString &name) {
    ThreadEntry *t = new ThreadEntry();
    t->name = name;
    t->p = p;
    tMutex.lock();
    tList.append(t);
    tMutex.unlock();
    print();
}

void ZThreadPool::removeThread(QThread *p) {
    tMutex.lock();
    foreach(ThreadEntry *t, tList) {
        if(t->p == p) {
            tList.removeOne(t);
            delete t;
            break;
        }
    }
    if(tList.size() == 0) {
        emit sigPoolEmpty();
    }
    tMutex.unlock();
    print();
}

void ZThreadPool::slot_remove(QObject *obj) {
    removeThread((QThread *)obj);
}

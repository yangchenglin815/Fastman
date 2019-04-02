#include "baidu_core.h"
#include <windows.h>
#include <QProcess>
#include <time.h>
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>
#include <QStringList>
#include <QTextCodec>
#include "globalcontroller.h"
#include "logman.h"

//////////////////////////////////////////////////////////////////////////
//  百度云下载接口
//////////////////////////////////////////////////////////////////////////

yun_file_object::yun_file_object()
{
	m_file_size = 0;
    m_read_size = 0;
    _percent = 0;
    _status = 0;
    m_is_pause = FALSE;
    _broken_download = 0;
}

yun_file_object::~yun_file_object()
{
    pause_task();
}

int yun_file_object::add_task(QString url, QString yunurl, QString path, QString saveFile)
{
    m_yun_url = yunurl;
    m_down_url = url;
    m_save_path = path;
    m_save_name = saveFile;

    //get_down_file_name();

    return 0;
}

int yun_file_object::pause_task()
{
    if (!m_is_pause) {
        m_is_pause = TRUE;
        CloseHandle(_hRead);
        _hRead = NULL;
        TerminateProcess(_pi.hProcess, 0);
        CloseHandle(_pi.hProcess);
        ZeroMemory(&_pi,sizeof(_pi));
    }

    return 0;
}

int yun_file_object::start()
{
    QStringList args;
    unsigned int r;
    QByteArray out;

    QString cmd = qApp->applicationDirPath() + "/yundl.dll";

    QString url = "\"";
    url += m_yun_url;
    url += "\"";

    char szbuf[4096];
    strncpy(szbuf, m_down_url.toLocal8Bit().data(), sizeof(szbuf) - 4);
    char *p = strrchr(szbuf, '/');
    if (p == NULL) {
        return 1;
    }
    p++;
    _fn = p;

    QString savePath = "\"";
    savePath += m_save_name;
    savePath += "\"";

    //QFile::remove(savePath);

    QString timeStr = QString::number(time(NULL));

    QString content = m_yun_url;
    content += m_save_name;
    content += DIYFLASH_VER_CODE;
    content += timeStr;

    QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
    QByteArray data = utf8->fromUnicode(content);

    QString md5;
    QCryptographicHash hash1(QCryptographicHash::Md5);
    hash1.addData(data);
    QByteArray d = hash1.result().toHex().toUpper();
    md5 = QString(d);
    QCryptographicHash hash2(QCryptographicHash::Md5);
    hash2.addData(md5.toLocal8Bit());
    d = hash2.result().toHex().toUpper();
    md5 = QString(d);
    QCryptographicHash hash3(QCryptographicHash::Md5);
    hash3.addData(md5.toLocal8Bit());
    d = hash3.result().toHex().toUpper();
    md5 = QString(d);
    QCryptographicHash hash4(QCryptographicHash::Md5);
    hash4.addData(md5.toLocal8Bit());
    d = hash4.result().toHex().toUpper();
    md5 = QString(d);

    QString tmpPath = "\"";
    tmpPath += m_save_path;
    tmpPath += "\"";

    QString guid = "\"";
    guid += LogMan::instance()->getBasicInfo().guid;
    guid += "\"";

    args.clear();
    args << url << savePath << DIYFLASH_VER_CODE << timeStr << tmpPath << md5 << guid;

    QString strArg = " ";
    strArg += args.join(" ");
    runCmd(cmd, strArg);

    return 0;
}

void yun_file_object::runCmd(QString app, QString cmdLine)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE hWrite;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&_hRead,&hWrite,&sa,0)) {
        return;
    }

    ZeroMemory(&_pi,sizeof(_pi));

    STARTUPINFOW si;
    ZeroMemory(&si,sizeof(STARTUPINFOW));
    si.cb = sizeof(STARTUPINFOW);
    GetStartupInfoW(&si);
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

    if (!CreateProcessW((LPWSTR)app.toStdWString().data(),
                        (LPWSTR)cmdLine.toStdWString().data(),
                        NULL,NULL,TRUE,NULL,NULL,NULL,&si,&_pi)) {
        m_is_pause = TRUE;
    }
    CloseHandle(hWrite);

    _buf.clear();

    m_is_pause = FALSE;
}

int yun_file_object::del_task(bool del_sucess_file)
{
    QFile::remove(m_save_name);

	return 0;
}

__int64 yun_file_object::get_current_size()
{
    if (m_read_size > m_file_size) {
        return 0;
    }

    return m_read_size;
}

__int64 yun_file_object::get_file_size()
{
	return m_file_size;
}


QString yun_file_object::get_down_file_name()
{
    return m_save_name;
}

double yun_file_object::query_down_pos()
{
    return _percent*100;
}

QString yun_file_object::get_down_url()
{
    return m_yun_url;
}

QString yun_file_object::get_save_path()
{
	return m_save_path;
}

void yun_file_object::parseLine(const QString &line)
{
    if (line.isEmpty() || line.compare("error:19", Qt::CaseInsensitive) == 0) {
        return;
    }
    if (line.compare("error:16", Qt::CaseInsensitive) == 0) {
        _status = 12;
        return;
    }
    if (line.compare("Download Success", Qt::CaseInsensitive) == 0) {
        _status = 11;
        return;
    }

    QStringList sl = line.split(",");
    if (sl.count() != 5) {
        return;
    }

    m_read_size = sl[2].toLongLong();
    m_file_size = sl[3].toLongLong();
    _broken_download = sl[4].toInt();

    _percent = (double)m_read_size / m_file_size;
}

int yun_file_object::get_broken_status()
{
    return _broken_download;
}

int yun_file_object::get_down_status()
{
    bool bend = false;
    char buffer[4096] = {0};
    DWORD bytesRead;

    if (m_is_pause) {
        return 12;
    }

    unsigned long lBytesRead;
    BOOL ret = PeekNamedPipe(_hRead, buffer, 1, &lBytesRead, 0, 0);
    if (!ret || !lBytesRead) {
        return _status;
    }

    memset(buffer, 0, sizeof(buffer));
    if (ReadFile(_hRead,buffer,sizeof(buffer) - 4,&bytesRead,NULL) == NULL) {
        bend = true;
    }

    _buf += buffer;

    //NLOG(LOG_T_ROMDOWN, LOG_A_PROGRESS, _buf.toLocal8Bit().data());

    QStringList sl = _buf.split("\r\n");
    if (!sl.empty()) {
        int n = sl.count();
        for (int i = 0; i < n; i++) {
            QString strLine = sl[i];
            if (strLine.isEmpty()) {
                continue;
            }
            parseLine(strLine);
        }

        // maybe broken line
        _buf = sl.last();
    }

    if (bend && _status == 0) {
        // close handle
        if (!m_is_pause) {
            m_is_pause = TRUE;
            CloseHandle(_hRead);
            _hRead = NULL;
            TerminateProcess(_pi.hProcess, 0);
            CloseHandle(_pi.hProcess);
            ZeroMemory(&_pi,sizeof(_pi));
        }

        if (m_file_size && m_read_size == m_file_size) {
            _status = 11;
        }
        else {
            _status = 12;
        }
    }

    // 下载未完成yundl.dll打印 Download Success 的问题
    if (_status == 11) {
//        if (m_file_size == 0 || m_read_size != m_file_size) {
//            _status = 12;
//        }
    }

    return _status;
}

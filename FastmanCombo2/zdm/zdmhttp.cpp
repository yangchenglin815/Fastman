#include "zdmhttp.h"
#include <curl/curl.h>
#include <QFileInfo>
#include <stdlib.h>
#include "zlog.h"
#include <QUrl>
#include <QHostInfo> 

bool ZDMHttp::global_init() {
    return curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK;
}

bool ZDMHttp::url_escape(const QByteArray &in, QString &out, bool preserve) {
    CURL *curl = curl_easy_init();
    if(curl == NULL) {
        return false;
    }

    char *tmp = curl_easy_escape(curl, in.data(), in.length());
    if(tmp == NULL) {
        return false;
    }

    if(preserve) {
        out = QString::fromUtf8(tmp);
    } else {
        out = QString::fromUtf8(tmp).toLower();
    }
    free(tmp);
    return true;
}

int ZDMHttp::getContentLength(char *url, long &resp_code, qint64 &content_length) {
    CURLcode r;
    CURL *curl;

    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    do {
        if((r = curl_easy_perform(curl)) != CURLE_OK) {
            DBG("curl_easy_perform r=%d\n", r);
            break;
        }

        if((r = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp_code)) != CURLE_OK) {
            break;
        }
        DBG("http code: %ld\n", resp_code);
        if(resp_code < 200 || resp_code > 299) {
            r = CURLE_RECV_ERROR;
            break;
        }

        double d_size;
        if((r = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &d_size)) != CURLE_OK) {
            r = CURLE_RECV_ERROR;
            break;
        }
        DBG("fetched remote size: %f\n", d_size);
        content_length = d_size;
    } while(0);
    curl_easy_cleanup(curl);
    return r;
}

int ZDMHttp::getDownloadData(char *url, qint64 from, qint64 to, ZDMHTTP_WRITE_FUNC func, void *p) {
    CURLcode r;
    CURL *curl;
    char range[128];

    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }

    QString sUrl = QString(url);
    QString sHost = QUrl(sUrl).host();
    int nPort = QUrl(sUrl).port();
    DBG("start download, /n");
    DBG("url is %s, sHost = %s,Port = %d \n",sUrl.toLocal8Bit().data(),sHost.toLocal8Bit().data(),nPort);
    //#if 0
    QHostInfo hostInfo = QHostInfo::fromName(sHost);

    QList<QHostAddress> addrList = hostInfo.addresses();
    if (!addrList.isEmpty())
    {
        for  (int i = 0; i < addrList.size(); i++)
        {
            //qDebug() << addrList.at(i);
            QString sIP = addrList.at(i).toString();
            DBG("name is %s,IP is the  %s.\n",sHost.toLocal8Bit().data(),sIP.toLocal8Bit().data());
        }
    }
    //#endif
    curl_easy_setopt(curl, CURLOPT_URL, url);//
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, p);

    DBG("http get range %lld -> %lld\n", from, to);
    if(to > 0) {
        sprintf(range, "%lld-%lld", from, to);
        curl_easy_setopt(curl, CURLOPT_RANGE, range);
    }
    r = curl_easy_perform(curl);
    DBG("curl get returns %d\n", r);
    curl_easy_cleanup(curl);

    return r;
}

static size_t curl_write_func(char *buf, size_t size, size_t nmemb, void *p) {
    size_t n = size * nmemb;
    QDataStream *stream = (QDataStream *)p;
    return stream->writeRawData(buf, n);
}

static size_t curl_read_func(void *buf, size_t size, size_t nmemb, void *p) {
    size_t n = size * nmemb;
    QDataStream *stream = (QDataStream *)p;
    return stream->readRawData((char *)buf, n);
}

int ZDMHttp::get(char *url, QDataStream &out) {
    CURLcode r;
    CURL *curl;
    DBG("get <%s>\n", url);
    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&out);

    r = curl_easy_perform(curl);
    DBG("curl get returns %d\n", r);
    curl_easy_cleanup(curl);

    return r;
}

int ZDMHttp::get(char *url, QByteArray &out) {
    QDataStream stream(&out, QIODevice::WriteOnly);
    return get(url, stream);
}

int ZDMHttp::post(char *url, char *params, QDataStream &out) {
    CURLcode r;
    CURL *curl;
    DBG("post <%s>\n", url);
    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&out);

    r = curl_easy_perform(curl);
    DBG("curl post returns %d\n", r);
    curl_easy_cleanup(curl);

    return r;
}

int ZDMHttp::post(char *url, char *params, QByteArray &out) {
    QDataStream stream(&out, QIODevice::WriteOnly);
    return post(url, params, stream);
}

static size_t curl_read_file(void *buf, size_t size, size_t nmemb, void *p) {
    FILE *fp = (FILE *) p;
    return fread(buf, size, nmemb, fp);
}

int ZDMHttp::postFile(char *url, const QString &path, QDataStream &out) {
    CURLcode r;
    CURL *curl;
    DBG("postFile <%s>\n", url);

    QFileInfo info(path);
    FILE *fp = fopen(path.toLocal8Bit().data(), "rb");
    if(fp == NULL) {
        DBG("open file failed!\n");
        return -1;
    }

    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);

    curl_slist *list = NULL;
    list = curl_slist_append(list, QString("Content-Length: %1").arg(info.size()).toLocal8Bit().data());
    list = curl_slist_append(list, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl, CURLOPT_POST, 1);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&out);

    r = curl_easy_perform(curl);
    DBG("curl post returns %d\n", r);
    curl_easy_cleanup(curl);
    curl_slist_free_all(list);
    fclose(fp);

    return r;
}

int ZDMHttp::postFile(char *url, const QString &path, QByteArray &out) {
    QDataStream stream(&out, QIODevice::WriteOnly);
    return postFile(url, path, stream);
}

int ZDMHttp::postFileByMutltiformpost(char *url, const QString &path, QByteArray &/*out*/)
{
    /* <DESC>
    * using the multi interface to do a multipart formpost without blocking
     * </DESC>
     */
    CURL *curl;

    CURLM *multi_handle;
    int still_running;

    struct curl_httppost *formpost=NULL;
    struct curl_httppost *lastptr=NULL;
    struct curl_slist *headerlist=NULL;
    static const char buf[] = "Expect:";

    /* Fill in the file upload field. This makes libcurl load data from
     the given file name when curl_easy_perform() is called. */
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "sendfile",
                 CURLFORM_FILE, path.toLocal8Bit().data(),
                 CURLFORM_END);

    /* Fill in the filename field */
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "filename",
                 CURLFORM_COPYCONTENTS, "dxfiles",
                 CURLFORM_END);

    /* Fill in the submit field too, even if this is rarely needed */
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "submit",
                 CURLFORM_COPYCONTENTS, "send",
                 CURLFORM_END);

    curl = curl_easy_init();
    multi_handle = curl_multi_init();

    /* initialize custom header list (stating that Expect: 100-continue is not
     wanted */
    headerlist = curl_slist_append(headerlist, buf);

    CURLMcode mc; /* curl_multi_fdset() return code */
    if(curl && multi_handle) {
        /* what URL that receives this POST */
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

        curl_multi_add_handle(multi_handle, curl);

        curl_multi_perform(multi_handle, &still_running);

        do {
            struct timeval timeout;
            int rc; /* select() return code */

            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = -1;

            long curl_timeo = -1;

            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);

            /* set a suitable timeout to play around with */
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            curl_multi_timeout(multi_handle, &curl_timeo);
            if(curl_timeo >= 0) {
                timeout.tv_sec = curl_timeo / 1000;
                if(timeout.tv_sec > 1)
                    timeout.tv_sec = 1;
                else
                    timeout.tv_usec = (curl_timeo % 1000) * 1000;
            }

            /* get file descriptors from the transfers */
            mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

            if(mc != CURLM_OK) {
                fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
                break;
            }

            /* On success the value of maxfd is guaranteed to be >= -1. We call
         select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
         no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
         to sleep 100ms, which is the minimum suggested value in the
         curl_multi_fdset() doc. */

            if(maxfd == -1) {
#ifdef _WIN32
                Sleep(100);
                rc = 0;
#else
                /* Portable sleep for platforms other than Windows. */
                struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
                rc = select(0, NULL, NULL, NULL, &wait);
#endif
            }
            else {
                /* Note that on some platforms 'timeout' may be modified by select().
           If you need access to the original value save a copy beforehand. */
                rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
            }

            switch(rc) {
            case -1:
                /* select error */
                break;
            case 0:
            default:
                /* timeout or readable/writable sockets */
                printf("perform!\n");
                curl_multi_perform(multi_handle, &still_running);
                printf("running: %d!\n", still_running);
                break;
            }
        } while(still_running);

        curl_multi_cleanup(multi_handle);

        /* always cleanup */
        curl_easy_cleanup(curl);

        /* then cleanup the formpost chain */
        curl_formfree(formpost);

        /* free slist */
        curl_slist_free_all (headerlist);
    }

    return mc;
}

int ZDMHttp::postJson(char *url, char *jsonData, QByteArray &out)
{
    QDataStream stream(&out, QIODevice::WriteOnly);
    return postJson(url, jsonData, stream);
}

int ZDMHttp::postJson(char *url, char *jsonData, QDataStream &out)
{
    CURLcode r;
    CURL *curl;
    DBG("postJson <%s>\n", url);

    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);

    curl_slist *list = NULL;
    list = curl_slist_append(list, "Content-Type: application/json;charset=UTF-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl, CURLOPT_POST, 1);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&out);

    r = curl_easy_perform(curl);
    DBG("curl post returns %d\n", r);
    if (CURLE_OK != r) {
        DBG("error,%s\n", curl_easy_strerror(r));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(list);

    return r;
}

int ZDMHttp::postData(char *url, QByteArray &in, QStringList& headers, QByteArray &out) {
    QDataStream ins(&in, QIODevice::ReadOnly);
    QDataStream ous(&out, QIODevice::WriteOnly);

    CURLcode r;
    CURL *curl;
    DBG("post <%s>\n", url);
    curl = curl_easy_init();
    if(curl == NULL) {
        return CURLE_FAILED_INIT;
    }

    curl_slist *hlist = NULL;
    foreach(const QString& h, headers) {
        hlist = curl_slist_append(hlist, h.toLocal8Bit().data());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl, CURLOPT_POST, 1);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_func);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&ins);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&ous);

    r = curl_easy_perform(curl);
    DBG("curl post returns %d\n", r);
    curl_easy_cleanup(curl);

    return r;
}

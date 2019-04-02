#include "zdmhttp.h"
#include <curl/curl.h>
#include <QFileInfo>
#include "zlog.h"

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

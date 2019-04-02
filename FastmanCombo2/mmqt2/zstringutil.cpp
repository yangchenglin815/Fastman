#include "zstringutil.h"
#include <QTextCodec>
#include <QDateTime>

static char getGbkPinyicode(int code) {
    if(code >= 1601 && code < 1637) return 'A';
    if(code >= 1637 && code < 1833) return 'B';
    if(code >= 1833 && code < 2078) return 'C';
    if(code >= 2078 && code < 2274) return 'D';
    if(code >= 2274 && code < 2302) return 'E';
    if(code >= 2302 && code < 2433) return 'F';
    if(code >= 2433 && code < 2594) return 'G';
    if(code >= 2594 && code < 2787) return 'H';
    if(code >= 2787 && code < 3106) return 'J';
    if(code >= 3106 && code < 3212) return 'K';
    if(code >= 3212 && code < 3472) return 'L';
    if(code >= 3472 && code < 3635) return 'M';
    if(code >= 3635 && code < 3722) return 'N';
    if(code >= 3722 && code < 3730) return 'O';
    if(code >= 3730 && code < 3858) return 'P';
    if(code >= 3858 && code < 4027) return 'Q';
    if(code >= 4027 && code < 4086) return 'R';
    if(code >= 4086 && code < 4390) return 'S';
    if(code >= 4390 && code < 4558) return 'T';
    if(code >= 4558 && code < 4684) return 'W';
    if(code >= 4684 && code < 4925) return 'X';
    if(code >= 4925 && code < 5249) return 'Y';
    if(code >= 5249 && code < 5590) return 'Z';
    return '_';
}

QString ZStringUtil::getSizeStr(quint64 size) {
    if(size > 1073741824) {
        return QString().sprintf("%.2fG", size/1073741824.0f);
    } else if(size > 1048576) {
        return QString().sprintf("%.2fM", size/1048576.0f);
    } else if(size > 1024) {
        return QString().sprintf("%.2fK", size/1024.0f);
    }
    return QString().sprintf("%lldB", size);
}

QString ZStringUtil::getTimeStr(quint64 time) {
    return QDateTime::fromMSecsSinceEpoch(time).toString("yyyy/MM/dd hh:mm:ss");
}

QString ZStringUtil::getPinyin(const QString &name) {
    QString ret;
    unsigned char high, low;
    int code;
    QByteArray data = toGBK(name);

    for(int i=0; i<data.size(); i++) {
        high = data[i];
        if(high < 0x80) {
            ret.append(high);
            continue;
        }

        low = data[i+1];
        if(high < 0xa1 || low < 0xa1) {
            continue;
        } else {
            code = (high-0xa0)*100 + low-0xa0;
        }
        ret.append(getGbkPinyicode(code));
        i++;
    }
    return ret.toUpper();
}

QString ZStringUtil::fromGBK(const QByteArray& data) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QString();
    }
    return c->toUnicode(data);
}

QByteArray ZStringUtil::toGBK(const QString& in) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QByteArray();
    }
    return c->fromUnicode(in);
}

QString ZStringUtil::fromUTF8(const QByteArray& data) {
    return QString::fromUtf8(data);
}

QByteArray ZStringUtil::toUTF8(const QString& in) {
    return in.toUtf8();
}

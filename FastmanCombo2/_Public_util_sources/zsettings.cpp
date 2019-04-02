#include "zsettings.h"
#include <QFile>

#include "zlog.h"

ZSettings::ZSettings(const QString &path)
{
    _path = path;
    beginGroup("main");
}

static bool isGroup(const QString& line, QString& group) {
    if(!line.startsWith('[') || !line.endsWith(']')) {
        return false;
    }

    QString pot = line.mid(1, line.size()-2);
    if(pot.contains('[') || pot.contains(']')) {
        return false;
    }

    group = pot;
    return true;
}

static bool isPair(const QString& line, QMap<QString, QString>& map) {
    int count = 0;
    int pos = 0;
    int n;
    while((n = line.indexOf('=', pos)) != -1) {
        count ++;
        pos = n+1;
    }
    if(count != 1) {
        return false;
    }

    QString key = line.mid(0, pos-1);
    key = key.trimmed();
    while(key.startsWith(' ')) key.remove(0, 1);
    while(key.endsWith(' ')) key.chop(1);

    QString value = line.mid(pos);
    while(value.startsWith(' ')) value.remove(0, 1);
    while(value.endsWith(' ')) value.chop(1);

    map.insert(key, value);
    return true;
}

void ZSettings::beginGroup(const QString &group) {
    QFile f(_path);
    char buf[4096];
    int n;

    bool isIn = false;
    _map.clear();
    if(f.open(QIODevice::ReadOnly)) {
        while((n = f.readLine(buf, sizeof(buf))) > 0) {
            while(buf[n-1] == '\r' || buf[n-1] == '\n') {
                --n;
            }

            QString line = QString::fromLocal8Bit(buf, n);
            // 有些 [Version] 行含有前后空�? 排除key=value 行后trim
            if (line.contains("=") == false) {
                line = line.trimmed().toLocal8Bit().data();
            }
            QString pot;
            if(isGroup(line, pot)) {
                // 有些 [Version] 有各种大小写
                isIn = (pot.contains(group, Qt::CaseInsensitive)
                        && pot.length() == group.length());
            } else if(isIn) {
                isPair(line, _map);
            }
        }

        f.close();
    }
}

void ZSettings::endGroup() {
    beginGroup("main");
}

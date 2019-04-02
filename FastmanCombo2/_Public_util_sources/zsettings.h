#ifndef ZSETTINGS_H
#define ZSETTINGS_H

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QMap>

class ZSettings
{
    QString _path;
    QString _group;
    QMap<QString, QString> _map;
public:
    ZSettings(const QString& path);

    void beginGroup(const QString& group);
    void endGroup();

    inline QStringList childKeys() const {
        return _map.keys();
    }

    inline QString value(const QString &key) const {
        return _map.value(key);
    }

    inline QString value(const QString &key, const QString& defaultValue) const {
        return _map.value(key, defaultValue);
    }
};

#endif // ZSETTINGS_H

#ifndef XMLUTIL_H
#define XMLUTIL_H

#include <QObject>

class XMLUtil : public QObject
{
    Q_OBJECT
public:
    explicit XMLUtil(const QString &file, QObject *parent = 0);

    bool setValue(const QString &node, const QString &value);


signals:

public slots:

private:
    QString m_file;

};

#endif // XMLUTIL_H

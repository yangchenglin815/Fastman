#include "xmlutil.h"

#include <QFile>
#include <QProcess>
#include <QDomDocument>

XMLUtil::XMLUtil(const QString &file, QObject *parent) : QObject(parent)
{
    m_file = file;

}

bool XMLUtil::setValue(const QString &node, const QString &value)
{

    return true;

}

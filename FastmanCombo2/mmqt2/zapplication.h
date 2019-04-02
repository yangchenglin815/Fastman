#ifndef ZAPPLICATION_H
#define ZAPPLICATION_H

#include <QObject>
#include <QApplication>

class ZApplication : public QApplication
{
public:
    quint64 m_ticket;

    ZApplication(int argc,char *argv[]);
    ~ZApplication(){}

    static quint64 currentMSecsSinceEpoch();
protected:
#ifdef _WIN32
    bool winEventFilter(MSG *message, long *result);
#endif
};

#endif // ZAPPLICATION_H

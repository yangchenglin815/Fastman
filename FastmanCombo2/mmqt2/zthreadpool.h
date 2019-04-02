#ifndef ZTHREADPOOL_H
#define ZTHREADPOOL_H

#include <QObject>
#include <QList>
#include <QMutex>

class ZThreadPool : public QObject
{
    Q_OBJECT

    class ThreadEntry {
    public:
        QString name;
        QThread *p;
    };

    QList<ThreadEntry *> tList;
    QMutex tMutex;
public:
    explicit ZThreadPool(QObject *parent = 0);
    ~ZThreadPool();

    void print();
    int size();
    void addThread(QThread *p, const QString& name);
    void removeThread(QThread *p);
public slots:
    void slot_remove(QObject *obj);
signals:
    void sigPoolEmpty();

};

#endif // ZTHREADPOOL_H

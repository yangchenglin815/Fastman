#ifndef ZTHREADPOOL_H
#define ZTHREADPOOL_H

#include <QThread>
#include <QList>
#include <QMutex>

class ThreadEntry {
public:
    QString name;
    QThread *p;
};

class ZThreadPool : public QObject
{
    Q_OBJECT
    QList<ThreadEntry *> tList;
    QMutex tMutex;
public:
    explicit ZThreadPool(QObject *parent = 0);
    ~ZThreadPool();

    void print();
    int size();
    void addThread(QThread *p, const QString& name);
    void removeThread(QThread *p);

    void registerThread(QThread *p, const QString& name);
public slots:
    void slot_remove(QObject *obj);
    void slot_checkAndRemove();
signals:
    void sigPoolEmpty();

};

#endif // ZTHREADPOOL_H

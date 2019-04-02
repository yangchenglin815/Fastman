#ifndef QMUTEX_H
#define QMUTEX_H
#if !defined(_WIN32)

#include <pthread.h>

class QMutex {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
public:
    QMutex();
    ~QMutex();

    bool tryLock();
    bool lock();
    bool unlock();

    bool wait();
    void notify();
};

#endif
#endif // QMUTEX_H

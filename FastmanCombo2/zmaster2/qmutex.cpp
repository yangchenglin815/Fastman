#if !defined(_WIN32)
#include "qmutex.h"
#include <sys/time.h>
#include <errno.h>

QMutex::QMutex() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

QMutex::~QMutex() {
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

bool QMutex::tryLock() {
    return pthread_mutex_trylock(&mutex) == 0;
}

bool QMutex::lock() {
    return pthread_mutex_lock(&mutex) == 0;
}

bool QMutex::unlock() {
    return pthread_mutex_unlock(&mutex) != 0;
}

bool QMutex::wait() {
    lock();
    int ret = pthread_cond_wait(&cond, &mutex);
    unlock();
    return ret == 0;
}

void QMutex::notify() {
    lock();
    pthread_cond_signal(&cond);
    unlock();
}
#endif

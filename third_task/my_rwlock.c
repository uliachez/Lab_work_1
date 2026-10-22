#include "my_rwlock.h"

#include <errno.h>

int my_rwlock_init(my_rwlock_t *lock) {
    if (lock == NULL) return EINVAL;

    int rc = pthread_mutex_init(&lock->mutex, NULL);
    if (rc != 0) return rc;

    rc = pthread_cond_init(&lock->readers_cv, NULL);
    if (rc != 0) {
        pthread_mutex_destroy(&lock->mutex);
        return rc;
    }

    rc = pthread_cond_init(&lock->writers_cv, NULL);
    if (rc != 0) {
        pthread_cond_destroy(&lock->readers_cv);
        pthread_mutex_destroy(&lock->mutex);
        return rc;
    }

    lock->active_readers = 0;
    lock->waiting_readers = 0;
    lock->waiting_writers = 0;
    lock->writer_active = 0;
    return 0;
}

int my_rwlock_rdlock(my_rwlock_t *lock) {
    if (lock == NULL) return EINVAL;
    int rc = pthread_mutex_lock(&lock->mutex);
    if (rc != 0) return rc;

    ++lock->waiting_readers;
    while (lock->writer_active || lock->waiting_writers > 0) {
        rc = pthread_cond_wait(&lock->readers_cv, &lock->mutex);
        if (rc != 0) {
            --lock->waiting_readers;
            pthread_mutex_unlock(&lock->mutex);
            return rc;
        }
    }
    --lock->waiting_readers;
    ++lock->active_readers;

    return pthread_mutex_unlock(&lock->mutex);
}

int my_rwlock_wrlock(my_rwlock_t *lock) {
    if (lock == NULL) return EINVAL;
    int rc = pthread_mutex_lock(&lock->mutex);
    if (rc != 0) return rc;

    ++lock->waiting_writers;
    while (lock->writer_active || lock->active_readers > 0) {
        rc = pthread_cond_wait(&lock->writers_cv, &lock->mutex);
        if (rc != 0) {
            --lock->waiting_writers;
            if (lock->waiting_writers == 0 && !lock->writer_active)
                pthread_cond_broadcast(&lock->readers_cv);
            pthread_mutex_unlock(&lock->mutex);
            return rc;
        }
    }
    --lock->waiting_writers;
    lock->writer_active = 1;

    return pthread_mutex_unlock(&lock->mutex);
}

int my_rwlock_unlock(my_rwlock_t *lock) {
    if (lock == NULL) return EINVAL;
    int rc = pthread_mutex_lock(&lock->mutex);
    if (rc != 0) return rc;

    if (lock->writer_active) {
        lock->writer_active = 0;
    } else if (lock->active_readers > 0) {
        --lock->active_readers;
    } else {
        pthread_mutex_unlock(&lock->mutex);
        return EPERM;
    }

    if (!lock->writer_active && lock->active_readers == 0) {
        if (lock->waiting_writers > 0)
            pthread_cond_signal(&lock->writers_cv);
        else if (lock->waiting_readers > 0)
            pthread_cond_broadcast(&lock->readers_cv);
    }
    return pthread_mutex_unlock(&lock->mutex);
}

int my_rwlock_destroy(my_rwlock_t *lock) {
    if (lock == NULL) return EINVAL;
    int rc = pthread_mutex_lock(&lock->mutex);
    if (rc != 0) return rc;

    if (lock->active_readers || lock->waiting_readers ||
        lock->waiting_writers || lock->writer_active) {
        pthread_mutex_unlock(&lock->mutex);
        return EBUSY;
    }

    rc = pthread_mutex_unlock(&lock->mutex);
    if (rc != 0) return rc;
    rc = pthread_cond_destroy(&lock->readers_cv);
    if (rc != 0) return rc;
    rc = pthread_cond_destroy(&lock->writers_cv);
    if (rc != 0) return rc;
    return pthread_mutex_destroy(&lock->mutex);
}

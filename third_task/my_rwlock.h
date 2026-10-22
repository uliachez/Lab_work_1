#ifndef MY_RWLOCK_H
#define MY_RWLOCK_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;        
    pthread_cond_t readers_cv;  /* условная переменная для ожидающих читателей */
    pthread_cond_t writers_cv;  /* условная переменная для ожидающих писателей */
    unsigned active_readers;   /* кол-во активных читателей */
    unsigned waiting_readers;  /* кол-во ожидающих читателей */
    unsigned waiting_writers;  /* кол-во ожидающих писателей */
    int writer_active;         /* работает писатель или нет (1 или 0) */
} my_rwlock_t;

int my_rwlock_init(my_rwlock_t *lock);
int my_rwlock_rdlock(my_rwlock_t *lock);
int my_rwlock_wrlock(my_rwlock_t *lock);
int my_rwlock_unlock(my_rwlock_t *lock);
int my_rwlock_destroy(my_rwlock_t *lock);

#endif

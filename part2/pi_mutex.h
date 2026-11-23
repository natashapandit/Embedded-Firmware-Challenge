#ifndef PI_MUTEX_H
#define PI_MUTEX_H

#include <pthread.h>
#include <stdbool.h>

typedef struct pi_waiter pi_waiter_t; // each blocked thread has a waiter record

typedef struct {
    pthread_mutex_t guard; // protects mutex bookkeeping
    pthread_t owner; // thread currently holding mutex
    bool locked; // whether the mutex has an owner
    int owner_base_priority; // priority to restore on unlock
    int owner_policy; // os scheduling policy of owner
    pi_waiter_t *waiters; // linked list of threads waiting for ownership
} pi_mutex_t;

int pi_mutex_init(pi_mutex_t *m);
int pi_mutex_lock(pi_mutex_t *m);
int pi_mutex_unlock(pi_mutex_t *m);
int pi_mutex_destroy(pi_mutex_t *m);

#endif

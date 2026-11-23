#define _POSIX_C_SOURCE 200809L
#include "pi_mutex.h"
#include <errno.h>
#include <sched.h>
#include <stddef.h>

// waiting thread keeps record on its own stack while blocked
struct pi_waiter{ 
    pthread_t thread; // waiting thread id
    int priority; // priority used to choose the next owner
    bool granted; // set when unlock hands ownership to waiter
    pthread_cond_t cv; // lets waiter sleep until ownership granted
    struct pi_waiter *next; // next waiter in linked list
};

// ask linux to change thread's scheduler priority - returns error if permission or policy prevents change
static int set_priority(pthread_t thread, int policy, int priority) {
    struct sched_param sp = {.sched_priority = priority};
    return pthread_setschedparam(thread, policy, &sp);
}

// initialize internal guard and mark custom mutex unlocked
int pi_mutex_init(pi_mutex_t *m){
    if(!m){
        return EINVAL;
    }
    int rc = pthread_mutex_init(&m->guard, NULL);
    if(rc){
        return rc;
    }
    m->locked = false;
    m->waiters = NULL;
    m->owner_base_priority = 0;
    m->owner_policy = SCHED_OTHER;
    return 0;
}

// acquire immediately if free, otherwise enqueue and sleep
int pi_mutex_lock(pi_mutex_t *m){
    if(!m){
        return EINVAL;
    }

    pthread_t self = pthread_self();
    int policy;
    struct sched_param sp;
    int rc = pthread_getschedparam(self, &policy, &sp);

    if(rc){
        return rc;
    }
    rc = pthread_mutex_lock(&m->guard);
    if(rc){
        return rc;
    }

    // not current owner so this thread becomes the owner
    if(!m->locked){
        m->locked = true;
        m->owner = self;
        m->owner_policy = policy;
        m->owner_base_priority = sp.sched_priority;
        pthread_mutex_unlock(&m->guard);
        return 0;
    }

    // non-recursive mutex must reject locking it twice by its owner
    if(pthread_equal(m->owner, self)){
        pthread_mutex_unlock(&m->guard);
        return EDEADLK;
    }

    // record stays alive until thread is awakened
    struct pi_waiter w = {.thread = self, .priority = sp.sched_priority, .granted = false, .next = m->waiters};
    rc = pthread_cond_init(&w.cv, NULL);
    if(rc){
        pthread_mutex_unlock(&m->guard);
        return rc;
    }
    // boost current owner before committing waiter
    int owner_policy;
    struct sched_param owner_sp;
    rc = pthread_getschedparam(m->owner, &owner_policy, &owner_sp);

    if(!rc && policy != owner_policy){
        rc = ENOTSUP;
    }
    if(!rc && sp.sched_priority > owner_sp.sched_priority){
        rc = set_priority(m->owner, owner_policy, sp.sched_priority);
    }
    if(rc){
        pthread_cond_destroy(&w.cv);
        pthread_mutex_unlock(&m->guard);
        return rc;
    }
    // put thread on waiting list, then sleep on its condition
    m->waiters = &w;
    // use loop to handle spurious wakes from condition-variable waits
    while(!w.granted){
        rc = pthread_cond_wait(&w.cv, &m->guard);
        if(rc){
            break;
        }
    }
    // unexpected cond-wait error
    if(rc && !w.granted){
        pi_waiter_t **p = &m->waiters;
        while(*p && *p != &w){
            p = &(*p)->next;
        }
        if(*p){
            *p = w.next;
        } 
    }

    pthread_mutex_unlock(&m->guard);
    pthread_cond_destroy(&w.cv);
    return rc;
}


// restore priority and transfer ownership to highest priority waiter
int pi_mutex_unlock(pi_mutex_t *m){
    if(!m){
        return EINVAL;
    }
    int rc = pthread_mutex_lock(&m->guard);
    if(rc){
        return rc;
    }
    if(!m->locked || !pthread_equal(m->owner, pthread_self())){
        pthread_mutex_unlock(&m->guard);
        return EPERM;
    }

    // restore owners original priority before transferring ownership
    rc = set_priority(m->owner, m->owner_policy, m->owner_base_priority);

    if(rc){
        pthread_mutex_unlock(&m->guard);
        return rc;
    }
    if(!m->waiters){
        m->locked = false;
    }
    else{
        // walk linked list to find highest-priority waiter
        pi_waiter_t **best = &m->waiters;
        for(pi_waiter_t **p = &m->waiters; *p; p = &(*p)->next){
            if((*p)->priority > (*best)->priority){
                best = p;
            }
        }
        pi_waiter_t *w = *best;
        *best = w->next;

        int policy;
        struct sched_param sp;
        rc = pthread_getschedparam(w->thread, &policy, &sp);
        if(rc){
            pthread_mutex_unlock(&m->guard);
            return rc;
        }

        m->owner = w->thread;
        m->owner_policy = policy;
        m->owner_base_priority = w->priority;

        w->granted = true;
        pthread_cond_signal(&w->cv);
    }

    pthread_mutex_unlock(&m->guard);
    return 0;
}

// destroy only when no thread owns or waits for this mutex
int pi_mutex_destroy(pi_mutex_t *m){
    if(!m){
        return EINVAL;
    }
    int rc = pthread_mutex_lock(&m->guard);
    if(rc){
        return rc;
    }

    bool busy = m->locked || m->waiters != NULL;
    pthread_mutex_unlock(&m->guard);
    return busy ? EBUSY : pthread_mutex_destroy(&m->guard);
}




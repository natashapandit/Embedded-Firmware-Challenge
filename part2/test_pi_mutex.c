#define _POSIX_C_SOURCE 200809L
#include "pi_mutex.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pi_mutex_t mutex;
static atomic_int low_holds = 0;
static atomic_int high_started = 0;
static atomic_int high_acquired = 0;
static atomic_int medium_finished = 0;
static atomic_int low_released = 0;
static atomic_int errors = 0;
static pthread_t low_id;

static void pause_ms(long ms){
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static void check(int rc, const char *what){
    if(rc){
        fprintf(stderr, "%s: %s (%d)\n", what, strerror(rc), rc);
        atomic_store(&errors, 1);
    }
}

static void show_priority(const char *label, pthread_t thread){
    int policy;
    struct sched_param sp;
    int rc = pthread_getschedparam(thread, &policy, &sp);
    if(rc){
        check(rc, "pthread_getschedparam");
    }
    else{
        printf("%s: policy=%s priority=%d\n", label, policy == SCHED_FIFO ? "SCHED_FIFO" : "other", sp.sched_priority);
    }
}

static void *low_thread(void *unused){
    (void)unused;
    int rc = pi_mutex_lock(&mutex);
    if(rc){
        check(rc, "low lock");
        return NULL;
    }
    puts("LOW: acquired mutex");
    atomic_store(&low_holds, 1);
    show_priority("LOW initially", pthread_self());

    while(!atomic_load(&high_started)){
        pause_ms(1);
    }
    pause_ms(100);
    show_priority("LOW after HIGH blocks", pthread_self());
    puts("LOW: releasing mutex");
    rc = pi_mutex_unlock(&mutex);
    check(rc, "low unlock");
    atomic_store(&low_released, 1);
    show_priority("LOW after unlock", pthread_self());
    return NULL;
}

static void *high_thread(void *unused){
    (void)unused;
    while(!atomic_load(&low_holds)){
        pause_ms(1);
    }
    puts("HIGH: trying to acquire mutex (should block)");
    atomic_store(&high_started, 1);
    int rc = pi_mutex_lock(&mutex);
    if(rc){
        check(rc, "high lock");
        return NULL;
    }
    puts("HIGH: acquired mutex after LOW released it");
    atomic_store(&high_acquired, 1);
    check(pi_mutex_unlock(&mutex), "high unlock");
    return NULL;
}

static void *medium_thread(void *unused){
    (void)unused;
    while(!atomic_load(&high_started)){
        pause_ms(1);
    }
    puts("MEDIUM: doing CPU work without using the mutex");
    volatile unsigned long x = 0;
    for(unsigned long i = 0; i < 10000000UL; i++){
        x += 1;
    }
    (void)x;
    atomic_store(&medium_finished, 1);
    puts("MEDIUM: finished CPU work");
    return NULL;
}

int main(void){
    int rc = pi_mutex_init(&mutex);
    if(rc) {
        check(rc, "pi_mutex_init");
        return 1;
    }

    const int priorities[] = {10, 20, 30};
    void *(*workers[])(void *) = {low_thread, medium_thread, high_thread};
    pthread_t threads[3];
    pthread_attr_t attrs[3];
    int created = 0;

    for(int i = 0; i < 3; i++){
        rc = pthread_attr_init(&attrs[i]);
        if(rc){
            check(rc, "pthread_attr_init");
            break;
        }
        rc = pthread_attr_setinheritsched(&attrs[i], PTHREAD_EXPLICIT_SCHED);
        if(!rc){
            rc = pthread_attr_setschedpolicy(&attrs[i], SCHED_FIFO);
        }
        struct sched_param sp = {.sched_priority = priorities[i]};
        if(!rc){
            rc = pthread_attr_setschedparam(&attrs[i], &sp);
        }
        if(rc){
            check(rc, "configure scheduling attributes");
            pthread_attr_destroy(&attrs[i]);
            break;
        }
        rc = pthread_create(&threads[i], &attrs[i], workers[i], NULL);
        pthread_attr_destroy(&attrs[i]);
        if(rc){
            if(rc == EPERM){
                fputs("SKIP: OS denied SCHED_FIFO thread creation (EPERM).\n"
                      "Real priority inheritance requires real-time scheduling permission.\n"
                      "Try on Linux with CAP_SYS_NICE or suitable real-time limits.\n", stderr);
            }
            else{
                check(rc, "pthread_create");
            }
            break;
        }
        ++created;
        if(i == 0){
            low_id = threads[i];
        }
    }

    if(created != 3){
        atomic_store(&high_started, 1);
        for(int i = 0; i < created; ++i){
            pthread_join(threads[i], NULL);
        }
        pi_mutex_destroy(&mutex);
        return 2;
    }

    for(int i = 0; i < 3; ++i){
        pthread_join(threads[i], NULL);
    }
    rc = pi_mutex_destroy(&mutex);
    check(rc, "pi_mutex_destroy");
    if(!atomic_load(&high_acquired) || !atomic_load(&low_released)){
        fputs("FAIL: expected mutex handoff did not complete\n", stderr);
        return 1;
    }
    if(atomic_load(&errors)){
        return 1;
    }
    puts("PASS: HIGH acquired mutex after LOW released it");
    puts("Check priority log: LOW should rise from 10 to 30, then return to 10.");
    return 0;
}
#include "spsc_ringbuf.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sched.h>

#define COUNT 100000
static ringbuf_t shared;
static uint8_t shared_storage[64];

static void *producer(void *unused){
    (void)unused;
    for(size_t i = 0; i < COUNT; ++i){
        while(!ringbuf_push(&shared, (uint8_t)(i % 256))){
            sched_yield();
        }
    }
    return NULL;
}

static void *consumer(void *unused){
    (void)unused;
    for(size_t i = 0; i< COUNT; ++i){
        uint8_t value;
        while(!ringbuf_pop(&shared, &value)){
            sched_yield();
        }
        assert(value == (uint8_t)(i % 256));
    }
    return NULL;
}

int main(void){
    uint8_t storage[5];
    ringbuf_t rb;
    uint8_t out = 0;
    ringbuf_init(&rb, storage, 5);

    assert(ringbuf_get_used(&rb) == 0);
    assert(ringbuf_get_free(&rb) == 4);
    assert(!ringbuf_pop(&rb, &out));
    puts("PASS: initially empty");

    for(uint8_t i = 1; i <= 4; i++){
        assert(ringbuf_push(&rb, i));
    }
    assert(!ringbuf_push(&rb, 99));
    assert(ringbuf_get_used(&rb) == 4);
    assert(ringbuf_get_free(&rb) == 0);
    puts("PASS: full detection and capacity");

    for(uint8_t i = 1; i <= 2; i++){
        assert(ringbuf_pop(&rb, &out));
        assert(out == i);
    }
    assert(ringbuf_push(&rb, 5));
    assert(ringbuf_push(&rb, 6));
    for(uint8_t i = 3; i <= 6; i++){
        assert(ringbuf_pop(&rb, &out));
        assert(out == i);
    }
    assert(!ringbuf_pop(&rb, &out));
    assert(!ringbuf_pop(&rb, NULL));
    puts("PASS: FIFO order, wraparound, empty detection");

    uint8_t tiny[1];
    ringbuf_init(&rb, tiny, 1);
    assert(!ringbuf_push(&rb, 42));
    assert(!ringbuf_pop(&rb, &out));
    assert(ringbuf_get_used(&rb) == 0);
    assert(ringbuf_get_free(&rb) == 0);
    puts("PASS: capacity-one edge case");

    ringbuf_init(&shared, shared_storage, sizeof shared_storage);
    pthread_t prod, cons;
    assert(pthread_create(&prod, NULL, producer, NULL) == 0);
    assert(pthread_create(&cons, NULL, consumer, NULL) == 0);
    assert(pthread_join(prod, NULL) == 0);
    assert(pthread_join(cons, NULL) == 0);
    assert(ringbuf_get_used(&shared) == 0);
    puts("PASS: 100,000 concurrent SPSC transfers");
    puts("All ring buffer tests PASS");
    return 0;
}
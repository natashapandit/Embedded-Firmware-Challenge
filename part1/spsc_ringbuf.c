#include "spsc_ringbuf.h"

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t capacity){
    rb->buffer = storage; // storage contains the memory location for the buffer
    // atomic initialization for head and tail
    atomic_init(&rb->head, 0); 
    atomic_init(&rb->tail, 0);
    rb->capacity = capacity;
}

bool ringbuf_push(ringbuf_t *rb, uint8_t byte){ // producer - enqueue
    if(rb->capacity < 2){ // capacity of 1 cant hold any elements
        return false;
    }

    // atomic read tail and head
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t next_tail = (tail + 1) % rb->capacity; // updates where tail is pointing and wraps around if capacity reached (ex: 5%5 = 0)
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

    if(next_tail == head){
        return false;
    }
    rb->buffer[tail] = byte; // push new val to buffer
    atomic_store_explicit(&rb->tail, next_tail, memory_order_release); // update tail

    return true;
}

bool ringbuf_pop(ringbuf_t *rb, uint8_t *out){ // consumer - dequeue
    if(rb->capacity < 2 || out == NULL){ // capacity of 1 cant hold any elements
        return false;
    }

    // atomic read head and tail
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t next_head = (head + 1) % rb->capacity; // updates where head is pointing and wraps around if capacity reached (ex: 5%5 = 0)
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    if(head == tail){
        return false;
    }

    *out = rb->buffer[head]; // assign popped value to out
    
    atomic_store_explicit(&rb->head, next_head, memory_order_release); // update head

    return true;
}

size_t ringbuf_get_used(const ringbuf_t *rb){
    if(rb->capacity <2){ // prevent modulo by 0
        return 0;
    }

    // read atomic indices
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    if(tail >= head){ // avoid unsigned subtraction underflow
        return tail - head;
    }
    
    return rb->capacity - head + tail;
}

size_t ringbuf_get_free(const ringbuf_t *rb){
    if(rb->capacity < 2){ // invalid capacity
        return 0;
    }

    return rb->capacity - 1 - ringbuf_get_used(rb); // capacity minus tail minus indices used
}
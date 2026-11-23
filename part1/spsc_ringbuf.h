#ifndef SPSC_RINGBUF_H
#define SPSC_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h> // atomic variable - can be accessed by multiple execution contexts at a time safely

typedef struct {
    uint8_t *buffer; // caller provides storage, initialize as pointer for now
    _Atomic size_t head; // index of next byte consumer will read
    _Atomic size_t tail; // index where producer will write next byte
    size_t capacity;
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t capacity);
bool ringbuf_push(ringbuf_t *rb, uint8_t byte); // producer - enqueue
bool ringbuf_pop(ringbuf_t *rb, uint8_t *out); // consumer - dequeue

size_t ringbuf_get_used(const ringbuf_t *rb);
size_t ringbuf_get_free(const ringbuf_t *rb);

#endif
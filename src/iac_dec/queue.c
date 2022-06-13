#include "queue.h"

#include <stdlib.h>
#include <string.h>

#define QUEUE_CAPACITY  8

struct Queue {
    uint32_t front;
    uint32_t tail;
    uint32_t length;
    uint32_t capacity;

    void   **elem;
};

Queue* queue_new(uint32_t n)
{
    Queue *q = 0;
    q = (Queue *) malloc (sizeof(Queue));
    if (q) {
        memset(q, 0, sizeof (Queue));
        q->capacity = (n + QUEUE_CAPACITY - 1) >> 3 << 3;
        q->elem = (void **) malloc (sizeof (void *) * q->capacity);
        if (!q->elem) {
            free (q);
            q = 0;
        }
    }
    return q;
}

int queue_push(Queue* q, void *e)
{
    if (!q) return -5;
    if (!e) return -1;
    if (q->length == q->capacity) return -2;

    q->elem[q->tail] = e;
    ++q->length;
    q->tail = (q->tail + 1) % q->capacity;

    return 0;
}

void* queue_pop(Queue* q)
{
    if (!q || !q->length) return 0;

    void *e = q->elem[q->front];
    --q->length;
    q->front = (q->front + 1) % q->capacity;

    return e;
}

int queue_length(Queue* q)
{
    if (!q) return 0;
    return q->length;
}

void queue_free(Queue* q, free_func f)
{
    if (q) {
        if (q->elem) {
            if (f) {
                void *e = 0;
                while ((e = queue_pop(q)))
                    (*f)(e);
            }
            free (q->elem);
        }
        free (q);
    }
}

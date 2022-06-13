#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdint.h>

typedef struct Queue Queue;
typedef void (*free_func)(void *elem);

Queue*  queue_new(uint32_t n);
int     queue_push(Queue*, void*);
void*   queue_pop(Queue*);
int     queue_length(Queue*);
void    queue_free(Queue*, free_func);
#endif /* _QUEUE_H_ */

#ifndef PQUEUE_H
#define PQUEUE_H
#include <stddef.h>

typedef int T;
typedef struct PriorityQueue PriorityQueue;

enum {
    PQUEUE_OK = 0,
    PQUEUE_OVERFLOW,
    PQUEUE_UNDERFLOW,
    PQUEUE_MALLOC_FAILURE
};

PriorityQueue *pqueue_new(size_t maxsize);
void pqueue_delete(PriorityQueue *pq);
int pqueue_push(PriorityQueue *pq, T item, int priority);
int pqueue_pop(PriorityQueue *pq, T *item);
int pqueue_peek(PriorityQueue *pq, T *item);
int pqueue_foreach(PriorityQueue* pq, int (*action)(PriorityQueue *q,
                                                    T el,
                                                    void *ctx), void *ctx);
#endif /* ifndef PQUEUE_H */

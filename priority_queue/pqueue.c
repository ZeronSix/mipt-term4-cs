#include "pqueue.h"
#include <assert.h>
#include <stdlib.h>

typedef struct PQItem {
    T item;
    int priority;
} PQItem;

struct PriorityQueue {
    size_t max_size;
    size_t size;
    PQItem data[1];
};

PriorityQueue *pqueue_new(size_t max_size) {
    assert(max_size > 0 && "max_size should be greater than 0!");

    PriorityQueue *pq = (PriorityQueue *)calloc(1, sizeof(PriorityQueue) + sizeof(PQItem) * (max_size - 1));
    if (!pq)
        return NULL;

    /*
    PQItem *data = (PQItem *)calloc(max_size, sizeof(PQItem));
    if (!data) {
        free(pq);
        return NULL;
    } */
    pq->max_size = max_size;
    pq->size = 0;
    //pq->data = data;
    return pq;
}

void pqueue_delete(PriorityQueue *pq) {
    assert(pq && "pq should be non-NULL!");
    //free(pq->data);
    free(pq);
}

#define PQUEUE_VERIFY(pq) { \
    assert(pq && "pq should be non-NULL!"); \
    assert(pq->data && "pq should be non-NULL!"); \
}

#define PQUEUE_LEFT(i) (2 * i + 1)
#define PQUEUE_RIGHT(i) (2 * i + 2)
#define PQUEUE_PARENT(i) ((i + 1) / 2 - 1)

static void pqueue_max_heapify(PriorityQueue *pq, size_t i) {
    PQUEUE_VERIFY(pq);
    size_t l = PQUEUE_LEFT(i);
    size_t r = PQUEUE_RIGHT(i);
    size_t largest = 0;

    if (l < pq->size && pq->data[l].priority > pq->data[i].priority) {
        largest = l;
    }
    else {
        largest = i;
    }
    if (r < pq->size && pq->data[r].priority > pq->data[largest].priority) {
        largest = r;
    }
    if (largest != i) {
        PQItem tmp = pq->data[i];
        pq->data[i] = pq->data[largest];
        pq->data[largest] = tmp;
        pqueue_max_heapify(pq, largest);
    }
}

int pqueue_push(PriorityQueue *pq, T item, int priority) {
    PQUEUE_VERIFY(pq);

    if (pq->size >= pq->max_size) {
        return PQUEUE_OVERFLOW;
    }
    pq->data[pq->size].item = item;
    pq->data[pq->size].priority = priority;
    pq->size++;

    int i = pq->size - 1;
    while (i > 0 &&
           pq->data[PQUEUE_PARENT(i)].priority < pq->data[i].priority) {
        PQItem tmp = pq->data[PQUEUE_PARENT(i)];
        pq->data[PQUEUE_PARENT(i)] = pq->data[i];
        pq->data[i] = tmp;
        i = PQUEUE_PARENT(i);
    }

    return PQUEUE_OK;
}

int pqueue_pop(PriorityQueue *pq, T *item) {
    PQUEUE_VERIFY(pq);

    if (item) {
        int result = pqueue_peek(pq, item);
        if (result != PQUEUE_OK) {
            return result;
        }
    }

    pq->size--;
    if (pq->size != 0) {
        pq->data[0] = pq->data[pq->size];
        pqueue_max_heapify(pq, 0);
    }

    return PQUEUE_OK;
}

int pqueue_peek(PriorityQueue *pq, T *item) {
    PQUEUE_VERIFY(pq);
    assert(item && "Item should be NON-NULL!");

    if (pq->size == 0) {
        return PQUEUE_UNDERFLOW;
    }
    *item = pq->data[0].item;

    return PQUEUE_OK;
}

int pqueue_foreach(PriorityQueue *pq, int (*action)(PriorityQueue *q,
                                                    T el,
                                                    void *ctx), void *ctx) {
    PQUEUE_VERIFY(pq);
    assert(action && "Action should be NON-NULL!");

    size_t size = pq->size;
    while (pq->size > 0) {
        action(pq, pq->data[0].item, ctx);
        PQItem tmp = pq->data[0];
        pqueue_pop(pq, NULL);
        pq->data[pq->size] = tmp;
    }
    pq->size = size;
    pqueue_max_heapify(pq, 0);

    return PQUEUE_OK;
}

#undef PQUEUE_VERIFY

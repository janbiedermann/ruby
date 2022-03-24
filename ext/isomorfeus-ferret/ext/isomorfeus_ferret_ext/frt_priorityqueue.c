#include <string.h>
#include "frt_priorityqueue.h"

#define START_CAPA 127

FrtPriorityQueue *frt_pq_new(int capa, frt_lt_ft less_than, frt_free_ft free_elem)
{
    FrtPriorityQueue *pq = FRT_ALLOC(FrtPriorityQueue);
    pq->size = 0;
    pq->capa = capa;
    pq->mem_capa = (START_CAPA > capa ? capa : START_CAPA) + 1;
    pq->heap = FRT_ALLOC_N(void *, pq->mem_capa);
    memset(pq->heap, 0, (sizeof(void *))*(pq->mem_capa));
    pq->less_than_i = less_than;

    /* need to set this yourself if you want to change it */
    pq->free_elem_i = free_elem ? free_elem : &frt_dummy_free;
    return pq;
}

FrtPriorityQueue *frt_pq_clone(FrtPriorityQueue *pq)
{
    FrtPriorityQueue *new_pq = FRT_ALLOC(FrtPriorityQueue);
    memcpy(new_pq, pq, sizeof(FrtPriorityQueue));
    new_pq->heap = FRT_ALLOC_N(void *, new_pq->mem_capa);
    memcpy(new_pq->heap, pq->heap, sizeof(void *) * (new_pq->size + 1));

    return new_pq;
}

void frt_pq_clear(FrtPriorityQueue *pq)
{
    int i;
    for (i = 1; i <= pq->size; i++) {
        pq->free_elem_i(pq->heap[i]);
        pq->heap[i] = NULL;
    }
    pq->size = 0;
}

void frt_pq_free(FrtPriorityQueue *pq)
{
    free(pq->heap);
    free(pq);
}

void frt_pq_destroy(FrtPriorityQueue *pq)
{
    frt_pq_clear(pq);
    frt_pq_free(pq);
}

/**
 * This method is used internally by frt_pq_push. It is similar to frt_pq_down except
 * that where frt_pq_down reorders the elements from the top, pq_up reorders from
 * the bottom.
 *
 * @param pq the PriorityQueue to reorder
 */
static void frt_pq_up(FrtPriorityQueue *pq)
{
    void **heap = pq->heap;
    void *node;
    int i = pq->size;
    int j = i >> 1;

    node = heap[i];

    while ((j > 0) && pq->less_than_i(node, heap[j])) {
        heap[i] = heap[j];
        i = j;
        j = j >> 1;
    }
    heap[i] = node;
}

void frt_pq_down(FrtPriorityQueue *pq)
{
    register int i = 1;
    register int j = 2;         /* i << 1; */
    register int k = 3;         /* j + 1;  */
    register int size = pq->size;
    void **heap = pq->heap;
    void *node = heap[i];       /* save top node */

    if ((k <= size) && (pq->less_than_i(heap[k], heap[j]))) {
        j = k;
    }

    while ((j <= size) && pq->less_than_i(heap[j], node)) {
        heap[i] = heap[j];      /* shift up child */
        i = j;
        j = i << 1;
        k = j + 1;
        if ((k <= size) && pq->less_than_i(heap[k], heap[j])) {
            j = k;
        }
    }
    heap[i] = node;
}

void frt_pq_push(FrtPriorityQueue *pq, void *elem)
{
    pq->size++;
    if (pq->size >= pq->mem_capa) {
        pq->mem_capa <<= 1;
        FRT_REALLOC_N(pq->heap, void *, pq->mem_capa);
    }
    pq->heap[pq->size] = elem;
    frt_pq_up(pq);
}

FrtPriorityQueueInsertEnum frt_pq_insert(FrtPriorityQueue *pq, void *elem)
{
    if (pq->size < pq->capa) {
        frt_pq_push(pq, elem);
        return FRT_PQ_ADDED;
    }

    if (pq->size > 0 && pq->less_than_i(pq->heap[1], elem)) {
        pq->free_elem_i(pq->heap[1]);
        pq->heap[1] = elem;
        frt_pq_down(pq);
        return FRT_PQ_INSERTED;
    }

    pq->free_elem_i(elem);
    return FRT_PQ_DROPPED;
}

void *frt_pq_top(FrtPriorityQueue *pq)
{
    return pq->size ? pq->heap[1] : NULL;
}

void *frt_pq_pop(FrtPriorityQueue *pq)
{
    if (pq->size > 0) {
        void *result = pq->heap[1];       /* save first value */
        pq->heap[1] = pq->heap[pq->size]; /* move last to first */
        pq->heap[pq->size] = NULL;
        pq->size--;
        frt_pq_down(pq);                      /* adjust heap */
        return result;
    }
    return NULL;
}

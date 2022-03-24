#include "frt_hashset.h"
#include <string.h>

/*
 * The HashSet contains an array +elems+ of the elements that have been added.
 * It always has +size+ elements so +size+ ane +elems+ can be used to iterate
 * over all alements in the HashSet. It also uses a Hash to keep track of
 * which elements have been added and their index in the +elems+ array.
 */
static FrtHashSet *hs_alloc(frt_free_ft free_func)
{
    FrtHashSet *hs = FRT_ALLOC(FrtHashSet);
    hs->size = 0;
    hs->first = hs->last = NULL;
    hs->free_elem_i = free_func ? free_func : &frt_dummy_free;
    return hs;
}

FrtHashSet *frt_hs_new(frt_hash_ft hash_func, frt_eq_ft eq_func, frt_free_ft free_func)
{
    FrtHashSet *hs = hs_alloc(free_func);
    hs->ht = frt_h_new(hash_func, eq_func, NULL, NULL);
    return hs;
}

FrtHashSet *frt_hs_new_str(frt_free_ft free_func)
{
    FrtHashSet *hs = hs_alloc(free_func);
    hs->ht = frt_h_new_str((frt_free_ft) NULL, NULL);
    return hs;
}

FrtHashSet *frt_hs_new_ptr(frt_free_ft free_func)
{
    FrtHashSet *hs = hs_alloc(free_func);
    hs->ht = frt_h_new_ptr(NULL);
    return hs;
}

static void clear(FrtHashSet *hs, bool destroy)
{
    FrtHashSetEntry *curr, *next = hs->first;
    frt_free_ft do_free = destroy ? hs->free_elem_i : &frt_dummy_free;
    while (NULL != (curr = next)) {
        next = curr->next;
        do_free(curr->elem);
        free(curr);
    }
    hs->first = hs->last = NULL;
    hs->size = 0;
}

void frt_hs_clear(FrtHashSet *hs)
{
    clear(hs, true);
    frt_h_clear(hs->ht);
}

void frt_hs_free(FrtHashSet *hs)
{
    clear(hs, false);
    frt_h_destroy(hs->ht);
    free(hs);
}

void frt_hs_destroy(FrtHashSet *hs)
{
    clear(hs, true);
    frt_h_destroy(hs->ht);
    free(hs);
}

static void append(FrtHashSet *hs, void *elem)
{
    FrtHashSetEntry *entry = FRT_ALLOC(FrtHashSetEntry);
    entry->elem = elem;
    entry->prev = hs->last;
    entry->next = NULL;
    if (!hs->first) {
        hs->first = hs->last = entry;
    }
    else {
        hs->last->next = entry;
        hs->last = entry;
    }
    frt_h_set(hs->ht, elem, entry);
    hs->size++;
}

FrtHashKeyStatus frt_hs_add(FrtHashSet *hs, void *elem)
{
    FrtHashKeyStatus has_elem = frt_h_has_key(hs->ht, elem);
    switch (has_elem)
    {
        /* We don't want to keep two of the same elem so free if necessary */
        case FRT_HASH_KEY_EQUAL:
            hs->free_elem_i(elem);
            return has_elem;

        /* No need to do anything */
        case FRT_HASH_KEY_SAME:
            return has_elem;

        /* add the elem to the array, resizing if necessary */
        case FRT_HASH_KEY_DOES_NOT_EXIST:
            break;

    }

    append(hs, elem);
    return has_elem;
}

int frt_hs_add_safe(FrtHashSet *hs, void *elem)
{
    switch(frt_h_has_key(hs->ht, elem))
    {
        /* element can't be added */
        case FRT_HASH_KEY_EQUAL: return false;

        /* the exact same element has already been added */
        case FRT_HASH_KEY_SAME : return true;

        /* add the elem to the array, resizing if necessary */
        case FRT_HASH_KEY_DOES_NOT_EXIST : break;
    }
    append(hs, elem);
    return true;
}

void *frt_hs_rem(FrtHashSet *hs, const void *elem)
{
    void *return_elem;
    FrtHashSetEntry *entry = (FrtHashSetEntry *)frt_h_get(hs->ht, elem);
    if (entry == NULL) return NULL;

    if (hs->first == hs->last) {
        hs->first = hs->last = NULL;
    }
    else if (hs->first == entry) {
        hs->first = entry->next;
        hs->first->prev = NULL;
    }
    else if (hs->last == entry) {
        hs->last = entry->prev;
        hs->last->next = NULL;
    }
    else {
        entry->prev->next = entry->next;
        entry->next->prev = entry->prev;
    }
    return_elem = entry->elem;
    frt_h_del(hs->ht, return_elem);
    free(entry);
    hs->size--;
    return return_elem;
}

int frt_hs_del(FrtHashSet *hs, const void *elem)
{
    void *tmp_elem = frt_hs_rem(hs, elem);
    if (tmp_elem != NULL) {
        hs->free_elem_i(tmp_elem);
        return 1;
    }
    return 0;
}

FrtHashKeyStatus frt_hs_exists(FrtHashSet *hs, const void *elem)
{
    return frt_h_has_key(hs->ht, elem);
}

FrtHashSet *frt_hs_merge(FrtHashSet *hs, FrtHashSet * other)
{
    FrtHashSetEntry *entry = other->first;
    for (; entry != NULL; entry = entry->next) {
        frt_hs_add(hs, entry->elem);
    }
    /* Now free the other hashset. It is no longer needed. No need, however,
     * to delete the elements as they were either destroyed or added to the
     * new hashset.  */
    frt_hs_free(other);
    return hs;
}

void *frt_hs_orig(FrtHashSet *hs, const void *elem)
{
    FrtHashSetEntry *entry = (FrtHashSetEntry *)frt_h_get(hs->ht, elem);
    return entry ? entry->elem : NULL;
}

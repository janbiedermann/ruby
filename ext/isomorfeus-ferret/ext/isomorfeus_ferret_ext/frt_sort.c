#include <string.h>
#include "frt_search.h"
#include "frt_index.h"
#include "frt_field_index.h"

/***************************************************************************
 *
 * FrtSortField
 *
 ***************************************************************************/

static FrtSortField *sort_field_alloc(FrtSymbol field,
    SortType type,
    bool reverse,
    int (*compare)(void *index_ptr, FrtHit *hit1, FrtHit *hit2),
    void (*get_val)(void *index_ptr, FrtHit *hit1, FrtComparable *comparable),
    const FrtFieldIndexClass *field_index_class)
{
    FrtSortField *self      = FRT_ALLOC(FrtSortField);
    self->field             = field;
    self->type              = type;
    self->reverse           = reverse;
    self->field_index_class = field_index_class;
    self->compare           = compare;
    self->get_val           = get_val;
    return self;
}

FrtSortField *frt_sort_field_new(FrtSymbol field, SortType type, bool reverse)
{
    FrtSortField *sf = NULL;
    switch (type) {
        case FRT_SORT_TYPE_SCORE:
            sf = frt_sort_field_score_new(reverse);
            break;
        case FRT_SORT_TYPE_DOC:
            sf = frt_sort_field_doc_new(reverse);
            break;
        case FRT_SORT_TYPE_BYTE:
            sf = frt_sort_field_byte_new(field, reverse);
            break;
        case FRT_SORT_TYPE_INTEGER:
            sf = frt_sort_field_int_new(field, reverse);
            break;
        case FRT_SORT_TYPE_FLOAT:
            sf = frt_sort_field_float_new(field, reverse);
            break;
        case FRT_SORT_TYPE_STRING:
            sf = frt_sort_field_string_new(field, reverse);
            break;
        case FRT_SORT_TYPE_AUTO:
            sf = frt_sort_field_auto_new(field, reverse);
            break;
    }
    return sf;
}

void frt_sort_field_destroy(void *p)
{
    free(p);
}

/*
 * field:<type>!
 */
char *frt_sort_field_to_s(FrtSortField *self)
{
    char *str;
    const char *type = NULL;
    switch (self->type) {
        case FRT_SORT_TYPE_SCORE:
            type = "<SCORE>";
            break;
        case FRT_SORT_TYPE_DOC:
            type = "<DOC>";
            break;
        case FRT_SORT_TYPE_BYTE:
            type = "<byte>";
            break;
        case FRT_SORT_TYPE_INTEGER:
            type = "<integer>";
            break;
        case FRT_SORT_TYPE_FLOAT:
            type = "<float>";
            break;
        case FRT_SORT_TYPE_STRING:
            type = "<string>";
            break;
        case FRT_SORT_TYPE_AUTO:
            type = "<auto>";
            break;
    }
    if (self->field) {
        const char *field_name = rb_id2name(self->field);
        str = FRT_ALLOC_N(char, 3 + strlen(field_name) + strlen(type));
        sprintf(str, "%s:%s%s", field_name, type, (self->reverse ? "!" : ""));
    }
    else {
        str = FRT_ALLOC_N(char, 2 + strlen(type));
        sprintf(str, "%s%s", type, (self->reverse ? "!" : ""));
    }
    return str;
}

/***************************************************************************
 * ScoreSortField
 ***************************************************************************/

static void sf_score_get_val(void *index, FrtHit *hit, FrtComparable *comparable)
{
    (void)index;
    comparable->val.f = hit->score;
}

static int sf_score_compare(void *index_ptr, FrtHit *hit2, FrtHit *hit1)
{
    float val1 = hit1->score;
    float val2 = hit2->score;
    (void)index_ptr;

    if (val1 > val2) return 1;
    else if (val1 < val2) return -1;
    else return 0;
}

FrtSortField *frt_sort_field_score_new(bool reverse)
{
    return sort_field_alloc((ID)NULL, FRT_SORT_TYPE_SCORE, reverse, &sf_score_compare, &sf_score_get_val, NULL);
}

const FrtSortField FRT_SORT_FIELD_SCORE = {
    NULL,               /* field_index_class */
    (ID)NULL,               /* field */
    FRT_SORT_TYPE_SCORE,    /* type */
    false,              /* reverse */
    &sf_score_compare,  /* compare */
    &sf_score_get_val,  /* get_val */
};

const FrtSortField FRT_SORT_FIELD_SCORE_REV = {
    NULL,               /* field_index_class */
    (ID)NULL,               /* field */
    FRT_SORT_TYPE_SCORE,    /* type */
    true,               /* reverse */
    &sf_score_compare,  /* compare */
    &sf_score_get_val,  /* get_val */
};

/**************************************************************************
 * DocSortField
 ***************************************************************************/

static void sf_doc_get_val(void *index, FrtHit *hit, FrtComparable *comparable)
{
    (void)index;
    comparable->val.l = hit->doc;
}

static int sf_doc_compare(void *index_ptr, FrtHit *hit1, FrtHit *hit2)
{
    int val1 = hit1->doc;
    int val2 = hit2->doc;
    (void)index_ptr;

    if (val1 > val2) return 1;
    else if (val1 < val2) return -1;
    else return 0;
}

FrtSortField *frt_sort_field_doc_new(bool reverse)
{
    return sort_field_alloc((ID)NULL, FRT_SORT_TYPE_DOC, reverse,
                            &sf_doc_compare, &sf_doc_get_val, NULL);
}

const FrtSortField FRT_SORT_FIELD_DOC = {
    NULL,               /* field_index_class */
    (ID)NULL,               /* field */
    FRT_SORT_TYPE_DOC,      /* type */
    false,              /* reverse */
    &sf_doc_compare,    /* compare */
    &sf_doc_get_val,    /* get_val */
};

const FrtSortField FRT_SORT_FIELD_DOC_REV = {
    NULL,               /* field_index_class */
    (ID)NULL,               /* field */
    FRT_SORT_TYPE_DOC,      /* type */
    true,               /* reverse */
    &sf_doc_compare,    /* compare */
    &sf_doc_get_val,    /* get_val */
};

/***************************************************************************
 * ByteSortField
 ***************************************************************************/

static void sf_byte_get_val(void *index, FrtHit *hit, FrtComparable *comparable)
{
    comparable->val.l = ((long *)index)[hit->doc];
}

static int sf_byte_compare(void *index, FrtHit *hit1, FrtHit *hit2)
{
    long val1 = ((long *)index)[hit1->doc];
    long val2 = ((long *)index)[hit2->doc];
    if (val1 > val2) return 1;
    else if (val1 < val2) return -1;
    else return 0;
}

FrtSortField *frt_sort_field_byte_new(FrtSymbol field, bool reverse)
{
    return sort_field_alloc(field, FRT_SORT_TYPE_BYTE, reverse,
                            &sf_byte_compare, &sf_byte_get_val,
                            &FRT_BYTE_FIELD_INDEX_CLASS);
}

/***************************************************************************
 * IntegerSortField
 ***************************************************************************/

static void sf_int_get_val(void *index, FrtHit *hit, FrtComparable *comparable)
{
    comparable->val.l = ((long *)index)[hit->doc];
}

static int sf_int_compare(void *index, FrtHit *hit1, FrtHit *hit2)
{
    long val1 = ((long *)index)[hit1->doc];
    long val2 = ((long *)index)[hit2->doc];
    if (val1 > val2) return 1;
    else if (val1 < val2) return -1;
    else return 0;
}

FrtSortField *frt_sort_field_int_new(FrtSymbol field, bool reverse)
{
    return sort_field_alloc(field, FRT_SORT_TYPE_INTEGER, reverse,
                            &sf_int_compare, &sf_int_get_val,
                            &FRT_INTEGER_FIELD_INDEX_CLASS);
}

/***************************************************************************
 * FloatSortField
 ***************************************************************************/

static void sf_float_get_val(void *index, FrtHit *hit, FrtComparable *comparable)
{
    comparable->val.f = ((float *)index)[hit->doc];
}

static int sf_float_compare(void *index, FrtHit *hit1, FrtHit *hit2)
{
    float val1 = ((float *)index)[hit1->doc];
    float val2 = ((float *)index)[hit2->doc];
    if (val1 > val2) return 1;
    else if (val1 < val2) return -1;
    else return 0;
}

FrtSortField *frt_sort_field_float_new(FrtSymbol field, bool reverse)
{
    return sort_field_alloc(field, FRT_SORT_TYPE_FLOAT, reverse,
                            &sf_float_compare, &sf_float_get_val,
                            &FRT_FLOAT_FIELD_INDEX_CLASS);
}

/***************************************************************************
 * StringSortField
 ***************************************************************************/

static void sf_string_get_val(void *index, FrtHit *hit, FrtComparable *comparable)
{
    comparable->val.s
        = ((FrtStringIndex *)index)->values[
        ((FrtStringIndex *)index)->index[hit->doc]];
}

static int sf_string_compare(void *index, FrtHit *hit1, FrtHit *hit2)
{
    char *s1 = ((FrtStringIndex *)index)->values[
        ((FrtStringIndex *)index)->index[hit1->doc]];
    char *s2 = ((FrtStringIndex *)index)->values[
        ((FrtStringIndex *)index)->index[hit2->doc]];

    if (s1 == NULL) return s2 ? 1 : 0;
    if (s2 == NULL) return -1;

#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
    return strcmp(s1, s2);
#else
    return strcoll(s1, s2);
#endif

    /*
     * TODO: investigate whether it would be a good idea to presort strings.
     *
    int val1 = index->index[hit1->doc];
    int val2 = index->index[hit2->doc];
    if (val1 > val2) return 1;
    else if (val1 < val2) return -1;
    else return 0;
    */
}

FrtSortField *frt_sort_field_string_new(FrtSymbol field, bool reverse)
{
    return sort_field_alloc(field, FRT_SORT_TYPE_STRING, reverse,
                            &sf_string_compare, &sf_string_get_val,
                            &FRT_STRING_FIELD_INDEX_CLASS);
}

/***************************************************************************
 * AutoSortField
 ***************************************************************************/

FrtSortField *frt_sort_field_auto_new(FrtSymbol field, bool reverse)
{
    return sort_field_alloc(field, FRT_SORT_TYPE_AUTO, reverse, NULL, NULL, NULL);
}

/***************************************************************************
 *
 * FieldSortedHitQueue
 *
 ***************************************************************************/

/***************************************************************************
 * Comparator
 ***************************************************************************/

typedef struct Comparator {
    void *index;
    bool  reverse : 1;
    int   (*compare)(void *index_ptr, FrtHit *hit1, FrtHit *hit2);
} Comparator;

static Comparator *comparator_new(void *index, bool reverse,
                  int (*compare)(void *index_ptr, FrtHit *hit1, FrtHit *hit2))
{
    Comparator *self = FRT_ALLOC(Comparator);
    self->index = index;
    self->reverse = reverse;
    self->compare = compare;
    return self;
}

/***************************************************************************
 * Sorter
 ***************************************************************************/

typedef struct Sorter {
    Comparator **comparators;
    int c_cnt;
    FrtSort *sort;
} Sorter;

#define SET_AUTO(upper_type, lower_type) \
    sf->type = FRT_SORT_TYPE_ ## upper_type;\
    sf->field_index_class = &FRT_ ## upper_type ## _FIELD_INDEX_CLASS;\
    sf->compare = sf_ ## lower_type ## _compare;\
    sf->get_val = sf_ ## lower_type ## _get_val

static void sort_field_auto_evaluate(FrtSortField *sf, char *text)
{
    int int_val;
    float float_val;
    int text_len = 0, scan_len = 0;

    text_len = (int)strlen(text);
    sscanf(text, "%d%n", &int_val, &scan_len);
    if (scan_len == text_len) {
        SET_AUTO(INTEGER, int);
    } else {
        sscanf(text, "%f%n", &float_val, &scan_len);
        if (scan_len == text_len) {
            SET_AUTO(FLOAT, float);
        } else {
            SET_AUTO(STRING, string);
        }
    }
}

static Comparator *sorter_get_comparator(FrtSortField *sf, FrtIndexReader *ir)
{
    void *index = NULL;
    if (sf->type > FRT_SORT_TYPE_DOC) {
        FrtFieldIndex *field_index = NULL;
        if (sf->type == FRT_SORT_TYPE_AUTO) {
            FrtTermEnum *te = frt_ir_terms(ir, sf->field);
            if (te) {
                if (!te->next(te) && (ir->num_docs(ir) > 0)) {
                    FRT_RAISE(FRT_ARG_ERROR,
                        "Cannot sort by field \"%s\" as there are no terms "
                        "in that field in the index.", rb_id2name(sf->field));
                }
                sort_field_auto_evaluate(sf, te->curr_term);
                te->close(te);
            }
        }
        frt_mutex_lock(&ir->field_index_mutex);
        field_index = frt_field_index_get(ir, sf->field, sf->field_index_class);
        frt_mutex_unlock(&ir->field_index_mutex);
        index = field_index->index;
    }
    return comparator_new(index, sf->reverse, sf->compare);
}

static void sorter_destroy(Sorter *self)
{
    int i;

    for (i = 0; i < self->c_cnt; i++) {
        free(self->comparators[i]);
    }
    free(self->comparators);
    free(self);
}

static Sorter *sorter_new(FrtSort *sort)
{
    Sorter *self = FRT_ALLOC(Sorter);
    self->c_cnt = sort->size;
    self->comparators = FRT_ALLOC_AND_ZERO_N(Comparator *, self->c_cnt);
    self->sort = sort;
    return self;
}

/***************************************************************************
 * FieldSortedHitQueue
 ***************************************************************************/

static bool fshq_less_than(const void *hit1, const void *hit2)
{
    int cmp = 0;
    printf("Whoops, shouldn't call this.\n");
    if (cmp != 0) {
        return cmp;
    } else {
        return ((FrtHit *)hit1)->score < ((FrtHit *)hit2)->score;
    }
}

static bool fshq_lt(Sorter *sorter, FrtHit *hit1, FrtHit *hit2)
{
    Comparator *comp;
    int diff = 0, i;
    for (i = 0; i < sorter->c_cnt && diff == 0; i++) {
        comp = sorter->comparators[i];
        if (comp->reverse) {
            diff = comp->compare(comp->index, hit2, hit1);
        } else {
            diff = comp->compare(comp->index, hit1, hit2);
        }
    }

    if (diff != 0) {
        return diff > 0;
    } else {
        return hit1->doc > hit2->doc;
    }
}

void fshq_pq_down(FrtPriorityQueue *pq)
{
    register int i = 1;
    register int j = 2;     /* i << 1; */
    register int k = 3;     /* j + 1; */
    FrtHit **heap = (FrtHit **)pq->heap;
    FrtHit *node = heap[i];    /* save top node */
    Sorter *sorter = (Sorter *)heap[0];

    if ((k <= pq->size) && fshq_lt(sorter, heap[k], heap[j])) {
        j = k;
    }

    while ((j <= pq->size) && fshq_lt(sorter, heap[j], node)) {
        heap[i] = heap[j];  /* shift up child */
        i = j;
        j = i << 1;
        k = j + 1;
        if ((k <= pq->size) && fshq_lt(sorter, heap[k], heap[j])) {
            j = k;
        }
    }
    heap[i] = node;
}

FrtHit *frt_fshq_pq_pop(FrtPriorityQueue *pq)
{
    if (pq->size > 0) {
        FrtHit *hit = (FrtHit *)pq->heap[1];   /* save first value */
        pq->heap[1] = pq->heap[pq->size];   /* move last to first */
        pq->heap[pq->size] = NULL;
        pq->size--;
        fshq_pq_down(pq);                   /* adjust heap */
        return hit;
    } else {
        return NULL;
    }
}

static void fshq_pq_up(FrtPriorityQueue *pq)
{
    FrtHit **heap = (FrtHit **)pq->heap;
    FrtHit *node;
    int i = pq->size;
    int j = i >> 1;
    Sorter *sorter = (Sorter *)heap[0];
    node = heap[i];

    while ((j > 0) && fshq_lt(sorter, node, heap[j])) {
        heap[i] = heap[j];
        i = j;
        j = j >> 1;
    }
    heap[i] = node;
}

void frt_fshq_pq_insert(FrtPriorityQueue *pq, FrtHit *hit)
{
    if (pq->size < pq->capa) {
        FrtHit *new_hit = FRT_ALLOC(FrtHit);
        memcpy(new_hit, hit, sizeof(FrtHit));
        pq->size++;
        if (pq->size >= pq->mem_capa) {
            pq->mem_capa <<= 1;
            FRT_REALLOC_N(pq->heap, void *, pq->mem_capa);
        }
        pq->heap[pq->size] = new_hit;
        fshq_pq_up(pq);
    } else if (pq->size > 0
               && fshq_lt((Sorter *)pq->heap[0], (FrtHit *)pq->heap[1], hit)) {
        memcpy(pq->heap[1], hit, sizeof(FrtHit));
        fshq_pq_down(pq);
    }
}

void frt_fshq_pq_destroy(FrtPriorityQueue *self)
{
    sorter_destroy((Sorter *)self->heap[0]);
    frt_pq_destroy(self);
}

FrtPriorityQueue *frt_fshq_pq_new(int size, FrtSort *sort, FrtIndexReader *ir)
{
    FrtPriorityQueue *self = frt_pq_new(size, &fshq_less_than, &free);
    int i;
    Sorter *sorter = sorter_new(sort);
    FrtSortField *sf;

    for (i = 0; i < sort->size; i++) {
        sf = sort->sort_fields[i];
        sorter->comparators[i] = sorter_get_comparator(sf, ir);
    }
    self->heap[0] = sorter;

    return self;
}

FrtHit *frt_fshq_pq_pop_fd(FrtPriorityQueue *pq)
{
    if (pq->size <= 0) {
        return NULL;
    }
    else {
        int j;
        Sorter *sorter = (Sorter *)pq->heap[0];
        const int cmp_cnt = sorter->c_cnt;
        FrtSortField **sort_fields = sorter->sort->sort_fields;
        FrtHit *hit = (FrtHit *)pq->heap[1];   /* save first value */
        FrtFieldDoc *field_doc;
        FrtComparable *comparables;
        Comparator **comparators = sorter->comparators;
        pq->heap[1] = pq->heap[pq->size];   /* move last to first */
        pq->heap[pq->size] = NULL;
        pq->size--;
        fshq_pq_down(pq);                   /* adjust heap */

        field_doc = (FrtFieldDoc *)frt_emalloc(sizeof(FrtFieldDoc) + sizeof(FrtComparable) * cmp_cnt);
        comparables = field_doc->comparables;
        memcpy(field_doc, hit, sizeof(FrtHit));
        field_doc->size = cmp_cnt;

        for (j = 0; j < cmp_cnt; j++) {
            FrtSortField *sf = sort_fields[j];
            Comparator *comparator = comparators[j];
            sf->get_val(comparator->index, hit, &(comparables[j]));
            comparables[j].type = sf->type;
            comparables[j].reverse = comparator->reverse;
        }
        free(hit);
        return (FrtHit *)field_doc;
    }
}

/***************************************************************************
 * FieldDocSortedHitQueue
 ***************************************************************************/

bool frt_fdshq_lt(FrtFieldDoc *fd1, FrtFieldDoc *fd2)
{
    int i;
    bool c = false;
    bool all_equal = false;
    int sc;
    FrtComparable *cmps1 = fd1->comparables;
    FrtComparable *cmps2 = fd2->comparables;

    for (i = 0; (i < fd1->size) && (!c); i++) {
        int type = cmps1[i].type;
        if (cmps1[i].reverse) {
            switch (type) {
                case FRT_SORT_TYPE_SCORE:
                    if (cmps1[i].val.f > cmps2[i].val.f) { all_equal = false; c = true; }
                    else if (cmps1[i].val.f == cmps2[i].val.f) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_FLOAT:
                    if (cmps1[i].val.f < cmps2[i].val.f) { all_equal = false; c = true; }
                    else if (cmps1[i].val.f == cmps2[i].val.f) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_DOC:
                    if (fd1->hit.doc < fd2->hit.doc) { all_equal = false; c = true; }
                    break;
                case FRT_SORT_TYPE_INTEGER:
                    if (cmps1[i].val.l < cmps2[i].val.l) { all_equal = false; c = true; }
                    else if (cmps1[i].val.l == cmps2[i].val.l) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_BYTE:
                    if (cmps1[i].val.l > cmps2[i].val.l) { all_equal = false; c = true; }
                    else if (cmps1[i].val.l == cmps2[i].val.l) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_STRING:
                    do {
                        char *s1 = cmps1[i].val.s;
                        char *s2 = cmps2[i].val.s;
                        if ((s1 == NULL) && s2) { all_equal = false; }
                        else if (s1 && (s2 == NULL)) { all_equal = false; c = true; }
                        else if (s1 && s2) {
#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
                            sc = strcmp(s1, s2);
#else
                            sc = strcoll(s1, s2);
#endif
                            if (sc < 0) { all_equal = false; c = true; }
                            else if (sc == 0) { if (!c) all_equal = true; }
                            else { all_equal = false; }
                        } else { all_equal = false; }
                    } while (0);
                    break;
                default:
                    FRT_RAISE(FRT_ARG_ERROR, "Unknown sort type: %d.", type);
                    break;
            }
        } else {
            switch (type) {
                case FRT_SORT_TYPE_SCORE:
                    if (cmps1[i].val.f < cmps2[i].val.f) { all_equal = false; c = true; }
                    else if (cmps1[i].val.f == cmps2[i].val.f) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_FLOAT:
                    if (cmps1[i].val.f > cmps2[i].val.f) { all_equal = false; c = true; }
                    else if (cmps1[i].val.f == cmps2[i].val.f) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_DOC:
                    if (fd1->hit.doc > fd2->hit.doc) { all_equal = false; c = true; }
                    break;
                case FRT_SORT_TYPE_INTEGER:
                    if (cmps1[i].val.l > cmps2[i].val.l) { all_equal = false; c = true; }
                    else if (cmps1[i].val.l == cmps2[i].val.l) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_BYTE:
                    if (cmps1[i].val.l < cmps2[i].val.l) { all_equal = false; c = true; }
                    else if (cmps1[i].val.l == cmps2[i].val.l) { if (!c) all_equal = true; }
                    else { all_equal = false; }
                    break;
                case FRT_SORT_TYPE_STRING:
                    do {
                        char *s1 = cmps1[i].val.s;
                        char *s2 = cmps2[i].val.s;
                        if (s1 && (s2 == NULL)) { if (!c) all_equal = false; }
                        else if ((s1 == NULL) && s2) { all_equal = false; c = true; }
                        else if (s1 && s2) {
#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
                            sc = strcmp(s1, s2);
#else
                            sc = strcoll(s1, s2);
#endif
                            if (sc > 0) { all_equal = false; c = true; }
                            else if (sc == 0) { if (!c) all_equal = true; }
                            else { all_equal = false; }
                        } else { all_equal = false; }
                    } while (0);
                    break;
                default:
                    FRT_RAISE(FRT_ARG_ERROR, "Unknown sort type: %d.", type);
                    break;
            }
        }
        if (!all_equal) break;
    }
    if (all_equal) {
        if (cmps1[0].reverse) {
            if (fd1->hit.doc > fd2->hit.doc) c = true;
        } else {
            if (fd1->hit.doc > fd2->hit.doc) c = true;
        }
    }
    return c;
}

/***************************************************************************
 *
 * Sort
 *
 ***************************************************************************/

#define SORT_INIT_SIZE 4

FrtSort *frt_sort_new()
{
    FrtSort *self = FRT_ALLOC(FrtSort);
    self->size = 0;
    self->capa = SORT_INIT_SIZE;
    self->sort_fields = FRT_ALLOC_N(FrtSortField *, SORT_INIT_SIZE);
    self->destroy_all = true;
    self->start = 0;

    return self;
}

void frt_sort_clear(FrtSort *self)
{
    int i;
    if (self->destroy_all) {
        for (i = 0; i < self->size; i++) {
            frt_sort_field_destroy(self->sort_fields[i]);
        }
    }
    self->size = 0;
}

void frt_sort_destroy(void *p)
{
    FrtSort *self = (FrtSort *)p;
    frt_sort_clear(self);
    free(self->sort_fields);
    free(self);
}

void frt_sort_add_sort_field(FrtSort *self, FrtSortField *sf)
{
    if (self->size == self->capa) {
        self->capa <<= 1;
        FRT_REALLOC_N(self->sort_fields, FrtSortField *, self->capa);
    }

    self->sort_fields[self->size] = sf;
    self->size++;
}

char *frt_sort_to_s(FrtSort *self)
{
    int i, len = 20;
    char *s;
    char *str;
    char **sf_strs = FRT_ALLOC_N(char *, self->size);

    for (i = 0; i < self->size; i++) {
        sf_strs[i] = s = frt_sort_field_to_s(self->sort_fields[i]);
        len += (int)strlen(s) + 2;
    }

    str = FRT_ALLOC_N(char, len);
    s = frt_strapp(str, "Sort[");

    for (i = 0; i < self->size; i++) {
        s += sprintf(s, "%s, ", sf_strs[i]);
        free(sf_strs[i]);
    }
    free(sf_strs);

    if (self->size > 0) {
        s -= 2;
    }
    sprintf(s, "]");
    return str;
}

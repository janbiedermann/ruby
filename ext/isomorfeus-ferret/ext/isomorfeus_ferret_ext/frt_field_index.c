#include <string.h>
#include "frt_field_index.h"

/***************************************************************************
 *
 * FrtFieldIndex
 *
 ***************************************************************************/

static unsigned long long field_index_hash(const void *p)
{
    FrtFieldIndex *self = (FrtFieldIndex *)p;
    return frt_str_hash(rb_id2name(self->field)) ^ (unsigned long long)(self->klass);
}

static int field_index_eq(const void *p1, const void *p2)
{
    FrtFieldIndex *fi1 = (FrtFieldIndex *)p1;
    FrtFieldIndex *fi2 = (FrtFieldIndex *)p2;
    return (fi1->field == fi2->field) &&
        (fi1->klass->type == fi2->klass->type);
}

static void field_index_destroy(void *p)
{
    FrtFieldIndex *self = (FrtFieldIndex *)p;
    if (self->index) {
        self->klass->destroy_index(self->index);
    }
    free(self);
}

FrtFieldIndex *frt_field_index_get(FrtIndexReader *ir, FrtSymbol field,
                            const FrtFieldIndexClass *klass)
{
    int length = 0;
    FrtTermEnum *volatile te = NULL;
    FrtTermDocEnum *volatile tde = NULL;
    FrtFieldInfo *fi = frt_fis_get_field(ir->fis, field);
    const volatile int field_num = fi ? fi->number : -1;
    FrtFieldIndex *volatile self = NULL;
    FrtFieldIndex key;

    if (field_num < 0) {
        FRT_RAISE(FRT_ARG_ERROR,
              "Cannot sort by field \"%s\". It doesn't exist in the index.",
              rb_id2name(field));
    }

    if (!ir->field_index_cache) {
        ir->field_index_cache = frt_h_new(&field_index_hash, &field_index_eq,
                                      NULL, &field_index_destroy);
    }

    key.field = field;
    key.klass = klass;
    self = (FrtFieldIndex *)frt_h_get(ir->field_index_cache, &key);

    if (self == NULL) {
        self = FRT_ALLOC(FrtFieldIndex);
        self->klass = klass;
        /* FieldIndex only lives as long as the IndexReader lives so we can
         * just use the field_infos field symbol */
        self->field = fi->name;

        length = ir->max_doc(ir);
        if (length > 0) {
            FRT_TRY
            {
                void *index;
                tde = ir->term_docs(ir);
                te = ir->terms(ir, field_num);
                index = self->index = klass->create_index(length);
                while (te->next(te)) {
                    tde->seek_te(tde, te);
                    klass->handle_term(index, tde, te->curr_term);
                }
            }
            FRT_XFINALLY
                tde->close(tde);
                te->close(te);
            FRT_XENDTRY
        }
        frt_h_set(ir->field_index_cache, self, self);
    }

    return self;
}

/******************************************************************************
 * ByteFieldIndex < FieldIndex
 *
 * The ByteFieldIndex holds an array of integers for each document in the
 * index where the integer represents the sort value for the document.  This
 * index should only be used for sorting and not as a field cache of the
 * column's value.
 ******************************************************************************/
static void byte_handle_term(void *index_ptr,
                             FrtTermDocEnum *tde,
                             const char *text)
{
    long *index = (long *)index_ptr;
    long val = index[-1]++;
    (void)text;
    while (tde->next(tde)) {
        index[tde->doc_num(tde)] = val;
    }
}

static void *byte_create_index(int size)
{
    long *index = FRT_ALLOC_AND_ZERO_N(long, size + 1);
    index[0] = 1;
    return &index[1];
}

static void byte_destroy_index(void *p)
{
    long *index = (long *)p;
    free(&index[-1]);
}

const FrtFieldIndexClass FRT_BYTE_FIELD_INDEX_CLASS = {
    "byte",
    &byte_create_index,
    &byte_destroy_index,
    &byte_handle_term
};

/******************************************************************************
 * IntegerFieldIndex < FieldIndex
 ******************************************************************************/
static void *integer_create_index(int size)
{
    return FRT_ALLOC_AND_ZERO_N(long, size);
}

static void integer_handle_term(void *index_ptr,
                                FrtTermDocEnum *tde,
                                const char *text)
{
    long *index = (long *)index_ptr;
    long val;
    sscanf(text, "%ld", &val);
    while (tde->next(tde)) {
        index[tde->doc_num(tde)] = val;
    }
}

const FrtFieldIndexClass FRT_INTEGER_FIELD_INDEX_CLASS = {
    "integer",
    &integer_create_index,
    &free,
    &integer_handle_term
};

/******************************************************************************
 * FloatFieldIndex < FieldIndex
 ******************************************************************************/
#define VALUES_ARRAY_START_SIZE 8
static void *float_create_index(int size)
{
    return FRT_ALLOC_AND_ZERO_N(float, size);
}

static void float_handle_term(void *index_ptr,
                              FrtTermDocEnum *tde,
                              const char *text)
{
    float *index = (float *)index_ptr;
    float val;
    sscanf(text, "%g", &val);
    while (tde->next(tde)) {
        index[tde->doc_num(tde)] = val;
    }
}

const FrtFieldIndexClass FRT_FLOAT_FIELD_INDEX_CLASS = {
    "float",
    &float_create_index,
    &free,
    &float_handle_term
};

/******************************************************************************
 * StringFieldIndex < FieldIndex
 ******************************************************************************/

static void *string_create_index(int size)
{
    FrtStringIndex *self = FRT_ALLOC_AND_ZERO(FrtStringIndex);
    self->size = size;
    self->index = FRT_ALLOC_AND_ZERO_N(long, size);
    self->v_capa = VALUES_ARRAY_START_SIZE;
    self->v_size = 1; /* leave the first value as NULL */
    self->values = FRT_ALLOC_AND_ZERO_N(char *, VALUES_ARRAY_START_SIZE);
    return self;
}

static void string_destroy_index(void *p)
{
    FrtStringIndex *self = (FrtStringIndex *)p;
    int i;
    free(self->index);
    for (i = 0; i < self->v_size; i++) {
        free(self->values[i]);
    }
    free(self->values);
    free(self);
}

static void string_handle_term(void *index_ptr,
                               FrtTermDocEnum *tde,
                               const char *text)
{
    FrtStringIndex *index = (FrtStringIndex *)index_ptr;
    if (index->v_size >= index->v_capa) {
        index->v_capa *= 2;
        FRT_REALLOC_N(index->values, char *, index->v_capa);
    }
    index->values[index->v_size] = frt_estrdup(text);
    while (tde->next(tde)) {
        index->index[tde->doc_num(tde)] = index->v_size;
    }
    index->v_size++;
}

const FrtFieldIndexClass FRT_STRING_FIELD_INDEX_CLASS = {
    "string",
    &string_create_index,
    &string_destroy_index,
    &string_handle_term
};

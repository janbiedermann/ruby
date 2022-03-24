#include "frt_global.h"
#include "frt_search.h"
#include <string.h>

/***************************************************************************
 *
 * Filter
 *
 ***************************************************************************/

void frt_filt_destroy_i(FrtFilter *filt)
{
    frt_h_destroy(filt->cache);
    free(filt);
}

void frt_filt_deref(FrtFilter *filt)
{
    if (--(filt->ref_cnt) == 0) {
        filt->destroy_i(filt);
    }
}

FrtBitVector *frt_filt_get_bv(FrtFilter *filt, FrtIndexReader *ir)
{
    FrtCacheObject *co = (FrtCacheObject *)frt_h_get(filt->cache, ir);

    if (!co) {
        FrtBitVector *bv;
        if (!ir->cache) {
            frt_ir_add_cache(ir);
        }
        bv = filt->get_bv_i(filt, ir);
        co = frt_co_create(filt->cache, ir->cache, filt, ir,
                       (frt_free_ft)&frt_bv_destroy, (void *)bv);
    }
    return (FrtBitVector *)co->obj;
}

static char *filt_to_s_i(FrtFilter *filt)
{
    return frt_estrdup(rb_id2name(filt->name));
}

static unsigned long long frt_filt_hash_default(FrtFilter *filt)
{
    (void)filt;
    return 0;
}

static int frt_filt_eq_default(FrtFilter *filt, FrtFilter *o)
{
    (void)filt; (void)o;
    return false;
}

FrtFilter *frt_filt_create(size_t size, FrtSymbol name)
{
    FrtFilter *filt    = (FrtFilter *)frt_emalloc(size);
    filt->cache     = frt_co_hash_create();
    filt->name      = name;
    filt->to_s      = &filt_to_s_i;
    filt->hash      = &frt_filt_hash_default;
    filt->eq        = &frt_filt_eq_default;
    filt->destroy_i = &frt_filt_destroy_i;
    filt->ref_cnt   = 1;
    return filt;
}

unsigned long long frt_filt_hash(FrtFilter *filt)
{
    return frt_str_hash(rb_id2name(filt->name)) ^ filt->hash(filt);
}

int frt_filt_eq(FrtFilter *filt, FrtFilter *o)
{
    return ((filt == o)
            || ((filt->name == o->name)
                && (filt->eq == o->eq)
                && (filt->eq(filt, o))));
}

/***************************************************************************
 *
 * QueryFilter
 *
 ***************************************************************************/

#define QF(filt) ((QueryFilter *)(filt))
typedef struct QueryFilter
{
    FrtFilter super;
    FrtQuery *query;
} QueryFilter;

static char *qfilt_to_s(FrtFilter *filt)
{
    FrtQuery *query = QF(filt)->query;
    char *query_str = query->to_s(query, (FrtSymbol)NULL);
    char *filter_str = frt_strfmt("QueryFilter< %s >", query_str);
    free(query_str);
    return filter_str;
}

static FrtBitVector *qfilt_get_bv_i(FrtFilter *filt, FrtIndexReader *ir)
{
    FrtBitVector *bv = frt_bv_new_capa(ir->max_doc(ir));
    FrtSearcher *sea = frt_isea_new(ir);
    FrtWeight *weight = frt_q_weight(QF(filt)->query, sea);
    FrtScorer *scorer = weight->scorer(weight, ir);
    if (scorer) {
        while (scorer->next(scorer)) {
            frt_bv_set(bv, scorer->doc);
        }
        scorer->destroy(scorer);
    }
    weight->destroy(weight);
    free(sea);
    return bv;
}

static unsigned long long qfilt_hash(FrtFilter *filt)
{
    return frt_q_hash(QF(filt)->query);
}

static int qfilt_eq(FrtFilter *filt, FrtFilter *o)
{
    return frt_q_eq(QF(filt)->query, QF(o)->query);
}

static void qfilt_destroy_i(FrtFilter *filt)
{
    FrtQuery *query = QF(filt)->query;
    frt_q_deref(query);
    frt_filt_destroy_i(filt);
}

FrtFilter *frt_qfilt_new_nr(FrtQuery *query)
{
    FrtFilter *filt = filt_new(QueryFilter);

    QF(filt)->query = query;

    filt->get_bv_i  = &qfilt_get_bv_i;
    filt->hash      = &qfilt_hash;
    filt->eq        = &qfilt_eq;
    filt->to_s      = &qfilt_to_s;
    filt->destroy_i = &qfilt_destroy_i;
    return filt;
}

FrtFilter *frt_qfilt_new(FrtQuery *query)
{
    FRT_REF(query);
    return frt_qfilt_new_nr(query);
}

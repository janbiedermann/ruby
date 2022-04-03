#include <string.h>
#include "frt_global.h"
#include "frt_search.h"

/*****************************************************************************
 *
 * FrtRange
 *
 *****************************************************************************/

static char *range_to_s(FrtRange *range, FrtSymbol default_field, float boost) {
    char *buffer, *b;
    size_t flen, llen, ulen;
    const char *field_name = rb_id2name(range->field);

    flen = strlen(field_name);
    llen = range->lower_term ? strlen(range->lower_term) : 0;
    ulen = range->upper_term ? strlen(range->upper_term) : 0;
    buffer = FRT_ALLOC_N(char, flen + llen + ulen + 40);
    b = buffer;

    if (default_field != range->field) {
        memcpy(buffer, field_name, flen * sizeof(char));
        b += flen;
        *b = ':';
        b++;
    }

    if (range->lower_term) {
        *b = range->include_lower ? '[' : '{';
        b++;
        memcpy(b, range->lower_term, llen);
        b += llen;
    } else {
        *b = '<';
        b++;
    }

    if (range->upper_term && range->lower_term) {
        *b = ' '; b++;
    }

    if (range->upper_term) {
        memcpy(b, range->upper_term, ulen);
        b += ulen;
        *b = range->include_upper ? ']' : '}';
        b++;
    } else {
        *b = '>';
        b++;
    }

    *b = 0;
    if (boost != 1.0) {
        *b = '^';
        frt_dbl_to_s(b + 1, boost);
    }
    return buffer;
}

static void range_destroy(FrtRange *range)
{
    free(range->lower_term);
    free(range->upper_term);
    free(range);
}

static unsigned long long range_hash(FrtRange *filt)
{
    return filt->include_lower | (filt->include_upper << 1)
        | ((frt_str_hash(rb_id2name(filt->field))
            ^ (filt->lower_term ? frt_str_hash(filt->lower_term) : 0)
            ^ (filt->upper_term ? frt_str_hash(filt->upper_term) : 0)) << 2);
}

static int range_eq(FrtRange *filt, FrtRange *o)
{
    if ((filt->lower_term && !o->lower_term) || (!filt->lower_term && o->lower_term)) { return false; }
    if ((filt->upper_term && !o->upper_term) || (!filt->upper_term && o->upper_term)) { return false; }
    return ((filt->field == o->field)
            && ((filt->lower_term && o->lower_term) ? (strcmp(filt->lower_term, o->lower_term) == 0) : 1)
            && ((filt->upper_term && o->upper_term) ? (strcmp(filt->upper_term, o->upper_term) == 0) : 1)
            && (filt->include_lower == o->include_lower)
            && (filt->include_upper == o->include_upper));
}

static FrtRange *range_new(FrtSymbol field, const char *lower_term,
                 const char *upper_term, bool include_lower,
                 bool include_upper)
{
    FrtRange *range;

    if (!lower_term && !upper_term) {
        FRT_RAISE(FRT_ARG_ERROR, "Nil bounds for range. A range must include either "
              "lower bound or an upper bound");
    }
    if (include_lower && !lower_term) {
        FRT_RAISE(FRT_ARG_ERROR, "Lower bound must be non-nil to be inclusive. That "
              "is, if you specify :include_lower => true when you create a "
              "range you must include a :lower_term");
    }
    if (include_upper && !upper_term) {
        FRT_RAISE(FRT_ARG_ERROR, "Upper bound must be non-nil to be inclusive. That "
              "is, if you specify :include_upper => true when you create a "
              "range you must include a :upper_term");
    }
    if (upper_term && lower_term && (strcmp(upper_term, lower_term) < 0)) {
        FRT_RAISE(FRT_ARG_ERROR, "Upper bound must be greater than lower bound. "
              "\"%s\" < \"%s\"", upper_term, lower_term);
    }

    range = FRT_ALLOC(FrtRange);

    range->field = field;
    range->lower_term = lower_term ? frt_estrdup(lower_term) : NULL;
    range->upper_term = upper_term ? frt_estrdup(upper_term) : NULL;
    range->include_lower = include_lower;
    range->include_upper = include_upper;
    return range;
}

static FrtRange *trange_new(FrtSymbol field, const char *lower_term,
                  const char *upper_term, bool include_lower,
                  bool include_upper)
{
    FrtRange *range;
    int len;
    double upper_num, lower_num;

    if (!lower_term && !upper_term) {
        FRT_RAISE(FRT_ARG_ERROR, "Nil bounds for range. A range must include either "
              "lower bound or an upper bound");
    }
    if (include_lower && !lower_term) {
        FRT_RAISE(FRT_ARG_ERROR, "Lower bound must be non-nil to be inclusive. That "
              "is, if you specify :include_lower => true when you create a "
              "range you must include a :lower_term");
    }
    if (include_upper && !upper_term) {
        FRT_RAISE(FRT_ARG_ERROR, "Upper bound must be non-nil to be inclusive. That "
              "is, if you specify :include_upper => true when you create a "
              "range you must include a :upper_term");
    }
    if (upper_term && lower_term) {
        if ((!lower_term ||
             (sscanf(lower_term, "%lg%n", &lower_num, &len) &&
              (int)strlen(lower_term) == len)) &&
            (!upper_term ||
             (sscanf(upper_term, "%lg%n", &upper_num, &len) &&
              (int)strlen(upper_term) == len)))
        {
            if (upper_num < lower_num) {
                FRT_RAISE(FRT_ARG_ERROR, "Upper bound must be greater than lower bound."
                      " numbers \"%lg\" < \"%lg\"", upper_num, lower_num);
            }
        }
        else {
            if (upper_term && lower_term &&
                (strcmp(upper_term, lower_term) < 0)) {
                FRT_RAISE(FRT_ARG_ERROR, "Upper bound must be greater than lower bound."
                      " \"%s\" < \"%s\"", upper_term, lower_term);
            }
        }
    }

    range = FRT_ALLOC(FrtRange);

    range->field = field;
    range->lower_term = lower_term ? frt_estrdup(lower_term) : NULL;
    range->upper_term = upper_term ? frt_estrdup(upper_term) : NULL;
    range->include_lower = include_lower;
    range->include_upper = include_upper;
    return range;
}

/***************************************************************************
 *
 * FrtRangeFilter
 *
 ***************************************************************************/

#define RF(filt) ((FrtRangeFilter *)(filt))

static void frt_rfilt_destroy_i(FrtFilter *filt)
{
    range_destroy(RF(filt)->range);
    frt_filt_destroy_i(filt);
}

static char *frt_rfilt_to_s(FrtFilter *filt)
{
    char *rstr = range_to_s(RF(filt)->range, (FrtSymbol)NULL, 1.0);
    char *rfstr = frt_strfmt("RangeFilter< %s >", rstr);
    free(rstr);
    return rfstr;
}

static FrtBitVector *frt_rfilt_get_bv_i(FrtFilter *filt, FrtIndexReader *ir)
{
    FrtBitVector *bv = frt_bv_new_capa(ir->max_doc(ir));
    FrtRange *range = RF(filt)->range;
    FrtFieldInfo *fi = frt_fis_get_field(ir->fis, range->field);
    /* the field info exists we need to add docs to the bit vector, otherwise
     * we just return an empty bit vector */
    if (fi) {
        const char *lower_term =
            range->lower_term ? range->lower_term : FRT_EMPTY_STRING;
        const char *upper_term = range->upper_term;
        const bool include_upper = range->include_upper;
        const int field_num = fi->number;
        char *term;
        FrtTermEnum* te;
        FrtTermDocEnum *tde;
        bool check_lower;

        te = ir->terms(ir, field_num);
        if (te->skip_to(te, lower_term) == NULL) {
            te->close(te);
            return bv;
        }

        check_lower = !(range->include_lower || (lower_term == FRT_EMPTY_STRING));

        tde = ir->term_docs(ir);
        term = te->curr_term;
        do {
            if (!check_lower
                || (strcmp(term, lower_term) > 0)) {
                check_lower = false;
                if (upper_term) {
                    int compare = strcmp(upper_term, term);
                    /* Break if upper term is greater than or equal to upper
                     * term and include_upper is false or ther term is fully
                     * greater than upper term. This is optimized so that only
                     * one check is done except in last check or two */
                    if ((compare <= 0)
                        && (!include_upper || (compare < 0))) {
                        break;
                    }
                }
                /* we have a good term, find the docs */
                /* text is already pointing to term buffer text */
                tde->seek_te(tde, te);
                while (tde->next(tde)) {
                    frt_bv_set(bv, tde->doc_num(tde));
                }
            }
        } while (te->next(te));

        tde->close(tde);
        te->close(te);
    }

    return bv;
}

static unsigned long long frt_rfilt_hash(FrtFilter *filt) {
    return range_hash(RF(filt)->range);
}

static int frt_rfilt_eq(FrtFilter *filt, FrtFilter *o) {
    return range_eq(RF(filt)->range, RF(o)->range);
}

FrtFilter *frt_rfilt_alloc(void) {
    return filt_new(FrtRangeFilter);
}

FrtFilter *frt_rfilt_init(FrtFilter *filt, FrtSymbol field, const char *lower_term, const char *upper_term,
                  bool include_lower, bool include_upper) {
    RF(filt)->range = range_new(field, lower_term, upper_term, include_lower, include_upper);
    filt->get_bv_i  = &frt_rfilt_get_bv_i;
    filt->hash      = &frt_rfilt_hash;
    filt->eq        = &frt_rfilt_eq;
    filt->to_s      = &frt_rfilt_to_s;
    filt->destroy_i = &frt_rfilt_destroy_i;
    return filt;
}

FrtFilter *frt_rfilt_new(FrtSymbol field, const char *lower_term, const char *upper_term,
                  bool include_lower, bool include_upper) {
    FrtFilter *filt = frt_rfilt_alloc();
    return frt_rfilt_init(filt, field, lower_term, upper_term, include_lower, include_upper);
}

/***************************************************************************
 *
 * FrtRangeFilter
 *
 ***************************************************************************/

static char *frt_trfilt_to_s(FrtFilter *filt) {
    char *rstr = range_to_s(RF(filt)->range, (FrtSymbol)NULL, 1.0);
    char *rfstr = frt_strfmt("TypedRangeFilter< %s >", rstr);
    free(rstr);
    return rfstr;
}

typedef enum {
    TRC_NONE    = 0x00,
    TRC_LE      = 0x01,
    TRC_LT      = 0x02,
    TRC_GE      = 0x04,
    TRC_GE_LE   = 0x05,
    TRC_GE_LT   = 0x06,
    TRC_GT      = 0x08,
    TRC_GT_LE   = 0x09,
    TRC_GT_LT   = 0x0a
} TypedRangeCheck;

#define SET_DOCS(cond)\
do {\
    if (term[0] > '9') break; /* done */\
    sscanf(term, "%lg%n", &num, &len);\
    if (len == te->curr_term_len) { /* We have a number */\
        if (cond) {\
            tde->seek_te(tde, te);\
            while (tde->next(tde)) {\
                frt_bv_set(bv, tde->doc_num(tde));\
            }\
        }\
    }\
} while (te->next(te))

static FrtBitVector *frt_trfilt_get_bv_i(FrtFilter *filt, FrtIndexReader *ir) {
    FrtRange *range = RF(filt)->range;
    double lnum = 0.0, unum = 0.0;
    int len = 0;
    const char *lt = range->lower_term;
    const char *ut = range->upper_term;
    if ((!lt || (sscanf(lt, "%lg%n", &lnum, &len) && (int)strlen(lt) == len)) &&
        (!ut || (sscanf(ut, "%lg%n", &unum, &len) && (int)strlen(ut) == len)))
    {
        FrtBitVector *bv = frt_bv_new_capa(ir->max_doc(ir));
        FrtFieldInfo *fi = frt_fis_get_field(ir->fis, range->field);
        /* the field info exists we need to add docs to the bit vector,
         * otherwise we just return an empty bit vector */
        if (fi) {
            const int field_num = fi->number;
            char *term;
            double num;
            FrtTermEnum* te;
            FrtTermDocEnum *tde;
            TypedRangeCheck check = TRC_NONE;

            te = ir->terms(ir, field_num);
            if (te->skip_to(te, "+.") == NULL) {
                te->close(te);
                return bv;
            }

            tde = ir->term_docs(ir);
            term = te->curr_term;

            if (lt) {
                check = range->include_lower ? TRC_GE : TRC_GT;
            }
            if (ut) {
               check = (TypedRangeCheck)(check | (range->include_upper
                                                  ? TRC_LE
                                                  : TRC_LT));
            }

            switch(check) {
                case TRC_LE:
                    SET_DOCS(num <= unum);
                    break;
                case TRC_LT:
                    SET_DOCS(num <  unum);
                    break;
                case TRC_GE:
                    SET_DOCS(num >= lnum);
                    break;
                case TRC_GE_LE:
                    SET_DOCS(num >= lnum && num <= unum);
                    break;
                case TRC_GE_LT:
                    SET_DOCS(num >= lnum && num <  unum);
                    break;
                case TRC_GT:
                    SET_DOCS(num >  lnum);
                    break;
                case TRC_GT_LE:
                    SET_DOCS(num >  lnum && num <= unum);
                    break;
                case TRC_GT_LT:
                    SET_DOCS(num >  lnum && num <  unum);
                    break;
                case TRC_NONE:
                    /* should never happen. Error should have been rb_raised */
                    assert(false);
            }
            tde->close(tde);
            te->close(te);
        }

        return bv;
    } else {
        return frt_rfilt_get_bv_i(filt, ir);
    }
}

FrtFilter *frt_trfilt_alloc(void) {
    return filt_new(FrtRangeFilter);
}

FrtFilter *frt_trfilt_init(FrtFilter *filt, FrtSymbol field, const char *lower_term, const char *upper_term,
                   bool include_lower, bool include_upper) {
    RF(filt)->range = trange_new(field, lower_term, upper_term, include_lower, include_upper);

    filt->get_bv_i  = &frt_trfilt_get_bv_i;
    filt->hash      = &frt_rfilt_hash;
    filt->eq        = &frt_rfilt_eq;
    filt->to_s      = &frt_trfilt_to_s;
    filt->destroy_i = &frt_rfilt_destroy_i;
    return filt;
}

FrtFilter *frt_trfilt_new(FrtSymbol field, const char *lower_term, const char *upper_term,
                   bool include_lower, bool include_upper) {
    FrtFilter *filt = frt_trfilt_alloc();
    return frt_trfilt_init(filt, field, lower_term, upper_term, include_lower, include_upper);
}

/*****************************************************************************
 *
 * RangeQuery
 *
 *****************************************************************************/

#define RQ(query) ((FrtRangeQuery *)(query))

static char *frt_rq_to_s(FrtQuery *self, FrtSymbol field) {
    return range_to_s(RQ(self)->range, field, self->boost);
}

static void frt_rq_destroy(FrtQuery *self) {
    range_destroy(RQ(self)->range);
    frt_q_destroy_i(self);
}

static FrtMatchVector *rq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv, FrtTermVector *tv) {
    FrtRange *range = RQ(((FrtConstantScoreQuery *)self)->original)->range;
    if (tv->field == range->field) {
        const int term_cnt = tv->term_cnt;
        int i, j;
        char *upper_text = range->upper_term;
        char *lower_text = range->lower_term;
        int upper_limit = range->include_upper ? 1 : 0;

        i = lower_text ? frt_tv_scan_to_term_index(tv, lower_text) : 0;
        if (i < term_cnt && !range->include_lower && lower_text
            && 0 == strcmp(lower_text, tv->terms[i].text)) {
            i++;
        }

        for (; i < term_cnt; i++) {
            FrtTVTerm *tv_term = &(tv->terms[i]);
            char *text = tv_term->text;
            const int tv_term_freq = tv_term->freq;
            if (upper_text && strcmp(text, upper_text) >= upper_limit) {
                break;
            }
            for (j = 0; j < tv_term_freq; j++) {
                int pos = tv_term->positions[j];
                frt_matchv_add(mv, pos, pos);
            }
        }
    }
    return mv;
}

static FrtQuery *frt_rq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtQuery *csq;
    FrtRange *r = RQ(self)->range;
    FrtFilter *filter = frt_rfilt_new(r->field, r->lower_term, r->upper_term, r->include_lower, r->include_upper);
    (void)ir;
    csq = frt_csq_new_nr(filter);
    ((FrtConstantScoreQuery *)csq)->original = self;
    csq->get_matchv_i = &rq_get_matchv_i;
    return (FrtQuery *)csq;
}

static unsigned long long frt_rq_hash(FrtQuery *self) {
    return range_hash(RQ(self)->range);
}

static int frt_rq_eq(FrtQuery *self, FrtQuery *o) {
    return range_eq(RQ(self)->range, RQ(o)->range);
}

FrtQuery *frt_rq_new_less(FrtSymbol field, const char *upper_term, bool include_upper) {
    return frt_rq_new(field, NULL, upper_term, false, include_upper);
}

FrtQuery *frt_rq_new_more(FrtSymbol field, const char *lower_term, bool include_lower) {
    return frt_rq_new(field, lower_term, NULL, include_lower, false);
}

FrtQuery *frt_rq_alloc(void) {
    return frt_q_new(FrtRangeQuery);
}

FrtQuery *frt_rq_init(FrtQuery *self, FrtSymbol field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper) {
    FrtRange *range            = range_new(field, lower_term, upper_term, include_lower, include_upper);
    RQ(self)->range         = range;

    self->type              = RANGE_QUERY;
    self->rewrite           = &frt_rq_rewrite;
    self->to_s              = &frt_rq_to_s;
    self->hash              = &frt_rq_hash;
    self->eq                = &frt_rq_eq;
    self->destroy_i         = &frt_rq_destroy;
    self->create_weight_i   = &frt_q_create_weight_unsup;
    return self;
}

FrtQuery *frt_rq_new(FrtSymbol field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper) {
    FrtQuery *self = frt_rq_alloc();
    return frt_rq_init(self, field, lower_term, upper_term, include_lower, include_upper);
}

/*****************************************************************************
 *
 * TypedRangeQuery
 *
 *****************************************************************************/

#define SET_TERMS(cond)\
for (i = tv->term_cnt - 1; i >= 0; i--) {\
    FrtTVTerm *tv_term = &(tv->terms[i]);\
    char *text = tv_term->text;\
    double num;\
    sscanf(text, "%lg%n", &num, &len);\
    if ((int)strlen(text) == len) { /* We have a number */\
        if (cond) {\
            const int tv_term_freq = tv_term->freq;\
            for (j = 0; j < tv_term_freq; j++) {\
                int pos = tv_term->positions[j];\
                frt_matchv_add(mv, pos, pos);\
            }\
        }\
    }\
}\

static FrtMatchVector *trq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv, FrtTermVector *tv) {
    FrtRange *range = RQ(((FrtConstantScoreQuery *)self)->original)->range;
    if (tv->field == range->field) {
        double lnum = 0.0, unum = 0.0;
        int len = 0;
        const char *lt = range->lower_term;
        const char *ut = range->upper_term;
        if ((!lt
             || (sscanf(lt,"%lg%n",&lnum,&len) && (int)strlen(lt) == len))
            &&
            (!ut
             || (sscanf(ut,"%lg%n",&unum,&len) && (int)strlen(ut) == len)))
        {
            TypedRangeCheck check = TRC_NONE;
            int i = 0, j = 0;

            if (lt) {
               check = range->include_lower ? TRC_GE : TRC_GT;
            }
            if (ut) {
               check = (TypedRangeCheck)(check | (range->include_upper
                                                  ? TRC_LE
                                                  : TRC_LT));
            }

            switch(check) {
                case TRC_LE:
                    SET_TERMS(num <= unum);
                    break;
                case TRC_LT:
                    SET_TERMS(num <  unum);
                    break;
                case TRC_GE:
                    SET_TERMS(num >= lnum);
                    break;
                case TRC_GE_LE:
                    SET_TERMS(num >= lnum && num <= unum);
                    break;
                case TRC_GE_LT:
                    SET_TERMS(num >= lnum && num <  unum);
                    break;
                case TRC_GT:
                    SET_TERMS(num >  lnum);
                    break;
                case TRC_GT_LE:
                    SET_TERMS(num >  lnum && num <= unum);
                    break;
                case TRC_GT_LT:
                    SET_TERMS(num >  lnum && num <  unum);
                    break;
                case TRC_NONE:
                    /* should never happen. Error should have been rb_raised */
                    assert(false);
            }
        } else {
            return rq_get_matchv_i(self, mv, tv);
        }
    }
    return mv;
}

static FrtQuery *frt_trq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtQuery *csq;
    FrtRange *r = RQ(self)->range;
    FrtFilter *filter = frt_trfilt_new(r->field, r->lower_term, r->upper_term, r->include_lower, r->include_upper);
    (void)ir;
    csq = frt_csq_new_nr(filter);
    ((FrtConstantScoreQuery *)csq)->original = self;
    csq->get_matchv_i = &trq_get_matchv_i;
    return (FrtQuery *)csq;
}

FrtQuery *frt_trq_new_less(FrtSymbol field, const char *upper_term, bool include_upper) {
    return frt_trq_new(field, NULL, upper_term, false, include_upper);
}

FrtQuery *frt_trq_new_more(FrtSymbol field, const char *lower_term, bool include_lower) {
    return frt_trq_new(field, lower_term, NULL, include_lower, false);
}

FrtQuery *frt_trq_alloc(void) {
    return frt_q_new(FrtRangeQuery);
}

FrtQuery *frt_trq_init(FrtQuery *self, FrtSymbol field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper) {
    FrtRange *range         = trange_new(field, lower_term, upper_term, include_lower, include_upper);
    RQ(self)->range         = range;

    self->type              = TYPED_RANGE_QUERY;
    self->rewrite           = &frt_trq_rewrite;
    self->to_s              = &frt_rq_to_s;
    self->hash              = &frt_rq_hash;
    self->eq                = &frt_rq_eq;
    self->destroy_i         = &frt_rq_destroy;
    self->create_weight_i   = &frt_q_create_weight_unsup;
    return self;
}

FrtQuery *frt_trq_new(FrtSymbol field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper) {
    FrtQuery *self = frt_trq_alloc();
    return frt_trq_init(self, field, lower_term, upper_term, include_lower, include_upper);
}

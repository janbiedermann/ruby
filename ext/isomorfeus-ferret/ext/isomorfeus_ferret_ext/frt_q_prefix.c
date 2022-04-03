#include <string.h>
#include "frt_global.h"
#include "frt_search.h"

/****************************************************************************
 *
 * FrtPrefixQuery
 *
 ****************************************************************************/

#define PfxQ(query) ((FrtPrefixQuery *)(query))

static char *prq_to_s(FrtQuery *self, FrtSymbol default_field) {
    char *buffer, *bptr;
    const char *prefix = PfxQ(self)->prefix;
    size_t plen = strlen(prefix);
    const char *field_name = rb_id2name(PfxQ(self)->field);
    size_t flen = strlen(field_name);

    bptr = buffer = FRT_ALLOC_N(char, plen + flen + 35);

    if (PfxQ(self)->field != default_field) {
        bptr += sprintf(bptr, "%s:", field_name);
    }

    bptr += sprintf(bptr, "%s*", prefix);
    if (self->boost != 1.0) {
        *bptr = '^';
        frt_dbl_to_s(++bptr, self->boost);
    }

    return buffer;
}

static FrtQuery *prq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    const int field_num = frt_fis_get_field_num(ir->fis, PfxQ(self)->field);
    FrtQuery *volatile q = frt_multi_tq_new_conf(PfxQ(self)->field,
                                          FrtMTQMaxTerms(self), 0.0);
    q->boost = self->boost;        /* set the boost */

    if (field_num >= 0) {
        const char *prefix = PfxQ(self)->prefix;
        FrtTermEnum *te = ir->terms_from(ir, field_num, prefix);
        const char *term = te->curr_term;
        size_t prefix_len = strlen(prefix);

        FRT_TRY
            do {
                if (strncmp(term, prefix, prefix_len) != 0) {
                    break;
                }
                frt_multi_tq_add_term(q, term);       /* found a match */
            } while (te->next(te));
        FRT_XFINALLY
            te->close(te);
        FRT_XENDTRY
    }

    return q;
}

static void prq_destroy(FrtQuery *self) {
    free(PfxQ(self)->prefix);
    frt_q_destroy_i(self);
}

static unsigned long long prq_hash(FrtQuery *self) {
    return frt_str_hash(rb_id2name(PfxQ(self)->field)) ^ frt_str_hash(PfxQ(self)->prefix);
}

static int prq_eq(FrtQuery *self, FrtQuery *o) {
    return (strcmp(PfxQ(self)->prefix, PfxQ(o)->prefix) == 0)
        && (PfxQ(self)->field == PfxQ(o)->field);
}

FrtQuery *frt_prefixq_alloc(void) {
    return frt_q_new(FrtPrefixQuery);
}

FrtQuery *frt_prefixq_init(FrtQuery *self, FrtSymbol field, const char *prefix) {
    PfxQ(self)->field       = field;
    PfxQ(self)->prefix      = frt_estrdup(prefix);
    FrtMTQMaxTerms(self)    = PREFIX_QUERY_MAX_TERMS;

    self->type              = PREFIX_QUERY;
    self->rewrite           = &prq_rewrite;
    self->to_s              = &prq_to_s;
    self->hash              = &prq_hash;
    self->eq                = &prq_eq;
    self->destroy_i         = &prq_destroy;
    self->create_weight_i   = &frt_q_create_weight_unsup;

    return self;
}

FrtQuery *frt_prefixq_new(FrtSymbol field, const char *prefix) {
    FrtQuery *self = frt_prefixq_alloc();
    return frt_prefixq_init(self, field, prefix);
}

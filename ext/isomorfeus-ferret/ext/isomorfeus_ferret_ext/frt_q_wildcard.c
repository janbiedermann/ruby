#include <string.h>
#include "frt_global.h"
#include "frt_search.h"

/****************************************************************************
 *
 * FrtWildCardQuery
 *
 ****************************************************************************/

#define WCQ(query) ((FrtWildCardQuery *)(query))

static char *wcq_to_s(FrtQuery *self, FrtSymbol default_field) {
    char *buffer, *bptr;
    const char *field_name = rb_id2name(WCQ(self)->field);
    const char *pattern = WCQ(self)->pattern;
    bptr = buffer = FRT_ALLOC_N(char, strlen(pattern) + strlen(field_name) + 35);

    if (WCQ(self)->field != default_field) {
        bptr += sprintf(bptr, "%s:", field_name);
    }
    bptr += sprintf(bptr, "%s", pattern);

    if (self->boost != 1.0) {
        *bptr = '^';
        frt_dbl_to_s(++bptr, self->boost);
    }

    return buffer;
}

bool frt_wc_match(const char *pattern, const char *text) {
    const char *p = pattern, *t = text, *xt;

    /* include '\0' as we need to match empty string */
    const char *text_last = t + strlen(t);

    for (;; p++, t++) {

        /* end of text so make sure end of pattern doesn't matter */
        if (*t == '\0') {
            while (*p) {
                if (*p != FRT_WILD_STRING) {
                    return false;
                }
                p++;
            }
            return true;
        }

        /* If we've gone past the end of the pattern, return false. */
        if (*p == '\0') {
            return false;
        }

        /* Match a single character, so continue. */
        if (*p == FRT_WILD_CHAR) {
            continue;
        }

        if (*p == FRT_WILD_STRING) {
            /* Look at the character beyond the '*'. */
            p++;
            /* Examine the string, starting at the last character. */
            for (xt = text_last; xt >= t; xt--) {
                if (frt_wc_match(p, xt)) return true;
            }
            return false;
        }
        if (*p != *t) {
            return false;
        }
    }

    return false;
}

static FrtQuery *wcq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtQuery *q;
    const char *pattern = WCQ(self)->pattern;
    const char *first_star = strchr(pattern, FRT_WILD_STRING);
    const char *first_ques = strchr(pattern, FRT_WILD_CHAR);

    if (NULL == first_star && NULL == first_ques) {
        q = frt_tq_new(WCQ(self)->field, pattern);
        q->boost = self->boost;
    }
    else {
        const int field_num = frt_fis_get_field_num(ir->fis, WCQ(self)->field);
        q = frt_multi_tq_new_conf(WCQ(self)->field, FrtMTQMaxTerms(self), 0.0);

        if (field_num >= 0) {
            FrtTermEnum *te;
            char prefix[FRT_MAX_WORD_SIZE] = "";
            int prefix_len;

            pattern = (first_ques && (!first_star || first_star > first_ques))
                ? first_ques : first_star;

            prefix_len = (int)(pattern - WCQ(self)->pattern);

            if (prefix_len > 0) {
                memcpy(prefix, WCQ(self)->pattern, prefix_len);
                prefix[prefix_len] = '\0';
            }

            te = ir->terms_from(ir, field_num, prefix);

            if (te != NULL) {
                const char *term = te->curr_term;
                const char *pat_term = term + prefix_len;
                do {
                    if (prefix[0] && strncmp(term, prefix, prefix_len) != 0) {
                        break;
                    }

                    if (frt_wc_match(pattern, pat_term)) {
                        frt_multi_tq_add_term(q, term);
                    }
                } while (te->next(te) != NULL);
                te->close(te);
            }
        }
    }

    return q;
}

static void wcq_destroy(FrtQuery *self) {
    free(WCQ(self)->pattern);
    frt_q_destroy_i(self);
}

static unsigned long long wcq_hash(FrtQuery *self) {
    return frt_str_hash(rb_id2name(WCQ(self)->field)) ^ frt_str_hash(WCQ(self)->pattern);
}

static int wcq_eq(FrtQuery *self, FrtQuery *o) {
    return (strcmp(WCQ(self)->pattern, WCQ(o)->pattern) == 0)
        && (WCQ(self)->field == WCQ(o)->field);
}

FrtQuery *frt_wcq_alloc(void) {
    return frt_q_new(FrtWildCardQuery);
}

FrtQuery *frt_wcq_init(FrtQuery *self, FrtSymbol field, const char *pattern) {
    WCQ(self)->field        = field;
    WCQ(self)->pattern      = frt_estrdup(pattern);
    FrtMTQMaxTerms(self)    = FRT_WILD_CARD_QUERY_MAX_TERMS;

    self->type              = WILD_CARD_QUERY;
    self->rewrite           = &wcq_rewrite;
    self->to_s              = &wcq_to_s;
    self->hash              = &wcq_hash;
    self->eq                = &wcq_eq;
    self->destroy_i         = &wcq_destroy;
    self->create_weight_i   = &frt_q_create_weight_unsup;

    return self;
}

FrtQuery *frt_wcq_new(FrtSymbol field, const char *pattern) {
    FrtQuery *self = frt_wcq_alloc();
    return frt_wcq_init(self, field, pattern);
}

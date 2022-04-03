#include <string.h>
#include <ctype.h>
#include "frt_analysis.h"
#include "frt_hash.h"
#include "libstemmer.h"

/*****************************************************************************/
/*** Helpers *****************************************************************/
/*****************************************************************************/

/* initialized in frt_global.c */
extern rb_encoding *utf8_encoding;
extern OnigCodePoint cp_apostrophe;
extern OnigCodePoint cp_dot;
extern OnigCodePoint cp_comma;
extern OnigCodePoint cp_backslash;
extern OnigCodePoint cp_slash;
extern OnigCodePoint cp_underscore;
extern OnigCodePoint cp_dash;
extern OnigCodePoint cp_hyphen;
extern OnigCodePoint cp_at;
extern OnigCodePoint cp_ampersand;
extern OnigCodePoint cp_colon;

static int cp_isnumpunc(OnigCodePoint cp) {
    return (cp == cp_dot || cp == cp_comma || cp == cp_backslash || cp == cp_slash || cp == cp_underscore || cp == cp_dash);
}

static int cp_isurlpunc(OnigCodePoint cp) {
    return (cp == cp_dot || cp == cp_slash || cp == cp_dash || cp == cp_underscore);
}

static int cp_enc_isurlc(OnigCodePoint cp, rb_encoding *enc) {
    return (cp_isurlpunc(cp) || rb_enc_isalnum(cp, enc));
}

static int cp_isurlxatpunc(OnigCodePoint cp) {
    return (cp == cp_dot || cp == cp_slash || cp == cp_dash || cp == cp_underscore || cp == cp_at);
}

static int cp_enc_isurlxatc(OnigCodePoint cp, rb_encoding *enc){
    return (cp_isurlxatpunc(cp) || rb_enc_isalnum(cp, enc));
}

static bool cp_enc_istok(OnigCodePoint cp, rb_encoding *enc) {
    if (rb_enc_isspace(cp, enc)) /* most common so check first. */
        return false;
    if (rb_enc_isalnum(cp, enc) || cp_isnumpunc(cp) ||
        cp == cp_ampersand || cp == cp_at || cp == cp_apostrophe || cp == cp_colon) {
        return true;
    }
    return false;
}

static inline int get_cp(char *start, char *end, int *cp_len, rb_encoding *enc) {
    if (start >= end) {
        *cp_len = 0;
        return 0;
    }
    return rb_enc_codepoint_len(start, end, cp_len, enc);
}

/*****************************************************************************/
/*** FrtToken ****************************************************************/
/*****************************************************************************/

FrtToken *frt_tk_set(FrtToken *tk, char *text, int tlen, off_t start, off_t end, int pos_inc, rb_encoding *encoding) {
    if (tlen >= FRT_MAX_WORD_SIZE) {
        tlen = FRT_MAX_WORD_SIZE - 1; // TODO: this may invalidate mbc's
    }

    if (encoding == utf8_encoding) {
        memcpy(tk->text, text, sizeof(char) * tlen);
    } else {
        const unsigned char *sp = (unsigned char *)text;
        unsigned char *dp = (unsigned char *)tk->text;
        rb_econv_t *ec = rb_econv_open(rb_enc_name(encoding), "UTF-8", RUBY_ECONV_INVALID_REPLACE);
        assert(ec != NULL);
        rb_econv_convert(ec, &sp, (unsigned char *)text + tlen, &dp, (unsigned char *)tk->text + FRT_MAX_WORD_SIZE - 1, 0);
        rb_econv_close(ec);
        tlen = dp - (unsigned char *)tk->text;
    }
    tk->text[tlen] = '\0';
    tk->len = tlen;    // in utf8_encoding
    tk->start = start; // in original encoding
    tk->end = end;     // in original encoding
    tk->pos_inc = pos_inc;
    return tk;
}

static FrtToken *frt_tk_set_ts(FrtToken *tk, char *start, char *end, char *text, int pos_inc, rb_encoding *encoding) {
    return frt_tk_set(tk, start, (int)(end - start), (off_t)(start - text), (off_t)(end - text), pos_inc, encoding);
}

FrtToken *frt_tk_set_no_len(FrtToken *tk, char *text, off_t start, off_t end, int pos_inc, rb_encoding *encoding) {
    return frt_tk_set(tk, text, (int)strlen(text), start, end, pos_inc, encoding);
}

int frt_tk_eq(FrtToken *tk1, FrtToken *tk2) {
    return (strcmp((char *)tk1->text, (char *)tk2->text) == 0 &&
            tk1->start == tk2->start && tk1->end == tk2->end &&
            tk1->pos_inc == tk2->pos_inc);
}

int frt_tk_cmp(FrtToken *tk1, FrtToken *tk2) {
    int cmp;
    if (tk1->start > tk2->start) {
        cmp = 1;
    } else if (tk1->start < tk2->start) {
        cmp = -1;
    } else {
        if (tk1->end > tk2->end) {
            cmp = 1;
        } else if (tk1->end < tk2->end) {
            cmp = -1;
        } else {
            cmp = strcmp((char *)tk1->text, (char *)tk2->text);
        }
    }
    return cmp;
}

void frt_tk_destroy(void *p) {
    free(p);
}

FrtToken *frt_tk_new(void) {
    return FRT_ALLOC(FrtToken);
}

/*****************************************************************************/
/*** FrtTokenStream **********************************************************/
/*****************************************************************************/

void frt_ts_deref(FrtTokenStream *ts) {
    if (--ts->ref_cnt <= 0)
        ts->destroy_i(ts);
}

static FrtTokenStream *ts_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding) {
    ts->t = ts->text = text;
    ts->length = strlen(text);
    ts->encoding = encoding;
    return ts;
}

FrtTokenStream *frt_ts_clone_size(FrtTokenStream *orig_ts, size_t size) {
    FrtTokenStream *ts = (FrtTokenStream *)frt_ecalloc(size);
    memcpy(ts, orig_ts, size);
    ts->ref_cnt = 1;
    return ts;
}

FrtTokenStream *frt_ts_alloc_i(size_t size) {
    return (FrtTokenStream *)frt_ecalloc(size);
}

FrtTokenStream *frt_ts_init(FrtTokenStream *ts) {
    ts->destroy_i = (void (*)(FrtTokenStream *))&free;
    ts->reset = &ts_reset;
    ts->ref_cnt = 1;
    return ts;
}

FrtTokenStream *frt_ts_new_i(size_t size) {
    FrtTokenStream *ts = frt_ts_alloc_i(size);
    return frt_ts_init(ts);
}

/*****************************************************************************/
/*** FrtCachedTokenStream ****************************************************/
/*****************************************************************************/

#define CTS(token_stream) ((FrtCachedTokenStream *)(token_stream))

static FrtTokenStream *cts_clone_i(FrtTokenStream *orig_ts) {
    return frt_ts_clone_size(orig_ts, sizeof(FrtCachedTokenStream));
}

static FrtTokenStream *frt_cts_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtCachedTokenStream));
}

static FrtTokenStream *frt_cts_init(FrtTokenStream *ts) {
    ts->reset   = &ts_reset;
    ts->clone_i = &cts_clone_i;
    ts->ref_cnt = 1;
    return ts;
}

static FrtTokenStream *frt_cts_new(void) {
    FrtTokenStream *ts = frt_cts_alloc();
    return frt_cts_init(ts);
}

/*****************************************************************************/
/*** Tokenizer ***************************************************************/
/*****************************************************************************/

/*****************************************************************************/
/*** FrtNonTokenizer *********************************************************/
/*****************************************************************************/

static FrtToken *nt_next(FrtTokenStream *ts) {
    if (ts->t) {
        size_t len = strlen(ts->t);
        ts->t = NULL;
        return frt_tk_set(&(CTS(ts)->token), ts->text, len, 0, len, 1, ts->encoding);
    } else {
        return NULL;
    }
}

FrtTokenStream *frt_non_tokenizer_new(void) {
    FrtTokenStream *ts = frt_cts_new();
    ts->next = &nt_next;
    return ts;
}

/*****************************************************************************/
/*** FrtWhiteSpaceTokenizer **************************************************/
/*****************************************************************************/

static FrtToken *wst_next(FrtTokenStream *ts)
{
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *start;
    char *t = ts->t;

    cp = get_cp(t, end, &cp_len, enc);
    if (cp < 1)
        return NULL;

    while (cp_len > 0 && rb_enc_isspace(cp, enc)) {
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }

    start = t;
    if (start >= end)
        return NULL;

    do {
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    } while (cp_len > 0 && !rb_enc_isspace(cp, enc));

    ts->t = t;
    return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
}

FrtTokenStream *frt_whitespace_tokenizer_alloc(void) {
    return frt_cts_alloc();
}

FrtTokenStream *frt_whitespace_tokenizer_init(FrtTokenStream *ts, bool lowercase) {
    ts = frt_cts_init(ts);
    ts->next = &wst_next;
    if (lowercase)
        ts = frt_lowercase_filter_new(ts);
    return ts;
}

FrtTokenStream *frt_whitespace_tokenizer_new(bool lowercase) {
    FrtTokenStream *ts = frt_whitespace_tokenizer_alloc();
    return frt_whitespace_tokenizer_init(ts, lowercase);
}

/*****************************************************************************/
/*** FrtLetterTokenizer ******************************************************/
/*****************************************************************************/

static FrtToken *lt_next(FrtTokenStream *ts) {
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *start;
    char *t = ts->t;

    cp = get_cp(t, end, &cp_len, enc);
    if (cp < 1)
        return NULL;

    while (cp_len > 0 && !rb_enc_isalpha(cp, enc)) {
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }

    start = t;
    if (start >= end)
        return NULL;

    do {
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    } while (cp_len > 0 && rb_enc_isalpha(cp, enc));

    ts->t = t;
    return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
}

FrtTokenStream *frt_letter_tokenizer_alloc(void) {
    return frt_cts_alloc();
}

FrtTokenStream *frt_letter_tokenizer_init(FrtTokenStream *ts, bool lowercase) {
    ts = frt_cts_init(ts);
    ts->next = &lt_next;
    if (lowercase)
        ts = frt_lowercase_filter_new(ts);
    return ts;
}

FrtTokenStream *frt_letter_tokenizer_new(bool lowercase) {
    FrtTokenStream *ts = frt_letter_tokenizer_alloc();
    return frt_letter_tokenizer_init(ts, lowercase);
}

/*****************************************************************************/
/*** FrtStandardTokenizer ****************************************************/
/*****************************************************************************/

#define STDTS(token_stream) ((FrtStandardTokenizer *)(token_stream))

static int std_get_alnum(FrtTokenStream *ts, char *token, OnigCodePoint cp, int *cp_len_p, OnigCodePoint *cp_out_p, rb_encoding *enc) {
    char *end = ts->text + ts->length;
    char *t = ts->t;
    char *tt = ts->t;
    int cp_len = *cp_len_p;

    while (cp > 0 && rb_enc_isalnum(cp, enc)) {
        if ((t - ts->t + cp_len) < FRT_MAX_WORD_SIZE)
            tt += cp_len;
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }

    memcpy(token, ts->t, tt - ts->t);
    token[tt - ts->t] = '\0';

    *cp_out_p = cp;
    *cp_len_p = cp_len;
    return t - ts->t;
}

/* (alnum)((punc)(alnum))+ where every second sequence of alnum must contain at
 * least one digit.
 * (alnum) = [a-zA-Z0-9]
 * (punc) = [_\/.,-]
 */
static int std_get_number(FrtTokenStream *ts, char *start, char *end, OnigCodePoint cp, int cp_len_a, rb_encoding *enc) {

    OnigCodePoint cp_1 = 0;
    char *t = start;
    int cp_len = cp_len_a;
    int cp_1_len = 0;
    int last_seen_digit = 2;
    int seen_digit = false;

    while (cp > 0 && last_seen_digit >= 0) {
        while ((cp > 0) && rb_enc_isalnum(cp, enc)) {
            if ((last_seen_digit < 2) && rb_enc_isdigit(cp, enc)) {
                last_seen_digit = 2;
            }
            if ((seen_digit == false) && rb_enc_isdigit(cp, enc)) {
                seen_digit = true;
            }
            t += cp_len;
            cp = get_cp(t, end, &cp_len, enc);
        }
        last_seen_digit--;
        cp_1 = get_cp(t + cp_len, end, &cp_1_len, enc);
        if (!cp_isnumpunc(cp) || !rb_enc_isalnum(cp_1, enc)) {
            break;
        }
        t += cp_len;
        cp = cp_1;
        cp_len = cp_1_len;
    }
    if (seen_digit) {
        return t - start;
    } else {
        return 0;
    }
}

static int std_get_apostrophe(FrtTokenStream *ts, char *input, OnigCodePoint cp, int *cp_len_p, rb_encoding *enc) {
    int cp_len = *cp_len_p;
    char *end = ts->text + ts->length;
    char *t = input;

    while (cp_len > 0 && (rb_enc_isalpha(cp, enc) || cp == cp_apostrophe)) {
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }
    return (int)(t - input);
}

static char *std_get_url(FrtTokenStream *ts, char *start, char *end, char *token, int *len, int bufred) {
    rb_encoding *enc = ts->encoding;
    OnigCodePoint cp;
    OnigCodePoint prev_cp = 0;
    int cp_len = 0;
    int prev_cp_len = 0;
    char *t = start;
    char *tt = start;

    cp = get_cp(t, end, &cp_len, enc);
    while (cp > 0 && cp_enc_isurlc(cp, enc)) {
        if (cp_isurlpunc(cp) && cp_isurlpunc(prev_cp)) {
            break; /* can't have two puncs in a row */
        }
        prev_cp = cp;
        prev_cp_len = cp_len;
        t += cp_len;
        if (((t + cp_len) - start) <= (FRT_MAX_WORD_SIZE - bufred))
            tt += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }

    /* strip trailing punc */
    if (t == tt && cp_isurlpunc(prev_cp)) {
        tt -= prev_cp_len;
    }

    *len = (tt - start) + bufred;
    memcpy(token, start, tt - start);
    token[tt - start] = '\0';

    return t;
}

/* Company names can contain '@' and '&' like AT&T and Excite@Home. */
static int std_get_company_name(FrtTokenStream *ts, char *start, char* end) {
    rb_encoding *enc = ts->encoding;
    char * t = start;
    OnigCodePoint cp;
    int cp_len = 0;

    cp = get_cp(t, end, &cp_len, enc);
    while (cp > 0 && (rb_enc_isalpha(cp, enc) || cp == cp_at || cp == cp_ampersand)) {
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }

    return t - start;
}

static int std_advance_to_start(FrtTokenStream *ts, int *cp_len_p, OnigCodePoint *cp_out_p, rb_encoding *enc) {
    int cp_len = 0;
    int cp_next = 0;
    int cp_len_next = 0;
    OnigCodePoint cp;
    char *end = ts->text + ts->length;
    char *t = ts->t;

    cp = get_cp(t, end, &cp_len, enc);
    while (cp > 0 && !rb_enc_isalnum(cp, enc)) {
        if (cp_isnumpunc(cp)) {
            cp_next = get_cp(t + cp_len, end, &cp_len_next, enc);
            if (cp_next > 0 && rb_enc_isdigit(cp_next, enc))
                break;
        }
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }
    ts->t = t;
    *cp_out_p = cp;
    *cp_len_p = cp_len;
    return (t < end);
}

static FrtToken *std_next(FrtTokenStream *ts) {
    char *s;
    char *t;
    char *start = NULL;
    char *end;
    char *num_end = NULL;
    char token[FRT_MAX_WORD_SIZE + 1];
    OnigCodePoint cp = 0;
    OnigCodePoint cp_1 = 0;
    OnigCodePoint cp_2 = 0;
    OnigCodePoint prev_cp = 0;
    int cp_len = 0;
    int cp_1_len = 0;
    int cp_2_len = 0;
    int token_i = 0;
    int len;
    bool is_acronym;
    bool seen_at_symbol;
    rb_encoding *enc = ts->encoding;

    /* advance to start and return first cp and len */
    if (!std_advance_to_start(ts, &cp_len, &cp, enc))
        return NULL;

    end = ts->text + ts->length;
    start = t = ts->t;

    /* get all alnums */
    token_i = std_get_alnum(ts, token, cp, &cp_len, &cp, enc);
    t += token_i;

    if (t >= end && token_i > 0) {
        ts->t += token_i;
        return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
    }

    // already got cp and cp_len from get_alnum above
    // cp = get_cp(t, end, &cp_len, enc);
    if (cp < 1)
        return NULL;

    if (!cp_enc_istok(cp, enc)) {
        /* very common case, ie a plain word, so check and return */
        ts->t = t + cp_len;
        return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
    }

    if (cp == cp_apostrophe) {       /* apostrophe case. */
        t += std_get_apostrophe(ts, t, cp, &cp_len, enc);
        ts->t = t;
        len = (int)(t - start);
        /* strip possesive */
        /* TODO: wont work with multibyte */
        if ((t[-1] == 's' || t[-1] == 'S') && t[-2] == '\'') {
            t -= 2;
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
            CTS(ts)->token.end += 2;
        }
        else if (t[-1] == '\'') {
            t -= 1;
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
            CTS(ts)->token.end += 1;
        }
        else {
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
        }
        return &(CTS(ts)->token);
    }

    // already got cp and cp_len from get_alnum above
    // cp = get_cp(t, end, &cp_len, enc);
    if (cp == cp_ampersand) {        /* ampersand case. */
        t += std_get_company_name(ts, t, end);
        ts->t = t;
        return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
    }

    // already got cp and cp_len from get_alnum above
    // cp = get_cp(start, end, &cp_len, enc);
    if ((rb_enc_isdigit(cp, enc) || cp_isnumpunc(cp))
        && ((len = std_get_number(ts, start, end, cp, cp_len, enc)) > 0)) { /* possibly a number */
        num_end = start + len;
        cp = get_cp(num_end, end, &cp_len, enc);
        if (cp > 0 && !cp_enc_istok(cp, enc)) { /* won't find a longer token */
            ts->t = num_end;
            return frt_tk_set_ts(&(CTS(ts)->token), start, num_end, ts->text, 1, enc);
        }
        /* else there may be a longer token so check */
    }

    // already got cp and cp_len from get_alnum or the last block above
    // cp = get_cp(t, end, &cp_len, enc);
    cp_1 = get_cp(t + cp_len, end, &cp_1_len, enc);
    cp_2 = get_cp(t + cp_len + cp_1_len, end, &cp_2_len, enc);
    if (cp == cp_colon && cp_1 == cp_slash && cp_2 == cp_slash) {
        /* check for a known url start */
        token[token_i] = '\0';
        t += cp_len + cp_1_len + cp_2_len;
        token_i += cp_len + cp_1_len + cp_2_len;
        cp = get_cp(t, end, &cp_len, enc);
        while (cp > 0 && cp == cp_slash) {
            t += cp_len;
            cp = get_cp(t, end, &cp_len, enc);
        }
        if (rb_enc_isalpha(cp, enc) &&
               (memcmp(token, "ftp", 3) == 0 ||
                memcmp(token, "http", 4) == 0 ||
                memcmp(token, "https", 5) == 0 ||
                memcmp(token, "file", 4) == 0)) {
            ts->t = std_get_url(ts, t, end, token, &len, 0); /* dispose of first part of the URL */
        } else {              /* still treat as url but keep the first part */
            token_i = (int)(t - start);
            memcpy(token, start, token_i * sizeof(char));
            ts->t = std_get_url(ts, t, end, token + token_i, &len, token_i); /* keep start */
        }
        return frt_tk_set(&(CTS(ts)->token), token, len,
                      (off_t)(start - ts->text),
                      (off_t)(ts->t - ts->text), 1, enc);
    }

    /* now see how long a url we can find. */
    is_acronym = true;
    seen_at_symbol = false;

    cp = get_cp(t, end, &cp_len, enc);
    while (cp_enc_isurlxatc(cp, enc)) {
        if (is_acronym && !rb_enc_isalpha(cp, enc) && (cp != cp_dot)) {
            is_acronym = false;
        }
        if (cp_isurlxatpunc(cp) && cp_isurlxatpunc(prev_cp)) {
            break; /* can't have two punctuation characters in a row */
        }
        if (cp == cp_at) {
            if (seen_at_symbol) {
                break; /* we can only have one @ symbol */
            }
            else {
                seen_at_symbol = true;
            }
        }
        prev_cp = cp;
        t += cp_len;
        cp = get_cp(t, end, &cp_len, enc);
    }
    if (cp_isurlxatpunc(prev_cp) && t > ts->t) {
        t -= cp_len;                /* strip trailing punctuation */
    }

    if (t < ts->t || (num_end != NULL && num_end < ts->t)) {
        fprintf(stderr, "Warning: encoding error. Please check that you are using the correct locale for your input");
        return NULL;
    } else if (num_end == NULL || t > num_end) {
        ts->t = t;

        if (is_acronym) {   /* check it is one letter followed by one '.' */
            cp_len = 0;
            for (s = start; s < t - 1; s += cp_len) {
                cp = get_cp(s, end, &cp_len, enc);
                cp_1 = get_cp(s + cp_len, end, &cp_1_len, enc);
                if (rb_enc_isalpha(cp, enc) && (cp_1 != cp_dot))
                    is_acronym = false;
            }
        }
        if (is_acronym) {   /* strip '.'s */
            cp_len = 0;
            for (s = start + token_i; s < t; s += cp_len) {
                cp = get_cp(s, end, &cp_len, enc);
                if (cp > 0 && cp != cp_dot) {
                    memcpy(token + token_i, s, cp_len);
                    token_i += cp_len;
                }
            }
            token[token_i] = '\0';
            frt_tk_set(&(CTS(ts)->token), token, token_i,
                   (off_t)(start - ts->text),
                   (off_t)(t - ts->text), 1, enc);
        } else { /* just return the url as is */
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
        }
    } else {                  /* return the number */
        ts->t = num_end;
        frt_tk_set_ts(&(CTS(ts)->token), start, num_end, ts->text, 1, enc);
    }
    return &(CTS(ts)->token);
}

static FrtTokenStream *std_ts_clone_i(FrtTokenStream *orig_ts) {
    return frt_ts_clone_size(orig_ts, sizeof(FrtStandardTokenizer));
}

FrtTokenStream *frt_standard_tokenizer_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtStandardTokenizer));
}

FrtTokenStream *frt_standard_tokenizer_init(FrtTokenStream *ts, bool lowercase) {
    ts = frt_ts_init(ts);
    ts->clone_i = &std_ts_clone_i;
    ts->next    = &std_next;
    if (lowercase)
        ts = frt_lowercase_filter_new(ts);
    return ts;
}

FrtTokenStream *frt_standard_tokenizer_new(bool lowercase) {
    FrtTokenStream *ts = frt_standard_tokenizer_alloc();
    return frt_standard_tokenizer_init(ts, lowercase);
}

/*****************************************************************************/
/*** FrtFilters **************************************************************/
/*****************************************************************************/

#define TkFilt(filter) ((FrtTokenFilter *)(filter))

FrtTokenStream *frt_filter_clone_size(FrtTokenStream *ts, size_t size) {
    FrtTokenStream *ts_new = frt_ts_clone_size(ts, size);
    TkFilt(ts_new)->sub_ts = TkFilt(ts)->sub_ts->clone_i(TkFilt(ts)->sub_ts);
    return ts_new;
}

static FrtTokenStream *filter_clone_i(FrtTokenStream *ts) {
    return frt_filter_clone_size(ts, sizeof(FrtTokenFilter));
}

static FrtTokenStream *filter_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding) {
    TkFilt(ts)->sub_ts->reset(TkFilt(ts)->sub_ts, text, encoding);
    return ts;
}

static void filter_destroy_i(FrtTokenStream *ts) {
    frt_ts_deref(TkFilt(ts)->sub_ts);
    free(ts);
}

FrtTokenStream *frt_tf_alloc_i(size_t size) {
    return (FrtTokenStream *)frt_ecalloc(size);
}

FrtTokenStream *frt_tf_init(FrtTokenStream *ts, FrtTokenStream *sub_ts) {
    ts->clone_i        = &filter_clone_i;
    ts->destroy_i      = &filter_destroy_i;
    ts->reset          = &filter_reset;
    ts->ref_cnt        = 1;
    TkFilt(ts)->sub_ts = sub_ts;
    return ts;
}

FrtTokenStream *frt_tf_new_i(size_t size, FrtTokenStream *sub_ts) {
    FrtTokenStream *ts = frt_tf_alloc_i(size);
    return frt_tf_init(ts, sub_ts);
}

/*****************************************************************************/
/**** FrtStopFilter **********************************************************/
/*****************************************************************************/

#define StopFilt(filter) ((FrtStopFilter *)(filter))

static void sf_destroy_i(FrtTokenStream *ts) {
    frt_h_destroy(StopFilt(ts)->words);
    filter_destroy_i(ts);
}

static FrtTokenStream *sf_clone_i(FrtTokenStream *orig_ts) {
    FrtTokenStream *new_ts = frt_filter_clone_size(orig_ts, sizeof(FrtMappingFilter));
    FRT_REF(StopFilt(new_ts)->words);
    return new_ts;
}

static FrtToken *sf_next(FrtTokenStream *ts) {
    int pos_inc = 0;
    FrtHash *words = StopFilt(ts)->words;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);

    while ((tk != NULL) && (frt_h_get(words, tk->text) != NULL)) {
        pos_inc += tk->pos_inc;
        tk = tf->sub_ts->next(tf->sub_ts);
    }

    if (tk != NULL) {
        tk->pos_inc += pos_inc;
    }

    return tk;
}

FrtTokenStream *frt_stop_filter_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtStopFilter));
}

FrtTokenStream *frt_stop_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts) {
    frt_tf_init(ts, sub_ts);
    ts->next      = &sf_next;
    ts->destroy_i = &sf_destroy_i;
    ts->clone_i   = &sf_clone_i;
    return ts;
}

void frt_stop_filter_set_words_len(FrtTokenStream *ts, const char **words, int len) {
    int i;
    char *word;
    FrtHash *word_table = frt_h_new_str(&free, (frt_free_ft) NULL);
    for (i = 0; i < len; i++) {
        word = frt_estrdup(words[i]);
        frt_h_set(word_table, word, word);
    }
    StopFilt(ts)->words = word_table;
}

FrtTokenStream *frt_stop_filter_new_with_words_len(FrtTokenStream *sub_ts, const char **words, int len) {
    FrtTokenStream *ts = frt_stop_filter_alloc();
    ts = frt_stop_filter_init(ts, sub_ts);
    frt_stop_filter_set_words_len(ts, words, len);
    return ts;
}

void frt_stop_filter_set_words(FrtTokenStream *ts, const char **words) {
    char *word;
    FrtHash *word_table = frt_h_new_str(&free, (frt_free_ft) NULL);
    while (*words) {
        word = frt_estrdup(*words);
        frt_h_set(word_table, word, word);
        words++;
    }
    StopFilt(ts)->words = word_table;
}

FrtTokenStream *frt_stop_filter_new_with_words(FrtTokenStream *sub_ts, const char **words) {
    FrtTokenStream *ts = frt_stop_filter_alloc();
    frt_stop_filter_init(ts, sub_ts);
    frt_stop_filter_set_words(ts, words);
    return ts;
}

FrtTokenStream *frt_stop_filter_new(FrtTokenStream *sub_ts) {
    return frt_stop_filter_new_with_words(sub_ts, FRT_FULL_ENGLISH_STOP_WORDS);
}

/*****************************************************************************/
/*** MappingFilter ***********************************************************/
/*****************************************************************************/

#define MFilt(filter) ((FrtMappingFilter *)(filter))

static void mf_destroy_i(FrtTokenStream *ts) {
    frt_mulmap_destroy(MFilt(ts)->mapper);
    filter_destroy_i(ts);
}

static FrtTokenStream *mf_clone_i(FrtTokenStream *orig_ts) {
    FrtTokenStream *new_ts = frt_filter_clone_size(orig_ts, sizeof(FrtMappingFilter));
    FRT_REF(MFilt(new_ts)->mapper);
    return new_ts;
}

static FrtToken *mf_next(FrtTokenStream *ts) {
    char buf[FRT_MAX_WORD_SIZE + 1];
    FrtMultiMapper *mapper = MFilt(ts)->mapper;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);
    if (tk != NULL) {
        tk->len = frt_mulmap_map_len(mapper, buf, tk->text, FRT_MAX_WORD_SIZE);
        memcpy(tk->text, buf, tk->len + 1);
    }
    return tk;
}

static FrtTokenStream *mf_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding) {
    FrtMultiMapper *mm = MFilt(ts)->mapper;
    if (mm->d_size == 0)
        frt_mulmap_compile(MFilt(ts)->mapper);
    filter_reset(ts, text, encoding);
    return ts;
}

FrtTokenStream *frt_mapping_filter_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtMappingFilter));
}

void frt_mapping_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts) {
    ts->next           = &mf_next;
    ts->destroy_i      = &mf_destroy_i;
    ts->clone_i        = &mf_clone_i;
    ts->reset          = &mf_reset;
    MFilt(ts)->mapper  = frt_mulmap_new();
}

FrtTokenStream *frt_mapping_filter_new(FrtTokenStream *sub_ts) {
    FrtTokenStream *ts = frt_mapping_filter_alloc();
    frt_mapping_filter_init(ts, sub_ts);
    return ts;
}

FrtTokenStream *frt_mapping_filter_add(FrtTokenStream *ts, const char *pattern, const char *replacement) {
    frt_mulmap_add_mapping(MFilt(ts)->mapper, pattern, replacement);
    return ts;
}

/*****************************************************************************/
/*** FrtHyphenFilter *********************************************************/
/*****************************************************************************/

#define HyphenFilt(filter) ((FrtHyphenFilter *)(filter))

static FrtTokenStream *hf_clone_i(FrtTokenStream *orig_ts) {
    FrtTokenStream *new_ts = frt_filter_clone_size(orig_ts, sizeof(FrtHyphenFilter));
    return new_ts;
}

static FrtToken *hf_next(FrtTokenStream *ts) {
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = utf8_encoding;
    FrtHyphenFilter *hf = HyphenFilt(ts);
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = hf->tk;

    if (hf->pos < hf->len) {
        const int pos = hf->pos;
        const int text_len = strlen(hf->text + pos);
        strcpy(tk->text, hf->text + pos);
        tk->pos_inc = ((pos != 0) ? 1 : 0);
        tk->start = hf->start + pos;
        tk->end = tk->start + text_len;
        hf->pos += text_len + 1;
        tk->len = text_len;
        return tk;
    } else {
        char *t;
        char *end;

        bool seen_hyphen = false;
        bool seen_other_punc = false;
        hf->tk = tk = tf->sub_ts->next(tf->sub_ts);
        if (NULL == tk) return NULL;
        t = tk->text;
        end = tk->text + tk->len;
        get_cp(t, end, &cp_len, enc);
        t += cp_len; // skip first
        cp = get_cp(t, end, &cp_len, enc);
        while (cp > 0) {
            if (cp == cp_dash || cp == cp_hyphen) {
                seen_hyphen = true;
            } else if (!rb_enc_isalpha(cp, enc)) {
                seen_other_punc = true;
                break;
            }
            t += cp_len;
            cp = get_cp(t, end, &cp_len, enc);
        }
        if (seen_hyphen && !seen_other_punc) {
            char *q = hf->text;
            char *r = tk->text;
            t = tk->text;
            end = tk->text + tk->len;
            cp = 0;
            cp = get_cp(t, end, &cp_len, enc);
            while (cp > 0) {
                if (cp == cp_dash || cp == cp_hyphen) {
                    *q = '\0';
                    q++;
                } else {
                    memcpy(q, t, cp_len);
                    if (r!=t) memcpy(r, t, cp_len);
                    r += cp_len;
                    q += cp_len;
                }
                t += cp_len;
                cp = get_cp(t, end, &cp_len, enc);
            }
            *r = *q = '\0';
            hf->start = tk->start;
            hf->pos = 0;
            hf->len = q - hf->text;
            tk->len = r - tk->text;
        }
    }
    return tk;
}

FrtTokenStream *frt_hyphen_filter_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtHyphenFilter));
}

FrtTokenStream *frt_hyphen_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts) {
    frt_tf_init(ts, sub_ts);
    ts->next    = &hf_next;
    ts->clone_i = &hf_clone_i;
    return ts;
}

FrtTokenStream *frt_hyphen_filter_new(FrtTokenStream *sub_ts) {
    FrtTokenStream *ts = frt_hyphen_filter_alloc();
    return frt_hyphen_filter_init(ts, sub_ts);
}

/*****************************************************************************/
/*** FrtLowercaseFilter ******************************************************/
/*****************************************************************************/

static FrtToken *lcf_next(FrtTokenStream *ts) {
    int len = 0;
    OnigCaseFoldType fold_type = ONIGENC_CASE_DOWNCASE;
    rb_encoding *enc = utf8_encoding; // Token encoding is always UTF-8
    char buf[FRT_MAX_WORD_SIZE + 20]; // CASE_MAPPING_ADDITIONAL_LENGTH
    char *buf_end = buf + FRT_MAX_WORD_SIZE + 19;

    FrtToken *tk = TkFilt(ts)->sub_ts->next(TkFilt(ts)->sub_ts);
    if (tk == NULL) { return tk; }
    if (tk->len < 1) { return tk; }

    const OnigUChar *t = (const OnigUChar *)tk->text;

    len = enc->case_map(&fold_type, &t, (const OnigUChar *)(tk->text + tk->len), (OnigUChar *)buf, (OnigUChar *)buf_end, enc);
    tk->len = len;
    memcpy(tk->text, buf, len);
    tk->text[len] = '\0';

    return tk;
}

FrtTokenStream *frt_lowercase_filter_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtTokenFilter));
}

void frt_lowercase_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts) {
    frt_tf_init(ts, sub_ts);
    ts->next = &lcf_next;
}

FrtTokenStream *frt_lowercase_filter_new(FrtTokenStream *sub_ts) {
    FrtTokenStream *ts = frt_lowercase_filter_alloc();
    frt_lowercase_filter_init(ts, sub_ts);
    return ts;
}

/*****************************************************************************/
/*** FrtStemFilter ***********************************************************/
/*****************************************************************************/

#define StemFilt(filter) ((FrtStemFilter *)(filter))

static void stemf_destroy_i(FrtTokenStream *ts) {
    sb_stemmer_delete(StemFilt(ts)->stemmer);
    free(StemFilt(ts)->algorithm);
    free(StemFilt(ts)->charenc);
    filter_destroy_i(ts);
}

static FrtToken *stemf_next(FrtTokenStream *ts) {
    int len;
    const sb_symbol *stemmed;
    struct sb_stemmer *stemmer = StemFilt(ts)->stemmer;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);
    if (tk == NULL) {
        return tk;
    }
    stemmed = sb_stemmer_stem(stemmer, (sb_symbol *)tk->text, tk->len);
    len = sb_stemmer_length(stemmer);
    if (len >= FRT_MAX_WORD_SIZE) {
        len = FRT_MAX_WORD_SIZE - 1;
    }

    memcpy(tk->text, stemmed, len);
    tk->text[len] = '\0';
    tk->len = len;
    return tk;
}

static FrtTokenStream *stemf_clone_i(FrtTokenStream *orig_ts) {
    FrtTokenStream *new_ts    = frt_filter_clone_size(orig_ts, sizeof(FrtStemFilter));
    FrtStemFilter *stemf      = StemFilt(new_ts);
    FrtStemFilter *orig_stemf = StemFilt(orig_ts);
    stemf->stemmer = sb_stemmer_new(orig_stemf->algorithm, orig_stemf->charenc);
    stemf->algorithm = orig_stemf->algorithm ? frt_estrdup(orig_stemf->algorithm) : NULL;
    stemf->charenc = orig_stemf->charenc ? frt_estrdup(orig_stemf->charenc) : NULL;
    return new_ts;
}

FrtTokenStream *frt_stem_filter_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(FrtStemFilter));
}

void frt_stem_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts, const char *algorithm) {
    frt_tf_init(ts, sub_ts);
    ts->next      = &stemf_next;
    ts->destroy_i = &stemf_destroy_i;
    ts->clone_i   = &stemf_clone_i;

    char *my_algorithm = NULL;
    char *s = NULL;

    if (algorithm) {
        my_algorithm = frt_estrdup(algorithm);

        /* algorithms are lowercase */
        s = my_algorithm;
        while (*s) {
            *s = tolower(*s);
            s++;
        }
        StemFilt(ts)->algorithm = my_algorithm;
    }

    StemFilt(ts)->stemmer   = sb_stemmer_new(my_algorithm, "UTF_8");
}

FrtTokenStream *frt_stem_filter_new(FrtTokenStream *sub_ts, const char *algorithm) {
    FrtTokenStream *ts = frt_stem_filter_alloc();
    frt_stem_filter_init(ts, sub_ts, algorithm);
    return ts;
}

/*****************************************************************************/
/*** FrtAnalyzer *************************************************************/
/*****************************************************************************/

void frt_a_deref(FrtAnalyzer *a) {
    if (--a->ref_cnt <= 0)
        a->destroy_i(a);
}

static void frt_a_standard_destroy_i(FrtAnalyzer *a) {
    if (a->current_ts)
        frt_ts_deref(a->current_ts);
    free(a);
}

static FrtTokenStream *a_standard_get_ts(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding) {
    FrtTokenStream *ts;
    (void)field;
    ts = frt_ts_clone(a->current_ts);
    return ts->reset(ts, text, encoding);
}

FrtAnalyzer *frt_analyzer_alloc(void) {
    return (FrtAnalyzer *) FRT_ALLOC(FrtAnalyzer);
}

void frt_analyzer_init(FrtAnalyzer *a, FrtTokenStream *ts, void (*destroy_i)(FrtAnalyzer *a),
                       FrtTokenStream *(*get_ts)(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding)) {
    a->current_ts = ts;
    a->destroy_i = (destroy_i ? destroy_i : &frt_a_standard_destroy_i);
    a->get_ts = (get_ts ? get_ts : &a_standard_get_ts);
    a->ref_cnt = 1;
}

FrtAnalyzer *frt_analyzer_new(FrtTokenStream *ts, void (*destroy_i)(FrtAnalyzer *a),
                       FrtTokenStream *(*get_ts)(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding)) {
    FrtAnalyzer *a = frt_analyzer_alloc();
    frt_analyzer_init(a, ts, destroy_i, get_ts);
    return a;
}

/*****************************************************************************/
/*** FrtNonAnalyzer **********************************************************/
/*****************************************************************************/

FrtAnalyzer *frt_non_analyzer_new(void) {
    return frt_analyzer_new(frt_non_tokenizer_new(), NULL, NULL);
}

/*****************************************************************************/
/*** FrtWhiteSpaceAnalyzer ***************************************************/
/*****************************************************************************/

FrtAnalyzer *frt_whitespace_analyzer_alloc(void) {
    return frt_analyzer_alloc();
}

void frt_whitespace_analyzer_init(FrtAnalyzer *a, bool lowercase) {
    FrtTokenStream *ts = frt_whitespace_tokenizer_new(lowercase);
    frt_analyzer_init(a, ts, NULL, NULL);
}

FrtAnalyzer *frt_whitespace_analyzer_new(bool lowercase) {
    FrtAnalyzer *a = frt_whitespace_analyzer_alloc();
    frt_whitespace_analyzer_init(a, lowercase);
    return a;
}

/*****************************************************************************/
/*** FrtLetterAnalyzer *******************************************************/
/*****************************************************************************/

FrtAnalyzer *frt_letter_analyzer_alloc(void) {
    return frt_analyzer_alloc();
}

void frt_letter_analyzer_init(FrtAnalyzer *a, bool lowercase) {
    FrtTokenStream *ts = frt_letter_tokenizer_new(lowercase);
    frt_analyzer_init(a, ts, NULL, NULL);
}

FrtAnalyzer *frt_letter_analyzer_new(bool lowercase) {
    FrtAnalyzer *a = frt_letter_analyzer_alloc();
    frt_letter_analyzer_init(a, lowercase);
    return a;
}

/*****************************************************************************/
/*** FrtStandardAnalyzer *****************************************************/
/*****************************************************************************/

FrtAnalyzer *frt_standard_analyzer_alloc(void) {
    return frt_analyzer_alloc();
}

void frt_standard_analyzer_init(FrtAnalyzer *a, bool lowercase, const char **words) {
    FrtTokenStream *ts = frt_standard_tokenizer_new(lowercase);
    ts = frt_hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    frt_analyzer_init(a, ts, NULL, NULL);
}

FrtAnalyzer *frt_standard_analyzer_new_with_words(bool lowercase, const char **words) {
    FrtAnalyzer *a = frt_standard_analyzer_alloc();
    frt_standard_analyzer_init(a, lowercase, words);
    return a;
}

FrtAnalyzer *frt_standard_analyzer_new(bool lowercase) {
    return frt_standard_analyzer_new_with_words(lowercase, FRT_FULL_ENGLISH_STOP_WORDS);
}

/*****************************************************************************/
/*** FrtPerFieldAnalyzer *****************************************************/
/*****************************************************************************/

static void pfa_destroy_i(FrtAnalyzer *self) {
    frt_h_destroy(PFA(self)->dict);

    frt_a_deref(PFA(self)->default_a);
    free(self);
}

static FrtTokenStream *pfa_get_ts(FrtAnalyzer *self, FrtSymbol field, char *text, rb_encoding *encoding) {
    FrtAnalyzer *a = (FrtAnalyzer *)frt_h_get(PFA(self)->dict, (void *)field);
    if (a == NULL)
        a = PFA(self)->default_a;
    return frt_a_get_ts(a, field, text, encoding);
}

static void pfa_sub_a_destroy_i(void *p) {
    FrtAnalyzer *a = (FrtAnalyzer *) p;
    frt_a_deref(a);
}

void frt_pfa_add_field(FrtAnalyzer *self, FrtSymbol field, FrtAnalyzer *analyzer) {
    frt_h_set(PFA(self)->dict, (void *)field, analyzer);
}

FrtAnalyzer *frt_per_field_analyzer_alloc(void) {
    return (FrtAnalyzer *)frt_ecalloc(sizeof(FrtPerFieldAnalyzer));
}

void frt_per_field_analyzer_init(FrtAnalyzer *a, FrtAnalyzer *default_a) {
    a->destroy_i = &pfa_destroy_i;
    a->get_ts    = &pfa_get_ts;
    a->ref_cnt   = 1;

    PFA(a)->default_a = default_a;
    PFA(a)->dict = frt_h_new_ptr(&pfa_sub_a_destroy_i);
}

FrtAnalyzer *frt_per_field_analyzer_new(FrtAnalyzer *default_a) {
    FrtAnalyzer *a = frt_per_field_analyzer_alloc();
    frt_per_field_analyzer_init(a, default_a);
    return a;
}

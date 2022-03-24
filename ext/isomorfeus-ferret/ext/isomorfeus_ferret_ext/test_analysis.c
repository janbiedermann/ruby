#include "frt_analysis.h"
#include <string.h>
#include <libstemmer.h>
#include "test.h"

extern rb_encoding *utf8_encoding;

#define test_token(mtk, mstr, mstart, mend, menc) \
  tt_token(mtk, mstr, mstart, mend, menc, tc, __LINE__)

static void tt_token(FrtToken *tk, const char *str, int start, int end, rb_encoding * enc, TestCase *tc, int line_num)
{
    FrtToken frt_tk_exp;
    static char buf[3000];
    if (tk == NULL) {
        sprintf(buf, "Token1[NULL] != Token2[%d:%d:%s]\n",
                start, end, str);
        tst_assert(line_num, tc, false, buf);
        return;
    }
    if (!frt_tk_eq(frt_tk_set(&frt_tk_exp, (char *)str, (int)strlen(str), start, end, 1, enc), tk)) {
        sprintf(buf, "Token1[%d:%d:%s] != Token2[%d:%d:%s]\n",
                (int)tk->start, (int)tk->end, tk->text, start, end, str);
        tst_assert(line_num, tc, false, buf);
    }
    tst_int_equal(line_num, tc, strlen(tk->text), tk->len);
}

static void tt_token_pi(FrtToken *tk, const char *str, int start, int end, int pi, rb_encoding *enc, TestCase *tc, int line_num)
{
    FrtToken frt_tk_exp;
    static char buf[3000];
    if (tk == NULL) {
        sprintf(buf, "Token1[NULL] != Token2[%d:%d:%s]\n",
                start, end, str);
        tst_assert(line_num, tc, false, buf);
        return;
    }
    if (!frt_tk_eq(frt_tk_set(&frt_tk_exp, (char *)str, (int)strlen(str), start, end, pi, enc), tk)) {
        fprintf(stderr, "Token1[%d:%d:%s-%d] != \nToken2[%d:%d:%s-%d]\n",
                (int)tk->start, (int)tk->end, tk->text, tk->pos_inc,
                start, end, frt_tk_exp.text, pi);
        sprintf(buf, "Token1[%d:%d:%s-%d] != \nToken2[%d:%d:%s-%d]\n",
                (int)tk->start, (int)tk->end, tk->text, tk->pos_inc,
                start, end, frt_tk_exp.text, pi);
        tst_assert(line_num, tc, false, buf);
    }
    tst_int_equal(line_num, tc, strlen(tk->text), tk->len);
}

#define test_token_pi(mtk, mstr, mstart, mend, mpi, menc) \
  tt_token_pi(mtk, mstr, mstart, mend, mpi, menc, tc, __LINE__)

static void test_tk(TestCase *tc, void *data)
{
    FrtToken *tk1 = frt_tk_new();
    FrtToken *tk2 = frt_tk_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    (void)data;

    frt_tk_set_no_len(tk1, (char *)"DBalmain", 1, 8, 5, enc);
    frt_tk_set_no_len(tk2, (char *)"DBalmain", 1, 8, 5, enc);
    Assert(frt_tk_eq(tk1, tk2), "tokens are equal");
    frt_tk_set_no_len(tk2, (char *)"DBalmain", 1, 8, 1, enc);
    Assert(!frt_tk_eq(tk1, tk2), "tokens are not equal");

    frt_tk_set_no_len(tk2, (char *)"CBalmain", 1, 8, 5, enc);
    Assert(!frt_tk_eq(tk1, tk2), "tokens aren't equal");
    frt_tk_set_no_len(tk2, (char *)"DBalmain", 0, 8, 5, enc);
    Assert(!frt_tk_eq(tk1, tk2), "tokens aren't equal");
    frt_tk_set_no_len(tk2, (char *)"DBalmain", 1, 7, 5, enc);
    Assert(!frt_tk_eq(tk1, tk2), "tokens aren't equal");

    frt_tk_set_no_len(tk2, (char *)"CBalmain", 2, 7, 1, enc);
    Aiequal(-1, frt_tk_cmp(tk1, tk2));
    frt_tk_set_no_len(tk2, (char *)"EBalmain", 0, 9, 1, enc);
    Aiequal(1, frt_tk_cmp(tk1, tk2));
    frt_tk_set_no_len(tk2, (char *)"CBalmain", 1, 9, 1, enc);
    Aiequal(-1, frt_tk_cmp(tk1, tk2));
    frt_tk_set_no_len(tk2, (char *)"EBalmain", 1, 7, 1, enc);
    Aiequal(1, frt_tk_cmp(tk1, tk2));
    frt_tk_set_no_len(tk2, (char *)"EBalmain", 1, 8, 1, enc);
    Aiequal(-1, frt_tk_cmp(tk1, tk2));
    frt_tk_set_no_len(tk2, (char *)"CBalmain", 1, 8, 1, enc);
    Aiequal(1, frt_tk_cmp(tk1, tk2));

    Asequal("DBalmain", tk1->text);
    sprintf(tk1->text, "Hello");
    Asequal("Hello", tk1->text);
    Aiequal(1, tk1->start);
    Aiequal(8, tk1->end);
    Aiequal(5, tk1->pos_inc);
    frt_tk_destroy(tk1);
    frt_tk_destroy(tk2);
}

/****************************************************************************
 *
 * Non
 *
 ****************************************************************************/

static void test_non_tokenizer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_non_tokenizer_new();
    char text[100] = "DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$";
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), text, 0, strlen(text), enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

static void test_non_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_non_analyzer_new();
    char text[100] = "DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token(frt_ts_next(ts), text, 0, strlen(text), enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(a);
}

/****************************************************************************
 *
 * Whitespace
 *
 ****************************************************************************/

static void test_whitespace_tokenizer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_whitespace_tokenizer_new(false);
    char text[100] = "DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$";
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

static void test_mb_whitespace_tokenizer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_whitespace_tokenizer_new(false);
    char text[100] = "DBalmän@gmail.com is My e-mail 52   #$ address. 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ";
    rb_encoding *enc = utf8_encoding;
    (void)data;
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "DBalmän@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    test_token(frt_ts_next(ts), "ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ", 55, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    ts = frt_lowercase_filter_new(ts);
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "dbalmän@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    test_token(frt_ts_next(ts), "áägç®êëì¯úøã¬öîí", 55, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    ts = frt_whitespace_tokenizer_new(true);
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "dbalmän@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    test_token(frt_ts_next(ts), "áägç®êëì¯úøã¬öîí", 55, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
    frt_tk_destroy(tk);
}

static void test_whitespace_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_whitespace_analyzer_new(false);
    char text[100] = "DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(a);
}

static void test_mb_whitespace_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_whitespace_analyzer_new(false);
    char text[100] = "DBalmän@gmail.com is My e-mail 52   #$ address. 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ";
    rb_encoding *enc = utf8_encoding;
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token(frt_ts_next(ts), "DBalmän@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    test_token(frt_ts_next(ts), "ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ", 55, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_whitespace_analyzer_new(true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "dbalmän@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "#$", 37, 39, enc);
    test_token(frt_ts_next(ts), "address.", 40, 48, enc);
    test_token(frt_ts_next(ts), "23#!$", 49, 54, enc);
    test_token(frt_ts_next(ts), "áägç®êëì¯úøã¬öîí", 55, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(a);
}

/****************************************************************************
 *
 * Letter
 *
 ****************************************************************************/

static void test_letter_tokenizer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_letter_tokenizer_new(false);
    char text[100] = "DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$";
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "DBalmain", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

static void test_mb_letter_tokenizer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_letter_tokenizer_new(false);
    char text[100] = "DBalmän@gmail.com is My e-mail 52   #$ address. 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ";
    (void)data;
    rb_encoding *enc = utf8_encoding;
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "DBalmän", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    test_token(frt_ts_next(ts), "ÁÄGÇ", 55, 62, enc);
    test_token(frt_ts_next(ts), "ÊËÌ", 64, 70, enc);
    test_token(frt_ts_next(ts), "ÚØÃ", 72, 78, enc);
    test_token(frt_ts_next(ts), "ÖÎÍ", 80, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    ts = frt_lowercase_filter_new(ts);
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "dbalmän", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    test_token(frt_ts_next(ts), "áägç", 55, 62, enc);
    test_token(frt_ts_next(ts), "êëì", 64, 70, enc);
    test_token(frt_ts_next(ts), "úøã", 72, 78, enc);
    test_token(frt_ts_next(ts), "öîí", 80, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    ts = frt_letter_tokenizer_new(true);
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "dbalmän", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    test_token(frt_ts_next(ts), "áägç", 55, 62, enc);
    test_token(frt_ts_next(ts), "êëì", 64, 70, enc);
    test_token(frt_ts_next(ts), "úøã", 72, 78, enc);
    test_token(frt_ts_next(ts), "öîí", 80, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
    frt_tk_destroy(tk);
}

static void test_letter_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_letter_analyzer_new(true);
    char text[100] = "DBalmain@gmail.com is My e-mail 52   #$ address. 23#!$";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token(frt_ts_next(ts), "dbalmain", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(a);
}

static void test_mb_letter_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_letter_analyzer_new(false);
    char text[100] =
        "DBalmän@gmail.com is My e-mail 52   #$ address. 23#!$ "
        "ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ";
    rb_encoding *enc = utf8_encoding;
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token(frt_ts_next(ts), "DBalmän", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    test_token(frt_ts_next(ts), "ÁÄGÇ", 55, 62, enc);
    test_token(frt_ts_next(ts), "ÊËÌ", 64, 70, enc);
    test_token(frt_ts_next(ts), "ÚØÃ", 72, 78, enc);
    test_token(frt_ts_next(ts), "ÖÎÍ", 80, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_letter_analyzer_new(true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    test_token(frt_ts_next(ts), "dbalmän", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    test_token(frt_ts_next(ts), "áägç", 55, 62, enc);
    test_token(frt_ts_next(ts), "êëì", 64, 70, enc);
    test_token(frt_ts_next(ts), "úøã", 72, 78, enc);
    test_token(frt_ts_next(ts), "öîí", 80, 86, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_a_deref(a);
    frt_ts_deref(ts);
    frt_tk_destroy(tk);
}


/****************************************************************************
 *
 * Standard
 *
 ****************************************************************************/

static void do_standard_tokenizer(TestCase *tc, FrtTokenStream *ts)
{
    FrtToken *tk = frt_tk_new();
    char text[200] =
        "DBalmain@gmail.com is My e-mail -52  #$ Address. 23#!$ "
        "http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 "
        "underscored_word, won't we're";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "-52", 32, 35, enc);
    test_token(frt_ts_next(ts), "Address", 40, 47, enc);
    test_token(frt_ts_next(ts), "23", 49, 51, enc);
    test_token(frt_ts_next(ts), "www.google.com/results", 55, 85, enc);
    test_token(frt_ts_next(ts), "TNT", 86, 91, enc);
    test_token(frt_ts_next(ts), "123-1235-ASD-1234", 93, 110, enc);
    test_token(frt_ts_next(ts), "underscored_word", 111, 127, enc);
    test_token(frt_ts_next(ts), "won't", 129, 134, enc);
    test_token(frt_ts_next(ts), "we're", 135, 140, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    ts->reset(ts, (char *)"http://xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
              "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
              "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
              "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
              "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", enc);
    test_token(frt_ts_next(ts), "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxx", 0, 280, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
}

static void test_standard_tokenizer(TestCase *tc, void *data)
{
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    (void)data;
    do_standard_tokenizer(tc, ts);
    frt_ts_deref(ts);
}

static void test_legacy_standard_tokenizer(TestCase *tc, void *data)
{
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    (void)data;
    do_standard_tokenizer(tc, ts);
    frt_ts_deref(ts);
}

static void do_mb_standard_tokenizer(TestCase *tc, FrtTokenStream *ts)
{
    FrtToken *tk = frt_tk_new();
    char text[512] =
        "DBalmain@gmail.com is My e-mail -52  #$ Address. 23#!$ "
        "http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 "
        "underscored_word, won't we're 23#!$ ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ "
        "  badchar it's groups' Barnes&Noble file:///home/user/ "
        "svn://www.davebalmain.com/ www,.google.com www.google.com "
        "dave@balmain@gmail.com \"quoted string\" continue *star";
    rb_encoding *enc = utf8_encoding;
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "-52", 32, 35, enc);
    test_token(frt_ts_next(ts), "Address", 40, 47, enc);
    test_token(frt_ts_next(ts), "23", 49, 51, enc);
    test_token(frt_ts_next(ts), "www.google.com/results", 55, 85, enc);
    test_token(frt_ts_next(ts), "TNT", 86, 91, enc);
    test_token(frt_ts_next(ts), "123-1235-ASD-1234", 93, 110, enc);
    test_token(frt_ts_next(ts), "underscored_word", 111, 127, enc);
    test_token(frt_ts_next(ts), "won't", 129, 134, enc);
    test_token(frt_ts_next(ts), "we're", 135, 140, enc);
    test_token(frt_ts_next(ts), "23", 141, 143, enc);
    test_token(frt_ts_next(ts), "ÁÄGÇ", 147, 154, enc);
    test_token(frt_ts_next(ts), "ÊËÌ", 156, 162, enc);
    test_token(frt_ts_next(ts), "ÚØÃ", 164, 170, enc);
    test_token(frt_ts_next(ts), "ÖÎÍ", 172, 178, enc);
    test_token(frt_ts_next(ts), "badchar", 181, 188, enc);
    test_token(frt_ts_next(ts), "it", 189, 193, enc);
    test_token(frt_ts_next(ts), "groups", 194, 201, enc);
    test_token(frt_ts_next(ts), "Barnes&Noble", 202, 214, enc);
    test_token(frt_ts_next(ts), "home/user", 215, 233, enc);
    test_token(frt_ts_next(ts), "svn://www.davebalmain.com", 234, 260, enc);
    test_token(frt_ts_next(ts), "www", 261, 264, enc);
    test_token(frt_ts_next(ts), "google.com", 266, 276, enc);
    test_token(frt_ts_next(ts), "www.google.com", 277, 291, enc);
    test_token(frt_ts_next(ts), "dave@balmain", 292, 304, enc);
    test_token(frt_ts_next(ts), "gmail.com", 305, 314, enc);
    test_token(frt_ts_next(ts), "quoted", 316, 322, enc);
    test_token(frt_ts_next(ts), "string", 323, 329, enc);
    test_token(frt_ts_next(ts), "continue", 331, 339, enc);
    test_token(frt_ts_next(ts), "star", 341, 345, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);                    /* test ref_cnt */
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    ts->reset(ts, (char *)"http://xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", enc);
    test_token(frt_ts_next(ts), "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxx", 0, 280, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    ts->reset(ts, (char *)"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", enc);
    test_token(frt_ts_next(ts), "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
               "xxxxxxxxxxxxxxxxxxx", 0, 348, enc);
}

static void test_mb_standard_tokenizer(TestCase *tc, void *data)
{
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    (void)data;
    do_mb_standard_tokenizer(tc, ts);
    frt_ts_deref(ts);
}

static void test_mb_legacy_standard_tokenizer(TestCase *tc, void *data)
{
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    (void)data;
    do_mb_standard_tokenizer(tc, ts);
    frt_ts_deref(ts);
}

static void test_standard_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_standard_analyzer_new_with_words(FRT_ENGLISH_STOP_WORDS, true);
    char text[200] =
        "DBalmain@gmail.com is My e-mail and the Address. -23!$ "
        "http://www.google.com/results/ T.N.T. 123-1235-ASD-1234";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "my", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 3, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "tnt", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(a);
}

static void test_mb_standard_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a =
        frt_standard_analyzer_new_with_words(FRT_ENGLISH_STOP_WORDS, false);
    const char *words[] = { "is", "the", "-23", "tnt", NULL };
    char text[200] =
        "DBalmain@gmail.com is My e-mail and the Address. -23!$ "
        "http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 23#!$ "
        "ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ";
    rb_encoding *enc = utf8_encoding;
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc), *ts2;
    (void)data;

    test_token_pi(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "My", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "Address", 40, 47, 3, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "TNT", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-ASD-1234", 93, 110, 1, enc);
    test_token_pi(frt_ts_next(ts), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÁÄGÇ", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÊËÌ", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÚØÃ", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÖÎÍ", 142, 148, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_standard_analyzer_new(true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 3, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 3, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "tnt", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 1, enc);
    test_token_pi(frt_ts_next(ts), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts), "áägç", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts), "êëì", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts), "úøã", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts), "öîí", 142, 148, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_standard_analyzer_new_with_words(words, true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    ts2 = frt_a_get_ts(a, rb_intern("random"), text, enc);
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "my", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "and", 32, 35, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 2, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 2, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 2, enc);
    test_token_pi(frt_ts_next(ts), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts), "áägç", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts), "êëì", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts), "úøã", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts), "öîí", 142, 148, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    test_token_pi(frt_ts_next(ts2), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts2), "my", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts2), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts2), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts2), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts2), "and", 32, 35, 1, enc);
    test_token_pi(frt_ts_next(ts2), "address", 40, 47, 2, enc);
    test_token_pi(frt_ts_next(ts2), "www.google.com/results", 55, 85, 2, enc);
    test_token_pi(frt_ts_next(ts2), "123-1235-asd-1234", 93, 110, 2, enc);
    test_token_pi(frt_ts_next(ts2), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts2), "áägç", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts2), "êëì", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts2), "úøã", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts2), "öîí", 142, 148, 1, enc);
    Assert(frt_ts_next(ts2) == NULL, "Should be no more tokens");
    ts2->ref_cnt = 3;
    ts = frt_ts_clone(ts2);
    Aiequal(3, ts2->ref_cnt);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts2);
    Aiequal(2, ts2->ref_cnt);
    frt_ts_deref(ts2);
    Aiequal(1, ts2->ref_cnt);
    frt_ts_deref(ts2);
    frt_ts_deref(ts);
    frt_a_deref(a);
    frt_tk_destroy(tk);
}

static void test_legacy_standard_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a =
        frt_standard_analyzer_new_with_words(FRT_ENGLISH_STOP_WORDS, true);
    char text[200] =
        "DBalmain@gmail.com is My e-mail and the Address. -23!$ "
        "http://www.google.com/results/ T.N.T. 123-1235-ASD-1234";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "my", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 3, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "tnt", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(a);
}

static void test_mb_legacy_standard_analyzer(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a =
        frt_standard_analyzer_new_with_words(FRT_ENGLISH_STOP_WORDS, false);
    const char *words[] = { "is", "the", "-23", "tnt", NULL };
    char text[200] =
        "DBalmain@gmail.com is My e-mail and the Address. -23!$ "
        "http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 23#!$ "
        "ÁÄGÇ®ÊËÌ¯ÚØÃ¬ÖÎÍ";
    rb_encoding *enc = utf8_encoding;
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc), *ts2;
    (void)data;

    test_token_pi(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "My", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "Address", 40, 47, 3, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "TNT", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-ASD-1234", 93, 110, 1, enc);
    test_token_pi(frt_ts_next(ts), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÁÄGÇ", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÊËÌ", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÚØÃ", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts), "ÖÎÍ", 142, 148, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_standard_analyzer_new(true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 3, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 3, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "tnt", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 1, enc);
    test_token_pi(frt_ts_next(ts), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts), "áägç", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts), "êëì", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts), "úøã", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts), "öîí", 142, 148, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_standard_analyzer_new_with_words(words, true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    ts2 = frt_a_get_ts(a, rb_intern("random"), text, enc);
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "my", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "and", 32, 35, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 2, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 2, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 2, enc);
    test_token_pi(frt_ts_next(ts), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts), "áägç", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts), "êëì", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts), "úøã", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts), "öîí", 142, 148, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    test_token_pi(frt_ts_next(ts2), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts2), "my", 22, 24, 2, enc);
    test_token_pi(frt_ts_next(ts2), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts2), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts2), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts2), "and", 32, 35, 1, enc);
    test_token_pi(frt_ts_next(ts2), "address", 40, 47, 2, enc);
    test_token_pi(frt_ts_next(ts2), "www.google.com/results", 55, 85, 2, enc);
    test_token_pi(frt_ts_next(ts2), "123-1235-asd-1234", 93, 110, 2, enc);
    test_token_pi(frt_ts_next(ts2), "23", 111, 113, 1, enc);
    test_token_pi(frt_ts_next(ts2), "áägç", 117, 124, 1, enc);
    test_token_pi(frt_ts_next(ts2), "êëì", 126, 132, 1, enc);
    test_token_pi(frt_ts_next(ts2), "úøã", 134, 140, 1, enc);
    test_token_pi(frt_ts_next(ts2), "öîí", 142, 148, 1, enc);
    Assert(frt_ts_next(ts2) == NULL, "Should be no more tokens");
    ts2->ref_cnt = 3;
    ts = frt_ts_clone(ts2);
    Aiequal(3, ts2->ref_cnt);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts2);
    Aiequal(2, ts2->ref_cnt);
    frt_ts_deref(ts2);
    Aiequal(1, ts2->ref_cnt);
    frt_ts_deref(ts2);
    frt_ts_deref(ts);
    frt_a_deref(a);
    frt_tk_destroy(tk);
}

static void test_long_word(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtAnalyzer *a = frt_standard_analyzer_new_with_words(FRT_ENGLISH_STOP_WORDS, true);
    char text[400] =
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx two";
    char text_a[400] =
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    rb_encoding *enc = utf8_encoding;
    FrtTokenStream *ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    (void)data;

    test_token_pi(frt_ts_next(ts), text_a, 0, 290, 1, enc);
    test_token_pi(frt_ts_next(ts), "two", 291, 294, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    a = frt_standard_analyzer_new_with_words(FRT_ENGLISH_STOP_WORDS, true);
    ts = frt_a_get_ts(a, rb_intern("random"), text, enc);
    test_token_pi(frt_ts_next(ts), text_a, 0, 290, 1, enc);
    test_token_pi(frt_ts_next(ts), "two", 291, 294, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    frt_a_deref(a);
    frt_tk_destroy(tk);
}

/****************************************************************************
 *
 * Filters
 *
 ****************************************************************************/

static void test_lowercase_filter(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_lowercase_filter_new(frt_standard_tokenizer_new());
    char text[200] =
        "DBalmain@gmail.com is My e-mail 52   #$ Address. -23!$ http://www.google.com/results/ T.N.T. 123-1235-ASD-1234";
    (void)data;
    rb_encoding *enc = utf8_encoding;
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e-mail", 25, 31, enc);
    test_token(frt_ts_next(ts), "52", 32, 34, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    test_token(frt_ts_next(ts), "-23", 49, 52, enc);
    test_token(frt_ts_next(ts), "www.google.com/results", 55, 85, enc);
    test_token(frt_ts_next(ts), "tnt", 86, 91, enc);
    test_token(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

static void test_hyphen_filter(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_hyphen_filter_new(frt_lowercase_filter_new(frt_standard_tokenizer_new()));
    char text[200] =
        "DBalmain@gmail.com is My e-mail 52   #$ Address. -23!$ http://www.google.com/results/ T.N.T. 123-1235-ASD-1234 long-hyph-en-at-ed-word";
    (void)data;
    rb_encoding *enc = utf8_encoding;
    ts->reset(ts, text, enc);
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "is", 19, 21, 1, enc);
    test_token_pi(frt_ts_next(ts), "my", 22, 24, 1, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "52", 32, 34, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 1, enc);
    test_token_pi(frt_ts_next(ts), "-23", 49, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "www.google.com/results", 55, 85, 1, enc);
    test_token_pi(frt_ts_next(ts), "tnt", 86, 91, 1, enc);
    test_token_pi(frt_ts_next(ts), "123-1235-asd-1234", 93, 110, 1, enc);
    test_token_pi(frt_ts_next(ts), "longhyphenatedword", 111, 134, 1, enc);
    test_token_pi(frt_ts_next(ts), "long", 111, 115, 0, enc);
    test_token_pi(frt_ts_next(ts), "hyph", 116, 120, 1, enc);
    test_token_pi(frt_ts_next(ts), "en", 121, 123, 1, enc);
    test_token_pi(frt_ts_next(ts), "at", 124, 126, 1, enc);
    test_token_pi(frt_ts_next(ts), "ed", 127, 129, 1, enc);
    test_token_pi(frt_ts_next(ts), "word", 130, 134, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

const char *words[] = { "one", "four", "five", "seven", NULL };
static void test_stop_filter(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts =
        frt_stop_filter_new_with_words(frt_letter_tokenizer_new(false), words);
    char text[200] =
        "one, two, three, four, five, six, seven, eight, nine, ten.";
    (void)data;
    rb_encoding *enc = utf8_encoding;

    ts->reset(ts, text, enc);
    test_token_pi(frt_ts_next(ts), "two", 5, 8, 2, enc);
    test_token_pi(frt_ts_next(ts), "three", 10, 15, 1, enc);
    test_token_pi(frt_ts_next(ts), "six", 29, 32, 3, enc);
    test_token_pi(frt_ts_next(ts), "eight", 41, 46, 2, enc);
    test_token_pi(frt_ts_next(ts), "nine", 48, 52, 1, enc);
    test_token_pi(frt_ts_next(ts), "ten", 54, 57, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

static void test_mapping_filter(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_mapping_filter_new(frt_letter_tokenizer_new(false));
    char text[200] =
        "one, two, three, four, five, six, seven, eight, nine, ten.";
    char long_word[301] =
       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    (void)data;
    rb_encoding *enc = utf8_encoding;

    frt_mapping_filter_add(ts, "ne", "hello");
    frt_mapping_filter_add(ts, "four", long_word);

    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "ohello", 0, 3, enc);
    test_token(frt_ts_next(ts), "two", 5, 8, enc);
    test_token(frt_ts_next(ts), "three", 10, 15, enc);
    test_token(frt_ts_next(ts), long_word, 17, 21, enc);
    test_token(frt_ts_next(ts), "five", 23, 27, enc);
    test_token(frt_ts_next(ts), "six", 29, 32, enc);
    test_token(frt_ts_next(ts), "seven", 34, 39, enc);
    test_token(frt_ts_next(ts), "eight", 41, 46, enc);
    test_token(frt_ts_next(ts), "nihello", 48, 52, enc);
    test_token(frt_ts_next(ts), "ten", 54, 57, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");

    frt_mapping_filter_add(ts, "thr", "start");
    frt_mapping_filter_add(ts, "en", "goodbye");
    ts->reset(ts, text, enc);
    test_token(frt_ts_next(ts), "ohello", 0, 3, enc);
    test_token(frt_ts_next(ts), "two", 5, 8, enc);
    test_token(frt_ts_next(ts), "startee", 10, 15, enc);
    test_token(frt_ts_next(ts), long_word, 17, 21, enc);
    test_token(frt_ts_next(ts), "five", 23, 27, enc);
    test_token(frt_ts_next(ts), "six", 29, 32, enc);
    test_token(frt_ts_next(ts), "sevgoodbye", 34, 39, enc);
    test_token(frt_ts_next(ts), "eight", 41, 46, enc);
    test_token(frt_ts_next(ts), "nihello", 48, 52, enc);
    test_token(frt_ts_next(ts), "tgoodbye", 54, 57, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    FRT_REF(ts);
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
}

static void test_stemmer(TestCase *tc, void *data)
{
    int stemmer_cnt = 0;
    const char **stemmers = sb_stemmer_list();
    const char **st = stemmers;
    struct sb_stemmer *stemmer;
    (void)data;

    while (*st) {
        stemmer_cnt++;
        st++;
    }

    stemmer = sb_stemmer_new("english", NULL);

    Apnotnull(stemmer);
    sb_stemmer_delete(stemmer);
    Assert(stemmer_cnt >= 13, "There should be at least 10 stemmers");
}

static void test_stem_filter(TestCase *tc, void *data)
{
    FrtToken *tk = frt_tk_new();
    FrtTokenStream *ts = frt_stem_filter_new(frt_letter_tokenizer_new(true), "english");
    FrtTokenStream *ts2;
    char text[200] = "debate debates debated debating debater";
    char text2[200] = "dêbate dêbates dêbated dêbating dêbater";
    (void)data;

    rb_encoding *enc = utf8_encoding;
    ts->reset(ts, text, enc);
    ts2 = frt_ts_clone(ts);
    test_token(frt_ts_next(ts), "debat", 0, 6, enc);
    test_token(frt_ts_next(ts), "debat", 7, 14, enc);
    test_token(frt_ts_next(ts), "debat", 15, 22, enc);
    test_token(frt_ts_next(ts), "debat", 23, 31, enc);
    test_token(frt_ts_next(ts), "debat", 32, 39, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    ts->reset(ts, text2, enc);
    test_token(frt_ts_next(ts), "dêbate", 0, 7, enc);
    test_token(frt_ts_next(ts), "dêbate", 8, 16, enc);
    test_token(frt_ts_next(ts), "dêbate", 17, 25, enc);
    test_token(frt_ts_next(ts), "dêbate", 26, 35, enc);
    test_token(frt_ts_next(ts), "dêbater", 36, 44, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    FRT_REF(ts);
    Aiequal(2, ts->ref_cnt);
    frt_ts_deref(ts);
    Aiequal(1, ts->ref_cnt);
    frt_ts_deref(ts);
    test_token(frt_ts_next(ts2), "debat", 0, 6, enc);
    test_token(frt_ts_next(ts2), "debat", 7, 14, enc);
    test_token(frt_ts_next(ts2), "debat", 15, 22, enc);
    test_token(frt_ts_next(ts2), "debat", 23, 31, enc);
    test_token(frt_ts_next(ts2), "debat", 32, 39, enc);
    Assert(frt_ts_next(ts2) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts2);
}

static void test_per_field_analyzer(TestCase *tc, void *data)
{
    FrtTokenStream *ts;
    FrtToken *tk = frt_tk_new();
    char text[100] = "DBalmain@gmail.com is My E-mail 52   #$ address. 23#!$";
    FrtAnalyzer *pfa = frt_per_field_analyzer_new(frt_standard_analyzer_new(true));
    (void)data;
    rb_encoding *enc = utf8_encoding;

    frt_pfa_add_field(pfa, rb_intern("white"), frt_whitespace_analyzer_new(false));
    frt_pfa_add_field(pfa, rb_intern("white_l"), frt_whitespace_analyzer_new(true));
    frt_pfa_add_field(pfa, rb_intern("letter"), frt_letter_analyzer_new(false));
    frt_pfa_add_field(pfa, rb_intern("letter"), frt_letter_analyzer_new(true));
    frt_pfa_add_field(pfa, rb_intern("letter_u"), frt_letter_analyzer_new(false));
    ts = frt_a_get_ts(pfa, rb_intern("white"), text, enc);
    test_token_pi(frt_ts_next(ts), "DBalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "is", 19, 21, 1, enc);
    test_token_pi(frt_ts_next(ts), "My", 22, 24, 1, enc);
    test_token_pi(frt_ts_next(ts), "E-mail", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "52", 32, 34, 1, enc);
    test_token_pi(frt_ts_next(ts), "#$", 37, 39, 1, enc);
    test_token_pi(frt_ts_next(ts), "address.", 40, 48, 1, enc);
    test_token_pi(frt_ts_next(ts), "23#!$", 49, 54, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    ts = frt_a_get_ts(pfa, rb_intern("white_l"), text, enc);
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "is", 19, 21, 1, enc);
    test_token_pi(frt_ts_next(ts), "my", 22, 24, 1, enc);
    test_token_pi(frt_ts_next(ts), "e-mail", 25, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "52", 32, 34, 1, enc);
    test_token_pi(frt_ts_next(ts), "#$", 37, 39, 1, enc);
    test_token_pi(frt_ts_next(ts), "address.", 40, 48, 1, enc);
    test_token_pi(frt_ts_next(ts), "23#!$", 49, 54, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    ts = frt_a_get_ts(pfa, rb_intern("letter_u"), text, enc);
    test_token(frt_ts_next(ts), "DBalmain", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "My", 22, 24, enc);
    test_token(frt_ts_next(ts), "E", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    ts = frt_a_get_ts(pfa, rb_intern("letter"), text, enc);
    test_token(frt_ts_next(ts), "dbalmain", 0, 8, enc);
    test_token(frt_ts_next(ts), "gmail", 9, 14, enc);
    test_token(frt_ts_next(ts), "com", 15, 18, enc);
    test_token(frt_ts_next(ts), "is", 19, 21, enc);
    test_token(frt_ts_next(ts), "my", 22, 24, enc);
    test_token(frt_ts_next(ts), "e", 25, 26, enc);
    test_token(frt_ts_next(ts), "mail", 27, 31, enc);
    test_token(frt_ts_next(ts), "address", 40, 47, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_ts_deref(ts);
    ts = frt_a_get_ts(pfa, rb_intern("XXX"), text, enc);        /* should use default analyzer */
    test_token_pi(frt_ts_next(ts), "dbalmain@gmail.com", 0, 18, 1, enc);
    test_token_pi(frt_ts_next(ts), "email", 25, 31, 3, enc);
    test_token_pi(frt_ts_next(ts), "e", 25, 26, 0, enc);
    test_token_pi(frt_ts_next(ts), "mail", 27, 31, 1, enc);
    test_token_pi(frt_ts_next(ts), "52", 32, 34, 1, enc);
    test_token_pi(frt_ts_next(ts), "address", 40, 47, 1, enc);
    test_token_pi(frt_ts_next(ts), "23", 49, 51, 1, enc);
    Assert(frt_ts_next(ts) == NULL, "Should be no more tokens");
    frt_tk_destroy(tk);
    frt_ts_deref(ts);
    frt_a_deref(pfa);
}

TestSuite *ts_analysis(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_tk, NULL);

    /* Non */
    tst_run_test(suite, test_non_tokenizer, NULL);
    tst_run_test(suite, test_non_analyzer, NULL);

    /* Whitespace */
    tst_run_test(suite, test_whitespace_tokenizer, NULL);
    tst_run_test(suite, test_mb_whitespace_tokenizer, NULL);

    tst_run_test(suite, test_whitespace_analyzer, NULL);
    tst_run_test(suite, test_mb_whitespace_analyzer, NULL);

    /* Letter */
    tst_run_test(suite, test_letter_tokenizer, NULL);
    tst_run_test(suite, test_mb_letter_tokenizer, NULL);

    tst_run_test(suite, test_letter_analyzer, NULL);
    tst_run_test(suite, test_mb_letter_analyzer, NULL);

    /* Standard */
    tst_run_test(suite, test_standard_tokenizer, NULL);
    tst_run_test(suite, test_mb_standard_tokenizer, NULL);

    tst_run_test(suite, test_standard_analyzer, NULL);
    tst_run_test(suite, test_mb_standard_analyzer, NULL);

    /* LegacyStandard */
    tst_run_test(suite, test_legacy_standard_tokenizer, NULL);
    tst_run_test(suite, test_mb_legacy_standard_tokenizer, NULL);

    tst_run_test(suite, test_legacy_standard_analyzer, NULL);
    tst_run_test(suite, test_mb_legacy_standard_analyzer, NULL);

    tst_run_test(suite, test_long_word, NULL);

    /* PerField */
    tst_run_test(suite, test_per_field_analyzer, NULL);

    /* Filters */
    tst_run_test(suite, test_lowercase_filter, NULL);
    tst_run_test(suite, test_hyphen_filter, NULL);
    tst_run_test(suite, test_stop_filter, NULL);
    tst_run_test(suite, test_mapping_filter, NULL);
    tst_run_test(suite, test_stem_filter, NULL);

    tst_run_test(suite, test_stemmer, NULL);

    return suite;
}

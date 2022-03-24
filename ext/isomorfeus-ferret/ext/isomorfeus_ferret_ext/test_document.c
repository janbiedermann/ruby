#include "frt_document.h"
#include "test.h"

void test_df_standard(TestCase *tc, void *data)
{
    char *s;
    FrtDocField *df;
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    df = frt_df_add_data(frt_df_new(rb_intern("title")), frt_estrdup("Life of Pi"), enc);
    df->destroy_data = true;
    Aiequal(1, df->size);
    Asequal("title", rb_id2name(df->name));
    Asequal("Life of Pi", df->data[0]);
    Aiequal(strlen("Life of Pi"), df->lengths[0]);
    Asequal("title: \"Life of Pi\"", s = frt_df_to_s(df));
    Afequal(1.0, df->boost);
    free(s);
    frt_df_destroy(df);

    df = frt_df_add_data_len(frt_df_new(rb_intern("title")), (char *)"new title", 9, enc);
    Aiequal(1, df->size);
    Asequal("title", rb_id2name(df->name));
    Asequal("new title", df->data[0]);
    Aiequal(9, df->lengths[0]);
    frt_df_destroy(df);
}

void test_df_multi_fields(TestCase *tc, void *data)
{
    int i;
    char *s;
    FrtDocField *df;
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    df = frt_df_add_data(frt_df_new(rb_intern("title")), frt_estrdup("Vernon God Little"), enc);
    df->destroy_data = true;
    Aiequal(1, df->size);
    Asequal("title", rb_id2name(df->name));
    Asequal("Vernon God Little", df->data[0]);
    Aiequal(strlen("Vernon God Little"), df->lengths[0]);

    frt_df_add_data(df, frt_estrdup("some more data"), enc);
    Aiequal(2, df->size);
    Asequal("title: [\"Vernon God Little\", \"some more data\"]",
            s = frt_df_to_s(df));
    free(s);
    frt_df_add_data_len(df, frt_estrdup("and more data"), 14, enc);
    Aiequal(3, df->size);
    Asequal("title", rb_id2name(df->name));
    Asequal("Vernon God Little", df->data[0]);
    Asequal("some more data", df->data[1]);
    Asequal("and more data", df->data[2]);

    frt_df_destroy(df);

    df = frt_df_add_data(frt_df_new(rb_intern("data")), frt_estrdup("start"), enc);
    df->destroy_data = true;
    Aiequal(1, df->size);
    for (i = 0; i < 1000; i++) {
        char buf[100];
        sprintf(buf, "<<%d>>", i);
        frt_df_add_data(df, frt_estrdup(buf), enc);
        Aiequal(i + 2, df->size);
    }
    frt_df_destroy(df);
}

void test_doc(TestCase *tc, void *data)
{
    int i;
    FrtDocument *doc;
    FrtDocField *df;
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    doc = frt_doc_new();
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("title")), (char *)"title", enc));
    Aiequal(1, doc->size);
    df = frt_df_add_data(frt_df_new(rb_intern("data")), (char *)"data1", enc);
    frt_df_add_data(df, (char *)"data2", enc);
    frt_df_add_data(df, (char *)"data3", enc);
    frt_df_add_data(df, (char *)"data4", enc);
    frt_doc_add_field(doc, df);
    Aiequal(2, doc->size);
    Asequal("title", rb_id2name(frt_doc_get_field(doc, rb_intern("title"))->name));
    Aiequal(1, frt_doc_get_field(doc, rb_intern("title"))->size);
    Asequal("title", frt_doc_get_field(doc, rb_intern("title"))->data[0]);
    Asequal("data", rb_id2name(frt_doc_get_field(doc, rb_intern("data"))->name));
    Aiequal(4, frt_doc_get_field(doc, rb_intern("data"))->size);
    Asequal("data1", frt_doc_get_field(doc, rb_intern("data"))->data[0]);
    Asequal("data2", frt_doc_get_field(doc, rb_intern("data"))->data[1]);
    Asequal("data3", frt_doc_get_field(doc, rb_intern("data"))->data[2]);
    Asequal("data4", frt_doc_get_field(doc, rb_intern("data"))->data[3]);
    Afequal(1.0, doc->boost);
   frt_doc_destroy(doc);

    doc = frt_doc_new();
    for (i = 0; i < 1000; i++) {
        char buf[100];
        char *bufc;
        sprintf(buf, "<<%d>>", i);
        bufc = frt_estrdup(buf);
        df = frt_df_add_data(frt_df_new(rb_intern(bufc)), bufc, enc);
        df->destroy_data = true;
        frt_doc_add_field(doc, df);
        Aiequal(i + 1, doc->size);
    }

    for (i = 0; i < 1000; i++) {
        char buf[100];
        sprintf(buf, "<<%d>>", i);
        Aiequal(1, frt_doc_get_field(doc, rb_intern(buf))->size);
        Aiequal(strlen(buf), frt_doc_get_field(doc, rb_intern(buf))->lengths[0]);
        Asequal(buf, frt_doc_get_field(doc, rb_intern(buf))->data[0]);
    }
   frt_doc_destroy(doc);
}

void test_double_field_exception(TestCase *tc, void *data)
{
    volatile bool exception_thrown = false;
    FrtDocument *doc;
    FrtDocField *volatile df = NULL;
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    doc = frt_doc_new();
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("title")), (char *)"title", enc));

    FRT_TRY
        df = frt_df_add_data_len(frt_df_new(rb_intern("title")), (char *)"title", 5, enc);
        frt_doc_add_field(doc, df);
    case FRT_EXCEPTION:
        exception_thrown = true;
        FRT_HANDLED();
        break;
    case FRT_FINALLY:
        frt_df_destroy(df);
        break;
    FRT_ENDTRY

    Atrue(exception_thrown);

   frt_doc_destroy(doc);
}

TestSuite *ts_document(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_df_standard, NULL);
    tst_run_test(suite, test_df_multi_fields, NULL);
    tst_run_test(suite, test_doc, NULL);
    tst_run_test(suite, test_double_field_exception, NULL);

    return suite;
}

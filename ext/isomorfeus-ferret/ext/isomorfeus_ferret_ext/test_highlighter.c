#include "frt_search.h"
#include "frt_array.h"
#include "testhelper.h"
#include "test.h"

#undef close

#define ARRAY_SIZE 100

static void test_match_vector(TestCase *tc, void *data)
{
    const int match_test_count = 100;
    int i;
    FrtMatchVector *mv = frt_matchv_new();
    (void)data; /* suppress unused argument warning */
    srand(5);

    frt_matchv_add(mv, 0, 10);
    frt_matchv_add(mv, 200, 220);
    frt_matchv_add(mv, 50, 56);
    frt_matchv_add(mv, 50, 55);
    frt_matchv_add(mv, 57, 63);

    Aiequal(0  , mv->matches[0].start);
    Aiequal(10 , mv->matches[0].end);
    Aiequal(200, mv->matches[1].start);
    Aiequal(220, mv->matches[1].end);
    Aiequal(50 , mv->matches[2].start);
    Aiequal(56 , mv->matches[2].end);
    Aiequal(50 , mv->matches[3].start);
    Aiequal(55 , mv->matches[3].end);
    Aiequal(57 , mv->matches[4].start);
    Aiequal(63 , mv->matches[4].end);

    frt_matchv_sort(mv);

    Aiequal(0  , mv->matches[0].start);
    Aiequal(10 , mv->matches[0].end);
    Aiequal(50 , mv->matches[1].start);
    Aiequal(56 , mv->matches[1].end);
    Aiequal(50 , mv->matches[2].start);
    Aiequal(55 , mv->matches[2].end);
    Aiequal(57 , mv->matches[3].start);
    Aiequal(63 , mv->matches[3].end);
    Aiequal(200, mv->matches[4].start);
    Aiequal(220, mv->matches[4].end);

    frt_matchv_compact(mv);

    Aiequal(3  , mv->size);
    Aiequal(0  , mv->matches[0].start);
    Aiequal(10 , mv->matches[0].end);
    Aiequal(50 , mv->matches[1].start);
    Aiequal(63 , mv->matches[1].end);
    Aiequal(200, mv->matches[2].start);
    Aiequal(220, mv->matches[2].end);

    frt_matchv_destroy(mv);

    mv = frt_matchv_new();

    for (i = 0; i < match_test_count; i++)
    {
        int start = rand() % 10000000;
        int end = start + rand() % 100;
        frt_matchv_add(mv, start, end);
    }

    frt_matchv_sort(mv);

    for (i = 1; i < match_test_count; i++)
    {
        Assert(mv->matches[i].start > mv->matches[i-1].start
               || mv->matches[i].end > mv->matches[i-1].end,
               "Offset(%d:%d) < FrtOffset(%d:%d)",
               mv->matches[i].start, mv->matches[i].end,
               mv->matches[i-1].start, mv->matches[i-1].end);
    }
    frt_matchv_destroy(mv);
}

static void make_index(FrtStore *store)
{
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    frt_index_create(store, fis);
    frt_fis_deref(fis);
}

static void add_string_docs(FrtStore *store, const char *string[])
{
    FrtIndexWriter *iw = frt_iw_open(NULL, store, frt_whitespace_analyzer_new(true), NULL);
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    while (*string) {
        FrtDocument *doc = frt_doc_new();
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("field")), (char *)*string, enc));
        frt_iw_add_doc(iw, doc);
       frt_doc_destroy(doc);
        string++;
    }
    frt_iw_close(iw);
}

#define Chk_sea_mv(query, doc_num, expected) check_searcher_match_vector(tc, store, query, doc_num, expected)
static void check_searcher_match_vector(TestCase *tc, FrtStore *store, FrtQuery *query, int doc_num, const char *expected)
{
    FrtIndexReader *ir = frt_ir_open(NULL, store);
    FrtSearcher *sea = frt_isea_new(ir);
    FrtMatchVector *mv = frt_searcher_get_match_vector(sea, query, doc_num, rb_intern("field"));
    static int offset_array[ARRAY_SIZE];
    int matchv_size = s2l(expected, offset_array) / 2;
    int i;

    Aiequal(matchv_size, mv->size);
    frt_matchv_sort(mv);
    for (i = 0; i < matchv_size; i++) {
        Aiequal(offset_array[i<<1], mv->matches[i].start);
        Aiequal(offset_array[(i<<1)+1], mv->matches[i].end);
    }
    frt_matchv_destroy(mv);
    frt_searcher_close(sea);
}

#define Chk_mv(query, doc_num, expected) check_match_vector(tc, store, query, doc_num, expected)
static void check_match_vector(TestCase *tc, FrtStore *store, FrtQuery *query, int doc_num, const char *expected)
{
    FrtIndexReader *ir = frt_ir_open(NULL, store);
    FrtMatchVector *mv = frt_matchv_new();
    FrtTermVector *term_vector = ir->term_vector(ir, doc_num, rb_intern("field"));
    static int offset_array[ARRAY_SIZE];
    int matchv_size = s2l(expected, offset_array) / 2;
    int i;

    mv = query->get_matchv_i(query, mv, term_vector);
    Aiequal(matchv_size, mv->size);
    frt_matchv_sort(mv);
    for (i = 0; i < matchv_size; i++) {
        Aiequal(offset_array[i<<1], mv->matches[i].start);
        Aiequal(offset_array[(i<<1)+1], mv->matches[i].end);
    }
    frt_matchv_destroy(mv);
    frt_ir_close(ir);
    frt_tv_destroy(term_vector);
    check_searcher_match_vector(tc, store, query, doc_num, expected);
}

static void test_term_query(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q;
    const char *docs[] = {
        "the phrase has the word rabbit once",
        "rabbit one rabbit two rabbit three rabbit four",
        "Term doesn't appear in this sentence",
        NULL
    };
    make_index(store);
    add_string_docs(store, docs);
    q = frt_tq_new(rb_intern("field"), "rabbit");
    Chk_mv(q, 0, "5:5");
    Chk_mv(q, 1, "0:0 2:2 4:4 6:6");
    Chk_mv(q, 2, "");
    frt_q_deref(q);
    q = frt_tq_new(rb_intern("diff_field"), "rabbit");
    Chk_mv(q, 0, "");
    Chk_mv(q, 1, "");
    frt_q_deref(q);
}

static void test_phrase_query(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q;
    const char *docs[] = {
        "the magic phrase of the day is one two three not three "
            "two one one too three",
        "one two three and again one two three and maybe one more for good "
            "luck one two three",
        "phrase doesn't appear in this sentence",
        "multi phrase quick brown fox fast white wolf agile red fox quick "
            "pink hound",
        "multi phrase with slop brown quick fox the agile beautful and "
            "cunning white wolf",
        NULL
    };
    make_index(store);
    add_string_docs(store, docs);
    q = frt_phq_new(rb_intern("field"));
    frt_phq_add_term(q, "one", 1);
    frt_phq_add_term(q, "two", 1);
    frt_phq_add_term(q, "three", 1);
    Chk_mv(q, 0, "7:9");
    Chk_mv(q, 1, "0:2 5:7 15:17");
    Chk_mv(q, 2, "");
    ((FrtPhraseQuery *)q)->slop = 3;
    Chk_mv(q, 0, "7:9 12:16");
    ((FrtPhraseQuery *)q)->slop = 4;
    Chk_mv(q, 0, "7:9 11:13 12:16");
    frt_q_deref(q);

    /* test that it only works for the correct field */
    q = frt_phq_new(rb_intern("wrong_field"));
    frt_phq_add_term(q, "one", 1);
    frt_phq_add_term(q, "two", 1);
    frt_phq_add_term(q, "three", 1);
    Chk_mv(q, 0, "");
    Chk_mv(q, 1, "");
    Chk_mv(q, 2, "");
    ((FrtPhraseQuery *)q)->slop = 4;
    Chk_mv(q, 0, "");
    frt_q_deref(q);

    q = frt_phq_new(rb_intern("field"));
    frt_phq_add_term(q, "quick", 1);
    frt_phq_append_multi_term(q, "fast");
    frt_phq_append_multi_term(q, "agile");
    frt_phq_add_term(q, "brown", 1);
    frt_phq_append_multi_term(q, "pink");
    frt_phq_append_multi_term(q, "red");
    frt_phq_append_multi_term(q, "white");
    frt_phq_add_term(q, "fox", 1);
    frt_phq_append_multi_term(q, "wolf");
    frt_phq_append_multi_term(q, "hound");
    Chk_mv(q, 3, "2:4 5:7 8:10 11:13");
    Chk_mv(q, 4, "");
    ((FrtPhraseQuery *)q)->slop = 2;
    Chk_mv(q, 4, "4:6");
    ((FrtPhraseQuery *)q)->slop = 5;
    Chk_mv(q, 4, "4:6 8:13");
    frt_q_deref(q);
}

static void test_boolean_query(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q, *phq;
    const char *docs[] = {
        "one and some words and two and three and some more words one two",
        NULL
    };
    make_index(store);
    add_string_docs(store, docs);
    q = frt_bq_new(false);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "one"), FRT_BC_SHOULD);
    Chk_mv(q, 0, "0:0 12:12");
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "two"), FRT_BC_MUST);
    Chk_mv(q, 0, "0:0 5:5 12:12 13:13");
    phq = frt_phq_new(rb_intern("field"));
    frt_phq_add_term(phq, "one", 1);
    frt_phq_add_term(phq, "two", 1);
    Chk_mv(phq, 0, "12:13");
    frt_bq_add_query_nr(q, phq, FRT_BC_SHOULD);
    Chk_mv(q, 0, "0:0 5:5 12:13 12:12 13:13");
    frt_q_deref(q);
}

static void test_multi_term_query(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q;
    const char *docs[] = {
        "one and some words and two and three and some more words one two",
        NULL
    };
    make_index(store);
    add_string_docs(store, docs);
    q = frt_multi_tq_new(rb_intern("field"));
    frt_multi_tq_add_term(q, "one");
    Chk_mv(q, 0, "0:0 12:12");
    frt_multi_tq_add_term(q, "two");
    Chk_mv(q, 0, "0:0 5:5 12:12 13:13");
    frt_multi_tq_add_term(q, "and");
    Chk_mv(q, 0, "0:0 1:1 4:4 5:5 6:6 8:8 12:12 13:13");
    frt_q_deref(q);
}

static void test_span_queries(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q, *oq;
    const char *docs[] = {
        "one and some words an two and three words and some more words one two",
        "worda one wordb one worda one 2 wordb one 2 worda one two three wordb",
        NULL
    };
    make_index(store);
    add_string_docs(store, docs);
    q = frt_spantq_new(rb_intern("wrong_field"), "words");
    Chk_mv(q, 0, "");
    frt_q_deref(q);

    q = frt_spantq_new(rb_intern("field"), "words");
    Chk_mv(q, 0, "3:3 8:8 12:12");
    q = frt_spanfq_new_nr(q, 4);
    Chk_mv(q, 0, "3:3");
    ((FrtSpanFirstQuery *)q)->end = 8;
    Chk_mv(q, 0, "3:3");
    ((FrtSpanFirstQuery *)q)->end = 9;
    Chk_mv(q, 0, "3:3 8:8");
    ((FrtSpanFirstQuery *)q)->end = 12;
    Chk_mv(q, 0, "3:3 8:8");
    ((FrtSpanFirstQuery *)q)->end = 13;
    Chk_mv(q, 0, "3:3 8:8 12:12");

    oq = frt_spanoq_new();
    frt_spanoq_add_clause_nr(oq, q);
    Chk_mv(oq, 0, "3:3 8:8 12:12");
    frt_spanoq_add_clause_nr(oq, frt_spantq_new(rb_intern("field"), "one"));
    Chk_mv(oq, 0, "0:0 3:3 8:8 12:12 13:13");
    frt_spanoq_add_clause_nr(oq, frt_spantq_new(rb_intern("field"), "two"));
    Chk_mv(oq, 0, "0:0 3:3 5:5 8:8 12:12 13:13 14:14");
    frt_q_deref(oq);

    q = frt_spannq_new(1, true);
    frt_spannq_add_clause_nr(q, frt_spantq_new(rb_intern("field"), "worda"));
    Chk_mv(q, 0, "");
    Chk_mv(q, 1, "0:0 4:4 10:10");
    frt_spannq_add_clause_nr(q, frt_spantq_new(rb_intern("field"), "wordb"));
    Chk_mv(q, 1, "0:0 2:2");
    ((FrtSpanNearQuery *)q)->in_order = false;
    Chk_mv(q, 1, "0:0 2:2 4:4");
    ((FrtSpanNearQuery *)q)->slop = 2;
    Chk_mv(q, 1, "0:0 2:2 4:4 7:7 10:10");
    ((FrtSpanNearQuery *)q)->slop = 3;
    Chk_mv(q, 1, "0:0 2:2 4:4 7:7 10:10 14:14");

    q = frt_spanxq_new_nr(q, frt_spantq_new(rb_intern("field"), "2"));
    Chk_mv(q, 1, "0:0 2:2 4:4 10:10 14:14");
    frt_q_deref(q);
}

static void test_searcher_get_match_vector(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q;
    const char *docs[] = {
        "funnyword funniward funyword funywod funnywerd funniword finnywood",
        NULL
    };
    make_index(store);
    add_string_docs(store, docs);
    q = frt_fuzq_new_conf(rb_intern("field"), "funnyword", 0.9f, 0, 512);
    Chk_sea_mv(q, 0, "0:0");
    frt_q_deref(q);

    q = frt_fuzq_new_conf(rb_intern("field"), "funnyword", 0.8f, 0, 512);
    Chk_sea_mv(q, 0, "0:0 2:2 4:4 5:5");
    frt_q_deref(q);

    q = frt_fuzq_new_conf(rb_intern("field"), "funnyword", 0.5f, 0, 512);
    Chk_sea_mv(q, 0, "0:0 1:1 2:2 3:3 4:4 5:5 6:6");
    frt_q_deref(q);
}

static void test_searcher_highlight(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtQuery *q, *phq;
    FrtIndexWriter *iw;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    char **highlights;
    const char *docs[] = {
        "the words we are searching for are one and two also sometimes "
            "looking for them as a phrase like this; one two lets see "
            "how it goes",
        NULL
    };
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    make_index(store);
    add_string_docs(store, docs);

    iw = frt_iw_open(NULL, store, frt_letter_analyzer_new(true), NULL);
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("field")), (char *)"That's how it goes now.", enc));
    frt_iw_add_doc(iw, doc);
    frt_doc_destroy(doc);
    frt_iw_close(iw);

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    q = frt_tq_new(rb_intern("field"), "one");
    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 1,
                                    "<b>", "</b>", "...");
    Aiequal(1, frt_ary_size(highlights));
    Asequal("...are <b>one</b>...", highlights[0]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 2,
                                    "<b>", "</b>", "...");
    Aiequal(2, frt_ary_size(highlights));
    Asequal("...are <b>one</b>...", highlights[0]);
    Asequal("...this; <b>one</b>...", highlights[1]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 3,
                                    "<b>", "</b>", "...");
    Aiequal(3, frt_ary_size(highlights));
    Asequal("the words...", highlights[0]);
    Asequal("...are <b>one</b>...", highlights[1]);
    Asequal("...this; <b>one</b>...", highlights[2]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 4,
                                    "<b>", "</b>", "...");
    Aiequal(3, frt_ary_size(highlights));
    Asequal("the words we are...", highlights[0]);
    Asequal("...are <b>one</b>...", highlights[1]);
    Asequal("...this; <b>one</b>...", highlights[2]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 5,
                                    "<b>", "</b>", "...");
    Aiequal(2, frt_ary_size(highlights));
    Asequal("the words we are searching for are <b>one</b>...", highlights[0]);
    Asequal("...this; <b>one</b>...", highlights[1]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 20,
                                    "<b>", "</b>", "...");
    Aiequal(1, frt_ary_size(highlights));
    Asequal("the words we are searching for are <b>one</b> and two also "
            "sometimes looking for them as a phrase like this; <b>one</b> "
            "two lets see how it goes", highlights[0]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 1000, 1,
                                    "<b>", "</b>", "...");
    Aiequal(1, frt_ary_size(highlights));
    Asequal("the words we are searching for are <b>one</b> and two also "
            "sometimes looking for them as a phrase like this; <b>one</b> "
            "two lets see how it goes", highlights[0]);
    frt_ary_destroy(highlights, &free);

    frt_q_deref(q);

    q = frt_bq_new(false);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "one"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "two"), FRT_BC_SHOULD);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 15, 2,
                                    "<b>", "</b>", "...");
    Aiequal(2, frt_ary_size(highlights));
    Asequal("...<b>one</b> and <b>two</b>...", highlights[0]);
    Asequal("...this; <b>one</b> <b>two</b>...", highlights[1]);
    frt_ary_destroy(highlights, &free);

    phq = frt_phq_new(rb_intern("field"));
    frt_phq_add_term(phq, "one", 1);
    frt_phq_add_term(phq, "two", 1);

    frt_bq_add_query_nr(q, phq, FRT_BC_SHOULD);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 15, 2,
                                    "<b>", "</b>", "...");
    Aiequal(2, frt_ary_size(highlights));
    Asequal("...<b>one</b> and <b>two</b>...", highlights[0]);
    Asequal("...this; <b>one two</b>...", highlights[1]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 15, 1,
                                    "<b>", "</b>", "...");
    Aiequal(1, frt_ary_size(highlights));
    /* should have a higher priority since it the merger of three matches */
    Asequal("...this; <b>one two</b>...", highlights[0]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("not_a_field"), 15, 1,
                                    "<b>", "</b>", "...");
    Apnull(highlights);

    frt_q_deref(q);

    q = frt_tq_new(rb_intern("wrong_field"), "one");
    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("not_a_field"), 15, 1,
                                    "<b>", "</b>", "...");
    Apnull(highlights);

    frt_q_deref(q);

    q = frt_bq_new(false);
    phq = frt_phq_new(rb_intern("field"));
    frt_phq_add_term(phq, "the", 1);
    frt_phq_add_term(phq, "words", 1);
    frt_bq_add_query_nr(q, phq, FRT_BC_SHOULD);
    phq = frt_phq_new(rb_intern("field"));
    frt_phq_add_term(phq, "for", 1);
    frt_phq_add_term(phq, "are", 1);
    frt_phq_add_term(phq, "one", 1);
    frt_phq_add_term(phq, "and", 1);
    frt_phq_add_term(phq, "two", 1);
    frt_bq_add_query_nr(q, phq, FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "words"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "one"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "two"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("field"), "UnKnOwNfIeLd"), FRT_BC_SHOULD);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 1,
                                    "<b>", "</b>", "...");
    Aiequal(1, frt_ary_size(highlights));
    Asequal("<b>the words</b>...", highlights[0]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 10, 2,
                                    "<b>", "</b>", "...");
    Aiequal(2, frt_ary_size(highlights));
    Asequal("<b>the words</b>...", highlights[0]);
    Asequal("...<b>one</b> <b>two</b>...", highlights[1]);
    frt_ary_destroy(highlights, &free);

    frt_q_deref(q);

    q = frt_tq_new(rb_intern("field"), "goes");
    highlights = frt_searcher_highlight(sea, q, 0, rb_intern("field"), 13, 2,
                                    "<b>", "</b>", "...");
    Aiequal(2, frt_ary_size(highlights));
    Asequal("the words we...", highlights[0]);
    Asequal("...how it <b>goes</b>", highlights[1]);
    frt_ary_destroy(highlights, &free);

    highlights = frt_searcher_highlight(sea, q, 1, rb_intern("field"), 16, 1,
                                    "<b>", "</b>", "...");
    Aiequal(1, frt_ary_size(highlights));
    Asequal("...how it <b>goes</b> now.", highlights[0]);
    frt_ary_destroy(highlights, &free);
    frt_q_deref(q);

    frt_searcher_close(sea);
}

TestSuite *ts_highlighter(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_match_vector, NULL);
    tst_run_test(suite, test_term_query, store);
    tst_run_test(suite, test_phrase_query, store);
    tst_run_test(suite, test_boolean_query, store);
    tst_run_test(suite, test_multi_term_query, store);
    tst_run_test(suite, test_span_queries, store);

    tst_run_test(suite, test_searcher_get_match_vector, store);
    tst_run_test(suite, test_searcher_highlight, store);

    frt_store_deref(store);
    return suite;
}

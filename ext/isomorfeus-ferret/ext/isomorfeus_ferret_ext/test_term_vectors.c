#include "frt_index.h"
#include "test.h"

#define NUM_TERMS 100
#define TERM_LEN 10


static void test_posting(TestCase *tc, void *data)
{
    FrtMemoryPool *mp = (FrtMemoryPool *)data;
    FrtPostingList *pl;
    FrtPosting *p = frt_p_new(mp, 0, 10);
    Aiequal(0, p->doc_num);
    Aiequal(1, p->freq);
    Aiequal(10, p->first_occ->pos);
    Apnull(p->first_occ->next);

    pl = frt_pl_new(mp, "seven", 5, p);
    Aiequal(5, pl->term_len);
    Asequal("seven", pl->term);
    Apequal(p->first_occ, pl->last_occ);

    frt_pl_add_occ(mp, pl, 50);
    Apequal(pl->last_occ, p->first_occ->next);
    Aiequal(2, p->freq);
    Aiequal(50,  pl->last_occ->pos);
    Apnull(pl->last_occ->next);

    frt_pl_add_occ(mp, pl, 345);
    Apequal(pl->last_occ, p->first_occ->next->next);
    Aiequal(3, p->freq);
    Aiequal(345, pl->last_occ->pos);
    Apnull(pl->last_occ->next);
}

static FrtFieldInfos *create_tv_fis(void) {
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_NO);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv2"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv_with_positions"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_POSITIONS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv_with_offsets"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_OFFSETS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv_with_positions_offsets"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS));
    return fis;
}

static char **create_tv_terms(FrtMemoryPool *mp)
{
    int i;
    char term_buf[10];
    char **terms = FRT_MP_ALLOC_N(mp, char *, NUM_TERMS);
    for (i = 0; i < NUM_TERMS; i++) {
        sprintf(term_buf, "%09d", i);
        terms[i] = frt_mp_strdup(mp, term_buf);
    }
    return terms;
}

static FrtPostingList **create_tv_plists(FrtMemoryPool *mp, char **terms)
{
    int i, j;
    FrtPostingList **plists, *pl;
    plists = FRT_MP_ALLOC_N(mp, FrtPostingList *, NUM_TERMS);
    for (i = 0; i < NUM_TERMS; i++) {
        pl = plists[i] =
            frt_pl_new(mp, terms[i], 9, frt_p_new(mp, 0, 0));
        for (j = 1; j <= i; j++) {
            frt_pl_add_occ(mp, pl, j);
        }
    }
    return plists;
}

static FrtOffset *create_tv_offsets(FrtMemoryPool *mp)
{
    int i;
    FrtOffset *offsets = FRT_MP_ALLOC_N(mp, FrtOffset, NUM_TERMS);
    for (i = 0; i < NUM_TERMS; i++) {
        offsets[i].start = 5 * i;
        offsets[i].end = 5 * i + 4;
    }
    return offsets;
}

static void test_tv_single_doc(TestCase *tc, void *data)
{
    int i, j;
    FrtStore *store = frt_open_ram_store(NULL);
    FrtMemoryPool *mp = (FrtMemoryPool *)data;
    FrtFieldsReader *fr;
    FrtFieldsWriter *fw;
    FrtTermVector *tv;
    FrtHash *tvs;
    FrtFieldInfos *fis = create_tv_fis();
    char **terms = create_tv_terms(mp);
    FrtPostingList **plists = create_tv_plists(mp, terms);
    FrtOffset *offsets = create_tv_offsets(mp);
    FrtDocument *doc = frt_doc_new();

    fw = frt_fw_open(store, "_0", fis);
    frt_fw_close(fw);

    fr = frt_fr_open(store, "_0", fis);
    Aiequal(0, fr->size);
    frt_fr_close(fr);


    fw = frt_fw_open(store, "_0", fis);
    frt_fw_add_doc(fw, doc);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_positions"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_offsets"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_write_tv_index(fw);
    frt_fw_close(fw);
   frt_doc_destroy(doc);

    fr = frt_fr_open(store, "_0", fis);
    Aiequal(1, fr->size);

    /* test individual field's term vectors */
    tv = frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv"))->number);
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv"))->number, tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(0, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            Apnull(tv->terms[i].positions);
        }
        Apnull(tv->offsets);
    }
    if (tv) frt_tv_destroy(tv);

    tv = frt_fr_get_field_tv(fr, 0,
                         frt_fis_get_field(fis, rb_intern("tv_with_positions"))->number);
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv_with_positions"))->number,
                tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(0, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            for (j = 0; j <= i; j++) {
                Aiequal(j, tv->terms[i].positions[j]);
            }
        }
        Apnull(tv->offsets);
    }
    if (tv) frt_tv_destroy(tv);

    tv = frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv_with_offsets"))->number);
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv_with_offsets"))->number, tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(NUM_TERMS, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            Apnull(tv->terms[i].positions);
        }
        for (i = 0; i < NUM_TERMS; i++) {
            Aiequal(i * 5, tv->offsets[i].start);
            Aiequal(i * 5 + 4, tv->offsets[i].end);
        }
    }
    if (tv) frt_tv_destroy(tv);

    tv = frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number);
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number, tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(NUM_TERMS, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            for (j = 1; j <= i; j++) {
                Aiequal(j, tv->terms[i].positions[j]);
            }
        }
        for (i = 0; i < NUM_TERMS; i++) {
            Aiequal(i * 5, tv->offsets[i].start);
            Aiequal(i * 5 + 4, tv->offsets[i].end);
        }
    }
    if (tv) frt_tv_destroy(tv);

    tv = frt_fr_get_field_tv(fr, 0, frt_fis_get_or_add_field(fis, rb_intern("tv2"))->number);
    Apnull(tv);
    tv = frt_fr_get_field_tv(fr, 0, frt_fis_get_or_add_field(fis, rb_intern("new"))->number);
    Apnull(tv);

    /* test document's term vectors */
    tvs = frt_fr_get_tv(fr, 0);
    Aiequal(4, tvs->size);
    tv = (FrtTermVector*)frt_h_get(tvs, (void *)rb_intern("tv2"));
    Apnull(tv);
    tv = (FrtTermVector*)frt_h_get(tvs, (void *)rb_intern("other"));
    Apnull(tv);

    tv = (FrtTermVector*)frt_h_get(tvs, (void *)rb_intern("tv_with_positions_offsets"));
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number, tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(NUM_TERMS, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            for (j = 1; j <= i; j++) {
                Aiequal(j, tv->terms[i].positions[j]);
            }
        }
        for (i = 0; i < NUM_TERMS; i++) {
            Aiequal(i * 5, tv->offsets[i].start);
            Aiequal(i * 5 + 4, tv->offsets[i].end);
        }
    }
    frt_h_destroy(tvs);

    frt_fr_close(fr);
    frt_fis_deref(fis);
    frt_store_deref(store);
}

static void test_tv_multi_doc(TestCase *tc, void *data)
{
    int i, j;
    FrtStore *store = frt_open_ram_store(NULL);
    FrtMemoryPool *mp = (FrtMemoryPool *)data;
    FrtFieldsReader *fr;
    FrtFieldsWriter *fw;
    FrtTermVector *tv;
    FrtHash *tvs;
    FrtFieldInfos *fis = create_tv_fis();
    char **terms = create_tv_terms(mp);
    FrtPostingList **plists = create_tv_plists(mp, terms);
    FrtOffset *offsets = create_tv_offsets(mp);
    FrtDocument *doc = frt_doc_new();

    fw = frt_fw_open(store, "_0", fis);
    frt_fw_add_doc(fw, doc);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_write_tv_index(fw); frt_fw_add_doc(fw, doc);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_positions"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_write_tv_index(fw); frt_fw_add_doc(fw, doc);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_offsets"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_write_tv_index(fw); frt_fw_add_doc(fw, doc);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_write_tv_index(fw); frt_fw_add_doc(fw, doc);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_positions"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_offsets"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);
    frt_fw_add_postings(fw, frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number, plists, NUM_TERMS, offsets, NUM_TERMS);

    frt_fw_write_tv_index(fw);
    frt_fw_close(fw);
    frt_doc_destroy(doc);

    fr = frt_fr_open(store, "_0", fis);
    Aiequal(5, fr->size);

    tv = frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv"))->number);
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv"))->number, tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(0, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            Apnull(tv->terms[i].positions);
        }
        Apnull(tv->offsets);
    }
    Apnull(frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv_with_positions"))->number));
    Apnull(frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv_with_offsets"))->number));
    Apnull(frt_fr_get_field_tv(fr, 0, frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number));
    frt_tv_destroy(tv);

    tv = frt_fr_get_field_tv(fr, 3, frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number);
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number,
                tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(NUM_TERMS, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            for (j = 1; j <= i; j++) {
                Aiequal(j, tv->terms[i].positions[j]);
            }
        }
        for (i = 0; i < NUM_TERMS; i++) {
            Aiequal(i * 5, tv->offsets[i].start);
            Aiequal(i * 5 + 4, tv->offsets[i].end);
        }
    }
    frt_tv_destroy(tv);

    /* test document's term vector */
    tvs = frt_fr_get_tv(fr, 0);
    Aiequal(1, tvs->size);
    frt_h_destroy(tvs);

    tvs = frt_fr_get_tv(fr, 4);
    Aiequal(4, tvs->size);
    tv = (FrtTermVector*)frt_h_get(tvs, (void *)rb_intern("tv2"));
    Apnull(tv);
    tv = (FrtTermVector*)frt_h_get(tvs, (void *)rb_intern("other"));
    Apnull(tv);

    tv = (FrtTermVector*)frt_h_get(tvs, (void *)rb_intern("tv_with_positions_offsets"));
    if (Apnotnull(tv)) {
        Aiequal(frt_fis_get_field(fis, rb_intern("tv_with_positions_offsets"))->number, tv->field_num);
        Aiequal(NUM_TERMS, tv->term_cnt);
        Aiequal(NUM_TERMS, tv->offset_cnt);
        for (i = 0; i < NUM_TERMS; i++) {
            Asequal(terms[i], tv->terms[i].text);
            Aiequal(i + 1, tv->terms[i].freq);
            for (j = 1; j <= i; j++) {
                Aiequal(j, tv->terms[i].positions[j]);
            }
        }
        for (i = 0; i < NUM_TERMS; i++) {
            Aiequal(i * 5, tv->offsets[i].start);
            Aiequal(i * 5 + 4, tv->offsets[i].end);
        }
    }

    for (i = 0; i < NUM_TERMS; i++) {
        char buf[100];
        int len = sprintf(buf, "%s", tv->terms[i].text);
        assert(strlen(tv->terms[i].text) < 100);
        Aiequal(i, frt_tv_get_term_index(tv, buf));

        /* make the word lexically less than it was but greater than any other
         * word in the index that originally came before it. */
        buf[len - 1]--;
        buf[len    ] = '~';
        buf[len + 1] = '\0';
        Aiequal(-1, frt_tv_get_term_index(tv, buf));
        Aiequal(i, frt_tv_scan_to_term_index(tv, buf));

        /* make the word lexically more than it was by less than any other
         * word in the index that originally came after it. */
        buf[len - 1]++;
        buf[len    ] = '.';
        Aiequal(-1, frt_tv_get_term_index(tv, buf));
        Aiequal(i + 1, frt_tv_scan_to_term_index(tv, buf));
    }
    Aiequal(-1, frt_tv_get_term_index(tv, "UnKnOwN TeRm"));
    frt_h_destroy(tvs);
    frt_fr_close(fr);
    frt_fis_deref(fis);
    frt_store_deref(store);
}


TestSuite *ts_term_vectors(TestSuite *suite)
{
    FrtMemoryPool *mp = frt_mp_new();

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_posting, mp);
    frt_mp_reset(mp);
    tst_run_test(suite, test_tv_single_doc, mp);
    frt_mp_reset(mp);
    tst_run_test(suite, test_tv_multi_doc, mp);
    frt_mp_destroy(mp);
    return suite;
}

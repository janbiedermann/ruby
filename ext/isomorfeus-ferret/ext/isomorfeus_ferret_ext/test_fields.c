#include "frt_index.h"
#include "test.h"

#define do_field_prop_test(tc, fi, name, boost, is_stored,\
                           is_compressed, is_indexed, is_tokenized, omit_norms,\
                           store_term_vector, store_positions, store_offsets)\
        field_prop_test(tc, __LINE__, fi, name, boost, is_stored,\
                        is_compressed, is_indexed, is_tokenized, omit_norms,\
                        store_term_vector, store_positions, store_offsets)
#define T 1
#define F 0

void field_prop_test(TestCase *tc,
                     int line_num,
                     FrtFieldInfo *fi,
                     FrtSymbol name,
                     float boost,
                     bool is_stored,
                     bool is_compressed,
                     bool is_indexed,
                     bool is_tokenized,
                     bool omit_norms,
                     bool store_term_vector,
                     bool store_positions,
                     bool store_offsets) {
    tst_ptr_equal(line_num, tc, (void *)name, (void *)fi->name);
    tst_flt_equal(line_num, tc, boost, fi->boost);
    tst_int_equal(line_num, tc, is_stored,          fi_is_stored(fi));
    tst_int_equal(line_num, tc, is_compressed,      fi_is_compressed(fi));
    tst_int_equal(line_num, tc, is_indexed,         fi_is_indexed(fi));
    tst_int_equal(line_num, tc, is_tokenized,       fi_is_tokenized(fi));
    tst_int_equal(line_num, tc, omit_norms,         fi_omit_norms(fi));
    tst_int_equal(line_num, tc, store_term_vector,  fi_store_term_vector(fi));
    tst_int_equal(line_num, tc, store_positions,    fi_store_positions(fi));
    tst_int_equal(line_num, tc, store_offsets,      fi_store_offsets(fi));
}

/****************************************************************************
 *
 * FrtFieldInfo
 *
 ****************************************************************************/

static void test_fi_new(TestCase *tc, void *data)
{
    FrtFieldInfo *fi;
    (void)data; /* suppress unused argument warning */
    fi = frt_fi_new(rb_intern("name"), FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
    do_field_prop_test(tc, fi, rb_intern("name"), 1.0, F, F, F, F, F, F, F, F);
    frt_fi_deref(fi);
    fi = frt_fi_new(rb_intern("name"), FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    do_field_prop_test(tc, fi, rb_intern("name"), 1.0, T, F, T, T, F, T, F, F);
    frt_fi_deref(fi);
    fi = frt_fi_new(rb_intern("name"), FRT_STORE_COMPRESS, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_POSITIONS);
    do_field_prop_test(tc, fi, rb_intern("name"), 1.0, T, T, T, F, F, T, T, F);
    frt_fi_deref(fi);
    fi = frt_fi_new(rb_intern("name"), FRT_STORE_NO, FRT_INDEX_YES_OMIT_NORMS, FRT_TERM_VECTOR_WITH_OFFSETS);
    do_field_prop_test(tc, fi, rb_intern("name"), 1.0, F, F, T, T, T, T, F, T);
    frt_fi_deref(fi);
    fi = frt_fi_new(rb_intern("name"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED_OMIT_NORMS, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    fi->boost = 1000.0;
    do_field_prop_test(tc, fi, rb_intern("name"), 1000.0, F, F, T, F, T, T, T, T);
    frt_fi_deref(fi);
}

/****************************************************************************
 *
 * FrtFieldInfos
 *
 ****************************************************************************/

static void test_fis_basic(TestCase *tc, void *data)
{
    FrtFieldInfos *fis;
    FrtFieldInfo *fi;
    volatile bool arg_error = false;
    (void)data; /* suppress unused argument warning */

    fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("FFFFFFFF"), FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("TFTTFTFF"), FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("TTTFFTTF"), FRT_STORE_COMPRESS, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_POSITIONS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("FFTTTTFT"), FRT_STORE_NO, FRT_INDEX_YES_OMIT_NORMS, FRT_TERM_VECTOR_WITH_OFFSETS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("FFTFTTTT"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED_OMIT_NORMS, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS));

    fi = frt_fi_new(rb_intern("FFTFTTTT"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED_OMIT_NORMS, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    FRT_TRY
        Apnull(frt_fis_add_field(fis, fi));
    case FRT_ARG_ERROR:
        arg_error = true;
        FRT_HANDLED();
    FRT_XENDTRY
    Assert(arg_error, "exception should have been thrown");

    frt_fi_deref(fi);

    Apequal(frt_fis_get_field(fis, rb_intern("FFFFFFFF")), fis->fields[0]);
    Apequal(frt_fis_get_field(fis, rb_intern("TFTTFTFF")), fis->fields[1]);
    Apequal(frt_fis_get_field(fis, rb_intern("TTTFFTTF")), fis->fields[2]);
    Apequal(frt_fis_get_field(fis, rb_intern("FFTTTTFT")), fis->fields[3]);
    Apequal(frt_fis_get_field(fis, rb_intern("FFTFTTTT")), fis->fields[4]);

    Aiequal(0, frt_fis_get_field(fis, rb_intern("FFFFFFFF"))->number);
    Aiequal(1, frt_fis_get_field(fis, rb_intern("TFTTFTFF"))->number);
    Aiequal(2, frt_fis_get_field(fis, rb_intern("TTTFFTTF"))->number);
    Aiequal(3, frt_fis_get_field(fis, rb_intern("FFTTTTFT"))->number);
    Aiequal(4, frt_fis_get_field(fis, rb_intern("FFTFTTTT"))->number);

    Asequal("FFFFFFFF", rb_id2name(fis->fields[0]->name));
    Asequal("TFTTFTFF", rb_id2name(fis->fields[1]->name));
    Asequal("TTTFFTTF", rb_id2name(fis->fields[2]->name));
    Asequal("FFTTTTFT", rb_id2name(fis->fields[3]->name));
    Asequal("FFTFTTTT", rb_id2name(fis->fields[4]->name));

    fis->fields[1]->boost = 2.0;
    fis->fields[2]->boost = 3.0;
    fis->fields[3]->boost = 4.0;
    fis->fields[4]->boost = 5.0;

    do_field_prop_test(tc, fis->fields[0], rb_intern("FFFFFFFF"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, fis->fields[1], rb_intern("TFTTFTFF"), 2.0, T, F, T, T, F, T, F, F);
    do_field_prop_test(tc, fis->fields[2], rb_intern("TTTFFTTF"), 3.0, T, T, T, F, F, T, T, F);
    do_field_prop_test(tc, fis->fields[3], rb_intern("FFTTTTFT"), 4.0, F, F, T, T, T, T, F, T);
    do_field_prop_test(tc, fis->fields[4], rb_intern("FFTFTTTT"), 5.0, F, F, T, F, T, T, T, T);

    frt_fis_deref(fis);
}

static void test_fis_with_default(TestCase *tc, void *data)
{
    FrtFieldInfos *fis;
    (void)data; /* suppress unused argument warning */

    fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("name")), rb_intern("name"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("dave")), rb_intern("dave"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("wert")), rb_intern("wert"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, fis->fields[0], rb_intern("name"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, fis->fields[1], rb_intern("dave"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, fis->fields[2], rb_intern("wert"), 1.0, F, F, F, F, F, F, F, F);
    Apnull(frt_fis_get_field(fis, rb_intern("random")));
    frt_fis_deref(fis);

    fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("name")), rb_intern("name"), 1.0, T, F, T, T, F, T, F, F);
    frt_fis_deref(fis);
    fis = frt_fis_new(FRT_STORE_COMPRESS, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_POSITIONS);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("name")), rb_intern("name"), 1.0, T, T, T, F, F, T, T, F);
    frt_fis_deref(fis);
    fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_YES_OMIT_NORMS, FRT_TERM_VECTOR_WITH_OFFSETS);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("name")), rb_intern("name"), 1.0, F, F, T, T, T, T, F, T);
    frt_fis_deref(fis);
    fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_UNTOKENIZED_OMIT_NORMS, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("name")), rb_intern("name"), 1.0, F, F, T, F, T, T, T, T);
    frt_fis_deref(fis);
}

static void test_fis_rw(TestCase *tc, void *data)
{
    char *str;
    FrtFieldInfos *fis;
    FrtStore *store = frt_open_ram_store(NULL);
    FrtInStream *is;
    FrtOutStream *os;
    (void)data; /* suppress unused argument warning */

    fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_UNTOKENIZED_OMIT_NORMS,
                  FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("FFFFFFFF"), FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("TFTTFTFF"), FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("TTTFFTTF"), FRT_STORE_COMPRESS, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_WITH_POSITIONS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("FFTTTTFT"), FRT_STORE_NO, FRT_INDEX_YES_OMIT_NORMS, FRT_TERM_VECTOR_WITH_OFFSETS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("FFTFTTTT"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED_OMIT_NORMS, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS));
    fis->fields[1]->boost = 2.0;
    fis->fields[2]->boost = 3.0;
    fis->fields[3]->boost = 4.0;
    fis->fields[4]->boost = 5.0;
    os = store->new_output(store, "fields");
    frt_fis_write(fis, os);
    frt_os_close(os);

    /* these fields won't be saved be will added again later */
    Aiequal(5, fis->size);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("new_field")), rb_intern("new_field"), 1.0, T, F, T, F, T, T, T, T);
    Aiequal(6, fis->size);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("another")), rb_intern("another"), 1.0, T, F, T, F, T, T, T, T);
    Aiequal(7, fis->size);

    frt_fis_deref(fis);

    is = store->open_input(store, "fields");
    fis = frt_fis_read(is);
    frt_is_close(is);
    Aiequal(FRT_STORE_YES, fis->store);
    Aiequal(FRT_INDEX_UNTOKENIZED_OMIT_NORMS, fis->index);
    Aiequal(FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS, fis->term_vector);

    do_field_prop_test(tc, fis->fields[0], rb_intern("FFFFFFFF"), 1.0, F, F, F, F, F, F, F, F);
    do_field_prop_test(tc, fis->fields[1], rb_intern("TFTTFTFF"), 2.0, T, F, T, T, F, T, F, F);
    do_field_prop_test(tc, fis->fields[2], rb_intern("TTTFFTTF"), 3.0, T, T, T, F, F, T, T, F);
    do_field_prop_test(tc, fis->fields[3], rb_intern("FFTTTTFT"), 4.0, F, F, T, T, T, T, F, T);
    do_field_prop_test(tc, fis->fields[4], rb_intern("FFTFTTTT"), 5.0, F, F, T, F, T, T, T, T);
    Aiequal(5, fis->size);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("new_field")), rb_intern("new_field"), 1.0, T, F, T, F, T, T, T, T);
    Aiequal(6, fis->size);
    do_field_prop_test(tc, frt_fis_get_or_add_field(fis, rb_intern("another")), rb_intern("another"), 1.0, T, F, T, F, T, T, T, T);
    Aiequal(7, fis->size);
    str = frt_fis_to_s(fis);
    Asequal("default:\n"
            "  store: :yes\n"
            "  index: :untokenized_omit_norms\n"
            "  term_vector: :with_positions_offsets\n"
            "fields:\n"
            "  FFFFFFFF:\n"
            "    boost: 1.000000\n"
            "    store: :no\n"
            "    index: :no\n"
            "    term_vector: :no\n"
            "  TFTTFTFF:\n"
            "    boost: 2.000000\n"
            "    store: :yes\n"
            "    index: :yes\n"
            "    term_vector: :yes\n"
            "  TTTFFTTF:\n"
            "    boost: 3.000000\n"
            "    store: :compressed\n"
            "    index: :untokenized\n"
            "    term_vector: :with_positions\n"
            "  FFTTTTFT:\n"
            "    boost: 4.000000\n"
            "    store: :no\n"
            "    index: :omit_norms\n"
            "    term_vector: :with_offsets\n"
            "  FFTFTTTT:\n"
            "    boost: 5.000000\n"
            "    store: :no\n"
            "    index: :untokenized_omit_norms\n"
            "    term_vector: :with_positions_offsets\n"
            "  new_field:\n"
            "    boost: 1.000000\n"
            "    store: :yes\n"
            "    index: :untokenized_omit_norms\n"
            "    term_vector: :with_positions_offsets\n"
            "  another:\n"
            "    boost: 1.000000\n"
            "    store: :yes\n"
            "    index: :untokenized_omit_norms\n"
            "    term_vector: :with_positions_offsets\n", str);
    free(str);
    frt_fis_deref(fis);
    frt_store_deref(store);
}

/****************************************************************************
 *
 * FrtFieldsReader/FieldsWriter
 *
 ****************************************************************************/

#define BIN_DATA_LEN 1234

static char *prepare_bin_data(int len)
{
  int i;
  char *bin_data = FRT_ALLOC_N(char, len);
  for (i = 0; i < len; i++) {
    bin_data[i] = i;
  }
  return bin_data;
}

#define check_df_data(df, index, mdata)\
    do {\
        Aiequal(strlen(mdata), df->lengths[index]);\
        Asequal(mdata, df->data[index]);\
    } while (0)

#define check_df_bin_data(df, index, mdata, mlen)\
    do {\
        Aiequal(mlen, df->lengths[index]);\
        Assert(memcmp(mdata, df->data[index], mlen) == 0, "Data should be equal");\
    } while (0)

static FrtDocument *prepare_doc(void) {
    FrtDocument *doc = frt_doc_new();
    FrtDocField *df;
    char *bin_data = prepare_bin_data(BIN_DATA_LEN);
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("ignored")), (char *)"this fld's ignored", enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("unstored")), (char *)"unstored ignored", enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("stored")), (char *)"Yay, a stored field", enc));
    df = frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("stored_array")), (char *)"one", enc));
    df->destroy_data = false;
    frt_df_add_data(df, (char *)"two", enc);
    frt_df_add_data(df, (char *)"three", enc);
    frt_df_add_data(df, (char *)"four", enc);
    frt_df_add_data_len(df, bin_data, BIN_DATA_LEN, enc);
    frt_doc_add_field(doc, frt_df_add_data_len(frt_df_new(rb_intern("binary")), bin_data,
                                       BIN_DATA_LEN, enc))->destroy_data = true;
    df = frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("array")), (char *)"ichi", enc));
    frt_df_add_data(df, (char *)"ni", enc);
    frt_df_add_data(df, (char *)"san", enc);
    frt_df_add_data(df, (char *)"yon", enc);
    frt_df_add_data(df, (char *)"go", enc);

    return doc;
}

static FrtFieldInfos *prepare_fis(void) {
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_NO);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("ignored"), FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("unstored"), FRT_STORE_NO, FRT_INDEX_YES, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("stored"), FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("stored_array"), FRT_STORE_COMPRESS, FRT_INDEX_UNTOKENIZED, FRT_TERM_VECTOR_NO));
    return fis;
}

static void test_fields_rw_single(TestCase *tc, void *data)
{
    FrtStore *store = frt_open_ram_store(NULL);
    char *bin_data = prepare_bin_data(BIN_DATA_LEN);
    FrtDocument *doc = prepare_doc();
    FrtFieldInfos *fis = prepare_fis();
    FrtFieldsWriter *fw;
    FrtFieldsReader *fr;
    FrtDocField *df;
    (void)data;

    Aiequal(4, fis->size);
    Aiequal(6, doc->size);

    fw = frt_fw_open(store, "_0", fis);
    frt_fw_add_doc(fw, doc);
    frt_fw_write_tv_index(fw);
    frt_fw_close(fw);
   frt_doc_destroy(doc);

    Aiequal(6, fis->size);
    do_field_prop_test(tc, frt_fis_get_field(fis, rb_intern("binary")), rb_intern("binary"), 1.0, T, F, T, T, F, F, F, F);
    do_field_prop_test(tc, frt_fis_get_field(fis, rb_intern("array")), rb_intern("array"), 1.0, T, F, T, T, F, F, F, F);

    fr = frt_fr_open(store, "_0", fis);
    doc = frt_fr_get_doc(fr, 0);
    frt_fr_close(fr);

    Aiequal(4, doc->size);

    Apnull(frt_doc_get_field(doc, rb_intern("ignored")));
    Apnull(frt_doc_get_field(doc, rb_intern("unstored")));

    df = frt_doc_get_field(doc, rb_intern("stored"));
    Aiequal(1, df->size);
    check_df_data(df, 0, "Yay, a stored field");

    df = frt_doc_get_field(doc, rb_intern("stored_array"));
    Aiequal(5, df->size);
    check_df_data(df, 0, "one");
    check_df_data(df, 1, "two");
    check_df_data(df, 2, "three");
    check_df_data(df, 3, "four");
    check_df_bin_data(df, 4, bin_data, BIN_DATA_LEN);

    df = frt_doc_get_field(doc, rb_intern("binary"));
    Aiequal(1, df->size);
    check_df_bin_data(df, 0, bin_data, BIN_DATA_LEN);

    df = frt_doc_get_field(doc, rb_intern("array"));
    Aiequal(5, df->size);
    check_df_data(df, 0, "ichi");
    check_df_data(df, 1, "ni");
    check_df_data(df, 2, "san");
    check_df_data(df, 3, "yon");
    check_df_data(df, 4, "go");

    free(bin_data);
    frt_store_deref(store);
   frt_doc_destroy(doc);
    frt_fis_deref(fis);
}

static void test_fields_rw_multi(TestCase *tc, void *data)
{
    int i;
    FrtStore *store = frt_open_ram_store(NULL);
    char *bin_data = prepare_bin_data(BIN_DATA_LEN);
    FrtDocument *doc;
    FrtFieldInfos *fis = prepare_fis();
    FrtFieldsWriter *fw;
    FrtFieldsReader *fr;
    FrtDocField *df;
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    fw = frt_fw_open(store, "_as3", fis);
    for (i = 0; i < 100; i++) {
        char buf[100];
        char *bufc;
        sprintf(buf, "<<%d>>", i);
        bufc = frt_estrdup(buf);
        doc = frt_doc_new();
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern(bufc)), bufc, enc));
        frt_fw_add_doc(fw, doc);
        frt_fw_write_tv_index(fw);
       frt_doc_destroy(doc);
    }

    doc = prepare_doc();
    frt_fw_add_doc(fw, doc);
    frt_fw_write_tv_index(fw);
   frt_doc_destroy(doc);
    frt_fw_close(fw);

    Aiequal(106, fis->size);
    do_field_prop_test(tc, frt_fis_get_field(fis, rb_intern("binary")), rb_intern("binary"), 1.0, T, F, T, T, F, F, F, F);
    do_field_prop_test(tc, frt_fis_get_field(fis, rb_intern("array")), rb_intern("array"), 1.0, T, F, T, T, F, F, F, F);
    for (i = 0; i < 100; i++) {
        char buf[100];
        sprintf(buf, "<<%d>>", i);
        do_field_prop_test(tc, frt_fis_get_field(fis, rb_intern(buf)), rb_intern(buf), 1.0, T, F, T, T, F, F, F, F);
    }

    fr = frt_fr_open(store, "_as3", fis);
    doc = frt_fr_get_doc(fr, 100);
    frt_fr_close(fr);

    Aiequal(4, doc->size);

    Apnull(frt_doc_get_field(doc, rb_intern("ignored")));
    Apnull(frt_doc_get_field(doc, rb_intern("unstored")));

    df = frt_doc_get_field(doc, rb_intern("stored"));
    Aiequal(1, df->size);
    check_df_data(df, 0, "Yay, a stored field");

    df = frt_doc_get_field(doc, rb_intern("stored_array"));
    Aiequal(5, df->size);
    check_df_data(df, 0, "one");
    check_df_data(df, 1, "two");
    check_df_data(df, 2, "three");
    check_df_data(df, 3, "four");
    check_df_bin_data(df, 4, bin_data, BIN_DATA_LEN);

    df = frt_doc_get_field(doc, rb_intern("binary"));
    Aiequal(1, df->size);
    check_df_bin_data(df, 0, bin_data, BIN_DATA_LEN);

    df = frt_doc_get_field(doc, rb_intern("array"));
    Aiequal(5, df->size);
    check_df_data(df, 0, "ichi");
    check_df_data(df, 1, "ni");
    check_df_data(df, 2, "san");
    check_df_data(df, 3, "yon");
    check_df_data(df, 4, "go");

    free(bin_data);
    frt_store_deref(store);
   frt_doc_destroy(doc);
    frt_fis_deref(fis);
}

static void test_lazy_field_loading(TestCase *tc, void *data)
{
    FrtStore *store = frt_open_ram_store(NULL);
    FrtDocument *doc;
    FrtFieldInfos *fis = prepare_fis();
    FrtFieldsWriter *fw;
    FrtFieldsReader *fr;
    FrtDocField *df;
    FrtLazyDoc *lazy_doc;
    FrtLazyDocField *lazy_df;
    char *text, buf[1000];
    (void)data;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    fw = frt_fw_open(store, "_as3", fis);
    doc = frt_doc_new();
    df = frt_df_new(rb_intern("stored"));
    frt_df_add_data(df, (char *)"this is a stored field", enc);
    frt_df_add_data(df, (char *)"to be or not to be", enc);
    frt_df_add_data(df, (char *)"a stitch in time, saves nine", enc);
    frt_df_add_data(df, (char *)"the quick brown fox jumped over the lazy dog", enc);
    frt_df_add_data(df, (char *)"that's it folks", enc);
    frt_doc_add_field(doc, df);
    frt_fw_add_doc(fw, doc);
    frt_fw_write_tv_index(fw);
    frt_doc_destroy(doc);
    frt_fw_close(fw);

    fr = frt_fr_open(store, "_as3", fis);
    lazy_doc = frt_fr_get_lazy_doc(fr, 0);
    frt_fr_close(fr);
    frt_fis_deref(fis);
    frt_store_deref(store);

    lazy_df = frt_lazy_doc_get(lazy_doc, rb_intern("stored"));
    Apnull(lazy_doc->fields[0]->data[0].text);
    Asequal("this is a stored field", text = frt_lazy_df_get_data(lazy_df, 0));
    Asequal("this is a stored field", lazy_doc->fields[0]->data[0].text);
    Apequal(text, frt_lazy_df_get_data(lazy_df, 0));

    Apnull(lazy_doc->fields[0]->data[4].text);
    Asequal("that's it folks", text = frt_lazy_df_get_data(lazy_df, 4));
    Asequal("that's it folks", lazy_doc->fields[0]->data[4].text);
    Apequal(text, frt_lazy_df_get_data(lazy_df, 4));

    frt_lazy_df_get_bytes(lazy_df, buf, 17, 8);
    buf[8] = 0;
    Asequal("field to", buf);
    frt_lazy_df_get_bytes(lazy_df, buf, 126, 5);
    buf[5] = 0;
    Asequal("folks", buf);
    frt_lazy_df_get_bytes(lazy_df, buf, 0, 131);
    buf[131] = 0;
    Asequal("this is a stored field to be or not to be a stitch in time, "
             "saves nine the quick brown fox jumped over the lazy dog "
             "that's it folks", buf);

    frt_lazy_doc_close(lazy_doc);
}

TestSuite *ts_fields(TestSuite *suite)
{
    suite = ADD_SUITE(suite);
    tst_run_test(suite, test_fi_new, NULL);
    tst_run_test(suite, test_fis_basic, NULL);
    tst_run_test(suite, test_fis_with_default, NULL);
    tst_run_test(suite, test_fis_rw, NULL);
    tst_run_test(suite, test_fields_rw_single, NULL);
    tst_run_test(suite, test_fields_rw_multi, NULL);
    tst_run_test(suite, test_lazy_field_loading, NULL);

    return suite;
}

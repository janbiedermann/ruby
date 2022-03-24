#include "frt_index.h"
#include "testhelper.h"
#include "test.h"

static FrtSymbol body, title, text, author, year, changing_field, compressed_field, tag;

static FrtFieldInfos *prep_all_fis()
{
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_YES, FRT_TERM_VECTOR_NO);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv"), FRT_STORE_NO, FRT_INDEX_YES, FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv un-t"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED,
                              FRT_TERM_VECTOR_YES));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv+offsets"), FRT_STORE_NO, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_OFFSETS));
    frt_fis_add_field(fis, frt_fi_new(rb_intern("tv+offsets un-t"), FRT_STORE_NO, FRT_INDEX_UNTOKENIZED,
                              FRT_TERM_VECTOR_WITH_OFFSETS));
    return fis;

}

static void destroy_docs(FrtDocument **docs, int len)
{
  int i;
  for (i = 0; i < len; i++) {
   frt_doc_destroy(docs[i]);
  }
  free(docs);
}

static FrtFieldInfos *prep_book_fis()
{
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("year"), FRT_STORE_YES, FRT_INDEX_NO, FRT_TERM_VECTOR_NO));
    return fis;

}

FrtDocument *prep_book()
{
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(author),
            frt_estrdup("P.H. Newby"), enc))->destroy_data = true;
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(title),
            frt_estrdup("Something To Answer For"), enc))->destroy_data = true;
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(year),
            frt_estrdup("1969"), enc))->destroy_data = true;
    return doc;
}

#define BOOK_LIST_LENGTH 37
FrtDocument **prep_book_list()
{
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    FrtDocument **docs = FRT_ALLOC_N(FrtDocument *, BOOK_LIST_LENGTH);
    docs[0] = frt_doc_new();
    frt_doc_add_field(docs[0], frt_df_add_data(frt_df_new(author),
            frt_estrdup("P.H. Newby"), enc))->destroy_data = true;
    frt_doc_add_field(docs[0], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Something To Answer For"), enc))->destroy_data = true;
    frt_doc_add_field(docs[0], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1969"), enc))->destroy_data = true;
    docs[1] = frt_doc_new();
    frt_doc_add_field(docs[1], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Bernice Rubens"), enc))->destroy_data = true;
    frt_doc_add_field(docs[1], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Elected Member"), enc))->destroy_data = true;
    frt_doc_add_field(docs[1], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1970"), enc))->destroy_data = true;
    docs[2] = frt_doc_new();
    frt_doc_add_field(docs[2], frt_df_add_data(frt_df_new(author),
            frt_estrdup("V. S. Naipaul"), enc))->destroy_data = true;
    frt_doc_add_field(docs[2], frt_df_add_data(frt_df_new(title),
            frt_estrdup("In a Free State"), enc))->destroy_data = true;
    frt_doc_add_field(docs[2], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1971"), enc))->destroy_data = true;
    docs[3] = frt_doc_new();
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(author),
            frt_estrdup("John Berger"), enc))->destroy_data = true;
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(title),
            frt_estrdup("G"), enc))->destroy_data = true;
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1972"), enc))->destroy_data = true;
    docs[4] = frt_doc_new();
    frt_doc_add_field(docs[4], frt_df_add_data(frt_df_new(author),
            frt_estrdup("J. G. Farrell"), enc))->destroy_data = true;
    frt_doc_add_field(docs[4], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Siege of Krishnapur"), enc))->destroy_data = true;
    frt_doc_add_field(docs[4], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1973"), enc))->destroy_data = true;
    docs[5] = frt_doc_new();
    frt_doc_add_field(docs[5], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Stanley Middleton"), enc))->destroy_data = true;
    frt_doc_add_field(docs[5], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Holiday"), enc))->destroy_data = true;
    frt_doc_add_field(docs[5], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1974"), enc))->destroy_data = true;
    docs[6] = frt_doc_new();
    frt_doc_add_field(docs[6], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Nadine Gordimer"), enc))->destroy_data = true;
    frt_doc_add_field(docs[6], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Conservationist"), enc))->destroy_data = true;
    frt_doc_add_field(docs[6], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1974"), enc))->destroy_data = true;
    docs[7] = frt_doc_new();
    frt_doc_add_field(docs[7], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Ruth Prawer Jhabvala"), enc))->destroy_data = true;
    frt_doc_add_field(docs[7], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Heat and Dust"), enc))->destroy_data = true;
    frt_doc_add_field(docs[7], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1975"), enc))->destroy_data = true;
    docs[8] = frt_doc_new();
    frt_doc_add_field(docs[8], frt_df_add_data(frt_df_new(author),
            frt_estrdup("David Storey"), enc))->destroy_data = true;
    frt_doc_add_field(docs[8], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Saville"), enc))->destroy_data = true;
    frt_doc_add_field(docs[8], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1976"), enc))->destroy_data = true;
    docs[9] = frt_doc_new();
    frt_doc_add_field(docs[9], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Paul Scott"), enc))->destroy_data = true;
    frt_doc_add_field(docs[9], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Staying On"), enc))->destroy_data = true;
    frt_doc_add_field(docs[9], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1977"), enc))->destroy_data = true;
    docs[10] = frt_doc_new();
    frt_doc_add_field(docs[10], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Iris Murdoch"), enc))->destroy_data = true;
    frt_doc_add_field(docs[10], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Sea"), enc))->destroy_data = true;
    frt_doc_add_field(docs[10], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1978"), enc))->destroy_data = true;
    docs[11] = frt_doc_new();
    frt_doc_add_field(docs[11], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Penelope Fitzgerald"), enc))->destroy_data = true;
    frt_doc_add_field(docs[11], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Offshore"), enc))->destroy_data = true;
    frt_doc_add_field(docs[11], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1979"), enc))->destroy_data = true;
    docs[12] = frt_doc_new();
    frt_doc_add_field(docs[12], frt_df_add_data(frt_df_new(author),
            frt_estrdup("William Golding"), enc))->destroy_data = true;
    frt_doc_add_field(docs[12], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Rites of Passage"), enc))->destroy_data = true;
    frt_doc_add_field(docs[12], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1980"), enc))->destroy_data = true;
    docs[13] = frt_doc_new();
    frt_doc_add_field(docs[13], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Salman Rushdie"), enc))->destroy_data = true;
    frt_doc_add_field(docs[13], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Midnight's Children"), enc))->destroy_data = true;
    frt_doc_add_field(docs[13], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1981"), enc))->destroy_data = true;
    docs[14] = frt_doc_new();
    frt_doc_add_field(docs[14], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Thomas Keneally"), enc))->destroy_data = true;
    frt_doc_add_field(docs[14], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Schindler's Ark"), enc))->destroy_data = true;
    frt_doc_add_field(docs[14], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1982"), enc))->destroy_data = true;
    docs[15] = frt_doc_new();
    frt_doc_add_field(docs[15], frt_df_add_data(frt_df_new(author),
            frt_estrdup("J. M. Coetzee"), enc))->destroy_data = true;
    frt_doc_add_field(docs[15], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Life and Times of Michael K"), enc))->destroy_data = true;
    frt_doc_add_field(docs[15], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1983"), enc))->destroy_data = true;
    docs[16] = frt_doc_new();
    frt_doc_add_field(docs[16], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Anita Brookner"), enc))->destroy_data = true;
    frt_doc_add_field(docs[16], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Hotel du Lac"), enc))->destroy_data = true;
    frt_doc_add_field(docs[16], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1984"), enc))->destroy_data = true;
    docs[17] = frt_doc_new();
    frt_doc_add_field(docs[17], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Keri Hulme"), enc))->destroy_data = true;
    frt_doc_add_field(docs[17], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Bone People"), enc))->destroy_data = true;
    frt_doc_add_field(docs[17], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1985"), enc))->destroy_data = true;
    docs[18] = frt_doc_new();
    frt_doc_add_field(docs[18], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Kingsley Amis"), enc))->destroy_data = true;
    frt_doc_add_field(docs[18], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Old Devils"), enc))->destroy_data = true;
    frt_doc_add_field(docs[18], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1986"), enc))->destroy_data = true;
    docs[19] = frt_doc_new();
    frt_doc_add_field(docs[19], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Penelope Lively"), enc))->destroy_data = true;
    frt_doc_add_field(docs[19], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Moon Tiger"), enc))->destroy_data = true;
    frt_doc_add_field(docs[19], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1987"), enc))->destroy_data = true;
    docs[20] = frt_doc_new();
    frt_doc_add_field(docs[20], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Peter Carey"), enc))->destroy_data = true;
    frt_doc_add_field(docs[20], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Oscar and Lucinda"), enc))->destroy_data = true;
    frt_doc_add_field(docs[20], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1988"), enc))->destroy_data = true;
    docs[21] = frt_doc_new();
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Kazuo Ishiguro"), enc))->destroy_data = true;
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Remains of the Day"), enc))->destroy_data = true;
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1989"), enc))->destroy_data = true;
    docs[22] = frt_doc_new();
    frt_doc_add_field(docs[22], frt_df_add_data(frt_df_new(author),
            frt_estrdup("A. S. Byatt"), enc))->destroy_data = true;
    frt_doc_add_field(docs[22], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Possession"), enc))->destroy_data = true;
    frt_doc_add_field(docs[22], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1990"), enc))->destroy_data = true;
    docs[23] = frt_doc_new();
    frt_doc_add_field(docs[23], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Ben Okri"), enc))->destroy_data = true;
    frt_doc_add_field(docs[23], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Famished Road"), enc))->destroy_data = true;
    frt_doc_add_field(docs[23], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1991"), enc))->destroy_data = true;
    docs[24] = frt_doc_new();
    frt_doc_add_field(docs[24], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Michael Ondaatje"), enc))->destroy_data = true;
    frt_doc_add_field(docs[24], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The English Patient"), enc))->destroy_data = true;
    frt_doc_add_field(docs[24], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1992"), enc))->destroy_data = true;
    docs[25] = frt_doc_new();
    frt_doc_add_field(docs[25], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Barry Unsworth"), enc))->destroy_data = true;
    frt_doc_add_field(docs[25], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Sacred Hunger"), enc))->destroy_data = true;
    frt_doc_add_field(docs[25], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1992"), enc))->destroy_data = true;
    docs[26] = frt_doc_new();
    frt_doc_add_field(docs[26], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Roddy Doyle"), enc))->destroy_data = true;
    frt_doc_add_field(docs[26], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Paddy Clarke Ha Ha Ha"), enc))->destroy_data = true;
    frt_doc_add_field(docs[26], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1993"), enc))->destroy_data = true;
    docs[27] = frt_doc_new();
    frt_doc_add_field(docs[27], frt_df_add_data(frt_df_new(author),
            frt_estrdup("James Kelman"), enc))->destroy_data = true;
    frt_doc_add_field(docs[27], frt_df_add_data(frt_df_new(title),
            frt_estrdup("How Late It Was, How Late"), enc))->destroy_data = true;
    frt_doc_add_field(docs[27], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1994"), enc))->destroy_data = true;
    docs[28] = frt_doc_new();
    frt_doc_add_field(docs[28], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Pat Barker"), enc))->destroy_data = true;
    frt_doc_add_field(docs[28], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Ghost Road"), enc))->destroy_data = true;
    frt_doc_add_field(docs[28], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1995"), enc))->destroy_data = true;
    docs[29] = frt_doc_new();
    frt_doc_add_field(docs[29], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Graham Swift"), enc))->destroy_data = true;
    frt_doc_add_field(docs[29], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Last Orders"), enc))->destroy_data = true;
    frt_doc_add_field(docs[29], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1996"), enc))->destroy_data = true;
    docs[30] = frt_doc_new();
    frt_doc_add_field(docs[30], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Arundati Roy"), enc))->destroy_data = true;
    frt_doc_add_field(docs[30], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The God of Small Things"), enc))->destroy_data = true;
    frt_doc_add_field(docs[30], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1997"), enc))->destroy_data = true;
    docs[31] = frt_doc_new();
    frt_doc_add_field(docs[31], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Ian McEwan"), enc))->destroy_data = true;
    frt_doc_add_field(docs[31], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Amsterdam"), enc))->destroy_data = true;
    frt_doc_add_field(docs[31], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1998"), enc))->destroy_data = true;
    docs[32] = frt_doc_new();
    frt_doc_add_field(docs[32], frt_df_add_data(frt_df_new(author),
            frt_estrdup("J. M. Coetzee"), enc))->destroy_data = true;
    frt_doc_add_field(docs[32], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Disgrace"), enc))->destroy_data = true;
    frt_doc_add_field(docs[32], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1999"), enc))->destroy_data = true;
    docs[33] = frt_doc_new();
    frt_doc_add_field(docs[33], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Margaret Atwood"), enc))->destroy_data = true;
    frt_doc_add_field(docs[33], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Blind Assassin"), enc))->destroy_data = true;
    frt_doc_add_field(docs[33], frt_df_add_data(frt_df_new(year),
            frt_estrdup("2000"), enc))->destroy_data = true;
    docs[34] = frt_doc_new();
    frt_doc_add_field(docs[34], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Peter Carey"), enc))->destroy_data = true;
    frt_doc_add_field(docs[34], frt_df_add_data(frt_df_new(title),
            frt_estrdup("True History of the Kelly Gang"), enc))->destroy_data = true;
    frt_doc_add_field(docs[34], frt_df_add_data(frt_df_new(year),
            frt_estrdup("2001"), enc))->destroy_data = true;
    docs[35] = frt_doc_new();
    frt_doc_add_field(docs[35], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Yann Martel"), enc))->destroy_data = true;
    frt_doc_add_field(docs[35], frt_df_add_data(frt_df_new(title),
            frt_estrdup("The Life of Pi"), enc))->destroy_data = true;
    frt_doc_add_field(docs[35], frt_df_add_data(frt_df_new(year),
            frt_estrdup("2002"), enc))->destroy_data = true;
    docs[36] = frt_doc_new();
    frt_doc_add_field(docs[36], frt_df_add_data(frt_df_new(author),
            frt_estrdup("DBC Pierre"), enc))->destroy_data = true;
    frt_doc_add_field(docs[36], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Vernon God Little"), enc))->destroy_data = true;
    frt_doc_add_field(docs[36], frt_df_add_data(frt_df_new(year),
            frt_estrdup("2003"), enc))->destroy_data = true;

    return docs;
}

static void add_document_with_fields(FrtIndexWriter *iw, int i)
{
    FrtDocument **docs = prep_book_list();
    frt_iw_add_doc(iw, docs[i]);
    destroy_docs(docs, BOOK_LIST_LENGTH);
}

static FrtIndexWriter *create_book_iw_conf(FrtStore *store, const FrtConfig *config)
{
    FrtFieldInfos *fis = prep_book_fis();
    frt_index_create(store, fis);
    frt_fis_deref(fis);
    return frt_iw_open(store, frt_whitespace_analyzer_new(false), config);
}

static FrtIndexWriter *create_book_iw(FrtStore *store)
{
    return create_book_iw_conf(store, &frt_default_config);
}

#define IR_TEST_DOC_CNT 256

FrtDocument **prep_ir_test_docs()
{
    int i;
    char buf[2000] = "";
    FrtDocument **docs = FRT_ALLOC_N(FrtDocument *, IR_TEST_DOC_CNT);
    FrtDocField *df;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    docs[0] = frt_doc_new();
    frt_doc_add_field(docs[0], frt_df_add_data(frt_df_new(changing_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 "
                    "word3 word3"), enc))->destroy_data = true;
    frt_doc_add_field(docs[0], frt_df_add_data(frt_df_new(compressed_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 "
                    "word3 word3"), enc))->destroy_data = true;
    frt_doc_add_field(docs[0], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Where is Wally"), enc))->destroy_data = true;
    docs[1] = frt_doc_new();
    frt_doc_add_field(docs[1], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence read"), enc))->destroy_data = true;
    frt_doc_add_field(docs[1], frt_df_add_data(frt_df_new(tag),
            frt_estrdup("id_test"), enc))->destroy_data = true;
    docs[2] = frt_doc_new();
    frt_doc_add_field(docs[2], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some read Random Sentence read"), enc))->destroy_data = true;
    df = frt_df_new(tag);
    frt_df_add_data(df, frt_estrdup("one"), enc);
    frt_df_add_data(df, frt_estrdup("two"), enc);
    frt_df_add_data(df, frt_estrdup("three"), enc);
    frt_df_add_data(df, frt_estrdup("four"), enc);
    frt_doc_add_field(docs[2], df)->destroy_data = true;
    df = frt_df_new(compressed_field);
    frt_df_add_data(df, frt_estrdup("one"), enc);
    frt_df_add_data(df, frt_estrdup("two"), enc);
    frt_df_add_data(df, frt_estrdup("three"), enc);
    frt_df_add_data(df, frt_estrdup("four"), enc);
    frt_doc_add_field(docs[2], df)->destroy_data = true;
    docs[3] = frt_doc_new();
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(title),
            frt_estrdup("War And Peace"), enc))->destroy_data = true;
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(body),
            frt_estrdup("word3 word4 word1 word2 word1 "
                    "word3 word4 word1 word3 word3"), enc))->destroy_data = true;
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(author),
            frt_estrdup("Leo Tolstoy"), enc))->destroy_data = true;
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(year),
            frt_estrdup("1865"), enc))->destroy_data = true;
    frt_doc_add_field(docs[3], frt_df_add_data(frt_df_new(text),
            frt_estrdup("more text which is not stored"), enc))->destroy_data = true;
    docs[4] = frt_doc_new();
    frt_doc_add_field(docs[4], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    docs[5] = frt_doc_new();
    frt_doc_add_field(docs[5], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Here's Wally"), enc))->destroy_data = true;
    frt_doc_add_field(docs[5], frt_df_add_data(frt_df_new(text),
            frt_estrdup("so_that_norm_can_be_set"), enc))->destroy_data = true;
    docs[6] = frt_doc_new();
    frt_doc_add_field(docs[6], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence read read read read"
                    ), enc))->destroy_data = true;
    docs[7] = frt_doc_new();
    frt_doc_add_field(docs[7], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    docs[8] = frt_doc_new();
    frt_doc_add_field(docs[8], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    docs[9] = frt_doc_new();
    frt_doc_add_field(docs[9], frt_df_add_data(frt_df_new(body),
            frt_estrdup("read Some Random Sentence read this will be used after "
                    "unfinished next position read"), enc))->destroy_data = true;
    docs[10] = frt_doc_new();
    frt_doc_add_field(docs[10], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some read Random Sentence"), enc))->destroy_data = true;
    frt_doc_add_field(docs[10], frt_df_add_data(frt_df_new(changing_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 word3 "
                    "word3"), enc))->destroy_data = true;
    docs[11] = frt_doc_new();
    frt_doc_add_field(docs[11], frt_df_add_data(frt_df_new(body),
            frt_estrdup("And here too. Well, maybe Not"), enc))->destroy_data = true;
    docs[12] = frt_doc_new();
    frt_doc_add_field(docs[12], frt_df_add_data(frt_df_new(title),
            frt_estrdup("Shawshank Redemption"), enc))->destroy_data = true;
    frt_doc_add_field(docs[12], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    docs[13] = frt_doc_new();
    frt_doc_add_field(docs[13], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    docs[14] = frt_doc_new();
    frt_doc_add_field(docs[14], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    docs[15] = frt_doc_new();
    frt_doc_add_field(docs[15], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some read Random Sentence"), enc))->destroy_data = true;
    docs[16] = frt_doc_new();
    frt_doc_add_field(docs[16], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random read read Sentence"), enc))->destroy_data = true;
    docs[17] = frt_doc_new();
    frt_doc_add_field(docs[17], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random read Sentence"), enc))->destroy_data = true;
    frt_doc_add_field(docs[17], frt_df_add_data(frt_df_new(changing_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 word3 "
                    "word3"), enc))->destroy_data = true;
    docs[18] = frt_doc_new();
    frt_doc_add_field(docs[18], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Wally Wally Wally"), enc))->destroy_data = true;
    docs[19] = frt_doc_new();
    frt_doc_add_field(docs[19], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence"), enc))->destroy_data = true;
    frt_doc_add_field(docs[19], frt_df_add_data(frt_df_new(changing_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 word3 "
                    "word3"), enc))->destroy_data = true;
    docs[20] = frt_doc_new();
    frt_doc_add_field(docs[20], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Wally is where Wally usually likes to go. Wally Mart! "
                    "Wally likes shopping there for Where's Wally books. "
                    "Wally likes to read"), enc))->destroy_data = true;
    frt_doc_add_field(docs[20], frt_df_add_data(frt_df_new(changing_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 word3 "
                    "word3"), enc))->destroy_data = true;
    docs[21] = frt_doc_new();
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(body),
            frt_estrdup("Some Random Sentence read read read and more read read "
                    "read"), enc))->destroy_data = true;
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(changing_field),
            frt_estrdup("word3 word4 word1 word2 word1 word3 word4 word1 word3 "
                    "word3"), enc))->destroy_data = true;
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(rb_intern("new field")),
            frt_estrdup("zdata znot zto zbe zfound"), enc))->destroy_data = true;
    frt_doc_add_field(docs[21], frt_df_add_data(frt_df_new(title),
            frt_estrdup("title_too_long_for_max_word_lengthxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"), enc))->destroy_data = true;

    for (i = 1; i < 22; i++) {
        strcat(buf, "skip ");
    }
    for (i = 22; i < IR_TEST_DOC_CNT; i++) {
        strcat(buf, "skip ");
        docs[i] = frt_doc_new();
        frt_doc_add_field(docs[i], frt_df_add_data(frt_df_new(text),
            frt_estrdup(buf), enc))->destroy_data = true;
    }
    return docs;
}

#define NUM_STDE_TEST_DOCS 50
#define MAX_TEST_WORDS 1000

static void prep_stde_test_docs(FrtDocument **docs, int doc_cnt, int num_words,
                            FrtFieldInfos *fis)
{
    int i, j;
    char *buf = FRT_ALLOC_N(char, num_words * (TEST_WORD_LIST_MAX_LEN + 1));
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    for (i = 0; i < doc_cnt; i++) {
        docs[i] = frt_doc_new();
        for (j = 0; j < fis->size; j++) {
            if ((rand() % 2) == 0) {
                FrtDocField *df = frt_df_new(fis->fields[j]->name);
                frt_df_add_data(df, frt_estrdup(make_random_string(buf, num_words)), enc);
                df->destroy_data = true;
                frt_doc_add_field(docs[i], df);
            }
        }
    }
    free(buf);
}

static void prep_test_1seg_index(FrtStore *store, FrtDocument **docs,
                                 int doc_cnt, FrtFieldInfos *fis)
{
    int i;
    FrtDocWriter *dw;
    FrtIndexWriter *iw;
    FrtSegmentInfo *si = frt_si_new(frt_estrdup("_0"), doc_cnt, store);

    frt_index_create(store, fis);
    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), NULL);

    dw = frt_dw_open(iw, si);

    for (i = 0; i < doc_cnt; i++) {
        frt_dw_add_doc(dw, docs[i]);
    }

    frt_dw_close(dw);
    frt_iw_close(iw);
    frt_si_deref(si);
}

/****************************************************************************
 *
 * TermDocEnum
 *
 ****************************************************************************/

static void test_segment_term_doc_enum(TestCase *tc, void *data)
{
    int i, j;
    FrtStore *store = (FrtStore *)data;
    FrtFieldInfos *fis = prep_all_fis();
    FrtFieldInfo *fi;
    FrtSegmentFieldIndex *sfi;
    FrtTermInfosReader *tir;
    int skip_interval;
    FrtInStream *frq_in, *prx_in;
    FrtBitVector *bv = NULL;
    FrtTermDocEnum *tde, *tde_reader, *tde_skip_to;
    char buf[TEST_WORD_LIST_MAX_LEN + 1];
    FrtDocField *df;
    FrtDocument *docs[NUM_STDE_TEST_DOCS], *doc;

    prep_stde_test_docs(docs, NUM_STDE_TEST_DOCS, MAX_TEST_WORDS, fis);
    prep_test_1seg_index(store, docs, NUM_STDE_TEST_DOCS, fis);

    sfi = frt_sfi_open(store, "_0");
    tir = frt_tir_open(store, sfi, "_0");
    skip_interval = ((FrtSegmentTermEnum *)tir->orig_te)->skip_interval;
    frq_in = store->open_input(store, "_0.frq");
    prx_in = store->open_input(store, "_0.prx");
    tde = frt_stde_new(tir, frq_in, bv, skip_interval);
    tde_reader = frt_stde_new(tir, frq_in, bv, skip_interval);
    tde_skip_to = frt_stde_new(tir, frq_in, bv, skip_interval);

    fi = frt_fis_get_field(fis, rb_intern("tv"));
    for (i = 0; i < 300; i++) {
        int cnt = 0, ind = 0, doc_nums[3], freqs[3];
        const char *word = test_word_list[rand()%TEST_WORD_LIST_SIZE];
        tde->seek(tde, fi->number, word);
        tde_reader->seek(tde_reader, fi->number, word);
        while (tde->next(tde)) {
            if (cnt == ind) {
                cnt = tde_reader->read(tde_reader, doc_nums, freqs, 3);
                ind = 0;
            }
            Aiequal(doc_nums[ind], tde->doc_num(tde));
            Aiequal(freqs[ind], tde->freq(tde));
            ind++;

            doc = docs[tde->doc_num(tde)];
            df = frt_doc_get_field(doc, fi->name);
            if (Apnotnull(df)) {
                Assert(strstr((char *)df->data[0], word) != NULL,
                       "%s not found in doc[%d]\n\"\"\"\n%s\n\"\"\"\n",
                       word, tde->doc_num(tde), df->data[0]);
            }
            tde_skip_to->seek(tde_skip_to, fi->number, word);
            Atrue(tde_skip_to->skip_to(tde_skip_to, tde->doc_num(tde)));
            Aiequal(tde->doc_num(tde), tde_skip_to->doc_num(tde_skip_to));
            Aiequal(tde->freq(tde), tde_skip_to->freq(tde_skip_to));
        }
        Aiequal(ind, cnt);

        Atrue(! tde->next(tde));
        Atrue(! tde->next(tde));
        Atrue(! tde->skip_to(tde, 0));
        Atrue(! tde->skip_to(tde, 1000000));
    }
    tde->close(tde);
    tde_reader->close(tde_reader);
    tde_skip_to->close(tde_skip_to);


    tde = frt_stpe_new(tir, frq_in, prx_in, bv, skip_interval);
    tde_skip_to = frt_stpe_new(tir, frq_in, prx_in, bv, skip_interval);

    fi = frt_fis_get_field(fis, rb_intern("tv+offsets"));
    for (i = 0; i < 200; i++) {
        const char *word = test_word_list[rand()%TEST_WORD_LIST_SIZE];
        tde->seek(tde, fi->number, word);
        while (tde->next(tde)) {
            tde_skip_to->seek(tde_skip_to, fi->number, word);
            Atrue(tde_skip_to->skip_to(tde_skip_to, tde->doc_num(tde)));
            Aiequal(tde->doc_num(tde), tde_skip_to->doc_num(tde_skip_to));
            Aiequal(tde->freq(tde), tde_skip_to->freq(tde_skip_to));

            doc = docs[tde->doc_num(tde)];
            df = frt_doc_get_field(doc, fi->name);
            if (Apnotnull(df)) {
                Assert(strstr((char *)df->data[0], word) != NULL,
                       "%s not found in doc[%d]\n\"\"\"\n%s\n\"\"\"\n",
                       word, tde->doc_num(tde), df->data[0]);
                for (j = tde->freq(tde); j > 0; j--) {
                    int pos = tde->next_position(tde), t;
                    Aiequal(pos, tde_skip_to->next_position(tde_skip_to));
                    Asequal(word, get_nth_word(df->data[0], buf, pos, &t, &t));
                }
            }
        }
        Atrue(! tde->next(tde));
        Atrue(! tde->next(tde));
        Atrue(! tde->skip_to(tde, 0));
        Atrue(! tde->skip_to(tde, 1000000));

    }
    tde->close(tde);
    tde_skip_to->close(tde_skip_to);

    for (i = 0; i < NUM_STDE_TEST_DOCS; i++) {
       frt_doc_destroy(docs[i]);
    }
    frt_fis_deref(fis);
    frt_is_close(frq_in);
    frt_is_close(prx_in);
    frt_tir_close(tir);
    frt_sfi_close(sfi);
}

const char *double_word = "word word";
const char *triple_word = "word word word";

static void test_segment_tde_deleted_docs(TestCase *tc, void *data)
{
    int i, doc_num_expected, skip_interval;
    FrtStore *store = (FrtStore *)data;
    FrtDocWriter *dw;
    FrtDocument *doc;
    FrtIndexWriter *iw = create_book_iw(store);
    FrtSegmentFieldIndex *sfi;
    FrtTermInfosReader *tir;
    FrtInStream *frq_in, *prx_in;
    FrtBitVector *bv = frt_bv_new();
    FrtTermDocEnum *tde;
    FrtSegmentInfo *si = frt_si_new(frt_estrdup("_0"), NUM_STDE_TEST_DOCS, store);
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    dw = frt_dw_open(iw, si);

    for (i = 0; i < NUM_STDE_TEST_DOCS; i++) {
        doc = frt_doc_new();
        if ((rand() % 2) == 0) {
            frt_bv_set(bv, i);
            Aiequal(1, frt_bv_get(bv, i));
            frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("f")), (char *)double_word, enc));
        }
        else {
            frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("f")), (char *)triple_word, enc));
        }
        frt_dw_add_doc(dw, doc);
       frt_doc_destroy(doc);
    }
    Aiequal(NUM_STDE_TEST_DOCS, dw->doc_num);
    frt_dw_close(dw);
    frt_iw_close(iw);

    sfi = frt_sfi_open(store, "_0");
    tir = frt_tir_open(store, sfi, "_0");
    frq_in = store->open_input(store, "_0.frq");
    prx_in = store->open_input(store, "_0.prx");
    skip_interval = sfi->skip_interval;
    tde = frt_stpe_new(tir, frq_in, prx_in, bv, skip_interval);

    tde->seek(tde, 0, "word");
    doc_num_expected = 0;
    while (tde->next(tde)) {
        while (frt_bv_get(bv, doc_num_expected)) {
            doc_num_expected++;
        }
        Aiequal(doc_num_expected, tde->doc_num(tde));
        if (Aiequal(3, tde->freq(tde))) {
            for (i = 0; i < 3; i++) {
                Aiequal(i, tde->next_position(tde));
            }
        }
        doc_num_expected++;
    }
    tde->close(tde);

    frt_bv_destroy(bv);
    frt_is_close(frq_in);
    frt_is_close(prx_in);
    frt_tir_close(tir);
    frt_sfi_close(sfi);
    frt_si_deref(si);
}

/****************************************************************************
 *
 * Index
 *
 ****************************************************************************/

static void test_index_create(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    (void)tc;

    store->clear_all(store);
    Assert(!store->exists(store, "segments"),
           "segments shouldn't exist yet");
    frt_index_create(store, fis);
    Assert(store->exists(store, "segments"),
           "segments should now exist");
    frt_fis_deref(fis);
}

static void test_index_version(TestCase *tc, void *data)
{
    frt_u64 version;
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw = create_book_iw(store);
    FrtIndexReader *ir;

    add_document_with_fields(iw, 0);
    Atrue(frt_index_is_locked(store));  /* writer open, so dir is locked */
    frt_iw_close(iw);
    Atrue(!frt_index_is_locked(store));
    ir = frt_ir_open(store);
    Atrue(!frt_index_is_locked(store)); /* reader only, no lock */
    version = frt_sis_read_current_version(store);
    frt_ir_close(ir);

    /* modify index and check version has been incremented: */
    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &frt_default_config);
    add_document_with_fields(iw, 1);
    frt_iw_close(iw);
    ir = frt_ir_open(store);
    Atrue(version < frt_sis_read_current_version(store));
    Atrue(frt_ir_is_latest(ir));
    frt_ir_close(ir);
}

static void test_index_undelete_all_after_close(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw = create_book_iw(store);
    FrtIndexReader *ir;
    add_document_with_fields(iw, 0);
    add_document_with_fields(iw, 1);
    frt_iw_close(iw);
    ir = frt_ir_open(store);
    frt_ir_delete_doc(ir, 0);
    frt_ir_delete_doc(ir, 1);
    frt_ir_close(ir);
    ir = frt_ir_open(store);
    frt_ir_undelete_all(ir);
    Aiequal(2, ir->num_docs(ir)); /* nothing has really been deleted */
    frt_ir_close(ir);
    ir = frt_ir_open(store);
    Aiequal(2, ir->num_docs(ir)); /* nothing has really been deleted */
    Atrue(frt_ir_is_latest(ir));
    frt_ir_close(ir);
}

/****************************************************************************
 *
 * IndexWriter
 *
 ****************************************************************************/

static void test_fld_inverter(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtHash *plists;
    FrtHash *curr_plists;
    FrtPosting *p;
    FrtPostingList *pl;
    FrtDocWriter *dw;
    FrtIndexWriter *iw = create_book_iw(store);
    FrtDocField *df;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    dw = frt_dw_open(iw, frt_sis_new_segment(iw->sis, 0, iw->store));

    df = frt_df_new(rb_intern("no tv"));
    frt_df_add_data(df, (char *)"one two three four five two three four five three four five four five", enc);
    frt_df_add_data(df, (char *)"ichi ni san yon go ni san yon go san yon go yon go go", enc);
    frt_df_add_data(df, (char *)"The quick brown fox jumped over five lazy dogs", enc);

    curr_plists = frt_dw_invert_field(
        dw,
        frt_dw_get_fld_inv(dw, frt_fis_get_or_add_field(dw->fis, df->name)),
        df);

    Aiequal(18, curr_plists->size);

    plists = ((FrtFieldInverter *)frt_h_get_int(
            dw->fields, frt_fis_get_field(dw->fis, df->name)->number))->plists;


    pl = (FrtPostingList *)frt_h_get(curr_plists, "one");
    if (Apnotnull(pl)) {
        Asequal("one", pl->term);
        Aiequal(3, pl->term_len);

        p = pl->last;
        Aiequal(1, p->freq);
        Apequal(p->first_occ, pl->last_occ);
        Apnull(p->first_occ->next);
        Aiequal(0, p->first_occ->pos);
        Apequal(pl, ((FrtPostingList *)frt_h_get(plists, "one")));
    }

    pl = (FrtPostingList *)frt_h_get(curr_plists, "five");
    if (Apnotnull(pl)) {
        Asequal("five", pl->term);
        Aiequal(4, pl->term_len);
        Apnull(pl->last_occ->next);
        p = pl->last;
        Aiequal(5, p->freq);
        Aiequal(4, p->first_occ->pos);
        Aiequal(8, p->first_occ->next->pos);
        Aiequal(11, p->first_occ->next->next->pos);
        Aiequal(13, p->first_occ->next->next->next->pos);
        Aiequal(35, p->first_occ->next->next->next->next->pos);
        Apequal(pl, ((FrtPostingList *)frt_h_get(plists, "five")));
    }

    frt_df_destroy(df);

    df = frt_df_new(rb_intern("no tv"));
    frt_df_add_data(df, (char *)"seven new words and six old ones", enc);
    frt_df_add_data(df, (char *)"ichi ni one two quick dogs", enc);

    dw->doc_num++;
    frt_dw_reset_postings(dw->curr_plists);

    curr_plists = frt_dw_invert_field(
        dw,
        frt_dw_get_fld_inv(dw, frt_fis_get_or_add_field(dw->fis, df->name)),
        df);

    Aiequal(13, curr_plists->size);

    pl = (FrtPostingList *)frt_h_get(curr_plists, "one");
    if (Apnotnull(pl)) {
        Asequal("one", pl->term);
        Aiequal(3, pl->term_len);

        p = pl->first;
        Aiequal(1, p->freq);
        Apnull(p->first_occ->next);
        Aiequal(0, p->first_occ->pos);

        p = pl->last;
        Aiequal(1, p->freq);
        Apequal(p->first_occ, pl->last_occ);
        Apnull(p->first_occ->next);
        Aiequal(9, p->first_occ->pos);
        Apequal(pl, ((FrtPostingList *)frt_h_get(plists, "one")));
    }

    frt_df_destroy(df);

    frt_dw_close(dw);
    frt_iw_close(iw);
}

#define NUM_POSTINGS TEST_WORD_LIST_SIZE
static void test_postings_sorter(TestCase *tc, void *data)
{
    int i;
    FrtPostingList plists[NUM_POSTINGS], *p_ptr[NUM_POSTINGS];
    (void)data, (void)tc;
    for (i = 0; i < NUM_POSTINGS; i++) {
        plists[i].term = (char *)test_word_list[i];
        p_ptr[i] = &plists[i];
    }

    qsort(p_ptr, NUM_POSTINGS, sizeof(FrtPostingList *),
          (int (*)(const void *, const void *))&frt_pl_cmp);

    for (i = 1; i < NUM_POSTINGS; i++) {
        Assert(strcmp(p_ptr[i - 1]->term, p_ptr[i]->term) <= 0,
               "\"%s\" > \"%s\"", p_ptr[i - 1]->term, p_ptr[i]->term);
    }
}

static void test_iw_add_doc(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw = create_book_iw(store);
    FrtDocument **docs = prep_book_list();

    frt_iw_add_doc(iw, docs[0]);
    Aiequal(1, frt_iw_doc_count(iw));
    Assert(!store->exists(store, "_0.cfs"),
           "data shouldn't have been written yet");
    frt_iw_commit(iw);
    Assert(store->exists(store, "_0.cfs"), "data should now be written");
    frt_iw_close(iw);
    Assert(store->exists(store, "_0.cfs"), "data should still be there");

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &frt_default_config);
    frt_iw_add_doc(iw, docs[1]);
    Aiequal(2, frt_iw_doc_count(iw));
    Assert(!store->exists(store, "_1.cfs"),
           "data shouldn't have been written yet");
    Assert(store->exists(store, "_0.cfs"), "data should still be there");
    frt_iw_commit(iw);
    Assert(store->exists(store, "_1.cfs"), "data should now be written");
    frt_iw_close(iw);
    Assert(store->exists(store, "_1.cfs"), "data should still be there");
    Assert(store->exists(store, "_0.cfs"), "data should still be there");

    destroy_docs(docs, BOOK_LIST_LENGTH);
}

/*
 * Make sure we can open an index for create even when a
 * reader holds it open (this fails pre lock-less
 * commits on windows):
 */
static void test_create_with_reader(TestCase *tc, void *data)
{
    FrtStore *store = frt_open_fs_store(TEST_DIR);
    (void)data;
    FrtIndexWriter *iw;
    FrtIndexReader *ir, *ir2;
    FrtDocument *doc = prep_book();
    store->clear_all(store);

    /* add one document & close writer */
    iw = create_book_iw(store);
    frt_iw_add_doc(iw, doc);
    frt_iw_close(iw);

    /* now open reader: */
    ir = frt_ir_open(store);
    Aiequal(1, ir->num_docs(ir));

    /* now open index for create: */
    iw = create_book_iw(store);
    Aiequal(0, frt_iw_doc_count(iw));
    frt_iw_add_doc(iw, doc);
    frt_iw_close(iw);

    Aiequal(1, ir->num_docs(ir));
    ir2 = frt_ir_open(store);
    Aiequal(1, ir2->num_docs(ir));
    frt_ir_close(ir);
    frt_ir_close(ir2);
    store->clear_all(store);
    frt_store_deref(store);
   frt_doc_destroy(doc);
}

/*
 * Simulate a writer that crashed while writing segments
 * file: make sure we can still open the index (ie,
 * gracefully fallback to the previous segments file),
 * and that we can add to the index:
 */
static void test_simulated_crashed_writer(TestCase *tc, void *data)
{
    int i;
    long gen;
    off_t length;
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtIndexReader *ir;
    char file_name_in[FRT_SEGMENT_NAME_MAX_LENGTH];
    char file_name_out[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtInStream *is;
    FrtOutStream *os;
    FrtDocument **docs = prep_book_list();
    FrtConfig config = frt_default_config;
    config.max_buffered_docs = 3;

    iw = create_book_iw_conf(store, &config);
    for (i = 0; i < BOOK_LIST_LENGTH; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }

    /* close */
    frt_iw_close(iw);

    gen = frt_sis_current_segment_generation(store);
    /* segment generation should be > 1 */
    Atrue(gen > 1);

    /* Make the next segments file, with last byte
     * missing, to simulate a writer that crashed while
     * writing segments file: */
    frt_sis_curr_seg_file_name(file_name_in, store);
    frt_fn_for_generation(file_name_out, FRT_SEGMENTS_FILE_NAME, NULL, 1 + gen);
    is = store->open_input(store, file_name_in);
    os = store->new_output(store, file_name_out);
    length = frt_is_length(is);
    for(i = 0; i < length - 1; i++) {
        frt_os_write_byte(os, frt_is_read_byte(is));
    }
    frt_is_close(is);
    frt_os_close(os);

    ir = frt_ir_open(store);
    frt_ir_close(ir);

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);

    /* add all books */
    for (i = 0; i < BOOK_LIST_LENGTH; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }

    destroy_docs(docs, BOOK_LIST_LENGTH);
    frt_iw_close(iw);
}

/*
 * Simulate a corrupt index by removing last byte of
 * latest segments file and make sure we get an
 * IOException trying to open the index:
 */
static void test_simulated_corrupt_index1(TestCase *tc, void *data)
{
    int i;
    long gen;
    off_t length;
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtIndexReader *ir;
    char file_name_in[FRT_SEGMENT_NAME_MAX_LENGTH];
    char file_name_out[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtInStream *is;
    FrtOutStream *os;
    FrtDocument **docs = prep_book_list();
    FrtConfig config = frt_default_config;
    config.max_buffered_docs = 3;

    iw = create_book_iw_conf(store, &config);
    for (i = 0; i < BOOK_LIST_LENGTH; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }

    /* close */
    frt_iw_close(iw);

    gen = frt_sis_current_segment_generation(store);
    /* segment generation should be > 1 */
    Atrue(gen > 1);

    /* Make the next segments file, with last byte
     * missing, to simulate a writer that crashed while
     * writing segments file: */
    frt_sis_curr_seg_file_name(file_name_in, store);
    frt_fn_for_generation(file_name_out, FRT_SEGMENTS_FILE_NAME, "", 1 + gen);
    is = store->open_input(store, file_name_in);
    os = store->new_output(store, file_name_out);
    length = frt_is_length(is);
    for(i = 0; i < length - 1; i++) {
        frt_os_write_byte(os, frt_is_read_byte(is));
    }
    frt_is_close(is);
    frt_os_close(os);
    store->remove(store, file_name_in);

    FRT_TRY
        ir = frt_ir_open(store);
        frt_ir_close(ir);
        Afail("reader should have failed to open on a crashed index");
        break;
    case FRT_IO_ERROR:
        FRT_HANDLED();
        break;
    default:
        Afail("reader should have raised an FRT_IO_ERROR");
        FRT_HANDLED();
    FRT_XENDTRY
    destroy_docs(docs, BOOK_LIST_LENGTH);
}

/*
 * Simulate a corrupt index by removing one of the cfs
 * files and make sure we get an IOException trying to
 * open the index:
 */
static void test_simulated_corrupt_index2(TestCase *tc, void *data)
{
    int i;
    long gen;
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtIndexReader *ir;
    FrtDocument **docs = prep_book_list();
    FrtConfig config = frt_default_config;
    config.max_buffered_docs = 10;

    iw = create_book_iw_conf(store, &config);
    for (i = 0; i < BOOK_LIST_LENGTH; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }

    /* close */
    frt_iw_close(iw);

    gen = frt_sis_current_segment_generation(store);
    /* segment generation should be > 1 */
    Atrue(gen > 1);

    Atrue(store->exists(store, "_0.cfs"));
    store->remove(store, "_0.cfs");

    FRT_TRY
        ir = frt_ir_open(store);
        frt_ir_close(ir);
        Afail("reader should have failed to open on a crashed index");
        break;
    case FRT_IO_ERROR:
        FRT_HANDLED();
    FRT_XCATCHALL
        Afail("reader should have raised an FRT_IO_ERROR");
        FRT_HANDLED();
    FRT_XENDTRY
    destroy_docs(docs, BOOK_LIST_LENGTH);
}

static void test_iw_add_docs(TestCase *tc, void *data)
{
    int i;
    FrtConfig config = frt_default_config;
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtDocument **docs = prep_book_list();
    config.merge_factor = 4;
    config.max_buffered_docs = 3;

    iw = create_book_iw_conf(store, &config);
    for (i = 0; i < BOOK_LIST_LENGTH; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }
    frt_iw_optimize(iw);
    Aiequal(BOOK_LIST_LENGTH, frt_iw_doc_count(iw));

    frt_iw_close(iw);
    destroy_docs(docs, BOOK_LIST_LENGTH);
    if (!Aiequal(3, store->count(store))) {
        char *buf = frt_store_to_s(store);
        Tmsg("To many files: %s\n", buf);
        free(buf);
    }
}

void test_iw_add_empty_tv(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtDocument *doc;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    frt_fis_add_field(fis, frt_fi_new(rb_intern("no_tv"), FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_NO));
    frt_index_create(store, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &frt_default_config);
    doc = frt_doc_new();
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("tv1")), (char *)"", enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("tv2")), (char *)"", enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern("no_tv")), (char *)"one two three", enc));

    frt_iw_add_doc(iw, doc);
    frt_iw_commit(iw);
    Aiequal(1, frt_iw_doc_count(iw));
    frt_iw_close(iw);
   frt_doc_destroy(doc);
}

static void test_iw_del_terms(TestCase *tc, void *data)
{
    int i;
    FrtConfig config = frt_default_config;
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtIndexReader *ir;
    FrtDocument **docs = prep_book_list();
    const char *terms[3];
    config.merge_factor = 4;
    config.max_buffered_docs = 3;

    iw = create_book_iw_conf(store, &config);
    for (i = 0; i < BOOK_LIST_LENGTH; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }
    Aiequal(BOOK_LIST_LENGTH, frt_iw_doc_count(iw));
    frt_iw_close(iw);
    destroy_docs(docs, BOOK_LIST_LENGTH);

    ir = frt_ir_open(store);
    Aiequal(BOOK_LIST_LENGTH, ir->num_docs(ir));
    Aiequal(BOOK_LIST_LENGTH, ir->max_doc(ir));
    frt_ir_close(ir);

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);
    frt_iw_delete_term(iw, title, "State");
    frt_iw_close(iw);

    ir = frt_ir_open(store);
    Aiequal(BOOK_LIST_LENGTH - 1, ir->num_docs(ir));
    Aiequal(BOOK_LIST_LENGTH, ir->max_doc(ir));
    frt_ir_close(ir);

    /* test deleting multiple Terms */
    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);
    frt_iw_delete_term(iw, title, "The");
    frt_iw_delete_term(iw, title, "Blind");
    terms[0] = "Berger";
    terms[1] = "Middleton";
    terms[2] = "DBC";
    frt_iw_delete_terms(iw, author, (char **)terms, 3);
    frt_iw_close(iw);

    ir = frt_ir_open(store);
    Aiequal(BOOK_LIST_LENGTH - 17, ir->num_docs(ir));
    Aiequal(BOOK_LIST_LENGTH, ir->max_doc(ir));
    Atrue(!ir->is_deleted(ir, 0));
    Atrue(ir->is_deleted(ir, 1));
    Atrue(ir->is_deleted(ir, 2));
    Atrue(ir->is_deleted(ir, 3));
    Atrue(ir->is_deleted(ir, 4));
    Atrue(ir->is_deleted(ir, 5));
    Atrue(ir->is_deleted(ir, 6));
    Atrue(!ir->is_deleted(ir, 7));
    Atrue(!ir->is_deleted(ir, 9));
    Atrue(ir->is_deleted(ir, 10));
    Atrue(!ir->is_deleted(ir, 11));
    Atrue(!ir->is_deleted(ir, 16));
    Atrue(ir->is_deleted(ir, 17));
    Atrue(ir->is_deleted(ir, 18));
    Atrue(ir->is_deleted(ir, 21));
    Atrue(ir->is_deleted(ir, 23));
    Atrue(ir->is_deleted(ir, 24));
    Atrue(ir->is_deleted(ir, 28));
    Atrue(ir->is_deleted(ir, 30));
    Atrue(ir->is_deleted(ir, 33));
    Atrue(ir->is_deleted(ir, 35));
    Atrue(ir->is_deleted(ir, 36));
    frt_ir_commit(ir);

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);
    frt_iw_optimize(iw);
    frt_iw_close(iw);

    frt_ir_close(ir);

    ir = frt_ir_open(store);
    Aiequal(BOOK_LIST_LENGTH - 17, ir->num_docs(ir));
    Aiequal(BOOK_LIST_LENGTH - 17, ir->max_doc(ir));
    frt_ir_close(ir);
}

/****************************************************************************
 *
 * FrtIndexReader
 *
 ****************************************************************************/

static int segment_reader_type = 0;
static int multi_reader_type = 1;
static int multi_external_reader_type = 2;
static int add_indexes_reader_type = 3;

typedef struct ReaderTestEnvironment {
    FrtStore **stores;
    int store_cnt;
} ReaderTestEnvironment;

static void reader_test_env_destroy(ReaderTestEnvironment *rte)
{
    int i;
    for (i = 0; i < rte->store_cnt; i++) {
        frt_store_deref(rte->stores[i]);
    }
    free(rte->stores);
    free(rte);
}

static FrtIndexReader *reader_test_env_ir_open(ReaderTestEnvironment *rte)
{
    if (rte->store_cnt == 1) {
        return frt_ir_open(rte->stores[0]);
    }
    else {
        FrtIndexReader **sub_readers = FRT_ALLOC_N(FrtIndexReader *, rte->store_cnt);
        int i;
        for (i = 0; i < rte->store_cnt; i++) {
            sub_readers[i] = frt_ir_open(rte->stores[i]);
        }
        return (frt_mr_open(sub_readers, rte->store_cnt));
    }
}

static ReaderTestEnvironment *reader_test_env_new(int type)
{
    int i, j;
    FrtConfig config = frt_default_config;
    FrtIndexWriter *iw;
    FrtDocument **docs = prep_ir_test_docs();
    ReaderTestEnvironment *rte = FRT_ALLOC(ReaderTestEnvironment);
    int store_cnt = rte->store_cnt
        = (type >= multi_external_reader_type) ? 64 : 1;
    int doc_cnt = IR_TEST_DOC_CNT / store_cnt;

    rte->stores = FRT_ALLOC_N(FrtStore *, store_cnt);
    for (i = 0; i < store_cnt; i++) {
        FrtStore *store = rte->stores[i] = frt_open_ram_store();
        FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                                  FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
        int start_doc = i * doc_cnt;
        int end_doc = (i + 1) * doc_cnt;
        if (end_doc > IR_TEST_DOC_CNT) {
            end_doc = IR_TEST_DOC_CNT;
        }
        frt_index_create(store, fis);
        frt_fis_deref(fis);
        config.max_buffered_docs = 3;

        iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);

        for (j = start_doc; j < end_doc; j++) {
            int k;
            FrtDocument *doc = docs[j];
            /* add fields when needed. This is to make the FrtFieldInfos objects
             * different for multi_external_reader */
            for (k = 0; k < doc->size; k++) {
                FrtDocField *df = doc->fields[k];
                fis = iw->fis;
                if (NULL == frt_fis_get_field(fis, df->name)) {
                    if (author == df->name) {
                        frt_fis_add_field(fis, frt_fi_new(author, FRT_STORE_YES, FRT_INDEX_YES,
                                  FRT_TERM_VECTOR_WITH_POSITIONS));
                    } else if (title == df->name) {
                        frt_fis_add_field(fis, frt_fi_new(title, FRT_STORE_YES,
                                                  FRT_INDEX_UNTOKENIZED,
                                                  FRT_TERM_VECTOR_WITH_OFFSETS));
                    } else if (year == df->name) {
                        frt_fis_add_field(fis, frt_fi_new(year, FRT_STORE_YES,
                                                  FRT_INDEX_UNTOKENIZED,
                                                  FRT_TERM_VECTOR_NO));
                    } else if (text == df->name) {
                        frt_fis_add_field(fis, frt_fi_new(text, FRT_STORE_NO, FRT_INDEX_YES,
                                                  FRT_TERM_VECTOR_NO));
                    } else if (compressed_field == df->name) {
                        frt_fis_add_field(fis, frt_fi_new(compressed_field,
                                                  FRT_STORE_COMPRESS,
                                                  FRT_INDEX_YES,
                                                  FRT_TERM_VECTOR_NO));
                    }
                }
            }
            frt_iw_add_doc(iw, doc);
        }

        if (type == segment_reader_type) {
            frt_iw_optimize(iw);
        }
        frt_iw_close(iw);
    }

    if (type == add_indexes_reader_type) {
        /* Prepare store for Add Indexes test */
        FrtStore *store = frt_open_ram_store();
        FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                                  FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
        FrtIndexReader **readers = FRT_ALLOC_N(FrtIndexReader *, rte->store_cnt);
        int i;
        for (i = 0; i < rte->store_cnt; i++) {
            readers[i] = frt_ir_open(rte->stores[i]);
        }
        frt_index_create(store, fis);
        frt_fis_deref(fis);
        iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);
        frt_iw_add_readers(iw, readers, rte->store_cnt - 10);
        frt_iw_close(iw);
        iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);
        frt_iw_add_readers(iw, readers + (rte->store_cnt - 10), 10);
        frt_iw_close(iw);
        for (i = 0; i < rte->store_cnt; i++) {
            frt_ir_close(readers[i]);
            frt_store_deref(rte->stores[i]);
        }
        free(readers);
        rte->stores[0] = store;
        rte->store_cnt = 1;
    }

    destroy_docs(docs, IR_TEST_DOC_CNT);
    return rte;
}

static void write_ir_test_docs(FrtStore *store)
{
    int i;
    FrtConfig config = frt_default_config;
    FrtIndexWriter *iw;
    FrtDocument **docs = prep_ir_test_docs();

    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    frt_fis_add_field(fis, frt_fi_new(author, FRT_STORE_YES, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS));
    frt_fis_add_field(fis, frt_fi_new(title, FRT_STORE_YES, FRT_INDEX_UNTOKENIZED,
                              FRT_TERM_VECTOR_WITH_OFFSETS));
    frt_fis_add_field(fis, frt_fi_new(year, FRT_STORE_YES, FRT_INDEX_UNTOKENIZED,
                              FRT_TERM_VECTOR_NO));
    frt_fis_add_field(fis, frt_fi_new(text, FRT_STORE_NO, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_NO));
    frt_fis_add_field(fis, frt_fi_new(compressed_field, FRT_STORE_COMPRESS, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_NO));
    frt_index_create(store, fis);
    frt_fis_deref(fis);
    config.max_buffered_docs = 5;

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), &config);

    for (i = 0; i < IR_TEST_DOC_CNT; i++) {
        frt_iw_add_doc(iw, docs[i]);
    }
    frt_iw_close(iw);

    destroy_docs(docs, IR_TEST_DOC_CNT);
}

static void test_ir_open_empty_index(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    store->clear_all(store);
    FRT_TRY
        frt_ir_close(frt_ir_open(store));
        Afail("IndexReader should have failed when opening empty index");
        break;
    case FRT_FILE_NOT_FOUND_ERROR:
        FRT_HANDLED();
        break;
    default:
        Afail("IndexReader should have raised FileNotfound Exception");
        FRT_HANDLED();
    FRT_XENDTRY
}

static void test_ir_basic_ops(TestCase *tc, void *data)
{
    FrtIndexReader *ir = (FrtIndexReader *)data;

    Aiequal(IR_TEST_DOC_CNT, ir->num_docs(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));

    Aiequal(4, ir->doc_freq(ir, frt_fis_get_field(ir->fis, body)->number, "Wally"));
    Atrue(frt_ir_is_latest(ir));
}

static void test_ir_term_docpos_enum_skip_to(TestCase *tc,
                                             FrtTermDocEnum *tde,
                                             int field_num)
{
    /* test skip_to working skip interval */
    tde->seek(tde, field_num, "skip");

    Atrue(tde->skip_to(tde, 10));
    Aiequal(22, tde->doc_num(tde));
    Aiequal(22, tde->freq(tde));

    Atrue(tde->skip_to(tde, 100));
    Aiequal(100, tde->doc_num(tde));
    Aiequal(100, tde->freq(tde));

    tde->seek(tde, field_num, "skip");
    Atrue(tde->skip_to(tde, 85));
    Aiequal(85, tde->doc_num(tde));
    Aiequal(85, tde->freq(tde));

    Atrue(tde->skip_to(tde, 200));
    Aiequal(200, tde->doc_num(tde));
    Aiequal(200, tde->freq(tde));

    Atrue(tde->skip_to(tde, 255));
    Aiequal(255, tde->doc_num(tde));
    Aiequal(255, tde->freq(tde));

    Atrue(!tde->skip_to(tde, 256));

    tde->seek(tde, field_num, "skip");

    Atrue(!tde->skip_to(tde, 256));
}

#define AA3(x, a, b, c) x[0] = a; x[1] = b; x[2] = c;

static void test_ir_term_enum(TestCase *tc, void *data)
{
    FrtIndexReader *ir = (FrtIndexReader *)data;
    FrtTermEnum *te = frt_ir_terms(ir, author);

    Asequal("Leo", te->next(te));
    Asequal("Leo", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    Asequal("Tolstoy", te->next(te));
    Asequal("Tolstoy", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    Apnull(te->next(te));

    te->set_field(te, frt_fis_get_field_num(ir->fis, body));
    Asequal("And", te->next(te));
    Asequal("And", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);

    Asequal("Not", te->skip_to(te, "Not"));
    Asequal("Not", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    Asequal("Random", te->next(te));
    Asequal("Random", te->curr_term);
    Aiequal(16, te->curr_ti.doc_freq);

    te->set_field(te, frt_fis_get_field_num(ir->fis, text));
    Asequal("which", te->skip_to(te, "which"));
    Asequal("which", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    Apnull(te->next(te));

    te->set_field(te, frt_fis_get_field_num(ir->fis, title));
    Asequal("Shawshank Redemption", te->next(te));
    Asequal("Shawshank Redemption", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    Asequal("War And Peace", te->next(te));
    Asequal("War And Peace", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    te->close(te);

    te = frt_ir_terms_from(ir, body, "No");
    Asequal("Not", te->curr_term);
    Aiequal(1, te->curr_ti.doc_freq);
    Asequal("Random", te->next(te));
    Asequal("Random", te->curr_term);
    Aiequal(16, te->curr_ti.doc_freq);
    te->close(te);
}

static void test_ir_term_doc_enum(TestCase *tc, void *data)
{
    FrtIndexReader *ir = (FrtIndexReader *)data;

    FrtTermDocEnum *tde;
    FrtDocument *doc = frt_ir_get_doc_with_term(ir, tag, "id_test");
    int docs[3], expected_docs[3];
    int freqs[3], expected_freqs[3];

    Apnotnull(doc);
    Asequal("id_test", frt_doc_get_field(doc, tag)->data[0]);
    Asequal("Some Random Sentence read", frt_doc_get_field(doc, body)->data[0]);
    frt_doc_destroy(doc);

    /* test scanning */
    tde = ir_term_docs_for(ir, body, "Wally");

    Atrue(tde->next(tde));
    Aiequal(0, tde->doc_num(tde));
    Aiequal(1, tde->freq(tde));

    Atrue(tde->next(tde));
    Aiequal(5, tde->doc_num(tde));
    Aiequal(1, tde->freq(tde));

    Atrue(tde->next(tde));
    Aiequal(18, tde->doc_num(tde));
    Aiequal(3, tde->freq(tde));

    Atrue(tde->next(tde));
    Aiequal(20, tde->doc_num(tde));
    Aiequal(6, tde->freq(tde));
    Atrue(! tde->next(tde));

    /* test fast read. Use a small array to exercise repeat read */
    tde->seek(tde, frt_fis_get_field(ir->fis, body)->number, "read");
    Aiequal(3, tde->read(tde, docs, freqs, 3));
    AA3(expected_freqs, 1, 2, 4);
    AA3(expected_docs, 1, 2, 6);
    Aaiequal(expected_docs, docs, 3);
    Aaiequal(expected_freqs, freqs, 3);

    Aiequal(3, tde->read(tde, docs, freqs, 3));
    AA3(expected_docs, 9, 10, 15);
    AA3(expected_freqs, 3, 1, 1);
    Aaiequal(expected_docs, docs, 3);
    Aaiequal(expected_freqs, freqs, 3);

    Aiequal(3, tde->read(tde, docs, freqs, 3));
    AA3(expected_docs, 16, 17, 20);
    AA3(expected_freqs, 2, 1, 1);
    Aaiequal(expected_docs, docs, 3);
    Aaiequal(expected_freqs, freqs, 3);

    Aiequal(1, tde->read(tde, docs, freqs, 3));
    expected_docs[0] = 21;
    expected_freqs[0] = 6;
    Aaiequal(expected_docs, docs, 1);
    Aaiequal(expected_freqs, freqs, 1);

    Aiequal(0, tde->read(tde, docs, freqs, 3));

    test_ir_term_docpos_enum_skip_to(tc, tde,
                                     frt_fis_get_field(ir->fis, text)->number);
    tde->close(tde);

    /* test term positions */
    tde = frt_ir_term_positions_for(ir, body, "read");
    Aiequal(-1, tde->next_position(tde));

    Atrue(tde->next(tde));
    Aiequal(1, tde->doc_num(tde));
    Aiequal(1, tde->freq(tde));
    Aiequal(3, tde->next_position(tde));
    Aiequal(-1, tde->next_position(tde));

    Atrue(tde->next(tde));
    Aiequal(2, tde->doc_num(tde));
    Aiequal(2, tde->freq(tde));
    Aiequal(1, tde->next_position(tde));
    Aiequal(4, tde->next_position(tde));

    Atrue(tde->next(tde));
    Aiequal(6, tde->doc_num(tde));
    Aiequal(4, tde->freq(tde));
    Aiequal(3, tde->next_position(tde));
    Aiequal(4, tde->next_position(tde));

    Atrue(tde->next(tde));
    Aiequal(9, tde->doc_num(tde));
    Aiequal(3, tde->freq(tde));
    Aiequal(0, tde->next_position(tde));
    Aiequal(4, tde->next_position(tde));

    Atrue(tde->skip_to(tde, 16));
    Aiequal(16, tde->doc_num(tde));
    Aiequal(2, tde->freq(tde));
    Aiequal(2, tde->next_position(tde));

    Atrue(tde->skip_to(tde, 21));
    Aiequal(21, tde->doc_num(tde));
    Aiequal(6, tde->freq(tde));
    Aiequal(3, tde->next_position(tde));
    Aiequal(4, tde->next_position(tde));
    Aiequal(5, tde->next_position(tde));
    Aiequal(8, tde->next_position(tde));
    Aiequal(9, tde->next_position(tde));
    Aiequal(10, tde->next_position(tde));

    Atrue(! tde->next(tde));

    test_ir_term_docpos_enum_skip_to(tc, tde,
                                     frt_fis_get_field(ir->fis, text)->number);
    tde->close(tde);
}

static void test_ir_term_vectors(TestCase *tc, void *data)
{
    FrtIndexReader *ir = (FrtIndexReader *)data;

    FrtTermVector *tv = ir->term_vector(ir, 3, rb_intern("body"));
    FrtHash *tvs;

    Asequal("body", rb_id2name(tv->field));
    Aiequal(4, tv->term_cnt);
    Asequal("word1", tv->terms[0].text);
    Asequal("word2", tv->terms[1].text);
    Asequal("word3", tv->terms[2].text);
    Asequal("word4", tv->terms[3].text);
    Aiequal(3, tv->terms[0].freq);
    Aiequal(2, tv->terms[0].positions[0]);
    Aiequal(4, tv->terms[0].positions[1]);
    Aiequal(7, tv->terms[0].positions[2]);
    Aiequal(12, tv->offsets[tv->terms[0].positions[0]].start);
    Aiequal(17, tv->offsets[tv->terms[0].positions[0]].end);
    Aiequal(24, tv->offsets[tv->terms[0].positions[1]].start);
    Aiequal(29, tv->offsets[tv->terms[0].positions[1]].end);
    Aiequal(42, tv->offsets[tv->terms[0].positions[2]].start);
    Aiequal(47, tv->offsets[tv->terms[0].positions[2]].end);

    Aiequal(1, tv->terms[1].freq);
    Aiequal(3, tv->terms[1].positions[0]);
    Aiequal(18, tv->offsets[tv->terms[1].positions[0]].start);
    Aiequal(23, tv->offsets[tv->terms[1].positions[0]].end);

    Aiequal(4, tv->terms[2].freq);
    Aiequal(0, tv->terms[2].positions[0]);
    Aiequal(5, tv->terms[2].positions[1]);
    Aiequal(8, tv->terms[2].positions[2]);
    Aiequal(9, tv->terms[2].positions[3]);
    Aiequal(0,  tv->offsets[tv->terms[2].positions[0]].start);
    Aiequal(5,  tv->offsets[tv->terms[2].positions[0]].end);
    Aiequal(30, tv->offsets[tv->terms[2].positions[1]].start);
    Aiequal(35, tv->offsets[tv->terms[2].positions[1]].end);
    Aiequal(48, tv->offsets[tv->terms[2].positions[2]].start);
    Aiequal(53, tv->offsets[tv->terms[2].positions[2]].end);
    Aiequal(54, tv->offsets[tv->terms[2].positions[3]].start);
    Aiequal(59, tv->offsets[tv->terms[2].positions[3]].end);

    Aiequal(2, tv->terms[3].freq);
    Aiequal(1, tv->terms[3].positions[0]);
    Aiequal(6, tv->terms[3].positions[1]);
    Aiequal(6,  tv->offsets[tv->terms[3].positions[0]].start);
    Aiequal(11, tv->offsets[tv->terms[3].positions[0]].end);
    Aiequal(36, tv->offsets[tv->terms[3].positions[1]].start);
    Aiequal(41, tv->offsets[tv->terms[3].positions[1]].end);

    frt_tv_destroy(tv);

    tvs = ir->term_vectors(ir, 3);
    Aiequal(3, tvs->size);
    tv = (FrtTermVector *)frt_h_get(tvs, (void *)rb_intern("author"));
    if (Apnotnull(tv)) {
        Asequal("author", rb_id2name(tv->field));
        Aiequal(2, tv->term_cnt);
        Aiequal(0, tv->offset_cnt);
        Apnull(tv->offsets);
    }
    tv = (FrtTermVector *)frt_h_get(tvs, (void *)rb_intern("body"));
    if (Apnotnull(tv)) {
        Asequal("body", rb_id2name(tv->field));
        Aiequal(4, tv->term_cnt);
    }
    tv = (FrtTermVector *)frt_h_get(tvs, (void *)rb_intern("title"));
    if (Apnotnull(tv)) {
        Asequal("title", rb_id2name(tv->field));
        Aiequal(1, tv->term_cnt); /* untokenized */
        Aiequal(1, tv->offset_cnt);
        Asequal("War And Peace", tv->terms[0].text);
        Apnull(tv->terms[0].positions);
        Aiequal(0,  tv->offsets[0].start);
        Aiequal(13, tv->offsets[0].end);
    }
    frt_h_destroy(tvs);
}

static void test_ir_get_doc(TestCase *tc, void *data)
{
    FrtIndexReader *ir = (FrtIndexReader *)data;
    FrtDocument *doc = ir->get_doc(ir, 3);
    FrtDocField *df;
    Aiequal(4, doc->size);

    df = frt_doc_get_field(doc, author);
    Asequal(rb_id2name(author), rb_id2name(df->name));
    Asequal("Leo Tolstoy", df->data[0]);
    Afequal(df->boost, 1.0);

    df = frt_doc_get_field(doc, body);
    Asequal(rb_id2name(body), rb_id2name(df->name));
    Asequal("word3 word4 word1 word2 word1 "
            "word3 word4 word1 word3 word3", df->data[0]);
    Afequal(df->boost, 1.0);
    df = frt_doc_get_field(doc, title);
    Asequal(rb_id2name(title), rb_id2name(df->name));
    Asequal("War And Peace", df->data[0]);
    Afequal(df->boost, 1.0);

    df = frt_doc_get_field(doc, year);
    Asequal(rb_id2name(year), rb_id2name(df->name));
    Asequal("1865", df->data[0]);
    Afequal(df->boost, 1.0);

    df = frt_doc_get_field(doc, text);
    Apnull(df); /* text is not stored */

   frt_doc_destroy(doc);
}

static void test_ir_compression(TestCase *tc, void *data)
{
    int i;
    FrtIndexReader *ir = (FrtIndexReader *)data;
    FrtLazyDoc *lz_doc;
    FrtLazyDocField *lz_df1, *lz_df2;
    FrtDocument *doc = ir->get_doc(ir, 0);
    FrtDocField *df1, *df2;
    char buf1[20], buf2[20];
    Aiequal(3, doc->size);

    df1 = frt_doc_get_field(doc, changing_field);
    df2 = frt_doc_get_field(doc, compressed_field);
    Asequal(df1->data[0], df2->data[0]);
    Assert(df1->lengths[0] == df2->lengths[0], "Field lengths should be equal");
   frt_doc_destroy(doc);

    doc = ir->get_doc(ir, 2);
    df1 = frt_doc_get_field(doc, tag);
    df2 = frt_doc_get_field(doc, compressed_field);
    for (i = 0; i < 4; i++) {
        Asequal(df1->data[i], df2->data[i]);
        Assert(df1->lengths[i] == df2->lengths[i], "Field lengths not equal");
    }
   frt_doc_destroy(doc);

    lz_doc = ir->get_lazy_doc(ir, 0);
    lz_df1 = frt_lazy_doc_get(lz_doc, changing_field);
    lz_df2 = frt_lazy_doc_get(lz_doc, compressed_field);
    Asequal(frt_lazy_df_get_data(lz_df1, 0), frt_lazy_df_get_data(lz_df2, 0));
    frt_lazy_doc_close(lz_doc);

    lz_doc = ir->get_lazy_doc(ir, 2);
    lz_df1 = frt_lazy_doc_get(lz_doc, tag);
    lz_df2 = frt_lazy_doc_get(lz_doc, compressed_field);
    for (i = 0; i < 4; i++) {
        Asequal(frt_lazy_df_get_data(lz_df1, i), frt_lazy_df_get_data(lz_df2, i));
    }
    frt_lazy_doc_close(lz_doc);

    lz_doc = ir->get_lazy_doc(ir, 2);
    lz_df1 = frt_lazy_doc_get(lz_doc, tag);
    lz_df2 = frt_lazy_doc_get(lz_doc, compressed_field);
    frt_lazy_df_get_bytes(lz_df1, buf1, 5, 11);
    frt_lazy_df_get_bytes(lz_df2, buf2, 5, 11);
    buf2[11] = buf1[11] = '\0';
    Asequal(buf1, buf2);
    frt_lazy_doc_close(lz_doc);
}

static void test_ir_mtdpe(TestCase *tc, void *data)
{
    FrtIndexReader *ir = (FrtIndexReader *)data;
    const char *terms[3] = {"Where", "is", "books."};

    FrtTermDocEnum *tde = frt_mtdpe_new(ir, frt_fis_get_field(ir->fis, body)->number, (char **)terms, 3);

    Atrue(tde->next(tde));
    Aiequal(0, tde->doc_num(tde));
    Aiequal(2, tde->freq(tde));
    Aiequal(0, tde->next_position(tde));
    Aiequal(1, tde->next_position(tde));
    Atrue(tde->next(tde));
    Aiequal(20, tde->doc_num(tde));
    Aiequal(2, tde->freq(tde));
    Aiequal(1, tde->next_position(tde));
    Aiequal(17, tde->next_position(tde));
    Atrue(!tde->next(tde));
    tde->close(tde);
}

static void test_ir_norms(TestCase *tc, void *data)
{
    int i;
    frt_uchar *norms;
    FrtIndexReader *ir, *ir2;
    FrtIndexWriter *iw;
    int type = *((int *)data);
    ReaderTestEnvironment *rte;

    rte = reader_test_env_new(type);
    ir = reader_test_env_ir_open(rte);
    ir2 = reader_test_env_ir_open(rte);
    Atrue(!frt_index_is_locked(rte->stores[0]));

    frt_ir_set_norm(ir, 3, title, 1);
    Atrue(frt_index_is_locked(rte->stores[0]));
    frt_ir_set_norm(ir, 3, body, 12);
    frt_ir_set_norm(ir, 3, author, 145);
    frt_ir_set_norm(ir, 3, year, 31);
    frt_ir_set_norm(ir, 5, text, 202);
    frt_ir_set_norm(ir, 25, text, 20);
    frt_ir_set_norm(ir, 50, text, 200);
    frt_ir_set_norm(ir, 75, text, 155);
    frt_ir_set_norm(ir, 80, text, 0);
    frt_ir_set_norm(ir, 150, text, 255);
    frt_ir_set_norm(ir, 255, text, 76);

    frt_ir_commit(ir);
    Atrue(!frt_index_is_locked(rte->stores[0]));

    norms = frt_ir_get_norms(ir, text);

    Aiequal(202, norms[5]);
    Aiequal(20, norms[25]);
    Aiequal(200, norms[50]);
    Aiequal(155, norms[75]);
    Aiequal(0, norms[80]);
    Aiequal(255, norms[150]);
    Aiequal(76, norms[255]);

    norms = frt_ir_get_norms(ir, title);
    Aiequal(1, norms[3]);

    norms = frt_ir_get_norms(ir, body);
    Aiequal(12, norms[3]);

    norms = frt_ir_get_norms(ir, author);
    Aiequal(145, norms[3]);

    norms = frt_ir_get_norms(ir, year);
    /* Apnull(norms); */

    norms = FRT_ALLOC_N(frt_uchar, 356);
    frt_ir_get_norms_into(ir, text, norms + 100);
    Aiequal(202, norms[105]);
    Aiequal(20, norms[125]);
    Aiequal(200, norms[150]);
    Aiequal(155, norms[175]);
    Aiequal(0, norms[180]);
    Aiequal(255, norms[250]);
    Aiequal(76, norms[355]);

    frt_ir_commit(ir);

    for (i = 0; i < rte->store_cnt; i++) {
        iw = frt_iw_open(rte->stores[i], frt_whitespace_analyzer_new(false),
                     &frt_default_config);
        frt_iw_optimize(iw);
        frt_iw_close(iw);
    }

    frt_ir_close(ir);

    ir = reader_test_env_ir_open(rte);

    memset(norms, 0, 356);
    frt_ir_get_norms_into(ir, text, norms + 100);
    Aiequal(0, norms[102]);
    Aiequal(202, norms[105]);
    Aiequal(0, norms[104]);
    Aiequal(20, norms[125]);
    Aiequal(200, norms[150]);
    Aiequal(155, norms[175]);
    Aiequal(0, norms[180]);
    Aiequal(255, norms[250]);
    Aiequal(76, norms[355]);

    Atrue(!frt_index_is_locked(rte->stores[0]));
    frt_ir_set_norm(ir, 0, text, 155);
    Atrue(frt_index_is_locked(rte->stores[0]));
    frt_ir_close(ir);
    frt_ir_close(ir2);
    Atrue(!frt_index_is_locked(rte->stores[0]));
    reader_test_env_destroy(rte);
    free(norms);
}

static void test_ir_delete(TestCase *tc, void *data)
{
    int i;
    FrtStore *store = frt_open_ram_store();
    FrtIndexReader *ir, *ir2;
    FrtIndexWriter *iw;
    int type = *((int *)data);
    ReaderTestEnvironment *rte;

    rte = reader_test_env_new(type);
    ir = reader_test_env_ir_open(rte);
    ir2 = reader_test_env_ir_open(rte);

    Aiequal(false, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->num_docs(ir));
    Aiequal(false, ir->is_deleted(ir, 10));

    frt_ir_delete_doc(ir, 10);
    Aiequal(true, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 1, ir->num_docs(ir));
    Aiequal(true, ir->is_deleted(ir, 10));

    frt_ir_delete_doc(ir, 10);
    Aiequal(true, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 1, ir->num_docs(ir));
    Aiequal(true, ir->is_deleted(ir, 10));

    frt_ir_delete_doc(ir, IR_TEST_DOC_CNT - 1);
    Aiequal(true, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 2, ir->num_docs(ir));
    Aiequal(true, ir->is_deleted(ir, IR_TEST_DOC_CNT - 1));

    frt_ir_delete_doc(ir, IR_TEST_DOC_CNT - 2);
    Aiequal(true, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 3, ir->num_docs(ir));
    Aiequal(true, ir->is_deleted(ir, IR_TEST_DOC_CNT - 2));

    frt_ir_undelete_all(ir);
    Aiequal(false, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->num_docs(ir));
    Aiequal(false, ir->is_deleted(ir, 10));
    Aiequal(false, ir->is_deleted(ir, IR_TEST_DOC_CNT - 2));
    Aiequal(false, ir->is_deleted(ir, IR_TEST_DOC_CNT - 1));

    frt_ir_delete_doc(ir, 10);
    frt_ir_delete_doc(ir, 20);
    frt_ir_delete_doc(ir, 30);
    frt_ir_delete_doc(ir, 40);
    frt_ir_delete_doc(ir, 50);
    frt_ir_delete_doc(ir, IR_TEST_DOC_CNT - 1);
    Aiequal(true, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 6, ir->num_docs(ir));

    frt_ir_close(ir);

    ir = reader_test_env_ir_open(rte);

    Aiequal(true, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 6, ir->num_docs(ir));
    Aiequal(true, ir->is_deleted(ir, 10));
    Aiequal(true, ir->is_deleted(ir, 20));
    Aiequal(true, ir->is_deleted(ir, 30));
    Aiequal(true, ir->is_deleted(ir, 40));
    Aiequal(true, ir->is_deleted(ir, 50));
    Aiequal(true, ir->is_deleted(ir, IR_TEST_DOC_CNT - 1));

    frt_ir_undelete_all(ir);
    Aiequal(false, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT, ir->num_docs(ir));
    Aiequal(false, ir->is_deleted(ir, 10));
    Aiequal(false, ir->is_deleted(ir, 20));
    Aiequal(false, ir->is_deleted(ir, 30));
    Aiequal(false, ir->is_deleted(ir, 40));
    Aiequal(false, ir->is_deleted(ir, 50));
    Aiequal(false, ir->is_deleted(ir, IR_TEST_DOC_CNT - 1));

    frt_ir_delete_doc(ir, 10);
    frt_ir_delete_doc(ir, 20);
    frt_ir_delete_doc(ir, 30);
    frt_ir_delete_doc(ir, 40);
    frt_ir_delete_doc(ir, 50);
    frt_ir_delete_doc(ir, IR_TEST_DOC_CNT - 1);

    frt_ir_commit(ir);

    for (i = 0; i < rte->store_cnt; i++) {
        iw = frt_iw_open(rte->stores[i], frt_whitespace_analyzer_new(false),
                     &frt_default_config);
        frt_iw_optimize(iw);
        frt_iw_close(iw);
    }

    frt_ir_close(ir);
    ir = reader_test_env_ir_open(rte);

    Aiequal(false, ir->has_deletions(ir));
    Aiequal(IR_TEST_DOC_CNT - 6, ir->max_doc(ir));
    Aiequal(IR_TEST_DOC_CNT - 6, ir->num_docs(ir));

    Atrue(frt_ir_is_latest(ir));
    Atrue(!frt_ir_is_latest(ir2));

    frt_ir_close(ir);
    frt_ir_close(ir2);
    reader_test_env_destroy(rte);
    frt_store_deref(store);
}

static void test_ir_read_while_optimizing(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtIndexWriter *iw;

    write_ir_test_docs(store);

    ir = frt_ir_open(store);

    test_ir_term_doc_enum(tc, ir);

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), false);
    frt_iw_optimize(iw);
    frt_iw_close(iw);

    test_ir_term_doc_enum(tc, ir);

    frt_ir_close(ir);
}

static void test_ir_multivalue_fields(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtFieldInfo *fi;
    FrtDocument *doc = frt_doc_new();
    FrtDocField *df;
    FrtIndexWriter *iw;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    const char *body_text = "this is the body FrtDocument Field";
    const char *title_text = "this is the title FrtDocument Field";
    const char *author_text = "this is the author FrtDocument Field";
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    frt_index_create(store, fis);
    frt_fis_deref(fis);
    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), NULL);

    df = frt_doc_add_field(doc, frt_df_add_data(frt_df_new(tag), (char *)"Ruby", enc));
    frt_df_add_data(df, (char *)"C", enc);
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(body), (char *)body_text, enc));
    frt_df_add_data(df, (char *)"Lucene", enc);
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(title), (char *)title_text, enc));
    frt_df_add_data(df, (char *)"Ferret", enc);
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(author), (char *)author_text, enc));

    Aiequal(0, iw->fis->size);

    frt_iw_add_doc(iw, doc);

    fi = frt_fis_get_field(iw->fis, tag);
    Aiequal(true, fi_is_stored(fi));
    Aiequal(false, fi_is_compressed(fi));
    Aiequal(true, fi_is_indexed(fi));
    Aiequal(true, fi_is_tokenized(fi));
    Aiequal(true, fi_has_norms(fi));
    Aiequal(true, fi_store_term_vector(fi));
    Aiequal(true, fi_store_offsets(fi));
    Aiequal(true, fi_store_positions(fi));

   frt_doc_destroy(doc);
    frt_iw_close(iw);

    ir = frt_ir_open(store);

    doc = ir->get_doc(ir, 0);
    Aiequal(4, doc->size);
    df = frt_doc_get_field(doc, tag);
    Aiequal(4, df->size);
    Asequal("Ruby",   df->data[0]);
    Asequal("C",      df->data[1]);
    Asequal("Lucene", df->data[2]);
    Asequal("Ferret", df->data[3]);

    df = frt_doc_get_field(doc, body);
    Aiequal(1, df->size);
    Asequal(body_text, df->data[0]);

    df = frt_doc_get_field(doc, title);
    Aiequal(1, df->size);
    Asequal(title_text, df->data[0]);

    df = frt_doc_get_field(doc, author);
    Aiequal(1, df->size);
    Asequal(author_text, df->data[0]);

   frt_doc_destroy(doc);
    frt_ir_delete_doc(ir, 0);
    frt_ir_close(ir);
}

/***************************************************************************
 *
 * IndexSuite
 *
 ***************************************************************************/
TestSuite *ts_index(TestSuite *suite)
{
    FrtIndexReader *ir;
    FrtStore *fs_store, *store = frt_open_ram_store();
    ReaderTestEnvironment *rte = NULL;
    /* FrtStore *store = frt_open_fs_store(TEST_DIR); */

    /* initialize FrtSymbols */
    body             = rb_intern("body");
    title            = rb_intern("title");
    text             = rb_intern("text");
    author           = rb_intern("author");
    year             = rb_intern("year");
    changing_field   = rb_intern("changing_field");
    compressed_field = rb_intern("compressed_field");
    tag              = rb_intern("tag");

    srand(5);
    suite = tst_add_suite(suite, "test_term_doc_enum");

    /* FrtTermDocEnum */
    tst_run_test(suite, test_segment_term_doc_enum, store);
    tst_run_test(suite, test_segment_tde_deleted_docs, store);

    suite = ADD_SUITE(suite);
    /* Index */
    tst_run_test(suite, test_index_create, store);
    tst_run_test(suite, test_index_version, store);
    tst_run_test(suite, test_index_undelete_all_after_close, store);

    /* FrtIndexWriter */
    tst_run_test(suite, test_fld_inverter, store);
    tst_run_test(suite, test_postings_sorter, NULL);
    tst_run_test(suite, test_iw_add_doc, store);
    tst_run_test(suite, test_iw_add_docs, store);
    tst_run_test(suite, test_iw_add_empty_tv, store);
    tst_run_test(suite, test_iw_del_terms, store);
    tst_run_test(suite, test_create_with_reader, store);
    tst_run_test(suite, test_simulated_crashed_writer, store);
    tst_run_test(suite, test_simulated_corrupt_index1, store);
    tst_run_test(suite, test_simulated_corrupt_index2, store);

    /* FrtIndexReader */
    tst_run_test(suite, test_ir_open_empty_index, store);

    /* Test SEGMENT Reader */
    rte = reader_test_env_new(segment_reader_type);
    ir = reader_test_env_ir_open(rte);
    tst_run_test_with_name(suite, test_ir_basic_ops, ir, "test_segment_reader_basic_ops");
    tst_run_test_with_name(suite, test_ir_get_doc, ir, "test_segment_get_doc");
    tst_run_test_with_name(suite, test_ir_compression, ir, "test_segment_compression");
    tst_run_test_with_name(suite, test_ir_term_enum, ir, "test_segment_term_enum");
    tst_run_test_with_name(suite, test_ir_term_doc_enum, ir, "test_segment_term_doc_enum");
    tst_run_test_with_name(suite, test_ir_term_vectors, ir, "test_segment_term_vectors");
    tst_run_test_with_name(suite, test_ir_mtdpe, ir, "test_segment_multiple_term_doc_pos_enum");
    tst_run_test_with_name(suite, test_ir_norms, &segment_reader_type, "test_segment_norms");
    tst_run_test_with_name(suite, test_ir_delete, &segment_reader_type, "test_segment_reader_delete");
    frt_ir_close(ir);
    reader_test_env_destroy(rte);

    /* Test MULTI Reader */
    rte = reader_test_env_new(multi_reader_type);
    ir = reader_test_env_ir_open(rte);

    tst_run_test_with_name(suite, test_ir_basic_ops, ir,
                           "test_multi_reader_basic_ops");
    tst_run_test_with_name(suite, test_ir_get_doc, ir,
                           "test_multi_get_doc");
    tst_run_test_with_name(suite, test_ir_compression, ir,
                           "test_multi_compression");
    tst_run_test_with_name(suite, test_ir_term_enum, ir,
                           "test_multi_term_enum");
    tst_run_test_with_name(suite, test_ir_term_doc_enum, ir,
                           "test_multi_term_doc_enum");
    tst_run_test_with_name(suite, test_ir_term_vectors, ir,
                           "test_multi_term_vectors");
    tst_run_test_with_name(suite, test_ir_mtdpe, ir,
                           "test_multi_multiple_term_doc_pos_enum");

    tst_run_test_with_name(suite, test_ir_norms, &multi_reader_type,
                           "test_multi_norms");
    tst_run_test_with_name(suite, test_ir_delete, &multi_reader_type,
                           "test_multi_reader_delete");
    frt_ir_close(ir);
    reader_test_env_destroy(rte);

    /* Test MULTI Reader with seperate stores */
    rte = reader_test_env_new(multi_external_reader_type);
    ir = reader_test_env_ir_open(rte);

    tst_run_test_with_name(suite, test_ir_basic_ops, ir,
                           "test_multi_ext_reader_basic_ops");
    tst_run_test_with_name(suite, test_ir_get_doc, ir,
                           "test_multi_ext_get_doc");
    tst_run_test_with_name(suite, test_ir_compression, ir,
                           "test_multi_ext_compression");
    tst_run_test_with_name(suite, test_ir_term_enum, ir,
                           "test_multi_ext_term_enum");
    tst_run_test_with_name(suite, test_ir_term_doc_enum, ir,
                           "test_multi_ext_term_doc_enum");
    tst_run_test_with_name(suite, test_ir_term_vectors, ir,
                           "test_multi_ext_term_vectors");
    tst_run_test_with_name(suite, test_ir_mtdpe, ir,
                           "test_multi_ext_multiple_term_doc_pos_enum");

    tst_run_test_with_name(suite, test_ir_norms, &multi_external_reader_type,
                           "test_multi_ext_norms");
    tst_run_test_with_name(suite, test_ir_delete, &multi_external_reader_type,
                           "test_multi_ext_reader_delete");

    frt_ir_close(ir);
    reader_test_env_destroy(rte);

    /* Test Add Indexes */
    rte = reader_test_env_new(add_indexes_reader_type);
    ir = reader_test_env_ir_open(rte);

    tst_run_test_with_name(suite, test_ir_basic_ops, ir,
                           "test_add_indexes_reader_basic_ops");
    tst_run_test_with_name(suite, test_ir_get_doc, ir,
                           "test_add_indexes_get_doc");
    tst_run_test_with_name(suite, test_ir_compression, ir,
                           "test_add_indexes_compression");
    tst_run_test_with_name(suite, test_ir_term_enum, ir,
                           "test_add_indexes_term_enum");
    tst_run_test_with_name(suite, test_ir_term_doc_enum, ir,
                           "test_add_indexes_term_doc_enum");
    tst_run_test_with_name(suite, test_ir_term_vectors, ir,
                           "test_add_indexes_term_vectors");
    tst_run_test_with_name(suite, test_ir_mtdpe, ir,
                           "test_add_indexes_multiple_term_doc_pos_enum");

    tst_run_test_with_name(suite, test_ir_norms, &add_indexes_reader_type,
                           "test_add_indexes_norms");
    tst_run_test_with_name(suite, test_ir_delete, &add_indexes_reader_type,
                           "test_add_indexes_reader_delete");

    frt_ir_close(ir);
    reader_test_env_destroy(rte);

    /* Other FrtIndexReader Tests */
    tst_run_test_with_name(suite, test_ir_read_while_optimizing, store,
                           "test_ir_read_while_optimizing_in_ram");

    fs_store = frt_open_fs_store(TEST_DIR);
    tst_run_test_with_name(suite, test_ir_read_while_optimizing, fs_store,
                           "test_ir_read_while_optimizing_on_disk");
    fs_store->clear_all(fs_store);
    frt_store_deref(fs_store);

    tst_run_test(suite, test_ir_multivalue_fields, store);

    frt_store_deref(store);
    return suite;
}

#include "frt_store.h"
#include "frt_index.h"
#include "testhelper.h"
#include "test.h"

void test_compound_reader(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    char *p;
    FrtOutStream *os = store->new_output(store, "cfile");
    FrtInStream *is1;
    FrtInStream *is2;
    FrtStore *c_reader;
    frt_os_write_vint(os, 2);
    frt_os_write_u64(os, 29);
    frt_os_write_string(os, "file1");
    frt_os_write_u64(os, 33);
    frt_os_write_string(os, "file2");
    frt_os_write_u32(os, 20);
    frt_os_write_string(os, "this is file 2");
    frt_os_close(os);

    c_reader = frt_open_cmpd_store(store, "cfile");
    Aiequal(4, c_reader->length(c_reader, "file1"));
    Aiequal(15, c_reader->length(c_reader, "file2"));
    is1 = c_reader->open_input(c_reader, "file1");
    is2 = c_reader->open_input(c_reader, "file2");
    Aiequal(20, frt_is_read_u32(is1));
    Asequal("this is file 2", p = frt_is_read_string(is2)); free(p);
    frt_is_close(is1);
    frt_is_close(is2);
    frt_store_deref(c_reader);
}

void test_compound_writer(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    char *p;
    FrtOutStream *os1 = store->new_output(store, "file1");
    FrtOutStream *os2 = store->new_output(store, "file2");
    FrtCompoundWriter *cw;
    FrtInStream *is;

    frt_os_write_u32(os1, 20);
    frt_os_write_string(os2,"this is file2");
    frt_os_close(os1);
    frt_os_close(os2);
    cw = frt_open_cw(store, (char *)"cfile");
    frt_cw_add_file(cw, (char *)"file1");
    frt_cw_add_file(cw, (char *)"file2");
    frt_cw_close(cw);

    is = store->open_input(store, "cfile");
    Aiequal(2, frt_is_read_vint(is));
    Aiequal(29, frt_is_read_u64(is));
    Asequal("file1", p = frt_is_read_string(is)); free(p);
    Aiequal(33, frt_is_read_u64(is));
    Asequal("file2", p = frt_is_read_string(is)); free(p);
    Aiequal(20, frt_is_read_u32(is));
    Asequal("this is file2", p = frt_is_read_string(is)); free(p);

    frt_is_close(is);
}

void test_compound_io(TestCase *tc, void *data)
{
    FrtStore *c_reader;
    FrtInStream *is1, *is2, *is3;
    FrtStore *store = (FrtStore *)data;
    FrtCompoundWriter *cw;
    char *p;
    FrtOutStream *os1 = store->new_output(store, "file1");
    FrtOutStream *os2 = store->new_output(store, "file2");
    FrtOutStream *os3 = store->new_output(store, "file3");
    char long_string[10000];
    const char *short_string = "this is a short string";
    int slen = (int)strlen(short_string);
    int i;

    for (i = 0; i < 20; i++) {
        frt_os_write_u32(os1, rand()%10000);
    }

    for (i = 0; i < 10000 - slen; i += slen) {
        sprintf(long_string + i, "%s", short_string);
    }
    long_string[i] = 0;
    frt_os_write_string(os2, long_string);
    frt_os_write_string(os3, short_string);
    frt_os_close(os1);
    frt_os_close(os2);
    frt_os_close(os3);
    cw = frt_open_cw(store, (char *)"cfile");
    frt_cw_add_file(cw, (char *)"file1");
    frt_cw_add_file(cw, (char *)"file2");
    frt_cw_add_file(cw, (char *)"file3");
    frt_cw_close(cw);

    c_reader = frt_open_cmpd_store(store, "cfile");
    is1 = c_reader->open_input(c_reader, "file1");
    for (i = 0; i < 20; i++) {
        Assert(frt_is_read_u32(is1) < 10000, "should be a rand between 0 and 10000");
    }
    frt_is_close(is1);
    is2 = c_reader->open_input(c_reader, "file2");
    Asequal(long_string, p = frt_is_read_string(is2)); free(p);
    frt_is_close(is2);
    is3 = c_reader->open_input(c_reader, "file3");
    Asequal(short_string, p = frt_is_read_string(is3)); free(p);
    frt_is_close(is3);

    frt_store_deref(c_reader);
}

#define MAX_TEST_WORDS 50
#define TEST_FILE_CNT 100

void test_compound_io_many_files(TestCase *tc, void *data)
{
    static const int MAGIC = 250777;

    FrtStore *store = (FrtStore *)data;
    char buf[MAX_TEST_WORDS * (TEST_WORD_LIST_MAX_LEN + 1)];
    char *str;
    int i;
    FrtOutStream *os;
    FrtInStream *is;
    FrtCompoundWriter *cw;
    FrtStore *c_reader;

    cw = frt_open_cw(store, (char *)"_.cfs");
    for (i = 0; i < TEST_FILE_CNT; i++) {
        sprintf(buf, "_%d.txt", i);
        frt_cw_add_file(cw, buf);
        os = store->new_output(store, buf);
        frt_os_write_string(os, make_random_string(buf, MAX_TEST_WORDS));
        frt_os_write_vint(os, MAGIC);
        frt_os_close(os);
    }
    frt_cw_close(cw);

    c_reader = frt_open_cmpd_store(store, "_.cfs");
    for (i = 0; i < TEST_FILE_CNT; i++) {
        sprintf(buf, "_%d.txt", i);
        is = c_reader->open_input(c_reader, buf);
        str = frt_is_read_string(is);

        free(str);
        Aiequal(MAGIC, frt_is_read_vint(is));
        Aiequal(frt_is_length(is), frt_is_pos(is));
        frt_is_close(is);
    }
    frt_store_deref(c_reader);
}

TestSuite *ts_compound_io(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store();

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_compound_reader, store);
    tst_run_test(suite, test_compound_writer, store);
    tst_run_test(suite, test_compound_io, store);
    tst_run_test(suite, test_compound_io_many_files, store);

    frt_store_deref(store);

    return suite;
}

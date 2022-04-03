#include "frt_store.h"
#include "test_store.h"
#include "test.h"

/**
 * Test ramo_write_to which basically writes the contents of a RAM output
 * stream to another output stream, usually a FileSystem output stream.
 */
void test_write_to(TestCase *tc, void *data)
{
    int i;
    char *tmp;
    FrtStore *ram_store = frt_open_ram_store(NULL);
    FrtStore *fs_store = frt_open_fs_store("./test/testdir/store");
    char str[18] = "³³ øãíøäÄ";
    char buf[18000] = "";
    FrtOutStream *ostream, *fs_ostream;
    FrtInStream *istream;
    (void)data;/* suppress unused parameter warning */

    fs_store->clear(fs_store);

    for (i = 0; i < 1000; i++) {
        strcat(buf, str);
    }

    ostream = ram_store->new_output(ram_store, "_rw_funny_string.cfs");
    frt_os_write_string(ostream, str);
    frt_os_write_string(ostream, buf);

    fs_ostream = fs_store->new_output(fs_store, "_rw_funny_string.cfs");
    frt_ramo_write_to(ostream, fs_ostream);

    frt_os_close(ostream);
    frt_os_close(fs_ostream);

    istream = fs_store->open_input(fs_store, "_rw_funny_string.cfs");
    Asequal(str, tmp = frt_is_read_string(istream));
    free(tmp);
    Asequal(buf, tmp = frt_is_read_string(istream));
    free(tmp);
    frt_is_close(istream);

    Aiequal(17021, fs_store->length(fs_store, "_rw_funny_string.cfs"));
    frt_store_deref(ram_store);
    ram_store = frt_open_ram_store_and_copy(NULL, fs_store, false);

    istream = ram_store->open_input(ram_store, "_rw_funny_string.cfs");
    Asequal(str, tmp = frt_is_read_string(istream));
    free(tmp);
    Asequal(buf, tmp = frt_is_read_string(istream));
    free(tmp);
    frt_is_close(istream);
    Aiequal(17021, ram_store->length(ram_store, "_rw_funny_string.cfs"));

    fs_store->clear_all(fs_store);
    frt_store_deref(fs_store);
    frt_store_deref(ram_store);
}

/**
 * Create the RAMStore test suite
 */
TestSuite *ts_ram_store(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);

    suite = ADD_SUITE(suite);

    create_test_store_suite(suite, store);

    tst_run_test(suite, test_write_to, NULL);

    frt_store_deref(store);

    return suite;
}

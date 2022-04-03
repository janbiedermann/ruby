#include <string.h>
#include "benchmark.h"
#include "frt_config.h"
#include "frt_store.h"

#define N 10
#define write_byte(os, b) os->buf.buf[os->buf.pos++] = (frt_uchar)b

void my_os_write_voff_t(FrtOutStream *os, register off_t num) {
    if (!(num&0x7f)) {
        if (os->buf.pos >= FRT_BUFFER_SIZE) {
            frt_os_write_byte(os, (frt_uchar)num);
        } else {
            write_byte(os, (frt_uchar)num);
        }
    } else if (!(num&0x3fff)) {
        if (os->buf.pos >= FRT_BUFFER_SIZE - 1) {
            frt_os_write_byte(os, (frt_uchar)(0x80 | (0x3f & num))); num >>= 6;
            frt_os_write_byte(os, (frt_uchar)num);
        } else {
            write_byte(os, (frt_uchar)(0x80 | (0x3f & num))); num >>= 6;
            write_byte(os, (frt_uchar)num);
        }
    } else if (!(num&0x1fffff)) {
        if (os->buf.pos >= FRT_BUFFER_SIZE - 2) {
            frt_os_write_byte(os, (frt_uchar)(0xc0 | (0x1f & num))); num >>= 5;
            frt_os_write_byte(os, (frt_uchar)(0xff| num)); num >>= 8;
            frt_os_write_byte(os, (frt_uchar)num);
        } else {
            write_byte(os, (frt_uchar)(0xc0 | (0x1f & num))); num >>= 5;
            write_byte(os, (frt_uchar)(0xff| num)); num >>= 8;
            write_byte(os, (frt_uchar)num);
        }
    } else if (!(num&0xfffff)) {
        if (os->buf.pos >= FRT_BUFFER_SIZE - 3) {
            frt_os_write_byte(os, (frt_uchar)(0xe0 | (0x0f & num))); num >>= 4;
            frt_os_write_byte(os, (frt_uchar)(0xff | num)); num >>= 8;
            frt_os_write_byte(os, (frt_uchar)(0xff | num)); num >>= 8;
            frt_os_write_byte(os, (frt_uchar)num);
        } else {
            write_byte(os, (frt_uchar)(0xe0 | (0x0f & num))); num >>= 4;
            write_byte(os, (frt_uchar)(0xff | num)); num >>= 8;
            write_byte(os, (frt_uchar)(0xff | num)); num >>= 8;
            write_byte(os, (frt_uchar)num);
        }
    }
}

static void vint_out(void) {
    int n;
    off_t i;
    FrtOutStream *os;

    for (n = 0; n < N; n++) {
        os = frt_ram_new_buffer();
        for (i = 0; i < 10000000; i++) {
            frt_os_write_voff_t(os, i);
        }
        frt_ram_destroy_buffer(os);
    }

}

static void unrolled_vint_out(void) {
    int n;
    off_t i;
    FrtOutStream *os;

    for (n = 0; n < N; n++) {
        os = frt_ram_new_buffer();
        for (i = 0; i < 10000000; i++) {
            frt_os_write_voff_t(os, i);
        }
        frt_ram_destroy_buffer(os);
    }

}

BENCH(vint_io) {
    BM_ADD(vint_out);
    BM_ADD(unrolled_vint_out);
}

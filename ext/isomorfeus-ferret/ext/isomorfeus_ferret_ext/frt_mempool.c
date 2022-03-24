#include "frt_global.h"
#include "frt_mempool.h"
#include <string.h>

FrtMemoryPool *frt_mp_new_capa(int chuck_size, int init_buf_capa)
{
    FrtMemoryPool *mp = FRT_ALLOC(FrtMemoryPool);
    mp->chunk_size = chuck_size;
    mp->buf_capa = init_buf_capa;
    mp->buffers = FRT_ALLOC_N(char *, init_buf_capa);

    mp->buffers[0] = mp->curr_buffer = (char *)frt_emalloc(mp->chunk_size);
    mp->buf_alloc = 1;
    mp->buf_pointer = 0;
    mp->pointer = 0;
    return mp;
}

FrtMemoryPool *frt_mp_new()
{
    return frt_mp_new_capa(FRT_MP_BUF_SIZE, FRT_MP_INIT_CAPA);
}

void *frt_mp_alloc(FrtMemoryPool *mp, int size)
{
    char *p;
    p = mp->curr_buffer + mp->pointer;
#if defined POSH_OS_SOLARIS || defined POSH_OS_SUNOS
    size = (((size - 1) >> 3) + 1) << 3;
#endif
    mp->pointer += size;

    if (mp->pointer > mp->chunk_size) {
        mp->buf_pointer++;
        if (mp->buf_pointer >= mp->buf_alloc) {
            mp->buf_alloc++;
            if (mp->buf_alloc >= mp->buf_capa) {
                mp->buf_capa <<= 1;
                FRT_REALLOC_N(mp->buffers, char *, mp->buf_capa);
            }
            mp->buffers[mp->buf_pointer] = (char *)frt_emalloc(mp->chunk_size);
        }
        p = mp->curr_buffer = mp->buffers[mp->buf_pointer];
        mp->pointer = size;
    }
    return p;
}

char *frt_mp_strdup(FrtMemoryPool *mp, const char *str)
{
    int len = strlen(str) + 1;
    return (char *)memcpy(frt_mp_alloc(mp, len), str, len);
}

char *frt_mp_strndup(FrtMemoryPool *mp, const char *str, int len)
{
    char *s = (char *)memcpy(frt_mp_alloc(mp, len + 1), str, len);
    s[len] = '\0';
    return s;
}

void *frt_mp_memdup(FrtMemoryPool *mp, const void *p, int len)
{
    return memcpy(frt_mp_alloc(mp, len), p, len);
}

int frt_mp_used(FrtMemoryPool *mp)
{
    return mp->buf_pointer * mp->chunk_size + mp->pointer;
}

void frt_mp_reset(FrtMemoryPool *mp)
{
    mp->buf_pointer = 0;
    mp->pointer = 0;
    mp->curr_buffer = mp->buffers[0];
}

void frt_mp_destroy(FrtMemoryPool *mp)
{
    int i;
    for (i = 0; i < mp->buf_alloc; i++) {
        free(mp->buffers[i]);
    }
    free(mp->buffers);
    free(mp);
}

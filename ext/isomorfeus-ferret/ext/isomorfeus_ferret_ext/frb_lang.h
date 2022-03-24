#ifndef FRT_LANG_H
#define FRT_LANG_H

#define RUBY_BINDINGS 1

#include <stdarg.h>
#include <ruby.h>

#undef close
#undef rename
#undef read

#define frt_emalloc xmalloc
#define frt_ecalloc(n) xcalloc(n, 1)
#define frt_erealloc xrealloc

#endif

ACLOCAL_AMFLAGS = -I m4

include_HEADERS = maps.h addr2line.h symtab.h 

lib_LTLIBRARIES = libsymtab.la libmaps.la libaddr2line.la

libsymtab_la_SOURCES = symtab.c
libsymtab_la_CFLAGS = @ELFUTILS_CFLAGS@
libsymtab_la_LDFLAGS = @ELFUTILS_LDFLAGS@

libmaps_la_SOURCES = maps.c
libmaps_la_LIBADD = libsymtab.la

libaddr2line_la_SOURCES = addr2line.c
libaddr2line_la_LIBADD = libmaps.la

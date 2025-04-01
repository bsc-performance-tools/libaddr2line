ACLOCAL_AMFLAGS = -I m4

include_HEADERS = maps.h addr2line.h symtab.h 

lib_LTLIBRARIES = 

if BUILD_LIBSYMTAB
lib_LTLIBRARIES += libsymtab.la 

libsymtab_la_SOURCES = symtab.c
libsymtab_la_CFLAGS = @ELFUTILS_CFLAGS@
libsymtab_la_LDFLAGS = @ELFUTILS_LDFLAGS@
endif

lib_LTLIBRARIES += libmaps.la 

libmaps_la_SOURCES = maps.c
if BUILD_LIBSYMTAB
libmaps_la_LIBADD = libsymtab.la
endif

lib_LTLIBRARIES += libaddr2line.la

libaddr2line_la_SOURCES = addr2line.c
libaddr2line_la_LIBADD = libmaps.la

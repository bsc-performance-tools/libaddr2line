ACLOCAL_AMFLAGS = -I m4

include_HEADERS = maps.h addr2line.h nm.h 

lib_LTLIBRARIES = libnm.la libmaps.la libaddr2line.la 

libnm_la_SOURCES = nm.c

libmaps_la_SOURCES = maps.c
libmaps_la_LIBADD = libnm.la

libaddr2line_la_SOURCES = addr2line.c
libaddr2line_la_LIBADD = libmaps.la

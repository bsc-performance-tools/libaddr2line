ACLOCAL_AMFLAGS = -I m4

include_HEADERS = maps.h addr2line.h nm.h 

lib_LTLIBRARIES = libmaps.la libaddr2line.la libnm.la

libmaps_la_SOURCES = maps.c

libaddr2line_la_SOURCES = addr2line.c
libaddr2line_la_LIBADD = libmaps.la

libnm_la_SOURCES = nm.c
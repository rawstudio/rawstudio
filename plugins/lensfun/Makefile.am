plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4\
	-funroll-loops

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LENSFUN_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = lensfun.la

libdir = $(datadir)/rawstudio/plugins/

lensfun_la_LIBADD = @PACKAGE_LIBS@ @LENSFUN_LIBS@ lensfun-avx.lo lensfun-sse2.lo lensfun-sse4.lo lensfun-c.lo
lensfun_la_LDFLAGS = -module -avoid-version
lensfun_la_SOURCES = lensfun-version.c lensfun-version.h
EXTRA_DIST = lensfun-avx.c lensfun-sse2.c lensfun-sse4.c lensfun.c

lensfun-c.lo: lensfun.c
	$(LTCOMPILE) -o lensfun-c.lo -c $(top_srcdir)/plugins/lensfun/lensfun.c

lensfun-sse2.lo: lensfun-sse2.c
if CAN_COMPILE_SSE2
SSE_FLAG=-msse2
else
SSE_FLAG=
endif
	$(LTCOMPILE) $(SSE_FLAG) -c $(top_srcdir)/plugins/lensfun/lensfun-sse2.c

lensfun-avx.lo: lensfun-avx.c
if CAN_COMPILE_AVX
AVX_FLAG=-mavx
else
AVX_FLAG=
endif
	$(LTCOMPILE) $(AVX_FLAG) -c $(top_srcdir)/plugins/lensfun/lensfun-avx.c

lensfun-sse4.lo: lensfun-sse4.c
if CAN_COMPILE_SSE4_1
SSE4_FLAG=-msse4.1
else
SSE4_FLAG=
endif
	$(LTCOMPILE) $(SSE4_FLAG) -c $(top_srcdir)/plugins/lensfun/lensfun-sse4.c

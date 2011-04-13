plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = resample.la

libdir = $(datadir)/rawstudio/plugins/

resample_la_LIBADD = @PACKAGE_LIBS@ resample-avx.lo resample-sse2.lo resample-sse4.lo resample-c.lo
resample_la_LDFLAGS = -module -avoid-version
resample_la_SOURCES =
 
EXTRA_DIST = resample-avx.c resample-sse2.c resample-sse4.c resample.c

resample-c.lo: resample.c
	$(LTCOMPILE) -o resample-c.o -c $(top_srcdir)/plugins/resample/resample.c

resample-sse2.lo: resample-sse2.c
if CAN_COMPILE_SSE2
SSE_FLAG=-msse2
else
SSE_FLAG=
endif
	$(LTCOMPILE) $(SSE_FLAG) -c $(top_srcdir)/plugins/resample/resample-sse2.c

resample-sse4.lo: resample-sse4.c
if CAN_COMPILE_SSE4_1
SSE4_FLAG=-msse4.1
else
SSE4_FLAG=
endif
	$(LTCOMPILE) $(SSE4_FLAG) -c $(top_srcdir)/plugins/resample/resample-sse4.c

resample-avx.lo: resample-avx.c
if CAN_COMPILE_AVX
AVX_FLAG=-mavx
else
AVX_FLAG=
endif
	$(LTCOMPILE) $(AVX_FLAG) -c $(top_srcdir)/plugins/resample/resample-avx.c

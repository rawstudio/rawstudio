plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS = -O4 -Wall -ffast-math -ffinite-math-only \
	-funsafe-math-optimizations -funroll-loops

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/ @PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/plugins/dcp/

lib_LTLIBRARIES = dcp.la

libdir = $(datadir)/rawstudio/plugins/

dcp_la_LIBADD = @PACKAGE_LIBS@ adobe-camera-raw-tone.lo dcp-sse2.lo dcp-sse4.lo dcp-avx.lo dcp-c.lo
dcp_la_LDFLAGS = -module -avoid-version
dcp_la_SOURCES = 
EXTRA_DIST = dcp.c dcp.h dcp-sse2.c dcp-sse4.c dcp-avx.c adobe-camera-raw-tone.c adobe-camera-raw-tone.h

adobe-camera-raw-tone.lo: adobe-camera-raw-tone.c adobe-camera-raw-tone.h
	$(LTCOMPILE) -c $(top_srcdir)/plugins/dcp/adobe-camera-raw-tone.c

dcp-c.lo: dcp.c adobe-camera-raw-tone.h
	$(LTCOMPILE) -o dcp-c.o -c $(top_srcdir)/plugins/dcp/dcp.c

if CAN_COMPILE_SSE4_1
SSE4_FLAG=-msse4.1
else
SSE4_FLAG=
endif

if CAN_COMPILE_SSE2
SSE2_FLAG=-msse2
else
SSE2_FLAG=
endif

if CAN_COMPILE_AVX
AVX_FLAG=-mavx
else
AVX_FLAG=
endif

dcp-sse2.lo: dcp-sse2.c dcp.h
	$(LTCOMPILE) $(SSE2_FLAG) -c $(top_srcdir)/plugins/dcp/dcp-sse2.c

dcp-sse4.lo: dcp-sse4.c dcp.h
	$(LTCOMPILE) $(SSE4_FLAG) -c $(top_srcdir)/plugins/dcp/dcp-sse4.c

dcp-avx.lo: dcp-avx.c dcp.h
	$(LTCOMPILE) $(AVX_FLAG) -c $(top_srcdir)/plugins/dcp/dcp-avx.c

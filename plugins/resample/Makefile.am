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

resample_la_LIBADD = @PACKAGE_LIBS@ resample-sse2.lo
resample_la_LDFLAGS = -module -avoid-version
EXTRA_DIST = resample-sse2.c

resample.lo: resample.c
	$(LTCOMPILE) -c $(top_srcdir)/plugins/resample/resample.c

resample-sse2.lo: resample-sse2.c
if CAN_COMPILE_SSE2
SSE_FLAG=-msse2
else
SSE_FLAG=
endif
	$(LTCOMPILE) $(SSE_FLAG) -c $(top_srcdir)/plugins/resample/resample-sse2.c


plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4\
	-funroll-loops

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = lensfun.la

libdir = $(datadir)/rawstudio/plugins/

lensfun_la_LIBADD = @PACKAGE_LIBS@ lensfun-sse2.lo
lensfun_la_LDFLAGS = -module -avoid-version
EXTRA_DIST = lensfun-sse2.c

lensfun.lo: lensfun.c
	$(LTCOMPILE) -c $(top_srcdir)/plugins/lensfun/lensfun.c

lensfun-sse2.lo: lensfun-sse2.c
if CAN_COMPILE_SSE2
SSE_FLAG=-msse2
else
SSE_FLAG=
endif
	$(LTCOMPILE) $(SSE_FLAG) -c $(top_srcdir)/plugins/lensfun/lensfun-sse2.c


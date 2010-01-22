plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS = -O4 -Wall -ffast-math -ffinite-math-only \
	-funsafe-math-optimizations

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -I../../librawstudio/ @PACKAGE_CFLAGS@

lib_LTLIBRARIES = dcp.la

libdir = $(datadir)/rawstudio/plugins/

dcp_la_LIBADD = @PACKAGE_LIBS@ dcp-sse.lo
dcp_la_LDFLAGS = -module -avoid-version

dcp_la_SOURCES = dcp.c dcp.h

dcp.lo: dcp.c
	$(LTCOMPILE) -DEXIT_CODE=0 -c dcp.c

dcp-sse.lo: dcp-sse.c
	$(LTCOMPILE) -DEXIT_CODE=1 -msse2 -c dcp-sse.c

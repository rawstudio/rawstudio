plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS = -O4 -Wall -ffast-math -funsafe-math-optimizations \
	-ffinite-math-only -g3

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -I../../librawstudio/ @PACKAGE_CFLAGS@

lib_LTLIBRARIES = dcp.la

libdir = $(datadir)/rawstudio/plugins/

dcp_la_LIBADD = @PACKAGE_LIBS@
dcp_la_LDFLAGS = -module -avoid-version
dcp_la_SOURCES = dcp.c

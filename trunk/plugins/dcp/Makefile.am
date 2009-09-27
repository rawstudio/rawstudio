plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall \
	-g3 \
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = dcp.la

libdir = $(datadir)/rawstudio/plugins/

dcp_la_LIBADD = @PACKAGE_LIBS@
dcp_la_LDFLAGS = -module -avoid-version
dcp_la_SOURCES = dcp.c

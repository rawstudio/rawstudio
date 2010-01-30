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

lib_LTLIBRARIES = demosaic.la

libdir = $(datadir)/rawstudio/plugins/

demosaic_la_LIBADD = @PACKAGE_LIBS@
demosaic_la_LDFLAGS = -module -avoid-version
demosaic_la_SOURCES = demosaic.c

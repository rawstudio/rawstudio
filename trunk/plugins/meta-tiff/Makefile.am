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

lib_LTLIBRARIES = meta_tiff.la

libdir = $(datadir)/rawstudio/plugins/

meta_tiff_la_LIBADD = @PACKAGE_LIBS@
meta_tiff_la_LDFLAGS = -module -avoid-version
meta_tiff_la_SOURCES =  tiff-meta.c

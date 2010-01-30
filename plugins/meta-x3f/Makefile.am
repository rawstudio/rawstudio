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

lib_LTLIBRARIES = meta_x3f.la

libdir = $(datadir)/rawstudio/plugins/

meta_x3f_la_LIBADD = @PACKAGE_LIBS@
meta_x3f_la_LDFLAGS = -module -avoid-version
meta_x3f_la_SOURCES =  x3f-meta.c

plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = sharpen.la

libdir = $(datadir)/rawstudio/plugins/

sharpen_la_LIBADD = @PACKAGE_LIBS@
sharpen_la_LDFLAGS = -module -avoid-version
sharpen_la_SOURCES = sharpen.c

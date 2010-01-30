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

lib_LTLIBRARIES = input_image16.la

libdir = $(datadir)/rawstudio/plugins/

input_image16_la_LIBADD = @PACKAGE_LIBS@
input_image16_la_LDFLAGS = -module -avoid-version
input_image16_la_SOURCES =  input-image16.c

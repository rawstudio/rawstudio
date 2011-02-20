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

lib_LTLIBRARIES = load_gdk.la

libdir = $(datadir)/rawstudio/plugins/

load_gdk_la_LIBADD = @PACKAGE_LIBS@
load_gdk_la_LDFLAGS = -module -avoid-version
load_gdk_la_SOURCES =  load-gdk.c exiv2-colorspace.cpp exiv2-colorspace.h

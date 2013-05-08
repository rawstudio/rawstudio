plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LCMS_CFLAGS@ @EXIV2_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = load_png.la

libdir = $(datadir)/rawstudio/plugins/

load_png_la_LIBADD = @PACKAGE_LIBS@ @LCMS_LIBS@ @EXIV2_LIBS@ @LIBJPEG@
load_png_la_LDFLAGS = -module -avoid-version
load_png_la_SOURCES =  load-png.c exiv2-colorspace.cpp exiv2-colorspace.h

plugindir = $(libdir)

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LIBCURL_CFLAGS@ @FLICKCURL_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = output_flickr.la

libdir = $(datadir)/rawstudio/plugins/

output_flickr_la_LIBADD = @PACKAGE_LIBS@ @LIBCURL_LIBS@ @FLICKCURL_LIBS@
output_flickr_la_LDFLAGS = -module -avoid-version -L/usr/lib -lflickcurl
output_flickr_la_SOURCES = output-flickr.c output-flickr.h

plugin_DATA = flickr-logo.svg

EXTRA_DIST = $(plugin_DATA)

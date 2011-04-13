plugindir = $(libdir)

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LIBCURL_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = output_picasa.la

libdir = $(datadir)/rawstudio/plugins/

output_picasa_la_LIBADD = @PACKAGE_LIBS@ @LIBCURL_LIBS@
output_picasa_la_LDFLAGS = -module -avoid-version -L/usr/lib
output_picasa_la_SOURCES = output-picasa.c rs-picasa-client.c rs-picasa-client.h

plugin_DATA = picasa-logo.svg

EXTRA_DIST = $(plugin_DATA)

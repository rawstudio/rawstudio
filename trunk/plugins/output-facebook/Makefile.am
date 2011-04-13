plugindir = $(libdir)

AM_CFLAGS =\
	-Wall\
	-Wstrict-aliasing=0\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LIBCURL_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = output_facebook.la

libdir = $(datadir)/rawstudio/plugins/

output_facebook_la_LIBADD = @PACKAGE_LIBS@ @LIBCURL_LIBS@
output_facebook_la_LDFLAGS = -module -avoid-version -L/usr/lib -lcurl
output_facebook_la_SOURCES = output-facebook.c output-facebook.h \
	rs-facebook-client.c rs-facebook-client.h \
	rs-facebook-client-param.c rs-facebook-client-param.h

plugin_DATA = facebook-logo.svg
EXTRA_DIST = $(plugin_DATA)

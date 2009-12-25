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

lib_LTLIBRARIES = output_facebook.la

libdir = $(datadir)/rawstudio/plugins/

output_facebook_la_LIBADD = @PACKAGE_LIBS@ 
output_facebook_la_LDFLAGS = -module -avoid-version -L/usr/lib -lflickcurl
output_facebook_la_SOURCES = output-facebook.c facebook.c

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

lib_LTLIBRARIES = resample.la

libdir = $(datadir)/rawstudio/plugins/

resample_la_LIBADD = @PACKAGE_LIBS@
resample_la_LDFLAGS = -module -avoid-version
resample_la_SOURCES = resample.c

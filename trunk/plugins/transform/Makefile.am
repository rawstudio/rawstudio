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

lib_LTLIBRARIES = transform.la

libdir = $(datadir)/rawstudio/plugins/

transform_la_LIBADD = @PACKAGE_LIBS@
transform_la_LDFLAGS = -module -avoid-version
transform_la_SOURCES =  transform.c

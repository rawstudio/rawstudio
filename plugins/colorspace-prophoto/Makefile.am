plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =	-Wall

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = colorspace_prophoto.la

libdir = $(datadir)/rawstudio/plugins/

colorspace_prophoto_la_LIBADD = @PACKAGE_LIBS@
colorspace_prophoto_la_LDFLAGS = -module -avoid-version
colorspace_prophoto_la_SOURCES = colorspace-prophoto.c

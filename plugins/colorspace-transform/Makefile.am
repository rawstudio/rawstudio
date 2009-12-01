plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-g3

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = colorspace_transform.la

libdir = $(datadir)/rawstudio/plugins/

colorspace_transform_la_LIBADD = @PACKAGE_LIBS@
colorspace_transform_la_LDFLAGS = -module -avoid-version
colorspace_transform_la_SOURCES = colorspace_transform.c

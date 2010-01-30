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

lib_LTLIBRARIES = exposure_mask.la

libdir = $(datadir)/rawstudio/plugins/

exposure_mask_la_LIBADD = @PACKAGE_LIBS@
exposure_mask_la_LDFLAGS = -module -avoid-version
exposure_mask_la_SOURCES = exposure-mask.c

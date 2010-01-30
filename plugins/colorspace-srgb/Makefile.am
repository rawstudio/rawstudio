plugindir = $(libdir)/rawstudio/plugins

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = colorspace_srgb.la

libdir = $(datadir)/rawstudio/plugins/

colorspace_srgb_la_LIBADD = @PACKAGE_LIBS@
colorspace_srgb_la_LDFLAGS = -module -avoid-version
colorspace_srgb_la_SOURCES = colorspace-srgb.c

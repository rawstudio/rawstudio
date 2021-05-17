AM_CFLAGS =	-Wall \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""@localedir@"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES = colorspace_prophoto.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

colorspace_prophoto_la_LIBADD = @PACKAGE_LIBS@
colorspace_prophoto_la_LDFLAGS = -module -avoid-version
colorspace_prophoto_la_SOURCES = colorspace-prophoto.c

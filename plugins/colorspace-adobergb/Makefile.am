AM_CFLAGS =	-Wall \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""@localedir@"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = colorspace_adobergb.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

colorspace_adobergb_la_LIBADD = @PACKAGE_LIBS@
colorspace_adobergb_la_LDFLAGS = -module -avoid-version
colorspace_adobergb_la_SOURCES = colorspace-adobergb.c

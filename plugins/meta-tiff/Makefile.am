AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""@localedir@"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = meta_tiff.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

meta_tiff_la_LIBADD = @PACKAGE_LIBS@
meta_tiff_la_LDFLAGS = -module -avoid-version
meta_tiff_la_SOURCES =  tiff-meta.c

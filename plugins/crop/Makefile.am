AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""@localedir@"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = crop.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

crop_la_LIBADD = @PACKAGE_LIBS@
crop_la_LDFLAGS = -module -avoid-version
crop_la_SOURCES =  crop.c

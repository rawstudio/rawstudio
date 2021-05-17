AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""@localedir@"\" \
	@PACKAGE_CFLAGS@ @LCMS_CFLAGS@ @EXIV2_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES = load_gdk.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

load_gdk_la_LIBADD = @PACKAGE_LIBS@ @LCMS_LIBS@ @EXIV2_LIBS@ @LIBJPEG@
load_gdk_la_LDFLAGS = -module -avoid-version
load_gdk_la_SOURCES =  load-gdk.c exiv2-colorspace.cpp exiv2-colorspace.h

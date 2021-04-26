AM_CFLAGS =\
	-Wall\
	-O4 \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""@localedir@"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES = cache.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

cache_la_LIBADD = @PACKAGE_LIBS@
cache_la_LDFLAGS = -module -avoid-version
cache_la_SOURCES = cache.c

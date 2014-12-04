AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = rotate.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

rotate_la_LIBADD = @PACKAGE_LIBS@
rotate_la_LDFLAGS = -module -avoid-version
rotate_la_SOURCES = rotate.c

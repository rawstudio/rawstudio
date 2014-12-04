AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = fuji_rotate.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

fuji_rotate_la_LIBADD = @PACKAGE_LIBS@
fuji_rotate_la_LDFLAGS = -module -avoid-version
fuji_rotate_la_SOURCES = fuji-rotate.c

AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = meta_ciff.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

meta_ciff_la_LIBADD = @PACKAGE_LIBS@
meta_ciff_la_LDFLAGS = -module -avoid-version
meta_ciff_la_SOURCES =  ciff-meta.c

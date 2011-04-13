plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @EXIV2_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = meta_exiv2.la

libdir = $(datadir)/rawstudio/plugins/

meta_exiv2_la_LIBADD = @PACKAGE_LIBS@ @EXIV2_LIBS@
meta_exiv2_la_LDFLAGS = -module -avoid-version
meta_exiv2_la_SOURCES =  exiv2-meta.c exiv2-metadata.cpp exiv2-metadata.h

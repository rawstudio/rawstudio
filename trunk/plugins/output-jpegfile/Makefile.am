plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = output_jpegfile.la

libdir = $(datadir)/rawstudio/plugins/

output_jpegfile_la_LIBADD = @PACKAGE_LIBS@ @LIBJPEG@
output_jpegfile_la_LDFLAGS = -module -avoid-version
output_jpegfile_la_SOURCES = output-jpegfile.c

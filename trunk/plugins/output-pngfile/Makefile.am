plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = output_pngfile.la

libdir = $(datadir)/rawstudio/plugins/

output_pngfile_la_LIBADD = @PACKAGE_LIBS@
output_pngfile_la_LDFLAGS = -module -avoid-version
output_pngfile_la_SOURCES = output-pngfile.c

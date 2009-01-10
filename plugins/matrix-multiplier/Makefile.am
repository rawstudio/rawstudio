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

lib_LTLIBRARIES = matrix_multiplier.la

libdir = $(datadir)/rawstudio/plugins/

matrix_multiplier_la_LIBADD = @PACKAGE_LIBS@
matrix_multiplier_la_LDFLAGS = -module -avoid-version
matrix_multiplier_la_SOURCES =  matrix-multiplier.c

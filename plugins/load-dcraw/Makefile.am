plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4\
	-DDCRAW_NOMAIN\
	-DDCRAW_NOLCMS\
	-DWITH_MMAP_HACK

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = load_dcraw.la

libdir = $(datadir)/rawstudio/plugins/

load_dcraw_la_LIBADD = @PACKAGE_LIBS@
load_dcraw_la_LDFLAGS = -module -avoid-version
load_dcraw_la_SOURCES =  dcrawloader.c dcraw.cc dcraw.h dcraw_api.cc dcraw_api.h mmap-hack.c mmap-hack.h

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

AM_CFLAGS += \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = meta_mrw.la

meta_mrw_la_LIBADD = @PACKAGE_LIBS@
meta_mrw_la_LDFLAGS = -module -avoid-version
meta_mrw_la_SOURCES =  mrw-meta.c

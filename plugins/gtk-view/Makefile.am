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

lib_LTLIBRARIES = gtk_view.la

libdir = $(datadir)/rawstudio/plugins/

gtk_view_la_LIBADD = @PACKAGE_LIBS@
gtk_view_la_LDFLAGS = -module -avoid-version
gtk_view_la_SOURCES =  gtk-view.c

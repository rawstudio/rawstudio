AM_CFLAGS =\
	-Wall\
	-O4\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = input_file.la

libdir = @RAWSTUDIO_PLUGINS_LIBS_DIR@

input_file_la_LIBADD = @PACKAGE_LIBS@
input_file_la_LDFLAGS = -module -avoid-version
input_file_la_SOURCES =  input-file.c

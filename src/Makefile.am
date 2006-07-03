## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

AM_CFLAGS =\
	-Wall\
	-O3\
	-DWITH_GCONF

bin_PROGRAMS = rawstudio

rawstudio_SOURCES = \
	rawstudio.c rawstudio.h \
	matrix.c matrix.h \
	gtk-helper.c gtk-helper.h \
	gtk-interface.c gtk-interface.h \
	conf_interface.c conf_interface.h \
	rs-cache.c rs-cache.h \
	drawingarea.c drawingarea.h \
	toolbox.c toolbox.h \
	rawfile.c rawfile.h \
	tiff-meta.c tiff-meta.h \
	ciff-meta.c ciff-meta.h \
	rs-image.c rs-image.h \
	dcraw_api.c dcraw_api.h \
	color.h \
	gettext.h

rawstudio_LDADD = @PACKAGE_LIBS@ dcraw_nomain.o $(INTLLIBS)

dcraw_nomain.o: dcraw.c
	$(COMPILE) -DDCRAW_NOMAIN -DNO_LCMS -o $@ -c $^


## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

AM_CFLAGS =\
	-Wall\
	-O4\
	-DWITH_GCONF\
	-DWITH_MMAP_HACK

bin_PROGRAMS = rawstudio

rawstudio_SOURCES = \
	rawstudio.c rawstudio.h \
	matrix.c matrix.h \
	gtk-helper.c gtk-helper.h \
	gtk-interface.c gtk-interface.h \
	gtk-save-dialog.c gtk-save-dialog.h \
	gtk-progress.c gtk-progress.h \
	conf_interface.c conf_interface.h \
	adobe-coeff.c adobe-coeff.h \
	rs-cache.c rs-cache.h \
	rs-batch.c rs-batch.h \
	drawingarea.c drawingarea.h \
	toolbox.c toolbox.h \
	rawfile.c rawfile.h \
	tiff-meta.c tiff-meta.h \
	ciff-meta.c ciff-meta.h \
	mrw-meta.c mrw-meta.h \
	x3f-meta.c x3f-meta.h \
	panasonic.c panasonic.h \
	rs-image.c rs-image.h \
	dcraw_api.c dcraw_api.h \
	rs-jpeg.c rs-jpeg.h \
	rs-tiff.c rs-tiff.h \
	color.h \
	gettext.h \
	rs-arch.h \
	arch-generic.c \
	arch-x86.c x86_cpu.h \
	filename.c filename.h \
	rs-crop.c rs-crop.h \
	rs-straighten.c rs-straighten.h \
	rs-render.c rs-render.h

rawstudio_LDADD = @PACKAGE_LIBS@ @LIBJPEG@ @LIBTIFF@ dcraw_nomain.o $(INTLLIBS)
dcraw_nomain.o: dcraw.c 
	$(COMPILE) -DDCRAW_NOMAIN -DNO_LCMS -o dcraw_nomain.o -c $(srcdir)/dcraw.c

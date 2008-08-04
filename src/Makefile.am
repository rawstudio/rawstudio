## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

AM_CFLAGS =\
	-Wall\
	-O4\
	-DWITH_GCONF\
	-DDCRAW_NOMAIN\
	-DDCRAW_NOLCMS\
	-DWITH_MMAP_HACK

AM_CXXFLAGS = $(AM_CFLAGS)

uidir = $(datadir)/rawstudio/
ui_DATA = ui.xml rawstudio.gtkrc

bin_PROGRAMS = rawstudio

EXTRA_DIST = \
	$(ui_DATA)

rawstudio_SOURCES = \
	rawstudio.c rawstudio.h \
	rs-utils.c rs-utils.h \
	rs-job.c rs-job.h \
	rs-actions.c rs-actions.h \
	rs-math.c rs-math.h \
	rs-preload.c rs-preload.h \
	gtk-helper.c gtk-helper.h \
	gtk-interface.c gtk-interface.h \
	gtk-save-dialog.c gtk-save-dialog.h \
	gtk-progress.c gtk-progress.h \
	conf_interface.c conf_interface.h \
	adobe-coeff.c adobe-coeff.h \
	rs-cms.c rs-cms.h \
	rs-cache.c rs-cache.h \
	rs-batch.c rs-batch.h \
	toolbox.c toolbox.h \
	rawfile.c rawfile.h \
	mmap-hack.c mmap-hack.h \
	tiff-meta.c tiff-meta.h \
	ciff-meta.c ciff-meta.h \
	mrw-meta.c mrw-meta.h \
	x3f-meta.c x3f-meta.h \
	rs-image.c rs-image.h \
	rs-photo.c rs-photo.h \
	dcraw_api.cc dcraw_api.h \
	dcraw.cc dcraw.h \
	rs-jpeg.c rs-jpeg.h \
	rs-tiff.c rs-tiff.h \
	color.h \
	gettext.h \
	rs-arch.h \
	arch-generic.c \
	arch-x86.c x86_cpu.h \
	filename.c filename.h \
	rs-spline.c  rs-spline.h \
	rs-store.c rs-store.h \
	rs-color-transform.c rs-color-transform.h \
	rs-preview-widget.c rs-preview-widget.h \
	rs-histogram.c rs-histogram.h \
	eog-pixbuf-cell-renderer.c eog-pixbuf-cell-renderer.h \
	rs-curve.c rs-curve.h \
	rs-pixbuf.c rs-pixbuf.h \
	rs-external-editor.c rs-external-editor.h \
	rs-dir-selector.c rs-dir-selector.h \
	rs-exif.cc rs-exif.h

rawstudio_LDADD = @PACKAGE_LIBS@ @LIBJPEG@ @LIBTIFF@ $(INTLLIBS)

AM_CFLAGS = -Wall -Werror -fno-strict-aliasing

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DWITH_GCONF \
	$(GTK_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

library_includedir=$(includedir)/@PACKAGE@-@VERSION@/
library_include_HEADERS = rawstudio.h rs-types.h rs-macros.h \
	rs-debug.h \
	rs-io-job.h \
	rs-io-job-checksum.h \
	rs-io-job-metadata.h \
	rs-io-job-prefetch.h \
	rs-io-job-tagging.h \
	rs-io.h \
	rs-plugin.h \
	rs-rawfile.h \
	rs-exif.h \
	rs-1d-function.h \
	rs-icc-profile.h \
	rs-color-space.h \
	rs-color-space-icc.h \
	rs-color-space-selector.h \
	rs-image.h \
	rs-image16.h \
	rs-lens.h \
	rs-lens-db.h \
	rs-lens-db-editor.h \
	rs-lens-fix.h \
	rs-library.h\
	rs-metadata.h \
	rs-filetypes.h \
	rs-filter.h \
	rs-filter-param.h \
	rs-filter-request.h \
	rs-filter-response.h \
	rs-output.h \
	rs-plugin-manager.h \
	rs-job-queue.h \
	rs-utils.h \
	rs-math.h \
	rs-color.h \
	rs-settings.h \
	rs-spline.h \
	rs-curve.h \
	rs-stock.h \
	rs-tiff.h \
	rs-tiff-ifd.h \
	rs-tiff-ifd-entry.h \
	rs-huesat-map.h \
	rs-dcp-file.h \
	rs-profile-factory.h \
	rs-profile-selector.h \
	rs-profile-camera.h \
	rs-gui-functions.h \
	x86-cpu.h

lib_LTLIBRARIES = librawstudio-1.1.la
librawstudio_1_1_la_SOURCES = \
	rs-debug.c rs-debug.h \
	rs-io-job.c rs-io-job.h \
	rs-io-job-checksum.c rs-io-job-checksum.h \
	rs-io-job-metadata.c rs-io-job-metadata.h \
	rs-io-job-prefetch.c rs-io-job-prefetch.h \
	rs-io-job-tagging.c rs-io-job-tagging.h \
	rs-io.c rs-io.h \
	rs-plugin.c rs-plugin.h \
	rs-rawfile.c rs-rawfile.h \
	rs-exif.cc rs-exif.h \
	rs-1d-function.c rs-1d-function.h \
	rs-icc-profile.c rs-icc-profile.h \
	rs-color-space.c rs-color-space.h \
	rs-color-space-icc.c rs-color-space-icc.h \
	rs-image.c rs-image.h \
	rs-image16.c rs-image16.h \
	rs-lens.c rs-lens.h \
	rs-lens-db.c rs-lens-db.h \
	rs-lens-db-editor.c rs-lens-db-editor.h \
	rs-lens-fix.c rs-lens-fix.h \
	rs-metadata.c rs-metadata.h \
	rs-filetypes.c rs-filetypes.h \
	rs-filter.c rs-filter.h \
	rs-filter-param.c rs-filter-param.h \
	rs-filter-request.c rs-filter-request.h \
	rs-filter-response.c rs-filter-response.h \
	rs-library.c rs-library.h\
	rs-output.c rs-output.h \
	rs-plugin-manager.c rs-plugin-manager.h \
	rs-job-queue.c rs-job-queue.h \
	rs-utils.c rs-utils.h \
	rs-math.c rs-math.h \
	rs-color.c rs-color.h \
	rs-settings.c rs-settings.h \
	rs-spline.c rs-spline.h \
	rs-curve.c rs-curve.h \
	rs-tiff.c rs-tiff.h \
	rs-tiff-ifd.c rs-tiff-ifd.h \
	rs-tiff-ifd-entry.c rs-tiff-ifd-entry.h \
	rs-huesat-map.c rs-huesat-map.h \
	rs-dcp-file.c rs-dcp-file.h \
	rs-profile-factory.c rs-profile-factory.h rs-profile-factory-model.h \
	rs-profile-selector.c rs-profile-selector.h \
	rs-profile-camera.c rs-profile-camera.h \
	rs-color-space-selector.c rs-color-space-selector.h \
	conf_interface.c conf_interface.h \
	rs-gui-functions.c rs-gui-functions.h \
	rs-stock.c rs-stock.h

librawstudio_1_1_la_LIBADD = @PACKAGE_LIBS@ @LIBJPEG@ @LIBTIFF@ $(INTLLIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rawstudio-1.1.pc

sharedir = $(datadir)/rawstudio/
share_DATA = lens_fix.xml

EXTRA_DIST = \
	$(share_DATA)

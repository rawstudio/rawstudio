AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

library_includedir=$(includedir)/@PACKAGE@-@VERSION@/
library_include_HEADERS = rawstudio.h rs-types.h rs-macros.h \
	rs-plugin.h \
	rs-rawfile.h \
	rs-image.h \
	rs-image16.h \
	rs-metadata.h \
	rs-filetypes.h \
	rs-filter.h \
	rs-plugin-manager.h \
	rs-utils.h \
	rs-math.h \
	rs-settings.h \
	rs-adobe-coeff.h \
	rs-color-transform.h \
	rs-spline.h \
	rs-curve.h \
	rs-stock.h \
	null.h

lib_LTLIBRARIES = librawstudio-1.1.la
librawstudio_1_1_la_SOURCES = \
	rs-plugin.c rs-plugin.h \
	rs-rawfile.c rs-rawfile.h \
	rs-image.c rs-image.h \
	rs-image16.c rs-image16.h \
	rs-metadata.c rs-metadata.h \
	rs-filetypes.c rs-filetypes.h \
	rs-filter.c rs-filter.h \
	rs-plugin-manager.c rs-plugin-manager.h \
	rs-utils.c rs-utils.h \
	rs-math.c rs-math.h \
	rs-settings.c rs-settings.h \
	rs-adobe-coeff.c rs-adobe-coeff.h \
	rs-color-transform.c rs-color-transform.h \
	rs-spline.c rs-spline.h \
	rs-curve.c rs-curve.h \
	rs-stock.c rs-stock.h \
	null.c null.h

librawstudio_1_1_la_LIBADD = @PACKAGE_LIBS@ @LIBJPEG@ @LIBTIFF@ $(INTLLIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rawstudio-1.1.pc

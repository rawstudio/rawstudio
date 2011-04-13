plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall \
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ @LCMS_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = colorspace_transform.la

libdir = $(datadir)/rawstudio/plugins/

colorspace_transform_la_LIBADD = @PACKAGE_LIBS@ @LCMS_LIBS@ colorspace_transform_avx.lo colorspace_transform_sse2.lo rs-cmm.lo colorspace_transform-c.lo
colorspace_transform_la_LDFLAGS = -module -avoid-version
colorspace_transform_la_SOURCES = 

EXTRA_DIST = colorspace_transform.c rs-cmm.c rs-cmm.h colorspace_transform_avx.lo colorspace_transform_sse2.c colorspace_transform.h

colorspace_transform-c.lo: colorspace_transform.c colorspace_transform.h
	$(LTCOMPILE) -o colorspace_transform-c.o -c $(top_srcdir)/plugins/colorspace-transform/colorspace_transform.c

rs-cmm.lo: rs-cmm.c rs-cmm.h
	$(LTCOMPILE) -c $(top_srcdir)/plugins/colorspace-transform/rs-cmm.c

colorspace_transform_sse2.lo: colorspace_transform_sse2.c colorspace_transform.h
if CAN_COMPILE_SSE2
SSE_FLAG=-msse2
else
SSE_FLAG=
endif
	$(LTCOMPILE) $(SSE_FLAG) -c $(top_srcdir)/plugins/colorspace-transform/colorspace_transform_sse2.c

colorspace_transform_avx.lo: colorspace_transform_avx.c colorspace_transform.h
if CAN_COMPILE_AVX
AVX_FLAG=-mavx
else
AVX_FLAG=
endif
	$(LTCOMPILE) $(AVX_FLAG) -c $(top_srcdir)/plugins/colorspace-transform/colorspace_transform_avx.c

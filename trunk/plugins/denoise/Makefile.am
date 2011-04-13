plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall \
	-g3 \
	-O4

AM_CXXFLAGS = $(AM_CFLAGS) @FFTW3F_CFLAGS@

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ -ffast-math \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = denoise.la

libdir = $(datadir)/rawstudio/plugins/

denoise_la_LIBADD = @PACKAGE_LIBS@ @FFTW3F_LIBS@
denoise_la_LDFLAGS = -module -avoid-version
denoise_la_SOURCES = denoise.c \
	complexblock.cpp complexblock.h \
	complexfilter.cpp complexfilter.h \
	complexfilter-x86.cpp \
	denoiseinterface.h \
	denoisethread.cpp denoisethread.h \
	fftdenoiser.cpp fftdenoiser.h \
	fftdenoiseryuv.cpp fftdenoiseryuv.h \
	fftwindow.cpp fftwindow.h \
	floatimageplane.cpp floatimageplane.h \
	floatplanarimage.cpp floatplanarimage-x86.cpp floatplanarimage.h \
	jobqueue.cpp jobqueue.h \
	planarimageslice.cpp planarimageslice.h

plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall \
	-g3 \
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = denoise.la

libdir = $(datadir)/rawstudio/plugins/

denoise_la_LIBADD = @PACKAGE_LIBS@
denoise_la_LDFLAGS = -module -avoid-version
denoise_la_SOURCES = denoise.c \
	complexblock.cpp \
	complexfilter.cpp \
  complexfilter-x86.cpp \
	denoisethread.cpp \
	fftdenoiser.cpp \
	fftdenoiseryuv.cpp\
	fftwindow.cpp \
	floatimageplane.cpp \
	floatplanarimage.cpp \
	jobqueue.cpp \
	planarimageslice.cpp

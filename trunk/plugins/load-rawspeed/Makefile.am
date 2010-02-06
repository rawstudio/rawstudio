plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O3

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/plugins/load-rawspeed/rawspeed/
 
lib_LTLIBRARIES = load_rawspeed.la

libdir = $(datadir)/rawstudio/plugins/

load_rawspeed_la_LIBADD = @PACKAGE_LIBS@
load_rawspeed_la_LDFLAGS = -module -avoid-version
load_rawspeed_la_SOURCES =  rawstudio-plugin-api.cpp \
	rawstudio-plugin.c \
	rawspeed/ArwDecoder.cpp \
	rawspeed/BitPumpJPEG.cpp \
	rawspeed/BitPumpMSB.cpp \
	rawspeed/BitPumpPlain.cpp \
	rawspeed/BlackArea.cpp \
	rawspeed/ByteStream.cpp \
	rawspeed/Camera.cpp \
	rawspeed/CameraMetaData.cpp \
	rawspeed/CameraMetadataException.cpp \
	rawspeed/ColorFilterArray.cpp \
	rawspeed/Common.cpp \
	rawspeed/Cr2Decoder.cpp \
	rawspeed/DngDecoder.cpp \
	rawspeed/DngDecoderSlices.cpp \
	rawspeed/FileIOException.cpp \
	rawspeed/FileMap.cpp \
	rawspeed/FileReader.cpp \
	rawspeed/IOException.cpp \
	rawspeed/LJpegDecompressor.cpp \
	rawspeed/LJpegPlain.cpp \
	rawspeed/NefDecoder.cpp \
	rawspeed/NikonDecompressor.cpp \
	rawspeed/OrfDecoder.cpp \
	rawspeed/PefDecoder.cpp \
	rawspeed/PentaxDecompressor.cpp \
	rawspeed/RawDecoder.cpp \
	rawspeed/RawDecoderException.cpp \
	rawspeed/RawImage.cpp \
	rawspeed/Rw2Decoder.cpp \
	rawspeed/StdAfx.cpp \
	rawspeed/TiffEntryBE.cpp \
	rawspeed/TiffEntry.cpp \
	rawspeed/TiffIFDBE.cpp \
	rawspeed/TiffIFD.cpp \
	rawspeed/TiffParser.cpp \
	rawspeed/TiffParserException.cpp \
	rawspeed/TiffParserHeaderless.cpp \
	rawspeed/TiffParserOlympus.cpp

rawspeeddir = $(datadir)/rawspeed
rawspeed_DATA = data/cameras.xml
EXTRA_DIST = $(rawspeed_DATA)
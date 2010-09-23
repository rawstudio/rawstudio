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

BUILT_SOURCES = $(top_srcdir)/plugins/load-rawspeed/rawspeed/StdAfx.h.gch
$(top_srcdir)/plugins/load-rawspeed/rawspeed/StdAfx.h.gch: $(top_srcdir)/plugins/load-rawspeed/rawspeed/StdAfx.h
	$(CXXCOMPILE) -c $<  

clean-local:
	-rm -f rawspeed/StdAfx.h.gch

lib_LTLIBRARIES = load_rawspeed.la

libdir = $(datadir)/rawstudio/plugins/

load_rawspeed_la_LIBADD = @PACKAGE_LIBS@
load_rawspeed_la_LDFLAGS = -module -avoid-version
load_rawspeed_la_SOURCES =  rawstudio-plugin-api.cpp rawstudio-plugin-api.h \
	rawstudio-plugin.c \
	rawspeed/ArwDecoder.cpp rawspeed/ArwDecoder.h \
	rawspeed/BitPumpJPEG.cpp rawspeed/BitPumpJPEG.h \
	rawspeed/BitPumpMSB.cpp rawspeed/BitPumpMSB.h \
	rawspeed/BitPumpMSB32.cpp rawspeed/BitPumpMSB32.h \
	rawspeed/BitPumpPlain.cpp rawspeed/BitPumpPlain.h \
	rawspeed/BlackArea.cpp rawspeed/BlackArea.h \
	rawspeed/ByteStream.cpp rawspeed/ByteStream.h \
	rawspeed/ByteStreamSwap.cpp rawspeed/ByteStreamSwap.h \
	rawspeed/Camera.cpp rawspeed/Camera.h \
	rawspeed/CameraMetaData.cpp rawspeed/CameraMetaData.h \
	rawspeed/CameraMetadataException.cpp rawspeed/CameraMetadataException.h \
	rawspeed/ColorFilterArray.cpp rawspeed/ColorFilterArray.h \
	rawspeed/Common.cpp rawspeed/Common.h \
	rawspeed/Cr2Decoder.cpp rawspeed/Cr2Decoder.h \
	rawspeed/DngDecoder.cpp rawspeed/DngDecoder.h \
	rawspeed/DngDecoderSlices.cpp rawspeed/DngDecoderSlices.h \
	rawspeed/FileIOException.cpp rawspeed/FileIOException.h \
	rawspeed/FileMap.cpp rawspeed/FileMap.h \
	rawspeed/FileReader.cpp rawspeed/FileReader.h \
	rawspeed/IOException.cpp rawspeed/IOException.h \
	rawspeed/LJpegDecompressor.cpp rawspeed/LJpegDecompressor.h \
	rawspeed/LJpegPlain.cpp rawspeed/LJpegPlain.h \
	rawspeed/NefDecoder.cpp rawspeed/NefDecoder.h \
	rawspeed/NikonDecompressor.cpp rawspeed/NikonDecompressor.h \
	rawspeed/OrfDecoder.cpp rawspeed/OrfDecoder.h \
	rawspeed/PefDecoder.cpp rawspeed/PefDecoder.h \
	rawspeed/PentaxDecompressor.cpp rawspeed/PentaxDecompressor.h \
	rawspeed/Point.h \
	rawspeed/RawDecoder.cpp rawspeed/RawDecoder.h \
	rawspeed/RawDecoderException.cpp rawspeed/RawDecoderException.h \
	rawspeed/RawImage.cpp rawspeed/RawImage.h \
	rawspeed/Rw2Decoder.cpp rawspeed/Rw2Decoder.h \
	rawspeed/StdAfx.cpp rawspeed/StdAfx.h \
	rawspeed/TiffEntryBE.cpp rawspeed/TiffEntryBE.h \
	rawspeed/TiffEntry.cpp rawspeed/TiffEntry.h \
	rawspeed/TiffIFDBE.cpp rawspeed/TiffIFDBE.h \
	rawspeed/TiffIFD.cpp rawspeed/TiffIFD.h \
	rawspeed/TiffParser.cpp rawspeed/TiffParser.h \
	rawspeed/TiffParserException.cpp rawspeed/TiffParserException.h \
	rawspeed/TiffParserHeaderless.cpp rawspeed/TiffParserHeaderless.h \
	rawspeed/TiffParserOlympus.cpp rawspeed/TiffParserOlympus.h \
	rawspeed/TiffTag.h

rawspeeddir = $(datadir)/rawspeed
rawspeed_DATA = data/cameras.xml
EXTRA_DIST = $(rawspeed_DATA)
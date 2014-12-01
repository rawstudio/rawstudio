plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O3\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/plugins/load-rawspeed/rawspeed/RawSpeed

AM_CXXFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES = load_rawspeed.la

libdir = $(datadir)/rawstudio/plugins/

load_rawspeed_la_LIBADD = @PACKAGE_LIBS@ @LIBJPEG@
load_rawspeed_la_LDFLAGS = -module -avoid-version
load_rawspeed_la_SOURCES =  rawstudio-plugin-api.cpp rawstudio-plugin-api.h \
	rawstudio-plugin.c \
	rawspeed/RawSpeed/ArwDecoder.cpp rawspeed/RawSpeed/ArwDecoder.h \
	rawspeed/RawSpeed/BitPumpJPEG.cpp rawspeed/RawSpeed/BitPumpJPEG.h \
	rawspeed/RawSpeed/BitPumpMSB32.cpp rawspeed/RawSpeed/BitPumpMSB32.h \
	rawspeed/RawSpeed/BitPumpMSB.cpp rawspeed/RawSpeed/BitPumpMSB.h \
	rawspeed/RawSpeed/BitPumpPlain.cpp rawspeed/RawSpeed/BitPumpPlain.h \
	rawspeed/RawSpeed/BlackArea.cpp rawspeed/RawSpeed/BlackArea.h \
	rawspeed/RawSpeed/ByteStream.cpp rawspeed/RawSpeed/ByteStream.h \
	rawspeed/RawSpeed/ByteStreamSwap.cpp rawspeed/RawSpeed/ByteStreamSwap.h \
	rawspeed/RawSpeed/Camera.cpp rawspeed/RawSpeed/Camera.h \
	rawspeed/RawSpeed/CameraMetaData.cpp rawspeed/RawSpeed/CameraMetaData.h \
	rawspeed/RawSpeed/CameraMetadataException.cpp rawspeed/RawSpeed/CameraMetadataException.h \
	rawspeed/RawSpeed/CameraSensorInfo.cpp rawspeed/RawSpeed/CameraSensorInfo.h \
	rawspeed/RawSpeed/ColorFilterArray.cpp rawspeed/RawSpeed/ColorFilterArray.h \
	rawspeed/RawSpeed/Common.cpp rawspeed/RawSpeed/Common.h \
	rawspeed/RawSpeed/Cr2Decoder.cpp rawspeed/RawSpeed/Cr2Decoder.h \
	rawspeed/RawSpeed/DcrDecoder.cpp rawspeed/RawSpeed/DcrDecoder.h \
	rawspeed/RawSpeed/DngDecoder.cpp rawspeed/RawSpeed/DngDecoder.h \
	rawspeed/RawSpeed/DngDecoderSlices.cpp rawspeed/RawSpeed/DngDecoderSlices.h \
	rawspeed/RawSpeed/DngOpcodes.cpp rawspeed/RawSpeed/DngOpcodes.h \
	rawspeed/RawSpeed/ErfDecoder.cpp rawspeed/RawSpeed/ErfDecoder.h \
	rawspeed/RawSpeed/FileIOException.cpp rawspeed/RawSpeed/FileIOException.h \
	rawspeed/RawSpeed/FileMap.cpp rawspeed/RawSpeed/FileMap.h \
	rawspeed/RawSpeed/FileReader.cpp rawspeed/RawSpeed/FileReader.h \
	rawspeed/RawSpeed/FileWriter.cpp rawspeed/RawSpeed/FileWriter.h \
	rawspeed/RawSpeed/HasselbladDecompressor.cpp rawspeed/RawSpeed/HasselbladDecompressor.h \
	rawspeed/RawSpeed/IOException.cpp rawspeed/RawSpeed/IOException.h \
	rawspeed/RawSpeed/KdcDecoder.cpp rawspeed/RawSpeed/KdcDecoder.h \
	rawspeed/RawSpeed/LJpegDecompressor.cpp rawspeed/RawSpeed/LJpegDecompressor.h \
	rawspeed/RawSpeed/LJpegPlain.cpp rawspeed/RawSpeed/LJpegPlain.h \
	rawspeed/RawSpeed/MefDecoder.cpp rawspeed/RawSpeed/MefDecoder.h \
	rawspeed/RawSpeed/MosDecoder.cpp rawspeed/RawSpeed/MosDecoder.h \
	rawspeed/RawSpeed/MrwDecoder.cpp rawspeed/RawSpeed/MrwDecoder.h \
	rawspeed/RawSpeed/NefDecoder.cpp rawspeed/RawSpeed/NefDecoder.h \
	rawspeed/RawSpeed/NikonDecompressor.cpp rawspeed/RawSpeed/NikonDecompressor.h \
	rawspeed/RawSpeed/OrfDecoder.cpp rawspeed/RawSpeed/OrfDecoder.h \
	rawspeed/RawSpeed/PefDecoder.cpp rawspeed/RawSpeed/PefDecoder.h \
	rawspeed/RawSpeed/PentaxDecompressor.cpp rawspeed/RawSpeed/PentaxDecompressor.h \
	rawspeed/RawSpeed/Point.h \
	rawspeed/RawSpeed/pugixml.cpp \
	rawspeed/RawSpeed/RafDecoder.cpp rawspeed/RawSpeed/RafDecoder.h \
	rawspeed/RawSpeed/RawDecoder.cpp rawspeed/RawSpeed/RawDecoder.h \
	rawspeed/RawSpeed/RawDecoderException.cpp rawspeed/RawSpeed/RawDecoderException.h \
	rawspeed/RawSpeed/RawImage.cpp rawspeed/RawSpeed/RawImage.h \
	rawspeed/RawSpeed/RawImageDataFloat.cpp \
	rawspeed/RawSpeed/RawImageDataU16.cpp \
	rawspeed/RawSpeed/RawParser.cpp rawspeed/RawSpeed/RawParser.h \
	rawspeed/RawSpeed/RawSpeed-API.h \
	rawspeed/RawSpeed/Rw2Decoder.cpp rawspeed/RawSpeed/Rw2Decoder.h \
	rawspeed/RawSpeed/SrwDecoder.cpp rawspeed/RawSpeed/SrwDecoder.h \
	rawspeed/RawSpeed/StdAfx.cpp rawspeed/RawSpeed/StdAfx.h \
	rawspeed/RawSpeed/ThreefrDecoder.cpp rawspeed/RawSpeed/ThreefrDecoder.h \
	rawspeed/RawSpeed/TiffEntryBE.cpp rawspeed/RawSpeed/TiffEntryBE.h \
	rawspeed/RawSpeed/TiffEntry.cpp rawspeed/RawSpeed/TiffEntry.h \
	rawspeed/RawSpeed/TiffIFDBE.cpp rawspeed/RawSpeed/TiffIFDBE.h \
	rawspeed/RawSpeed/TiffIFD.cpp rawspeed/RawSpeed/TiffIFD.h \
	rawspeed/RawSpeed/TiffParser.cpp rawspeed/RawSpeed/TiffParser.h \
	rawspeed/RawSpeed/TiffParserException.cpp rawspeed/RawSpeed/TiffParserException.h \
	rawspeed/RawSpeed/TiffParserHeaderless.cpp rawspeed/RawSpeed/TiffParserHeaderless.h \
	rawspeed/RawSpeed/TiffParserOlympus.cpp rawspeed/RawSpeed/TiffParserOlympus.h \
	rawspeed/RawSpeed/TiffTag.h \
	rawspeed/RawSpeed/X3fDecoder.cpp rawspeed/RawSpeed/X3fDecoder.h \
	rawspeed/RawSpeed/X3fParser.cpp rawspeed/RawSpeed/X3fParser.h \
	rawspeed/RawSpeed/pugixml.hpp rawspeed/RawSpeed/pugiconfig.hpp

rawspeeddir = $(datadir)/rawspeed
rawspeed_DATA = rawspeed/data/cameras.xml
EXTRA_DIST = $(rawspeed_DATA)

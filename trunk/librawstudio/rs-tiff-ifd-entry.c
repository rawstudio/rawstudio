#include "rs-tiff-ifd-entry.h"
#include "rs-tiff.h"

G_DEFINE_TYPE (RSTiffIfdEntry, rs_tiff_ifd_entry, G_TYPE_OBJECT)

static const struct {
	gushort tag;
	const char *description;
} tiff_tags[] = {
	{ 0x00fe, "NewSubfileType" },
	{ 0x00ff, "SubfileType" },
	{ 0x0100, "ImageWidth" },
	{ 0x0101, "ImageLength" },
	{ 0x0102, "BitsPerSample" },
	{ 0x0103, "Compression" },
	{ 0x0106, "PhotometricInterpretation" },
	{ 0x0107, "Threshholding" },
	{ 0x0108, "CellWidth" },
	{ 0x0109, "CellLength" },
	{ 0x0100, "ImageWidth" },
	{ 0x010a, "FillOrder" },
	{ 0x010d, "DocumentName" },
	{ 0x010e, "ImageDescription" },
	{ 0x010f, "Make" },
	{ 0x0110, "Model" },
	{ 0x0111, "StripOffsets" },
	{ 0x0112, "Orientation" },
	{ 0x0115, "SamplesPerPixel" },
	{ 0x0116, "RowsPerStrip" },
	{ 0x0117, "StripByteCounts" },
	{ 0x0118, "MinSampleValue" },
	{ 0x0119, "MaxSampleValue" },
	{ 0x011a, "XResolution" },
	{ 0x011b, "YResolution" },
	{ 0x011c, "PlanarConfiguration" },
	{ 0x011d, "PageName" },
	{ 0x011e, "XPosition" },
	{ 0x011f, "YPosition" },
	{ 0x0120, "FreeOffsets" },
	{ 0x0121, "FreeByteCounts" },
	{ 0x0122, "GrayResponseUnit" },
	{ 0x0123, "GrayResponseCurve" },
	{ 0x0124, "T4Options" },
	{ 0x0125, "T6Options" },
	{ 0x0128, "ResolutionUnit" },
	{ 0x0129, "PageNumber" },
	{ 0x012d, "TransferFunction" },
	{ 0x0131, "Software" },
	{ 0x0132, "DateTime" },
	{ 0x013b, "Artist" },
	{ 0x013c, "HostComputer" },
	{ 0x013d, "Predictor" },
	{ 0x013e, "WhitePoint" },
	{ 0x013f, "PrimaryChromaticities" },
	{ 0x0140, "ColorMap" },
	{ 0x0141, "HalftoneHints" },
	{ 0x0142, "TileWidth" },
	{ 0x0143, "TileLength" },
	{ 0x0144, "TileOffsets" },
	{ 0x0145, "TileByteCounts" },
	{ 0x014c, "InkSet" },
	{ 0x014d, "InkNames" },
	{ 0x014e, "NumberOfInks" },
	{ 0x0200, "JPEGProc" },
	{ 0x0201, "JPEGInterchangeFormat" },
	{ 0x0202, "JPEGInterchangeFormatLength" },
	{ 0x0203, "JPEGRestartInterval" },
	{ 0x0205, "JPEGLosslessPredictors" },
	{ 0x0206, "JPEGPointTransforms" },
	{ 0x0207, "JPEGQTables" },
	{ 0x0208, "JPEGDCTables" },
	{ 0x0209, "JPEGACTables" },
	{ 0x0211, "YCbCrCoefficients" },
	{ 0x0212, "YCbCrSubSampling" },
	{ 0x0213, "YCbCrPositioning" },
	{ 0x0214, "ReferenceBlackWhite" },
	{ 0x0150, "DotRange" },
	{ 0x0151, "TargetPrinter" },
	{ 0x0152, "ExtraSamples" },
	{ 0x0153, "SampleFormat" },
	{ 0x0154, "SMinSampleValue" },
	{ 0x0155, "SMaxSampleValue" },
	{ 0x0156, "TransferRange" },
	{ 0x8298, "Copyright" },
	/* EXIF specifics */
	{ 0x8769, "Exif IFD Pointer" },
	{ 0x8825, "GPS Info IFD Pointer" },
	/* DNG tags */
	{ 0xc612, "DNGVersion" },
	{ 0xc613, "DNGBackwardVersion" },
	{ 0xc614, "UniqueCameraModel" },
	{ 0xc615, "LocalizedCameraModel" },
	{ 0xc616, "CFAPlaneColor" },
	{ 0xc617, "CFALayout" },
	{ 0xc618, "LinearizationTable" },
	{ 0xc619, "BlackLevelRepeatDim" },
	{ 0xc61a, "BlackLevel" },
	{ 0xc61b, "BlackLevelDeltaH" },
	{ 0xc61c, "BlackLevelDeltaV" },
	{ 0xc61d, "WhiteLevel" },
	{ 0xc61e, "DefaultScale" },
	{ 0xc61f, "DefaultCropOrigin" },
	{ 0xc620, "DefaultCropSize" },
	{ 0xc621, "ColorMatrix1" },
	{ 0xc622, "ColorMatrix2" },
	{ 0xc623, "CameraCalibration1" },
	{ 0xc624, "CameraCalibration2" },
	{ 0xc625, "ReductionMatrix1" },
	{ 0xc626, "ReductionMatrix2" },
	{ 0xc627, "AnalogBalance" },
	{ 0xc628, "AsShotNeutral" },
	{ 0xc629, "AsShotWhiteXY" },
	{ 0xc62a, "BaselineExposure" },
	{ 0xc62b, "BaselineNoise" },
	{ 0xc62c, "BaselineSharpness" },
	{ 0xc62d, "BayerGreenSplit" },
	{ 0xc62e, "LinearResponseLimit" },
	{ 0xc62f, "CameraSerialNumber" },
	{ 0xc630, "LensInfo" },
	{ 0xc631, "ChromaBlurRadius" },
	{ 0xc632, "AntiAliasStrength" },
	{ 0xc633, "ShadowScale" },
	{ 0xc634, "DNGPrivateData" },
	{ 0xc635, "MakerNoteSafety" },
	{ 0xc65a, "CalibrationIlluminant1" },
	{ 0xc65b, "CalibrationIlluminant2" },
	{ 0xc65c, "BestQualityScale" },
	{ 0xc65d, "RawDataUniqueID" },
	{ 0xc68b, "OriginalRawFileName" },
	{ 0xc68c, "OriginalRawFileData" },
	{ 0xc68d, "ActiveArea" },
	{ 0xc68e, "MaskedAreas" },
	{ 0xc68f, "AsShotICCProfile" },
	{ 0xc690, "AsShotPreProfileMatrix" },
	{ 0xc691, "CurrentICCProfile" },
	{ 0xc692, "CurrentPreProfileMatrix" },
	{ 0xc6bf, "ColorimetricReference" },
	{ 0xc6f3, "CameraCalibrationSignature" },
	{ 0xc6f4, "ProfileCalibrationSignature" },
	{ 0xc6f5, "ExtraCameraProfiles" },
	{ 0xc6f6, "AsShotProfileName" },
	{ 0xc6f7, "NoiseReductionApplied" },
	{ 0xc6f8, "ProfileName" },
	{ 0xc6f9, "ProfileHueSatMapDims" },
	{ 0xc6fa, "ProfileHueSatMapData1" },
	{ 0xc6fb, "ProfileHueSatMapData2" },
	{ 0xc6fc, "ProfileToneCurve" },
	{ 0xc6fd, "ProfileEmbedPolicy" },
	{ 0xc6fe, "ProfileCopyright" },
	{ 0xc714, "ForwardMatrix1" },
	{ 0xc715, "ForwardMatrix2" },
	{ 0xc716, "PreviewApplicationName" },
	{ 0xc717, "PreviewApplicationVersion" },
	{ 0xc718, "PreviewSettingsName" },
	{ 0xc719, "PreviewSettingsDigest" },
	{ 0xc71a, "PreviewColorSpace" },
	{ 0xc71b, "PreviewDateTime" },
	{ 0xc71c, "RawImageDigest" },
	{ 0xc71d, "OriginalRawFileDigest" },
	{ 0xc71e, "SubTileBlockSize" },
	{ 0xc71f, "RowInterleaveFactor" },
	{ 0xc725, "ProfileLookTableDims" },
	{ 0xc726, "ProfileLookTableData" },
	{ 0xc740, "OpcodeList1" },
	{ 0xc741, "OpcodeList2" },
	{ 0xc74e, "OpcodeList3" },
	{ 0xc761, "NoiseProfile" },
	{ 0x0, NULL }
};

static void
rs_tiff_ifd_entry_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
rs_tiff_ifd_entry_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
rs_tiff_ifd_entry_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_tiff_ifd_entry_parent_class)->dispose (object);
}

static void
rs_tiff_ifd_entry_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_tiff_ifd_entry_parent_class)->finalize (object);
}

static void
rs_tiff_ifd_entry_class_init(RSTiffIfdEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = rs_tiff_ifd_entry_get_property;
	object_class->set_property = rs_tiff_ifd_entry_set_property;
	object_class->dispose = rs_tiff_ifd_entry_dispose;
	object_class->finalize = rs_tiff_ifd_entry_finalize;
}

static void
rs_tiff_ifd_entry_init(RSTiffIfdEntry *self)
{
}

RSTiffIfdEntry *
rs_tiff_ifd_entry_new(RSTiff *tiff, guint offset)
{
	RSTiffIfdEntry *entry = g_object_new(RS_TYPE_TIFF_IFD_ENTRY, NULL);

	entry->tag = rs_tiff_get_ushort(tiff, offset+0);
	entry->type = rs_tiff_get_ushort(tiff, offset+2);
	entry->count = rs_tiff_get_uint(tiff, offset+4);
	entry->value_offset = rs_tiff_get_uint(tiff, offset+8);

	return entry;
}

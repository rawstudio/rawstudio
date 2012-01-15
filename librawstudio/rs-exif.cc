/*
 * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>,
 * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <iostream>
#include <iomanip>
#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include "rs-exif.h"
#include <assert.h>
#include "rs-library.h"

#ifndef EXIV2_TEST_VERSION
# define EXIV2_TEST_VERSION(major,minor,patch) \
	( EXIV2_VERSION >= EXIV2_MAKE_VERSION((major),(minor),(patch)) )
#endif

#if EXIV2_TEST_VERSION(0,17,0)
#include <exiv2/convert.hpp>
#endif

extern "C" {
#include <rawstudio.h>
#include "config.h"

/* This list is mainly just a copy of the list in UFRaw - thanks again Udi! */
const static gchar *tags_to_delete[] = {
	/* Original TIFF data is no longer interesting */
	"Exif.Image.Orientation",
    "Exif.Image.ImageWidth",
	"Exif.Image.ImageLength",
	"Exif.Image.BitsPerSample",
	"Exif.Image.Compression",
	"Exif.Image.PhotometricInterpretation",
	"Exif.Image.FillOrder",
	"Exif.Image.SamplesPerPixel",
	"Exif.Image.StripOffsets",
	"Exif.Image.RowsPerStrip",
	"Exif.Image.StripByteCounts",
	"Exif.Image.XResolution",
	"Exif.Image.YResolution",
	"Exif.Image.PlanarConfiguration",
	"Exif.Image.ResolutionUnit",

	/* Delete colorspace information - we add our own */
	"Exif.Photo.ColorSpace",
	"Exif.Iop.InteroperabilityIndex",
	"Exif.Iop.InteroperabilityVersion",

	/* Delete various MakerNote fields only applicable to the raw file */

	// Nikon thumbnail data
#if EXIV2_TEST_VERSION(0,13,0)
	"Exif.Nikon3.Preview",
#endif

#if EXIV2_TEST_VERSION(0,18,0)
	"Exif.Image.DNGPrivateData",
        "Exif.Nikon3.RawImageCenter",
#else
        "Exif.Nikon3.NEFThumbnailSize",
#endif

#if EXIV2_TEST_VERSION(0,17,91)
	"Exif.NikonPreview.JPEGInterchangeFormat",
#endif

#if EXIV2_TEST_VERSION(0,15,99)		/* Exiv2 0.16-pre1 */
	// Pentax thumbnail data
	"Exif.Pentax.PreviewResolution",
	"Exif.Pentax.PreviewLength",
	"Exif.Pentax.PreviewOffset",
#endif

	// Minolta thumbnail data
	"Exif.Minolta.Thumbnail",
	"Exif.Minolta.ThumbnailOffset",
	"Exif.Minolta.ThumbnailLength",

#if EXIV2_TEST_VERSION(0,13,0)
	// Olympus thumbnail data
	"Exif.Olympus.Thumbnail",
	"Exif.Olympus.ThumbnailOffset",
	"Exif.Olympus.ThumbnailLength",
#endif
	NULL
};

static void exif_data_init(RS_EXIF_DATA *exif_data);

static void
exif_data_init(RS_EXIF_DATA *exif_data)
{
	gint i = 0;
    Exiv2::ExifData::iterator pos;
	Exiv2::ExifData *data = (Exiv2::ExifData *) exif_data;

	/* Do some advertising while we're at it :) */
	(*data)["Exif.Image.Software"] = "Rawstudio " RAWSTUDIO_VERSION;
	(*data)["Exif.Image.ProcessingSoftware"] = "Rawstudio " RAWSTUDIO_VERSION;

	/* Delete all tags from tags_to_delete */
	while(tags_to_delete[i])
	{
		if ((pos=(*data).findKey(Exiv2::ExifKey(tags_to_delete[i]))) != (*data).end())
			(*data).erase(pos);
		i++;
	}
}

RS_EXIF_DATA *
rs_exif_load_from_file(const gchar *filename)
{
	RS_EXIF_DATA *exif_data;
	try
	{
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(filename);
		assert(image.get() != 0);
		image->readMetadata();

		exif_data = new Exiv2::ExifData(image->exifData());

		exif_data_init(exif_data);
	}
	catch (Exiv2::AnyError& e)
	{
		g_warning("Could not load EXIF data from file %s", filename);
		return NULL;
	}

	return exif_data;
}

RS_EXIF_DATA *
rs_exif_load_from_rawfile(RAWFILE *rawfile)
{
	RS_EXIF_DATA *rs_exif_data;
	try
	{
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(
			(const Exiv2::byte*) raw_get_map(rawfile), raw_get_filesize(rawfile));

		assert(image.get() != 0);
		image->readMetadata();

		rs_exif_data = new Exiv2::ExifData(image->exifData());

		exif_data_init(rs_exif_data);
	}
	catch (Exiv2::AnyError& e)
	{
		g_warning("Could not load EXIF data");
		return NULL;
	}

	return rs_exif_data;
}

void
rs_exif_add_to_file(RS_EXIF_DATA *d, Exiv2::IptcData &iptc_data, const gchar *filename, RSExifFileType type)
{
	if (!d)
		return;

	try
	{
		Exiv2::ExifData *data = (Exiv2::ExifData *) d;
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(filename);

		/* Copy EXIF to XMP */
#if EXIV2_TEST_VERSION(0,17,0)
		Exiv2::XmpData xmp;
		Exiv2::copyExifToXmp(*data, xmp);
		image->setXmpData(xmp);
#endif

		/* Set new metadata on output image and save */
		if (type != RS_EXIF_FILE_TYPE_PNG)
		{
			Exiv2::ExifThumb exifThumb(*data);
			std::string thumbExt = exifThumb.extension();
			if (!thumbExt.empty())
				exifThumb.erase();
			image->setExifData(*data);
		}
		image->setIptcData(iptc_data);
		image->writeMetadata();
	}
	catch (Exiv2::AnyError& e)
	{
		g_warning("Couldn't add EXIF data to %s", filename);
	}
}

void
rs_exif_free(RS_EXIF_DATA *d)
{
	Exiv2::ExifData *data = (Exiv2::ExifData *) d;
	delete data;
}

static void
rs_add_cs_to_exif(RS_EXIF_DATA *d, const gchar *cs)
{
	if (!cs)
		return;

	Exiv2::ExifData *data = (Exiv2::ExifData *) d;

	if (g_str_equal(cs, "RSSrgb"))
	{
		(*data)["Exif.Photo.ColorSpace"] = 1;
		(*data)["Exif.Iop.InteroperabilityIndex"] = "R98";
		(*data)["Exif.Iop.InteroperabilityVersion"] = "0100";
	} 
	else if (g_str_equal(cs, "RSAdobeRGB"))
	{
		(*data)["Exif.Photo.ColorSpace"] = 65535;
		(*data)["Exif.Iop.InteroperabilityIndex"] = "R03";
		(*data)["Exif.Iop.InteroperabilityVersion"] = "0100";
	}
	else
		(*data)["Exif.Photo.ColorSpace"] = 65535;
}

static void 
rs_add_tags_exif(RS_EXIF_DATA *d, const gchar *input_filename)
{
	if (!d)
		return;
	Exiv2::ExifData *data = (Exiv2::ExifData *) d;
	RSLibrary *lib = rs_library_get_singleton();
	GList *tags = rs_library_photo_tags(lib, input_filename, FALSE);
	if (!tags || g_list_length (tags) == 0)
		return;

	GString *usercomment = g_string_new("charset=\"Undefined\" ");
	GString *xpkeyw = g_string_new("");
	GList *c = tags;
	do 
	{
		g_string_append(usercomment, (gchar*)(c->data));
		g_string_append(xpkeyw, (gchar*)(c->data));
		if (c->next)
		{
			g_string_append(xpkeyw, ",");
			g_string_append(usercomment, " ");
		}
		g_free(c->data);
	} while (c = c->next);

	g_list_free(tags);
	Exiv2::CommentValue comment(usercomment->str);
	(*data)["Exif.Photo.UserComment"] = comment;

	glong items_written;
	gunichar2 *w = g_utf8_to_utf16(xpkeyw->str, -1, NULL, &items_written, NULL);
	Exiv2::Value::AutoPtr v = Exiv2::Value::create(Exiv2::unsignedByte);
	v->read((const Exiv2::byte*)w, items_written * sizeof(gunichar2), Exiv2::invalidByteOrder);
	Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.XPKeywords");
	data->add(key, v.get());

	g_free(w);
	g_string_free(usercomment, TRUE);
	g_string_free(xpkeyw, TRUE);
}

static void 
rs_add_tags_iptc(Exiv2::IptcData &iptc_data, const gchar *input_filename, uint16_t format)
{
	/* Add overall tags */
	iptc_data["Iptc.Envelope.CharacterSet"] = "UTF-8";
	iptc_data["Iptc.Application2.Program"] = "Rawstudio";
	iptc_data["Iptc.Application2.ProgramVersion"] = VERSION;
	iptc_data["Iptc.Envelope.ModelVersion"] = 42;
	iptc_data["Iptc.Envelope.FileFormat"] = format;

	/* Add tags */
	RSLibrary *lib = rs_library_get_singleton();
	GList *tags = rs_library_photo_tags(lib, input_filename, FALSE);
	if (!tags || g_list_length(tags) == 0)
		return;

	do 
	{
		Exiv2::StringValue *v = new Exiv2::StringValue((char*)tags->data);
		iptc_data.add(Exiv2::IptcKey("Iptc.Application2.Keywords"), v);
		delete v;
		g_free(tags->data);
	} while (tags = tags->next);
	
	/* When we some day can access this information, enable this */
#if 0
enum {
	PRIO_U = 0,
	PRIO_D = 51,
	PRIO_1 = 1,
	PRIO_2 = 2,
	PRIO_3 = 3,
	PRIO_ALL = 255
};

	gboolean exported;
	gint priority;
	rs_cache_load_quick(input_filename, &priority, &exported);

	switch (priority)
	{
		case PRIO_1:
			iptc_data["Iptc.Application2.Urgency"] = "1";
			break;
		case PRIO_2:
			iptc_data["Iptc.Application2.Urgency"] = "2";
			break;
		case PRIO_3:
			iptc_data["Iptc.Application2.Urgency"] = "3";
			break;
	}
#endif	
}

gboolean
rs_exif_add_colorspace(const gchar *output_filename, const gchar *color_space, RSExifFileType type)
{
	/* Exiv2 prior to v0.20.0 cannot add tags to TIFF images without corrupting them */
	if (RS_EXIF_FILE_TYPE_TIFF == type)
		if (Exiv2::versionNumber() < 0x1400)
			return FALSE;

	if (output_filename)
	{
		RS_EXIF_DATA *exif;
		Exiv2::IptcData iptc_data;
		exif = new Exiv2::ExifData();
		exif_data_init(exif);
		
		rs_add_cs_to_exif(exif, color_space);
		
		rs_exif_add_to_file(exif, iptc_data, output_filename, type);
		rs_exif_free(exif);
		return TRUE;
	}
	return FALSE;
}

gboolean
rs_exif_copy(const gchar *input_filename, const gchar *output_filename, const gchar *color_space, RSExifFileType type)
{
	/* Exiv2 prior to v0.20.0 cannot add tags to TIFF images without corrupting them */
	if (RS_EXIF_FILE_TYPE_TIFF == type)
		if (Exiv2::versionNumber() < 0x1400)
			return FALSE;

	if (input_filename && output_filename)
	{
		RS_EXIF_DATA *exif;
		Exiv2::IptcData iptc_data;
		exif = rs_exif_load_from_file(input_filename);
		if (NULL == exif)
			return FALSE;
		rs_add_cs_to_exif(exif, color_space);
		rs_add_tags_exif(exif, input_filename);
		if (RS_EXIF_FILE_TYPE_JPEG == type)
			rs_add_tags_iptc(iptc_data, input_filename, 11);
		if (RS_EXIF_FILE_TYPE_TIFF == type)
			rs_add_tags_iptc(iptc_data, input_filename, 3);
		rs_exif_add_to_file(exif, iptc_data, output_filename, type);
		rs_exif_free(exif);
		return TRUE;
	}
	return FALSE;
}

} /* extern "C" */

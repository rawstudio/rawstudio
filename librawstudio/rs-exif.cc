/*
 * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>,
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

#ifndef EXIV2_TEST_VERSION
# define EXIV2_TEST_VERSION(major,minor,patch) \
	( EXIV2_VERSION >= EXIV2_MAKE_VERSION((major),(minor),(patch)) )
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

	/* Delete various MakerNote fields only applicable to the raw file */

	// Nikon thumbnail data
#if EXIV2_TEST_VERSION(0,13,0)
	"Exif.Nikon3.Preview",
#endif

#if EXIV2_TEST_VERSION(0,18,0)
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
	(*data)["Exif.Image.ProcessingSoftware"] = "Rawstudio " VERSION;

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
		return NULL;
	}

	return rs_exif_data;
}

void
rs_exif_add_to_file(RS_EXIF_DATA *d, const gchar *filename)
{
	if (!d)
		return;

	try
	{
		Exiv2::ExifData *data = (Exiv2::ExifData *) d;
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(filename);

		image->setExifData(*data);
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

gboolean
rs_exif_copy(const gchar *input_filename, const gchar *output_filename)
{
	if (input_filename && output_filename)
	{
		RS_EXIF_DATA *exif;

		exif = rs_exif_load_from_file(input_filename);
		rs_exif_add_to_file(exif, output_filename);
		rs_exif_free(exif);
	}
}

} /* extern "C" */

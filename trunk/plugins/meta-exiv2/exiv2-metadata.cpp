/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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

#include <gtk/gtk.h>
#include <iostream>
#include <iomanip>
#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <assert.h>
#include "exiv2-metadata.h"
#include <math.h>

#ifndef EXIV2_TEST_VERSION
# define EXIV2_TEST_VERSION(major,minor,patch) \
	( EXIV2_VERSION >= EXIV2_MAKE_VERSION((major),(minor),(patch)) )
#endif

#if EXIV2_TEST_VERSION(0,17,0)
#include <exiv2/convert.hpp>
#endif

#if EXIV2_TEST_VERSION(0,19,0)
#include <exiv2/easyaccess.hpp>
#endif

extern "C" {

/** INTERFACE **/

using namespace Exiv2;

static void 
set_metadata_maker(std::string maker, RSMetadata *meta)
{
	meta->make_ascii = rs_remove_tailing_spaces(g_strdup(maker.c_str()), TRUE);
	
	if (g_ascii_strncasecmp(meta->make_ascii, "Canon",5))
		meta->make = MAKE_CANON;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "CASIO", 5))
		meta->make = MAKE_CASIO;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Hasselblad", 10))
		meta->make = MAKE_HASSELBLAD;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "KODAK", 5))
		meta->make = MAKE_KODAK;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "EASTMAN KODAK", 13))
		meta->make = MAKE_KODAK;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Leica", 5))
		meta->make = MAKE_LEICA;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Minolta", 7))
		meta->make = MAKE_MINOLTA;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "KONICA MINOLTA", 14))
		meta->make = MAKE_MINOLTA;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Mamiya", 6))
		meta->make = MAKE_MAMIYA;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "NIKON", 5))
		meta->make = MAKE_NIKON;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "OLYMPUS", 7))
		meta->make = MAKE_OLYMPUS;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Panasonic", 9))
		meta->make = MAKE_PANASONIC;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "PENTAX", 6))
		meta->make = MAKE_PENTAX;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Phase One", 9))
		meta->make = MAKE_PHASEONE;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "Ricoh", 5))
		meta->make = MAKE_RICOH;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "SAMSUNG", 7))
		meta->make = MAKE_SAMSUNG;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "SONY", 7))
		meta->make = MAKE_SONY;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "FUJIFILM", 4))
		meta->make = MAKE_FUJIFILM;
	else if (0 == g_ascii_strncasecmp(meta->make_ascii, "SEIKO EPSON", 11))
		meta->make = MAKE_EPSON;
}

gboolean
exiv2_load_meta_interface(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	try {
		Image::AutoPtr img = ImageFactory::open((byte*)raw_get_map(rawfile), raw_get_filesize(rawfile));
		img->readMetadata();
		ExifData &exifData = img->exifData();

#if EXIV2_TEST_VERSION(0,17,0)
		/* We perfer XMP data, so copy it to EXIF */
		XmpData &xmpData = img->xmpData();
		if (!xmpData.empty())
			copyXmpToExif(xmpData, exifData);
#endif

		/* Parse Exif Data */
		if (!exifData.empty())
		{
			ExifData::const_iterator i;
			i = exifData.findKey(ExifKey("Exif.Image.Make"));
			if (i != exifData.end())
				set_metadata_maker(i->toString(), meta);
			
			i = exifData.findKey(ExifKey("Exif.Image.Model"));
			if (i != exifData.end())
				meta->model_ascii = g_strdup(i->toString().c_str());

#if EXIV2_TEST_VERSION(0,19,0)
			i = orientation(exifData);
			if (i != exifData.end())
			{
				std::auto_ptr<Exiv2::Value> val = i->getValue();
				if (val->count())
				{
					switch (val->toLong())
					{
							case 6: meta->orientation = 90;
								break;
							case 8: meta->orientation = 270;
								break;
					}
				}
			}
#endif

			i = exifData.findKey(ExifKey("Exif.Image.DateTimeOriginal"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.Image.DateTime"));
			if (i != exifData.end())
			{
				meta->time_ascii = g_strdup(i->toString().c_str());
				meta->timestamp = rs_exiftime_to_unixtime(meta->time_ascii);
			}

			i = exifData.findKey(ExifKey("Exif.Image.ExposureTime"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.Photo.ExposureTime"));
			if (i != exifData.end())
				meta->shutterspeed = 1.0 / i->getValue()->toFloat();
			else
			{
				i = exifData.findKey(ExifKey("Exif.Image.ShutterSpeedValue"));
				if (i == exifData.end())
					i = exifData.findKey(ExifKey("Exif.Photo.ShutterSpeedValue"));
				if (i != exifData.end())
					meta->shutterspeed = 1.0 / i->toFloat();
			}

			i = exifData.findKey(ExifKey("Exif.Image.FNumber"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.Photo.FNumber"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.Image.ApertureValue"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.Photo.ApertureValue"));
			if (i != exifData.end())
				meta->aperture = i->toFloat();

			i = exifData.findKey(ExifKey("Exif.Image.FocalLength"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.Photo.FocalLength"));
			if (i != exifData.end())
				meta->focallength = i->toFloat()-0.01;

#if EXIV2_TEST_VERSION(0,19,0)
			i = isoSpeed(exifData);
			if (i != exifData.end())
				meta->iso = i->toLong();

			/* Text based Lens Identifier */
			i = lensName(exifData);
			if (i != exifData.end())
			{
				TypeId type = i->typeId();
				if (type == unsignedShort || type == unsignedLong || type == signedShort || type == signedLong || type == unsignedByte || type == signedByte)
					meta->lens_id = i->toLong();
				else if (type == asciiString || type == string)
					meta->fixed_lens_identifier = g_strdup(i->toString().c_str());
			}
#endif

			/* Canon*/
			i = exifData.findKey(ExifKey("Exif.CanonCs.Lens"));
			if (i != exifData.end()
					&& i->value().count() >= 3
					&& i->value().typeId() == unsignedShort) 
			{
				float fu = i->value().toFloat(2);
				if (fu != 0.0) 
				{
					meta->lens_min_focal = i->toFloat(0) / fu;
					meta->lens_max_focal = i->toFloat(1) /fu;
				}
			}
			i = exifData.findKey(ExifKey("Exif.CanonCs.MinAperture"));
			if (i != exifData.end())
				meta->lens_min_aperture = (gfloat) exp(CanonEv(i->toFloat())*log(2)/2);
			i = exifData.findKey(ExifKey("Exif.CanonCs.MaxAperture"));
			if (i != exifData.end())
				meta->lens_max_aperture = (gfloat) exp(CanonEv(i->toFloat())*log(2)/2);

			/* Olympus */
			i = exifData.findKey(ExifKey("Exif.OlympusEq.MinFocalLength"));
			if (i != exifData.end())
				meta->lens_min_focal = i->toFloat();
			i = exifData.findKey(ExifKey("Exif.OlympusEq.MaxFocalLength"));
			if (i != exifData.end())
				meta->lens_max_focal = i->toFloat();


			/* Nikon */
			i = exifData.findKey(ExifKey("Exif.NikonLd1.MinFocalLength"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd2.MinFocalLength"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd3.MinFocalLength"));
			if (i != exifData.end())
				meta->lens_min_focal = 5.0 * pow(2.0, i->toLong()/24.0);

			i = exifData.findKey(ExifKey("Exif.NikonLd1.MaxFocalLength"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd2.MaxFocalLength"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd3.MaxFocalLength"));
			if (i != exifData.end())
				meta->lens_max_focal = 5.0 * pow(2.0, i->toLong()/24.0);

			i = exifData.findKey(ExifKey("Exif.NikonLd1.MaxApertureAtMinFocal"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd2.MaxApertureAtMinFocal"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd3.MaxApertureAtMinFocal"));
			if (i != exifData.end())
				meta->lens_min_aperture = i->toLong()/12.0;
			
			i = exifData.findKey(ExifKey("Exif.NikonLd1.MaxApertureAtMaxFocal"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd2.MaxApertureAtMaxFocal"));
			if (i == exifData.end())
				i = exifData.findKey(ExifKey("Exif.NikonLd3.MaxApertureAtMaxFocal"));
			if (i != exifData.end())
				meta->lens_max_aperture = i->toLong()/12.0;
				
			/* Fuji */
			i = exifData.findKey(ExifKey("Exif.Fujifilm.MinFocalLength"));
			if (i != exifData.end())
				meta->lens_min_focal = i->toFloat();
			i = exifData.findKey(ExifKey("Exif.Fujifilm.MaxFocalLength"));
			if (i != exifData.end())
				meta->lens_max_focal = i->toFloat();

			return TRUE;
		}
	} catch (Exiv2::Error& e) {
    g_debug("Exiv2 Metadata Loader:'%s'", e.what());
	}
	return FALSE;
}
/** END INTERFACE **/
} // extern "C"

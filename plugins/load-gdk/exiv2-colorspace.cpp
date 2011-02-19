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
#include <exiv2/easyaccess.hpp>
#include <assert.h>
#include "exiv2-colorspace.h"
#include <math.h>
#include <png.h>

#ifndef EXIV2_TEST_VERSION
# define EXIV2_TEST_VERSION(major,minor,patch) \
	( EXIV2_VERSION >= EXIV2_MAKE_VERSION((major),(minor),(patch)) )
#endif

#if EXIV2_TEST_VERSION(0,17,0)
#include <exiv2/convert.hpp>
#endif

extern "C" {

/** INTERFACE **/

using namespace Exiv2;

RSColorSpace*
exiv2_get_colorspace(const gchar *filename, gboolean *linear_guess)
{
#ifdef PNG_iCCP_SUPPORTED
	if (1)
	{
		*linear_guess = FALSE;
		RSColorSpace* profile = NULL;
		const gchar *icc_profile_title;
		const gchar *icc_profile;
		guint icc_profile_size;
		png_structp png_ptr = png_create_read_struct(
				PNG_LIBPNG_VER_STRING,
				NULL,
				NULL,
				NULL);
		FILE* fp = fopen(filename, "rb");
		if (fp)
		{
			png_byte sig[8];
      
			if (fread(sig, 1, 8, fp) && 0 == fseek(fp,0,SEEK_SET) && png_check_sig(sig, 8))
			{
				png_init_io(png_ptr, fp);
				png_infop info_ptr = png_create_info_struct(png_ptr);
				if (info_ptr)
				{
					png_read_info( png_ptr, info_ptr );

					int compression_type;
					/* Extract embedded ICC profile */
					if (info_ptr->valid & PNG_INFO_iCCP)
					{
						png_uint_32 retval = png_get_iCCP (png_ptr, info_ptr,
													(png_charpp) &icc_profile_title, &compression_type,
													(png_charpp) &icc_profile, (png_uint_32*) &icc_profile_size);
						if (retval != 0)
						{
							RSIccProfile *icc = rs_icc_profile_new_from_memory((gchar*)icc_profile, icc_profile_size, TRUE);
							profile = rs_color_space_icc_new_from_icc(icc);
						}
						gdouble gamma = 2.2;
						png_get_gAMA(png_ptr, info_ptr, &gamma);
						if (gamma < 1.1)
							*linear_guess = TRUE;
					}
				}
				png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			}
			fclose(fp);	
		}
		if (profile)
			return profile;
	}
#endif

	try {
		Image::AutoPtr img = ImageFactory::open(filename);
		img->readMetadata();
		ExifData &exifData = img->exifData();
		*linear_guess = FALSE;

		if (exifData.empty() && !img->xmpData().empty())
		{
			copyXmpToExif(img->xmpData(), exifData);
		}

		/* Parse Exif Data */
		if (!exifData.empty())
		{
			ExifData::const_iterator i;
			i = exifData.findKey(ExifKey("Exif.Image.BitsPerSample"));
			if (i != exifData.end())
				if (i->toLong() == 16)
					*linear_guess = TRUE;
			
			i = exifData.findKey(ExifKey("Exif.Photo.ColorSpace"));
			if (i != exifData.end())
			{
				if (i->toLong() == 1)
					return rs_color_space_new_singleton("RSSrgb");
			}

			/* Attempt to find ICC profile */
			i = exifData.findKey(ExifKey("Exif.Image.InterColorProfile"));
			if (i != exifData.end())
			{
				DataBuf buf(i->size());
				i->copy(buf.pData_, Exiv2::invalidByteOrder);
				if (buf.pData_ && buf.size_)
				{
					RSIccProfile *icc = rs_icc_profile_new_from_memory((gchar*)buf.pData_, buf.size_, TRUE);
					return rs_color_space_icc_new_from_icc(icc);
				}
			}

			i = exifData.findKey(ExifKey("Exif.Iop.InteroperabilityIndex"));
			if (i != exifData.end())
				if (0 == i->toString().compare("R03"))
					return rs_color_space_new_singleton("RSAdobeRGB");
		}
	} catch (Exiv2::Error& e) {
		g_debug("Exiv2 ColorSpace Loader:'%s", e.what());
	}
	g_debug("Exiv2 ColorSpace Loader: Could not determine colorspace, assume sRGB.");
	return rs_color_space_new_singleton("RSSrgb");
}
/** END INTERFACE **/
} // extern "C"

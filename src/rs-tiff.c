/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <tiffio.h>
#include "application.h"
#include "rs-tiff.h"

static void rs_tiff_generic_init(TIFF *output, guint w, guint h, const guint samples_per_pixel, const gchar *profile_filename, gboolean uncompressed);

static void
rs_tiff_generic_init(TIFF *output, guint w, guint h, const guint samples_per_pixel, const gchar *profile_filename, gboolean uncompressed)
{
	TIFFSetField(output, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(output, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(output, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
	TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	if (uncompressed)
		TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	else
	{
		TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
		TIFFSetField(output, TIFFTAG_ZIPQUALITY, 9);
	}
	if (profile_filename)
	{
		gchar *buffer = NULL;
		gsize length = 0;

		if (g_file_get_contents(profile_filename, &buffer, &length, NULL))
			TIFFSetField(output, TIFFTAG_ICCPROFILE, length, buffer);

		g_free(buffer);
	}
	TIFFSetField(output, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(output, 0));
}

gboolean
rs_tiff8_save(GdkPixbuf *pixbuf, const gchar *filename, const gchar *profile_filename, gboolean uncompressed)
{
	TIFF *output;
	gint row;

	if((output = TIFFOpen(filename, "w")) == NULL)
		return(FALSE);
	rs_tiff_generic_init(output, gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf), 3, profile_filename, uncompressed);
	TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 8);
	for(row=0;row<gdk_pixbuf_get_height(pixbuf);row++)
	{
		guchar *buf = GET_PIXBUF_PIXEL(pixbuf, 0, row);
		TIFFWriteScanline(output, buf, row, 0);
	}
	TIFFClose(output);
	return(TRUE);
}

gboolean
rs_tiff16_save(RS_IMAGE16 *image, const gchar *filename, const gchar *profile_filename, gboolean uncompressed)
{
	static GStaticMutex filename_lock = G_STATIC_MUTEX_INIT;
	TIFF *output;
	gint row;

	g_static_mutex_lock(&filename_lock);
	if ((!g_file_test(filename, G_FILE_TEST_EXISTS)) && (output = TIFFOpen(filename, "w")))
	{
		g_static_mutex_unlock(&filename_lock);
		rs_tiff_generic_init(output, image->w, image->h, image->channels, profile_filename, uncompressed);
		TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 16);
		for(row=0;row<image->h;row++)
		{
			gushort *buf = GET_PIXEL(image, 0, row);
			TIFFWriteScanline(output, buf, row, 0);
		}
		TIFFClose(output);
	}
	else
	{
		g_static_mutex_unlock(&filename_lock);
		return FALSE;
	}
	return TRUE ;
}

RS_IMAGE16 *
rs_tiff16_load(const gchar *filename)
{
	RS_IMAGE16 *image = NULL;

	g_assert(filename != NULL);

	TIFF* tif = TIFFOpen(filename, "r");

	if (tif)
	{
		guint w=0, h=0, samples_per_pixel=0, row, bits_per_sample=0;
		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

		if ((bits_per_sample == 16) && w && h)
			image = rs_image16_new(w, h, samples_per_pixel, 4);

		if (image)
			/* Write directly to pixel data */
			for(row=0;row<h;row++)
				TIFFReadScanline(tif, GET_PIXEL(image, 0, row), row, 0);
		TIFFClose(tif);
	}

	return image;
}

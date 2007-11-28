/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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
#include "rawstudio.h"
#include "rs-tiff.h"
#include "rs-image.h"

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
		struct stat st;
		guchar *buffer;
		gint fd;
		stat(profile_filename, &st);
		if (st.st_size>0)
			if ((fd = open(profile_filename, O_RDONLY)) != -1)
			{
				buffer = g_malloc(st.st_size);
				if (read(fd, &buffer, st.st_size) == st.st_size)
					TIFFSetField(output, TIFFTAG_ICCPROFILE, st.st_size, buffer);
				g_free(buffer);
				close(fd);
			}
	}
	TIFFSetField(output, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(output, 0));
}

gboolean
rs_tiff8_save(RS_IMAGE8 *image, const gchar *filename, const gchar *profile_filename, gboolean uncompressed)
{
	TIFF *output;
	gint row;

	if((output = TIFFOpen(filename, "w")) == NULL)
		return(FALSE);
	rs_tiff_generic_init(output, image->w, image->h, 3, profile_filename, uncompressed);
	TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 8);
	for(row=0;row<image->h;row++)
	{
		guchar *buf = image->pixels + image->rowstride * row;
		TIFFWriteScanline(output, buf, row, 0);
	}
	TIFFClose(output);
	return(TRUE);
}

gboolean
rs_tiff16_save(RS_IMAGE16 *image, const gchar *filename, const gchar *profile_filename, gboolean uncompressed)
{
	TIFF *output;
	gint row;

	if((output = TIFFOpen(filename, "w")) == NULL)
		return(FALSE);
	rs_tiff_generic_init(output, image->w, image->h, image->channels, profile_filename, uncompressed);
	TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 16);
	for(row=0;row<image->h;row++)
	{
		gushort *buf = image->pixels + image->rowstride * row;
		TIFFWriteScanline(output, buf, row, 0);
	}
	TIFFClose(output);
	return(TRUE);
}

/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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
#include <sys/mman.h>
#include <tiffio.h>
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"

static void rs_tiff_generic_init(TIFF *output, RS_IMAGE8 *image, const gchar *profile_filename);

void
rs_tiff_generic_init(TIFF *output, RS_IMAGE8 *image, const gchar *profile_filename)
{
	TIFFSetField(output, TIFFTAG_IMAGEWIDTH, image->w);
	TIFFSetField(output, TIFFTAG_IMAGELENGTH, image->h);
	TIFFSetField(output, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	if (profile_filename)
	{
		struct stat st;
		guchar *buffer;
		gint fd;
		stat(profile_filename, &st);
		if (st.st_size>0)
			if ((fd = open(profile_filename, O_RDONLY)) != -1)
			{
				buffer = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
				if (buffer)
				{
					TIFFSetField(output, TIFFTAG_ICCPROFILE, st.st_size, buffer);
					g_free(buffer);
					munmap(buffer, st.st_size);
				}
				close(fd);
			}
	}
	TIFFSetField(output, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(output, 0));
}

gboolean
rs_tiff8_save(RS_IMAGE8 *image, const gchar *filename, const gchar *profile_filename)
{
	TIFF *output;
	gint row;

	if((output = TIFFOpen(filename, "w")) == NULL)
		return(FALSE);
	rs_tiff_generic_init(output, image, profile_filename);
	TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 8);
	for(row=0;row<image->h;row++)
	{
		guchar *buf = image->pixels + image->rowstride * row;
		TIFFWriteScanline(output, buf, row, 0);
	}
	TIFFClose(output);
	return(TRUE);
}

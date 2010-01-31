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

#include <stdio.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "application.h"
#include "rs-jpeg.h"
#ifdef WIN32
#define HAVE_BOOLEAN
#define _BASETSD_H_
#endif
#include <jpeglib.h>

/* This function is an almost verbatim copy from little cms. Thanks Marti, you rock! */

#define ICC_MARKER  (JPEG_APP0 + 2) /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN  14        /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER  65533  /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER  (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)
#define ICC_MARKER_IDENT "ICC_PROFILE"

static void rs_jpeg_write_icc_profile(j_compress_ptr cinfo,
	const JOCTET *icc_data_ptr, guint icc_data_len);

static void
rs_jpeg_write_icc_profile(j_compress_ptr cinfo,
	const JOCTET *icc_data_ptr, guint icc_data_len)
{
	gchar *ident = ICC_MARKER_IDENT;
	guint num_markers; /* total number of markers we'll write */
	gint cur_marker = 1;       /* per spec, counting starts at 1 */
	guint length;      /* number of bytes to write in this marker */

	num_markers = icc_data_len / MAX_DATA_BYTES_IN_MARKER;
	if (num_markers * MAX_DATA_BYTES_IN_MARKER != icc_data_len)
		num_markers++;
	while (icc_data_len > 0)
	{
		length = icc_data_len;
		if (length > MAX_DATA_BYTES_IN_MARKER)
			length = MAX_DATA_BYTES_IN_MARKER;
		icc_data_len -= length;
		jpeg_write_m_header(cinfo, ICC_MARKER, (guint) (length + ICC_OVERHEAD_LEN));

		do {
			jpeg_write_m_byte(cinfo, *ident);
		} while(*ident++);
		jpeg_write_m_byte(cinfo, cur_marker);
		jpeg_write_m_byte(cinfo, (gint) num_markers);

		while (length--)
		{
			jpeg_write_m_byte(cinfo, *icc_data_ptr);
			icc_data_ptr++;
		}
		cur_marker++;
	}
	return;
}

gboolean
rs_jpeg_save(GdkPixbuf *pixbuf, const gchar *filename, const gint quality,
	const gchar *profile_filename)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE * outfile;
	JSAMPROW row_pointer[1];

	guchar *buffer;
	guint len;
	gint fd;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	if ((outfile = fopen(filename, "wb")) == NULL)
		return(FALSE);
	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = gdk_pixbuf_get_width(pixbuf);
	cinfo.image_height = gdk_pixbuf_get_height(pixbuf);
	cinfo.input_components = gdk_pixbuf_get_n_channels(pixbuf);
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	if (profile_filename)
	{
		struct stat st;
		stat(profile_filename, &st);
		if (st.st_size>0)
			if ((fd = open(profile_filename, O_RDONLY)) != -1)
			{
				gint bytes_read = 0;
				len = st.st_size;
				buffer = g_malloc(len);
				while(bytes_read < len)
					bytes_read += read(fd, buffer+bytes_read, len-bytes_read);
				close(fd);
				rs_jpeg_write_icc_profile(&cinfo, buffer, len);
				g_free(buffer);
			}
	}
	while (cinfo.next_scanline < cinfo.image_height)
	{
		row_pointer[0] = GET_PIXBUF_PIXEL(pixbuf, 0, cinfo.next_scanline);
		if (jpeg_write_scanlines(&cinfo, row_pointer, 1) != 1)
			break;
	}
	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
	return(TRUE);
}

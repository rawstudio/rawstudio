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
#include "rawstudio.h"
#include "rs-image.h"
#include "rawfile.h"
#include "tiff-meta.h"
#include "rs-render.h"
#include "panasonic.h"
#include "adobe-coeff.h"

static gboolean panasonic_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta);

typedef struct _panasonic {
	guint sensorstart;
	guint sensorwidth;
	guint sensorheight;
	guint sensorlength;
} PANASONIC;

static gboolean
panasonic_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	PANASONIC *panasonic = (PANASONIC *) (meta->data);
	gushort number_of_entries;
	gushort fieldtag=0;
	guint valuecount;
	guint uint_temp1=0;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries)) return(FALSE);
	if (number_of_entries>5000)
		return(FALSE);
	offset += 2;

	while(number_of_entries--)
	{
		raw_get_ushort(rawfile, offset, &fieldtag);
		raw_get_uint(rawfile, offset+4, &valuecount);
		offset += 8;
		switch(fieldtag)
		{
			case 0x0002: /* SensorWidth */
				if (panasonic)
					raw_get_uint(rawfile, offset, &panasonic->sensorwidth);
				break;
			case 0x0003: /* SensorHeight */
				if (panasonic)
					raw_get_uint(rawfile, offset, &panasonic->sensorheight);
				break;
			case 0x0017: /* ISO */
				raw_get_ushort(rawfile, offset, &meta->iso);
				break;
			case 0x0024: /* WB_RedLevel */
				raw_get_uint(rawfile, offset, &uint_temp1);
				meta->cam_mul[0] = uint_temp1;
				break;
			case 0x0025: /* WB_GreenLevel */
				raw_get_uint(rawfile, offset, &uint_temp1);
				meta->cam_mul[1] = meta->cam_mul[3] = uint_temp1;
				break;
			case 0x0026: /* WB_BlueLevel */
				raw_get_uint(rawfile, offset, &uint_temp1);
				meta->cam_mul[2] = uint_temp1;
				break;
			case 0x010f: /* Make */
				raw_get_uint(rawfile, offset, &uint_temp1);
				if (!meta->make_ascii)
					meta->make_ascii = raw_strdup(rawfile, uint_temp1, 32);
				meta->make = MAKE_PANASONIC;
				break;
			case 0x0110: /* Model */
				raw_get_uint(rawfile, offset, &uint_temp1);
				if (!meta->model_ascii)
					meta->model_ascii = raw_strdup(rawfile, uint_temp1, 32);
				break;
			case 0x0111: /* StripOffsets */
				if (panasonic)
					raw_get_uint(rawfile, offset, &panasonic->sensorstart);
				break;
			case 0x0112: /* Orientation */
				raw_get_ushort(rawfile, offset, &meta->orientation);
				switch (meta->orientation)
				{
					case 6: meta->orientation = 90;
						break;
					case 8: meta->orientation = 270;
						break;
				}
				break;
			case 0x0117: /* StripByteCounts */
				if (panasonic)
					raw_get_uint(rawfile, offset, &panasonic->sensorlength);
				break;
			case 0x8769: /* ExifOffset */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_ifd_walker(rawfile, uint_temp1, meta);
				break;
		}
		offset += 4;
	}
	return(TRUE);
}

void
rs_panasonic_load_meta(const gchar *filename, RS_METADATA *meta)
{
	RAWFILE *rawfile;
	guint next, offset;
	gushort ifd_num;

	rawfile = raw_open_file(filename);
	if (raw_init_file_tiff(rawfile, 0))
	{
		offset = rawfile->first_ifd_offset;
		do {
			if (!raw_get_ushort(rawfile, offset, &ifd_num)) break;
			if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break;
			panasonic_walker(rawfile, offset, meta);
			if (offset == next) break; /* avoid infinite loops */
			offset = next;
		} while (next>0);
	}
	rs_metadata_normalize_wb(meta);
	raw_close_file(rawfile);
	adobe_coeff_set(&meta->adobe_coeff, meta->make_ascii, meta->model_ascii);

	return;
}

GdkPixbuf *
rs_panasonic_load_thumb(const gchar *src)
{
	RS_PHOTO *photo;
	RS_IMAGE16 *image;
	GdkPixbuf *pixbuf = NULL;
	gchar *thumbname;

	thumbname = rs_thumb_get_name(src);
	if (thumbname)
	{
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
			if (pixbuf) return(pixbuf);
		}
	}

	photo = rs_panasonic_load_photo(src);
	rs_panasonic_load_meta(src, photo->metadata);
	switch (photo->metadata->orientation)
	{
		case 90: ORIENTATION_90(photo->orientation);
			break;
		case 180: ORIENTATION_180(photo->orientation);
			break;
		case 270: ORIENTATION_270(photo->orientation);
			break;
	}
	rs_photo_prepare(photo);
	image = rs_image16_transform(photo->input, NULL,
			NULL, NULL, NULL, 128, 128, TRUE, -1.0,
			0.0, photo->orientation);

	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, image->w, image->h);

	rs_render_nocms(photo, image->w, image->h, image->pixels,
		image->rowstride, gdk_pixbuf_get_pixels(pixbuf),
		gdk_pixbuf_get_rowstride(pixbuf),
		NULL);

	rs_image16_free(image);
	rs_photo_free(photo);

	if (thumbname)
	{
		gdk_pixbuf_save(pixbuf, thumbname, "png", NULL, NULL);
		g_free(thumbname);
	}

	return(pixbuf);
}

RS_PHOTO *
rs_panasonic_load_photo(const gchar *filename)
{
	RS_METADATA *meta;
	PANASONIC panasonic;
	RAWFILE *rawfile;
	RS_PHOTO *photo=NULL;
	gushort *in, *out;
	gint row, col;
	gint width, height;
	gint left_margin=0, top_margin=0;
	gint a,b,c,d;

	meta = rs_metadata_new();
	meta->data = (gpointer) &panasonic;
	rs_panasonic_load_meta(filename, meta);

	switch(panasonic.sensorwidth)
	{
		case 3304: /* FZ30 */
			left_margin = 0;
			width = 1640;
			top_margin = 0;
			height = 1229;
			a = 0; b = 1; c = 3; d = 2;
			break;
		case 3690: /* FZ50 */
		case 3770: /* FZ50 */
		case 3880: /* FZ50, untested */
			left_margin = 0;
			width = 1838;
			top_margin = 0;
			height = 1372;
			a = 2; b = 1; c = 3; d = 0;
			break;
		case 4330: /* LX2 */
			left_margin = 10;
			width = 2165-10-34;
			top_margin = 10;
			height = 1220-10-14;
			a = 2; b = 1; c = 3; d = 0;
			break;
		default: /* Try to load anything unknown without cropping */
			left_margin = 0;
			width = (panasonic.sensorwidth)/2;
			top_margin = 0;
			height = (panasonic.sensorheight)/2;
			a = 2; b = 1; c = 3; d = 0;
			break;
	}

	photo = rs_photo_new();
	photo->input = rs_image16_new(width, height, 4, 4);
	rawfile = raw_open_file(filename);

	in = rawfile->map+panasonic.sensorstart + (panasonic.sensorwidth*top_margin)*4 + left_margin*4;
	for(row=0; row<height*2; row+=2)
		for(col=0; col<width*2; col+=2)
		{
			out = &photo->input->pixels[row/2*photo->input->rowstride + col*photo->input->pixelsize/2];
			out[a] = in[row*panasonic.sensorwidth+col+0];
			out[b] = in[row*panasonic.sensorwidth+col+1];
			out[c] = in[(row+1)*panasonic.sensorwidth+col+0];
			out[d] = in[(row+1)*panasonic.sensorwidth+col+1];
		}

	photo->filename = g_strdup(filename);
	photo->active = TRUE;

	rs_metadata_free(meta);
	raw_close_file(rawfile);
	return(photo);
}

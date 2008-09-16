/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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
#include <math.h>
#include "raf-meta.h"
#include "rawstudio.h"
#include "rawfile.h"
#include "tiff-meta.h"
#include "color.h"
#include "rs-utils.h"
#include "rs-metadata.h"

void
rs_raf_load_meta(const gchar *filename, RSMetadata *meta)
{
	RAWFILE *rawfile;
	guint directory;
	guint directory_entries;
	guint entry;
	guint offset;
	gushort tag, length;
	gushort temp;

	rawfile = raw_open_file(filename);

	if (raw_strcmp(rawfile, 0, "FUJIFILM", 8))
	{
		raw_get_uint(rawfile, 84, &meta->preview_start);
		raw_get_uint(rawfile, 88, &meta->preview_length);
		raw_get_uint(rawfile, 92, &directory);

		raw_get_uint(rawfile, directory, &directory_entries);

		offset = directory+4;

		if (directory_entries < 256)
		{
			for(entry=0;entry<directory_entries;entry++)
			{
				raw_get_ushort(rawfile, offset, &tag);
				raw_get_ushort(rawfile, offset+2, &length);
				switch(tag)
				{
					case 0x2ff0: /* White balance */
						raw_get_ushort(rawfile, offset+4, &temp);
						meta->cam_mul[G] = temp;
						raw_get_ushort(rawfile, offset+6, &temp);
						meta->cam_mul[R] = temp;
						raw_get_ushort(rawfile, offset+8, &temp);
						meta->cam_mul[G2] = temp;
						raw_get_ushort(rawfile, offset+10, &temp);
						meta->cam_mul[B] = temp;
						rs_metadata_normalize_wb(meta);
						break;
				}
				offset = offset + 4 + length;
			}
		}
		rs_tiff_load_meta_from_rawfile(rawfile, meta->thumbnail_start+12, meta);
	}
	raw_close_file(rawfile);
}

GdkPixbuf *
rs_raf_load_thumb(const gchar *src)
{
	RAWFILE *rawfile;
	GdkPixbuf *pixbuf = NULL;
	guint start;
	guint length;

	rawfile = raw_open_file(src);
	if (raw_strcmp(rawfile, 0, "FUJIFILM", 8))
	{
		raw_get_uint(rawfile, 84, &start);
		raw_get_uint(rawfile, 88, &length);
		pixbuf = raw_get_pixbuf(rawfile, start, length);
	}

	if (pixbuf)
	{
		GdkPixbuf *pixbuf2;
		gint width = gdk_pixbuf_get_width(pixbuf);
		gint height = gdk_pixbuf_get_height(pixbuf);
		rs_constrain_to_bounding_box(128, 128, &width, &height);
		pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);

		g_object_unref(pixbuf);
		pixbuf = pixbuf2;

		/* Apparently raf-files does not contain any information about rotation ?! */
	}

	raw_close_file(rawfile);

	return pixbuf;
}

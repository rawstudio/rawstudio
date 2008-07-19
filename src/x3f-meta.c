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

#include <glib.h>
#include "rawstudio.h"
#include "rawfile.h"
#include "x3f-meta.h"

/* http://www.x3f.info/technotes/FileDocs/X3F_Format.pdf */

GdkPixbuf *
rs_x3f_load_thumb(const gchar *src)
{
	GdkPixbuf *pixbuf = NULL, *pixbuf2 = NULL;
	gdouble ratio=1.0;
	guint directory=0, directory_entries=0, n=0;
	guint data_offset=0, data_length=0, data_format=0;
	guint start=0, width=0, height=0, rowstride=0;
	RAWFILE *rawfile;

	raw_init();

	rawfile = raw_open_file(src);
	if (!rawfile) return(NULL);
	if (!raw_strcmp(rawfile, 0, "FOVb", 4))
	{
		raw_close_file(rawfile);
		return(NULL);
	}

	raw_set_byteorder(rawfile, 0x4949); /* x3f is always little endian */
	raw_get_uint(rawfile, raw_get_filesize(rawfile)-4, &directory);
	raw_get_uint(rawfile, directory+8, &directory_entries);
	for(n=0;n<(directory_entries*12);n+=12)
	{
		raw_get_uint(rawfile, directory+12+n, &data_offset);
		raw_get_uint(rawfile, directory+12+n+4, &data_length);
		if (raw_strcmp(rawfile, directory+12+n+8, "IMAG", 4))
		{
			if (raw_strcmp(rawfile, data_offset, "SECi", 4))
			{
				raw_get_uint(rawfile, data_offset+12, &data_format);
				if (data_format == 3)
				{
					raw_get_uint(rawfile, data_offset+16, &width);
					raw_get_uint(rawfile, data_offset+20, &height);
					raw_get_uint(rawfile, data_offset+24, &rowstride);
					start = data_offset+28;
				}
			}
		}
	}

	if (width > 0)
		pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+start, GDK_COLORSPACE_RGB, FALSE, 8,
			width, height, rowstride, NULL, NULL);

	if (pixbuf)
	{
		ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
		if (ratio>1.0)
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
		else
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = pixbuf2;
	}

	raw_close_file(rawfile);
	return(pixbuf);
}

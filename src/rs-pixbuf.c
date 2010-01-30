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

void
rs_pixbuf_draw_hline(GdkPixbuf *pixbuf, guint x, guint y, guint length, guint R, guint G, guint B, guint A)
{
	gint width, height;
	guint rowstride;
	guchar *pixels;
	gint channels;
	gint i;

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	channels = gdk_pixbuf_get_n_channels (pixbuf);

	for (i = x*channels; i < (length+x)*channels; i+=channels)
	{
		pixels[y*rowstride+i+0] = R;
		pixels[y*rowstride+i+1] = G;
		pixels[y*rowstride+i+2] = B;
		if (channels == 4)
			pixels[y*rowstride+i+3] = A;
	}
}

void
rs_pixbuf_draw_vline(GdkPixbuf *pixbuf, guint x, guint y, guint length, guint R, guint G, guint B, guint A)
{
	gint width, height;
	guint rowstride;
	guchar *pixels;
	gint channels;
	gint i;

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	channels = gdk_pixbuf_get_n_channels (pixbuf);

	for (i = y; i < y+length; i++)
	{
		pixels[i*rowstride+x*channels+0] = R;
		pixels[i*rowstride+x*channels+1] = G;
		pixels[i*rowstride+x*channels+2] = B;
		if (channels == 4)
			pixels[i*rowstride+x*channels+3] = A;
	}
}

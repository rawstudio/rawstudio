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

#include <rawstudio.h>
#include <gtk/gtk.h>

static void raw_mrw_walker(RAWFILE *rawfile, guint offset, RSMetadata *meta);

static void
raw_mrw_walker(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	guint rawstart=0;
	guint tag=0, len=0;
	gushort ushort_temp1=0;

	meta->make = MAKE_MINOLTA;

	raw_get_uint(rawfile, offset+4, &rawstart);
	rawstart += 8;
	offset += 8;
	while(offset < rawstart)
	{
		raw_get_uint(rawfile, offset, &tag);
		raw_get_uint(rawfile, offset+4, &len);
		offset += 8;

		switch (tag)
		{
			case 0x00545457: /* TTW */
				rs_filetype_meta_load(".tiff", meta, rawfile, offset);
				raw_reset_base(rawfile);
				break;
			case 0x00574247: /* WBG */
				/* rggb format */
				raw_get_ushort(rawfile, offset+4, &ushort_temp1);
				meta->cam_mul[0] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, offset+6, &ushort_temp1);
				meta->cam_mul[1] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, offset+8, &ushort_temp1);
				meta->cam_mul[3] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, offset+10, &ushort_temp1);
				meta->cam_mul[2] = (gdouble) ushort_temp1;
				rs_metadata_normalize_wb(meta);
				break;
		}
		offset += (len);
	}
	return;
}

static void
mrw_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	GdkPixbuf *pixbuf=NULL, *pixbuf2=NULL;
	guint start=0, length=0;

	raw_mrw_walker(rawfile, offset, meta);

	if ((meta->thumbnail_start>0) && (meta->thumbnail_length>0))
	{
		start = meta->thumbnail_start;
		length = meta->thumbnail_length;
	}
	else if ((meta->preview_start>0) && (meta->preview_length>0))
	{
		start = meta->preview_start;
		length = meta->preview_length;
	}

	if ((start>0) && (length>0))
	{
		guchar *thumbbuffer;
		gdouble ratio;
		GdkPixbufLoader *pl;

		start++; /* stupid! */
		length--;

		thumbbuffer = g_malloc(length+1);
		thumbbuffer[0] = '\xff';
		raw_strcpy(rawfile, start, thumbbuffer+1, length);
		pl = gdk_pixbuf_loader_new();
		gdk_pixbuf_loader_write(pl, thumbbuffer, length+1, NULL);
		pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
		gdk_pixbuf_loader_close(pl, NULL);
		g_free(thumbbuffer);
		
		if (pixbuf==NULL) return;
		ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
		if (ratio>1.0)
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
		else
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = pixbuf2;
		switch (meta->orientation)
		{
			/* this is very COUNTER-intuitive - gdk_pixbuf_rotate_simple() is wierd */
			case 90:
				pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
				break;
			case 270:
				pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
				break;
		}
		meta->thumbnail = pixbuf;
	}

	return;
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_meta_loader(".mrw", "Minolta raw", mrw_load_meta, 10, RS_LOADER_FLAGS_RAW);
}

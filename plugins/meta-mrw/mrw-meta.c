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

#include <rawstudio.h>
#include <gtk/gtk.h>

static void raw_mrw_walker(RAWFILE *rawfile, guint offset, RSMetadata *meta);

static void
raw_mrw_walker(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	guint rawstart=0;
	guint len=0;
	gushort ushort_temp1=0;
	gushort bayer_pattern=0;

	if (!raw_strcmp(rawfile, 1, "MRM", 3))
		return;

	meta->make = MAKE_MINOLTA;

	raw_get_uint(rawfile, offset+4, &rawstart);
	rawstart += 8;
	offset += 8;

	/*
	 * Known blocks:
	 * PRD: Picture Raw Dimensions
	 * TTW: Tiff Tags W??
	 * WBG: Block White Balance Gains
	 * RIF: Requested Image Format
	 * PAD: Padding
	 */

	while(offset < rawstart)
	{
		gchar identifier[4] = {0, 0, 0, 0};

		/* Read block identifier from raw. Ignore first byte, it's always \0 */
		raw_strcpy(rawfile, offset+1, identifier, 3);

		/* Move past block identifier and read length */
		offset += 4;
		raw_get_uint(rawfile, offset, &len);

		/* Move offset past length */
		offset += 4;
		if (g_str_equal(identifier, "TTW"))
		{
			rs_filetype_meta_load(".tiff", meta, rawfile, offset);

			/* DiMAGE A200 gives thumbnail offsets relative to FILE
			   start, not TIFF start */
			if(g_str_equal(meta->model_ascii, "DiMAGE A200"))
				meta->thumbnail_start -= raw_get_base(rawfile);

			raw_reset_base(rawfile);
		}
		else if (g_str_equal(identifier, "PRD"))
		{
			raw_get_ushort(rawfile, offset+22, &bayer_pattern);
		}
		else if (g_str_equal(identifier, "WBG"))
		{
			switch(bayer_pattern)
			{
				case 0x1: /* RGGB */
					raw_get_ushort(rawfile, offset+4, &ushort_temp1);
					meta->cam_mul[0] = (gdouble) ushort_temp1;
					raw_get_ushort(rawfile, offset+6, &ushort_temp1);
					meta->cam_mul[1] = (gdouble) ushort_temp1;
					raw_get_ushort(rawfile, offset+8, &ushort_temp1);
					meta->cam_mul[3] = (gdouble) ushort_temp1;
					raw_get_ushort(rawfile, offset+10, &ushort_temp1);
					meta->cam_mul[2] = (gdouble) ushort_temp1;
					break;
				case 0x4: /* GBRG */
					raw_get_ushort(rawfile, offset+4, &ushort_temp1);
					meta->cam_mul[1] = (gdouble) ushort_temp1;
					raw_get_ushort(rawfile, offset+6, &ushort_temp1);
					meta->cam_mul[2] = (gdouble) ushort_temp1;
					raw_get_ushort(rawfile, offset+8, &ushort_temp1);
					meta->cam_mul[0] = (gdouble) ushort_temp1;
					raw_get_ushort(rawfile, offset+10, &ushort_temp1);
					meta->cam_mul[3] = (gdouble) ushort_temp1;
					break;
				default:
					g_warning("unknown bayer pattern %x for %s", bayer_pattern, meta->model_ascii);
			}
			rs_metadata_normalize_wb(meta);
			break;
		}

		/* Move past block content */
		offset += len;
	}
	return;
}

static gboolean
mrw_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	GdkPixbuf *pixbuf=NULL, *pixbuf2=NULL;
	guint start=0, length=0;

	rs_io_lock();
	raw_mrw_walker(rawfile, offset, meta);
	rs_io_unlock();

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

		pixbuf = raw_get_pixbuf(rawfile, start, length);

		/* Some Minolta's replace byte 0 with something else than 0xff */
		if (!pixbuf)
		{
			length--;

			thumbbuffer = g_malloc(length);
			thumbbuffer[0] = '\xff';
			rs_io_lock();
			raw_strcpy(rawfile, start+1, thumbbuffer+1, length-1);
			rs_io_unlock();
			pl = gdk_pixbuf_loader_new();
			gdk_pixbuf_loader_write(pl, thumbbuffer, length, NULL);
			pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
			gdk_pixbuf_loader_close(pl, NULL);
			g_free(thumbbuffer);
		}
		
		if (pixbuf==NULL) return TRUE;
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
	return TRUE;
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_meta_loader(".mrw", "Minolta raw", mrw_load_meta, 10, RS_LOADER_FLAGS_RAW);
}

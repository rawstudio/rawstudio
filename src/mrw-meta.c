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
#include "rawfile.h"
#include "mrw-meta.h"
#include "tiff-meta.h"

static void raw_mrw_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta);

static void
raw_mrw_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	guint rawstart=0;
	guint tag=0, len=0;
	gushort ushort_temp1=0;

	guint next, toffset;
	gushort ifd_num;

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
				raw_init_file_tiff(rawfile, offset);
				toffset = rawfile->first_ifd_offset;
				do {
					if (!raw_get_ushort(rawfile, toffset, &ifd_num)) break;
					if (!raw_get_uint(rawfile, toffset+2+ifd_num*12, &next)) break;
					raw_ifd_walker(rawfile, toffset, meta);
					if (toffset == next) break; /* avoid infinite loops */
					toffset = next;
				} while (next>0);
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

void
rs_mrw_load_meta(const gchar *filename, RS_METADATA *meta)
{
	RAWFILE *rawfile;
	raw_init();
	if (!(rawfile = raw_open_file(filename)))
		return;

	raw_mrw_walker(rawfile, 0, meta);

	raw_close_file(rawfile);
	return;
}

GdkPixbuf *
rs_mrw_load_thumb(const gchar *src)
{
	RAWFILE *rawfile;
	GdkPixbuf *pixbuf=NULL, *pixbuf2=NULL;
	RS_METADATA meta;
	gchar *thumbname;
	guint start=0, length=0;

	raw_init();

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

	meta.thumbnail_start = 0;
	meta.thumbnail_length = 0;
	meta.preview_start = 0;
	meta.preview_length = 0;

	if (!(rawfile = raw_open_file(src)))
		return(NULL);
	raw_mrw_walker(rawfile, 0, &meta);

	if ((meta.thumbnail_start>0) && (meta.thumbnail_length>0))
	{
		start = meta.thumbnail_start;
		length = meta.thumbnail_length;
	}

	else if ((meta.preview_start>0) && (meta.preview_length>0))
	{
		start = meta.preview_start;
		length = meta.preview_length;
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
		
		if (pixbuf==NULL) return(NULL);
		ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
		if (ratio>1.0)
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
		else
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = pixbuf2;
		switch (meta.orientation)
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
		if (thumbname)
			gdk_pixbuf_save(pixbuf, thumbname, "png", NULL, NULL);
	}

	raw_close_file(rawfile);

	return(pixbuf);
}

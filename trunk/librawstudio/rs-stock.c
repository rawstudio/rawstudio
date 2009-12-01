/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#include <rawstudio.h>
#include <config.h>

static GtkIconFactory *rs_icon_factory = NULL;

static GtkStockItem rs_stock_items[] = {
	{ RS_STOCK_CROP, NULL, 0, 0, NULL },
	{ RS_STOCK_ROTATE, NULL, 0, 0, NULL },
	{ RS_STOCK_COLOR_PICKER, NULL, 0, 0, NULL },
	{ RS_STOCK_ROTATE_CLOCKWISE, "rotate cw", 0, 0, NULL },
	{ RS_STOCK_ROTATE_COUNTER_CLOCKWISE, "rotate ccw", 0, 0, NULL },
	{ RS_STOCK_FLIP, "flip", 0, 0, NULL },
	{ RS_STOCK_MIRROR, "mirror", 0, 0, NULL },
};

typedef struct _RSCursorItem RSCursorItem;

struct _RSCursorItem {
	const gchar *filename;
	const gint x_hot, y_hot;
};

static RSCursorItem rs_cursor_items[] = {
	{ "cursor-crop.png", 8, 8},
	{ "cursor-rotate.png", 8, 8},
	{ "cursor-color-picker.png", 8, 8},
};

static void
add_stock_icon (const gchar  *stock_id, const GdkPixbuf *pixbuf)
{
	GtkIconSource *source;
	GtkIconSet    *set;

	source = gtk_icon_source_new ();

	gtk_icon_source_set_size (source, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_icon_source_set_size_wildcarded (source, TRUE);

	gtk_icon_source_set_pixbuf (source, GDK_PIXBUF(pixbuf));
	g_object_unref (GDK_PIXBUF(pixbuf));

	set = gtk_icon_set_new ();

	gtk_icon_set_add_source (set, source);
	gtk_icon_source_free (source);

	gtk_icon_factory_add (rs_icon_factory, stock_id, set);

	gtk_icon_set_unref (set);
}

void
rs_stock_init(void)
{
	rs_icon_factory = gtk_icon_factory_new ();

	add_stock_icon (RS_STOCK_CROP, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/tool-crop.png", NULL));
	add_stock_icon (RS_STOCK_ROTATE, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/tool-rotate.png", NULL));
	add_stock_icon (RS_STOCK_COLOR_PICKER, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/tool-color-picker.png", NULL));
	add_stock_icon (RS_STOCK_ROTATE_CLOCKWISE, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_90.png", NULL));
	add_stock_icon (RS_STOCK_ROTATE_COUNTER_CLOCKWISE, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_270.png", NULL));
	add_stock_icon (RS_STOCK_FLIP, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_flip.png", NULL));
	add_stock_icon (RS_STOCK_MIRROR, gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/pixmaps/" PACKAGE "/transform_mirror.png", NULL));

	gtk_icon_factory_add_default (rs_icon_factory);

	gtk_stock_add_static (rs_stock_items, G_N_ELEMENTS (rs_stock_items));
}

GdkCursor* 
rs_cursor_new(GdkDisplay *display, RSCursorType cursor_type)
{
	RSCursorItem *cursor = &rs_cursor_items[cursor_type];
	GdkPixbuf *pixbuf = NULL;

	pixbuf = gdk_pixbuf_new_from_file(g_build_filename (PACKAGE_DATA_DIR "/pixmaps/" PACKAGE, cursor->filename, NULL), NULL);

	return gdk_cursor_new_from_pixbuf(display, pixbuf, cursor->x_hot,cursor->y_hot);
}

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

#ifndef RS_TOOLBOX_H
#define RS_TOOLBOX_H

#include <rawstudio.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "rs-settings.h"
#include "rs-image.h"
#include "rs-photo.h"

G_BEGIN_DECLS

#define RS_TYPE_TOOLBOX rs_toolbox_get_type()
#define RS_TOOLBOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TOOLBOX, RSToolbox))
#define RS_TOOLBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TOOLBOX, RSToolboxClass))
#define RS_IS_TOOLBOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TOOLBOX))
#define RS_IS_TOOLBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_TOOLBOX))
#define RS_TOOLBOX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_TOOLBOX, RSToolboxClass))

typedef struct _RSToolbox RSToolbox;

typedef struct {
	GtkScrolledWindowClass parent_class;
} RSToolboxClass;

GType rs_toolbox_get_type (void);

extern GtkWidget *
rs_toolbox_new (void);

extern GtkWidget *
rs_toolbox_add_widget(RSToolbox *toolbox, GtkWidget *widget, const gchar *title);

extern void
rs_toolbox_set_photo(RSToolbox *toolbox, RS_PHOTO *photo);

extern gint
rs_toolbox_get_selected_snapshot(RSToolbox *toolbox);

extern void
rs_toolbox_set_selected_snapshot(RSToolbox *toolbox, const gint snapshot);

extern void
rs_toolbox_set_histogram_input(RSToolbox *toolbox, RSFilter *input, RSColorSpace *display_color_space);

extern void
rs_toolbox_register_actions(RSToolbox *toolbox);

G_END_DECLS

#endif /* RS_TOOLBOX_H */

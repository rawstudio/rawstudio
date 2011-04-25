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

#ifndef RSCOLOR_SPACE_SELECTOR_H
#define RSCOLOR_SPACE_SELECTOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_COLOR_SPACE_SELECTOR rs_color_space_selector_get_type()
#define RS_COLOR_SPACE_SELECTOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_COLOR_SPACE_SELECTOR, RSColorSpaceSelector))
#define RS_COLOR_SPACE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_COLOR_SPACE_SELECTOR, RSColorSpaceSelectorClass))
#define RS_IS_COLOR_SPACE_SELECTOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_COLOR_SPACE_SELECTOR))
#define RS_IS_COLOR_SPACE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_COLOR_SPACE_SELECTOR))
#define RS_COLOR_SPACE_SELECTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_COLOR_SPACE_SELECTOR, RSColorSpaceSelectorClass))

typedef struct _RSColorSpaceSelectorPrivate RSColorSpaceSelectorPrivate;

typedef struct {
	GtkComboBox parent;

	RSColorSpaceSelectorPrivate *priv;
} RSColorSpaceSelector;

typedef struct {
	GtkComboBoxClass parent_class;
} RSColorSpaceSelectorClass;

GType
rs_color_space_selector_get_type(void);

GtkWidget *
rs_color_space_selector_new(void);

void
rs_color_space_selector_add_all(RSColorSpaceSelector *selector);

void
rs_color_space_selector_add_single(RSColorSpaceSelector *selector, const gchar* klass_name, const gchar* readable_name, RSColorSpace* space);

RSColorSpace *
rs_color_space_selector_set_selected_by_name(RSColorSpaceSelector *selector, const gchar *type_name);

G_END_DECLS

#endif /* RSCOLOR_SPACE_SELECTOR_H */

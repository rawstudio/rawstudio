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

#ifndef RS_DIR_SELECTOR_H
#define RS_DIR_SELECTOR_H

#include <gtk/gtk.h>

typedef struct _RSDirSelector RSDirSelector;
typedef struct _RSDirSelectorClass RSDirSelectorClass;

struct _RSDirSelectorClass
{
	GtkScrolledWindowClass parent_class;
};

GType rs_dir_selector_get_type (void);

/**
 * Creates a new RSDirSelection widget
 * @return A new RSDirSelector
 */
extern GtkWidget *rs_dir_selector_new(void);
extern void rs_dir_selector_set_root(RSDirSelector *selector, const gchar *root);
extern void rs_dir_selector_expand_path(RSDirSelector *selector, const gchar *expand);

#define RS_DIR_SELECTOR_TYPE_WIDGET             (rs_dir_selector_get_type ())
#define RS_DIR_SELECTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_DIR_SELECTOR_TYPE_WIDGET, RSDirSelector))
#define RS_DIR_SELECTOR_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), RS_DIR_SELECTOR_WIDGET, RSDirSelectorClass))
#define RS_IS_DIR_SELECTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_DIR_SELECTOR_TYPE_WIDGET))
#define RS_IS_DIR_SELECTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), RS_DIR_SELECTOR_TYPE_WIDGET))
#define RS_DIR_SELECTOR_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_DIR_SELECTOR_TYPE_WIDGET, RSDirSelector))

#endif /* RS_DIR_SELECTOR_H */

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

#ifndef RS_PROFILE_SELECTOR_H
#define RS_PROFILE_SELECTOR_H

#include <rs-dcp-file.h>
#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_PROFILE_SELECTOR rs_profile_selector_get_type()
#define RS_PROFILE_SELECTOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PROFILE_SELECTOR, RSProfileSelector))
#define RS_PROFILE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PROFILE_SELECTOR, RSProfileSelectorClass))
#define RS_IS_PROFILE_SELECTOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PROFILE_SELECTOR))
#define RS_IS_PROFILE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_PROFILE_SELECTOR))
#define RS_PROFILE_SELECTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_PROFILE_SELECTOR, RSProfileSelectorClass))

typedef struct {
	GtkComboBox parent;

	gpointer selected;
} RSProfileSelector;

typedef struct {
	GtkComboBoxClass parent_class;
} RSProfileSelectorClass;

GType rs_profile_selector_get_type(void);

RSProfileSelector *
rs_profile_selector_new(void);

void
rs_profile_selector_select_profile(RSProfileSelector *selector, gpointer profile);

void
rs_profile_selector_set_model_filter(RSProfileSelector *selector, GtkTreeModelFilter *filter);

G_END_DECLS

#endif /* RS_PROFILE_SELECTOR_H */

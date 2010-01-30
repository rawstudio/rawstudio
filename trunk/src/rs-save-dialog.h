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

#ifndef RS_SAVE_DIALOG_H
#define RS_SAVE_DIALOG_H

#include <rawstudio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_SAVE_DIALOG rs_save_dialog_get_type()
#define RS_SAVE_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_SAVE_DIALOG, RSSaveDialog))
#define RS_SAVE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_SAVE_DIALOG, RSSaveDialogClass))
#define RS_IS_SAVE_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_SAVE_DIALOG))
#define RS_IS_SAVE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_SAVE_DIALOG))
#define RS_SAVE_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_SAVE_DIALOG, RSSaveDialogClass))

typedef struct {
	GtkWindow parent;

	RSOutput *output;
	GtkWidget *vbox;
	GtkWidget *chooser;
	RS_CONFBOX *type_box;
	GtkWidget *file_pref;
	GtkWidget *pref_bin;
	gdouble w_original;
	gdouble h_original;
	gboolean keep_aspect;
	GtkSpinButton *w_spin;
	GtkSpinButton *h_spin;
	GtkSpinButton *p_spin;
	gulong w_signal;
	gulong h_signal;
	gint save_width;
	gint save_height;

	gboolean dispose_has_run;
	RSFilter *finput;
	RSFilter *fdemosaic;
	RSFilter *flensfun;
	RSFilter *ftransform_input;
	RSFilter *frotate;
	RSFilter *fcrop;
	RSFilter *fresample;
	RSFilter *fdcp;
	RSFilter *fdenoise;
	RSFilter *ftransform_display;
	RSFilter *fend;

	RS_PHOTO *photo;
	gint snapshot;
} RSSaveDialog;

typedef struct {
	GtkWindowClass parent_class;
} RSSaveDialogClass;

GType rs_save_dialog_get_type (void);

RSSaveDialog* rs_save_dialog_new (void);

void rs_save_dialog_set_photo(RSSaveDialog *dialog, RS_PHOTO *photo, gint snapshot);

G_END_DECLS

#endif /* RS_SAVE_DIALOG_H */

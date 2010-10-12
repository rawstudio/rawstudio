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

#ifndef RS_TAG_GUI_H
#define RS_TAG_GUI_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "application.h"
#include <rawstudio.h>

G_BEGIN_DECLS

#define RS_TYPE_LIBRARY rs_library_get_type()
#define RS_LIBRARY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_LIBRARY, RSLibrary))
#define RS_LIBRARY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_LIBRARY, RSLibraryClass))
#define RS_IS_LIBRARY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_LIBRARY))
#define RS_IS_LIBRARY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_LIBRARY))
#define RS_LIBRARY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_LIBRARY, RSLibraryClass))


GtkWidget *rs_tag_gui_toolbox_new(RSLibrary *library, RSStore *store);
GtkWidget *rs_library_tag_entry_new(RSLibrary *library);

G_END_DECLS

#endif /* RS_TAG_GUI_H */

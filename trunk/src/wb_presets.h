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
#include <glib.h>
#include <gettext.h>
#include "rawstudio.h"
#include "color.h"

// wb_data struct grabbed from UFraw (se copyright notice in wb_presets.c)
typedef struct {
    gchar *make;
    gchar *model;
    gchar *name;
    gint tuning;
    gdouble channel[4];
} wb_data;

enum {
	WB_PRESET_NAME,
	WB_PRESET_R,
	WB_PRESET_G,
	WB_PRESET_B,
	WB_PRESET_G2,
};

extern const gint wb_preset_count;
extern const wb_data wb_preset[];

extern GtkWidget *wb_preset_box_new(RS_BLOB *rs, gint n);
extern void wb_preset_box_set_make_model(GtkWidget *wb_preset_box[],
								  gchar *camera_make, gchar *camera_model);
extern void wb_preset_box_set(GtkWidget *wb_preset_box, gint selection);

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

#ifndef RS_ACTIONS_H
#define RS_ACTIONS_H

/**
 * Get the core action group
 * @return A pointer to the core action group
 */
extern GtkActionGroup *rs_get_core_action_group(RS_BLOB *rs);

/**
 * Set sensivity of an action
 * @param name The name of the action
 * @param sensitive The sensivity of the action
 */
extern void rs_core_action_group_set_sensivity(const gchar *name, gboolean sensitive);

/**
 * Activate an action
 * @param name The action to activate
 */
extern void rs_core_action_group_activate(const gchar *name);

/**
 * Set visibility of an action
 * @param name The name of the action
 * @param visibility The visibilty of the action
 */
extern void rs_core_action_group_set_visibility(const gchar *name, gboolean visibility);

/**
 * Add actions to global action group, see documentation for gtk_action_group_add_actions
 */
extern void rs_core_action_group_add_actions(const GtkActionEntry *entries, guint n_entries, gpointer user_data);

/**
 * Add radio action to global action group, see documentation for gtk_action_group_add_radio_actions()
 */
extern void rs_core_action_group_add_radio_actions(const GtkRadioActionEntry *entries, guint n_entries, gint value, GCallback on_change, gpointer user_data);

/**
 * Get a GtkAction by name
 * @param name The name of the action
 * @return A GtkAction or NULL (should not be unreffed)
 */
extern GtkAction *rs_core_action_group_get_action(const gchar *name);

#endif /* RS_ACTIONS_H */

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


#include "rs-library.h"
#include "rs-tag-gui.h"
#include "rs-store.h"
#include "gtk-interface.h"
#include "conf_interface.h"
#include "config.h"
#include "gettext.h"

static GtkWidget *tag_search_entry = NULL;


/* Carrier used for a few callbacks */
typedef struct {
	RSLibrary *library;
	RSStore *store;
} cb_carrier;

static void
load_photos(gpointer data, gpointer user_data) {
	RSStore *store = user_data;
	gchar *text = (gchar *) data;
	/* FIXME: Change this to be signal based at some point */
	rs_store_load_file(store, text);
	g_free(text);
}

static void 
search_changed(GtkEntry *entry, gpointer user_data)
{
	cb_carrier *carrier = user_data;
	const gchar *text = gtk_entry_get_text(entry);

	GList *tags = rs_split_string(text, " ");

	GList *photos = rs_library_search(carrier->library, tags);

	/* FIXME: deselect all photos in store */
	rs_store_remove(carrier->store, NULL, NULL);
	g_list_foreach(photos, load_photos, carrier->store);

	/* Fix size of iconview */
	rs_store_set_iconview_size(carrier->store, g_list_length(photos));

	GString *window_title = g_string_new("");
	g_string_printf(window_title, _("Tag search [%s]"), text);
	rs_window_set_title(window_title->str);
	g_string_free(window_title, TRUE);
	
	rs_conf_set_string(CONF_LIBRARY_TAG_SEARCH, text);
	rs_conf_unset(CONF_LWD);

	g_list_free(photos);
	g_list_free(tags);
}

GtkWidget *
rs_tag_gui_toolbox_new(RSLibrary *library, RSStore *store)
{
	g_assert(RS_IS_LIBRARY(library));
	g_assert(RS_IS_STORE(store));

	cb_carrier *carrier = g_new(cb_carrier, 1);
	GtkWidget *box = gtk_vbox_new(FALSE, 0);
	tag_search_entry = rs_library_tag_entry_new(library);

	carrier->library = library;
	carrier->store = store;
	g_signal_connect (tag_search_entry, "changed",
			  G_CALLBACK (search_changed), carrier);
	gtk_box_pack_start (GTK_BOX(box), tag_search_entry, FALSE, TRUE, 0);
	/* FIXME: Make sure to free carrier at some point */

	return box;
}

GtkWidget *
rs_library_tag_entry_new(RSLibrary *library)
{
	g_assert(RS_IS_LIBRARY(library));

	gboolean
	selected(GtkEntryCompletion *completion, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
	{
		GtkEntry *entry = GTK_ENTRY(gtk_entry_completion_get_entry(completion));
		gchar *current_text, *new_text;
		gchar *tag;
		gchar *target;

		gtk_tree_model_get (model, iter, 0, &tag, -1);
		current_text = g_strdup(gtk_entry_get_text(entry));

		/* Try to find the last tag entered */
		target = g_utf8_strrchr(current_text, -1, ' ');
		if (target)
			target++;
		else
			target = current_text;

		/* End the string just as the last tag starts */
		*target = '\0';

		/* Append selected tag */
		new_text = g_strconcat(current_text, tag, NULL);

		gtk_entry_set_text(entry, new_text);
		gtk_editable_set_position(GTK_EDITABLE(entry), -1);

		g_free(current_text);
		g_free(new_text);

		return TRUE;
	}

	gboolean
	match(GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
	{
		gboolean found = FALSE;
		GtkTreeModel *model;
		const gchar *needle;
		gchar *needle_normalized = NULL;
		gchar *needle_case_normalized = NULL;
		gchar *tag = NULL;
		gchar *tag_normalized = NULL;
		gchar *tag_case_normalized = NULL;

		/* Look for last tag if found */
		needle = g_utf8_strrchr(key, ' ',-1);
		if (needle)
			needle += 1;
		else
			needle = key;

		needle_normalized = g_utf8_normalize(needle, -1, G_NORMALIZE_ALL);
		if (needle_normalized)
		{
			needle_case_normalized = g_utf8_casefold(needle_normalized, -1);

			model = gtk_entry_completion_get_model (completion);
			gtk_tree_model_get (model, iter, 0, &tag, -1);
			if (tag)
			{
				tag_normalized = g_utf8_normalize(tag, -1, G_NORMALIZE_ALL);
				if (tag_normalized)
				{
					tag_case_normalized = g_utf8_casefold(tag_normalized, -1);

					if (g_str_has_prefix(tag_case_normalized, needle_case_normalized))
						found = TRUE;
				}
			}

		}
		g_free(needle_normalized);
		g_free(needle_case_normalized);
		g_free(tag);
		g_free(tag_normalized);
		g_free(tag_case_normalized);

		return found;
	}

	GtkWidget *entry = gtk_entry_new();
	GtkEntryCompletion *completion = gtk_entry_completion_new();
	GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
	GList *all_tags = rs_library_find_tag(library, "");
	GtkTreeIter iter;

	GList *node;

	for (node = g_list_first(all_tags); node != NULL; node = g_list_next(node))
	{
		gchar *tag = node->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, tag, -1);

		g_free(tag);
	}

	gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
	gtk_entry_completion_set_text_column(completion, 0);
	gtk_entry_completion_set_match_func(completion, match, NULL, NULL);
	g_signal_connect(completion, "match-selected", G_CALLBACK(selected), NULL);
	gtk_entry_set_completion (GTK_ENTRY(entry), completion);

	g_list_free(all_tags);

	return entry;
}

gboolean
rs_library_set_tag_search(gchar *str)
{
	if (!str)
		return FALSE;
	gtk_entry_set_text(GTK_ENTRY(tag_search_entry), str);
	return TRUE;
}

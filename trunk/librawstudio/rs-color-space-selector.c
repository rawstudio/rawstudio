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

#include <glib.h>
#include <gtk/gtk.h>
#include "rawstudio.h"
#include "rs-color-space-selector.h"

G_DEFINE_TYPE(RSColorSpaceSelector, rs_color_space_selector, GTK_TYPE_COMBO_BOX)

enum {
	COLUMN_TEXT,
	COLUMN_TYPENAME,
	COLUMN_COLORSPACE,
	NUM_COLUMNS
};

enum {
	SELECTED_SIGNAL,
	SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

#define COLOR_SPACE_SELECTOR_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RS_TYPE_COLOR_SPACE_SELECTOR, RSColorSpaceSelectorPrivate))

struct _RSColorSpaceSelectorPrivate {
	GtkTreeModel *model;

	gboolean dispose_has_run;
};

static void
rs_color_space_selector_dispose(GObject *object)
{
	RSColorSpaceSelector *selector = RS_COLOR_SPACE_SELECTOR(object);

	if (!selector->priv->dispose_has_run)
	{
		selector->priv->dispose_has_run = TRUE;
	}

	G_OBJECT_CLASS(rs_color_space_selector_parent_class)->dispose(object);
}

static void
changed(GtkComboBox *combo_box)
{
	GtkTreeIter iter;
	RSColorSpaceSelector *selector = RS_COLOR_SPACE_SELECTOR(combo_box);
	const gchar *type = NULL;

	if (gtk_combo_box_get_active_iter(combo_box, &iter))
	{
		gtk_tree_model_get(selector->priv->model, &iter, COLUMN_TYPENAME, &type, -1);

		if (type)
			g_signal_emit(selector, signals[SELECTED_SIGNAL], 0, type);
	}
}

static void
rs_color_space_selector_class_init(RSColorSpaceSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS(klass);

	g_type_class_add_private(klass, sizeof(RSColorSpaceSelectorPrivate));

	object_class->dispose = rs_color_space_selector_dispose;

	combo_class->changed = changed;

	signals[SELECTED_SIGNAL] = g_signal_new("colorspace-selected",
		G_TYPE_FROM_CLASS(klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
rs_color_space_selector_init(RSColorSpaceSelector *selector)
{
	GtkComboBox *combo = GTK_COMBO_BOX(selector);

	selector->priv = COLOR_SPACE_SELECTOR_PRIVATE(selector);

	selector->priv->model = GTK_TREE_MODEL(gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, RS_TYPE_COLOR_SPACE));

	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE );
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
		"markup", COLUMN_TEXT,
		NULL);

	gtk_combo_box_set_model(combo, selector->priv->model);

}

GtkWidget *
rs_color_space_selector_new(void)
{
	return g_object_new(RS_TYPE_COLOR_SPACE_SELECTOR, NULL);
}

void
rs_color_space_selector_add_all(RSColorSpaceSelector *selector)
{
	GType *spaces;
	guint n_spaces, i;
	GtkTreeIter iter;

	g_return_if_fail(RS_IS_COLOR_SPACE_SELECTOR(selector));

	spaces = g_type_children (RS_TYPE_COLOR_SPACE, &n_spaces);
	for (i = 0; i < n_spaces; i++)
	{
		RSColorSpaceClass *klass;
		klass = g_type_class_ref(spaces[i]);

		if (klass->is_internal)
		{
			gtk_list_store_append(GTK_LIST_STORE(selector->priv->model), &iter);
			gtk_list_store_set(GTK_LIST_STORE(selector->priv->model), &iter,
				COLUMN_TEXT, klass->name,
					COLUMN_TYPENAME, g_type_name(spaces[i]),
				COLUMN_COLORSPACE, rs_color_space_new_singleton(g_type_name(spaces[i])),
				-1);
		}
		g_type_class_unref(klass);
	}
}

void
rs_color_space_selector_add_single(RSColorSpaceSelector *selector, const gchar* klass_name, const gchar* readable_name, RSColorSpace* space)
{
	GtkTreeIter iter;

	g_return_if_fail(RS_IS_COLOR_SPACE_SELECTOR(selector));
	g_return_if_fail(klass_name != NULL);
	g_return_if_fail(readable_name != NULL);

	gtk_list_store_append(GTK_LIST_STORE(selector->priv->model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(selector->priv->model), &iter,
			COLUMN_TEXT, readable_name,
			COLUMN_TYPENAME, klass_name,
			COLUMN_COLORSPACE, space,
			-1);
}

RSColorSpace *
rs_color_space_selector_set_selected_by_name(RSColorSpaceSelector *selector, const gchar *type_name)
{

	
	RSColorSpace *ret = NULL;
	GtkTreeIter iter;
	gchar *type_name_haystack;

	g_return_val_if_fail(RS_IS_COLOR_SPACE_SELECTOR(selector), NULL);
	g_return_val_if_fail(type_name != NULL, NULL);

	if (gtk_tree_model_get_iter_first(selector->priv->model, &iter))
	{
		do {
			gtk_tree_model_get(selector->priv->model, &iter,
							    COLUMN_TYPENAME, &type_name_haystack,
							    COLUMN_COLORSPACE, &ret,
								-1);
			if (type_name_haystack)
			{
				if (g_strcmp0(type_name_haystack, type_name) == 0)
				{
					gtk_combo_box_set_active_iter(GTK_COMBO_BOX(selector), &iter);
					break;
				}
				g_free(type_name_haystack);
			}
		} while (gtk_tree_model_iter_next(selector->priv->model, &iter));
	}

	return ret;
}
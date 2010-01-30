#include "config.h"
#include "gettext.h"
#include "rs-profile-selector.h"
#include "rs-icc-profile.h"
#include "rs-profile-factory-model.h"

G_DEFINE_TYPE(RSProfileSelector, rs_profile_selector, GTK_TYPE_COMBO_BOX)

enum {
	DCP_SELECTED_SIGNAL,
	ICC_SELECTED_SIGNAL,
	ADD_SELECTED_SIGNAL,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = {0};

enum {
	COLUMN_NAME,
	COLUMN_POINTER,
	COLUMN_TYPE,
	NUM_COLUMNS
};

static void
rs_profile_selector_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_profile_selector_parent_class)->dispose(object);
}

static void
rs_profile_selector_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_profile_selector_parent_class)->finalize(object);
}

static void
rs_profile_selector_class_init(RSProfileSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	signals[DCP_SELECTED_SIGNAL] = g_signal_new("dcp-selected",
		G_TYPE_FROM_CLASS(klass),
	    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL,
		NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1, RS_TYPE_DCP_FILE);

	signals[ICC_SELECTED_SIGNAL] = g_signal_new("icc-selected",
		G_TYPE_FROM_CLASS(klass),
	    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL,
		NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1, RS_TYPE_ICC_PROFILE);

	signals[ADD_SELECTED_SIGNAL] = g_signal_new("add-selected",
		G_TYPE_FROM_CLASS(klass),
	    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL,
		NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

	object_class->dispose = rs_profile_selector_dispose;
	object_class->finalize = rs_profile_selector_finalize;
}

static void
changed(GtkComboBox *combo, gpointer data)
{
	GtkTreeIter iter, child_iter;
	gint type;
	gpointer profile;
	GtkTreeModel *model, *child_model;

	if (gtk_combo_box_get_active_iter(combo, &iter))
	{
		model = gtk_combo_box_get_model(combo);

		/* Find the original iter before sorting */
		gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT(model), &child_iter, &iter);
		child_model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(model));

		gtk_tree_model_get(child_model, &child_iter,
			COLUMN_POINTER, &profile,
			COLUMN_TYPE, &type,
			-1);
		RSProfileSelector *selector = RS_PROFILE_SELECTOR(combo);

		if (type == FACTORY_MODEL_TYPE_DCP)
		{
			g_signal_emit(RS_PROFILE_SELECTOR(combo), signals[DCP_SELECTED_SIGNAL], 0, profile);
			selector->selected = profile;
		}
		else if (type == FACTORY_MODEL_TYPE_ICC)
		{
			g_signal_emit(RS_PROFILE_SELECTOR(combo), signals[ICC_SELECTED_SIGNAL], 0, profile);
			selector->selected = profile;
		}
		else if (type == FACTORY_MODEL_TYPE_ADD)
		{
			/* If the user selects "add profile", we should not stay at this selection */
			rs_profile_selector_select_profile(selector, selector->selected);

			g_signal_emit(RS_PROFILE_SELECTOR(combo), signals[ADD_SELECTED_SIGNAL], 0, NULL);
		}
	}
}

static gboolean
separator_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint type = 0;

	gtk_tree_model_get(model, iter,
		COLUMN_TYPE, &type,
		-1);

	return (type == FACTORY_MODEL_TYPE_SEP);
}

static void
rs_profile_selector_init(RSProfileSelector *selector)
{
	GtkComboBox *combo = GTK_COMBO_BOX(selector);

	g_signal_connect(combo, "changed", G_CALLBACK(changed), NULL);

	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE );
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
		"markup", 0,
		NULL);

	gtk_combo_box_set_row_separator_func(combo, separator_func, NULL, NULL);
}

RSProfileSelector *
rs_profile_selector_new(void)
{
	return g_object_new(RS_TYPE_PROFILE_SELECTOR, NULL);
}

void
rs_profile_selector_select_profile(RSProfileSelector *selector, gpointer profile)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer current = NULL;

	g_assert(RS_IS_PROFILE_SELECTOR(selector));

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(selector));

	if (gtk_tree_model_get_iter_first(model, &iter))
		do {
			gtk_tree_model_get(model, &iter,
				COLUMN_POINTER, &current,
				-1);
			if (current == profile)
			{
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(selector), &iter);
				break;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
}

static void
modify_func(GtkTreeModel *filter, GtkTreeIter *iter, GValue *value, gint column, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter child_iter;

	gint type;
	gpointer profile;
	gchar *str;

	g_object_get(filter, "child-model", &model, NULL);
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter), &child_iter, iter);
	gtk_tree_model_get(model, &child_iter,
		FACTORY_MODEL_COLUMN_TYPE, &type,
		FACTORY_MODEL_COLUMN_PROFILE, &profile,
		-1);

	if (column == COLUMN_TYPE)
		g_value_set_int(value, type);
	else if (column == COLUMN_POINTER)
		g_value_set_pointer(value, profile);
	else if (column == COLUMN_NAME)
	{
		switch(type)
		{
			case FACTORY_MODEL_TYPE_DCP:
				str = g_strdup_printf("%s <small><small>(dcp)</small></small>", rs_dcp_file_get_name(profile));
				g_value_set_string(value, str);
				g_free(str);
				break;
			case FACTORY_MODEL_TYPE_ICC:
				str = g_strdup_printf("%s <small><small>(icc)</small></small>", "FIXME-name");
				g_value_set_string(value, str);
				g_free(str);
				break;
			case FACTORY_MODEL_TYPE_ADD:
				g_value_set_string(value, _("Add profile ..."));
				break;
		}
	}
}

static gint
sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	gint a_type, b_type;
	gchar *a_name, *b_name;


	/* You never know */
	if (a == b) return 0;
	if (!a) return 1;
	if (!b) return -1;

	gtk_tree_model_get(model, a,
		COLUMN_TYPE, &a_type,
		-1);
	gtk_tree_model_get(model, b,
		COLUMN_TYPE, &b_type,
		-1);

	if (a_type < b_type)
		return -1;
	else if (a_type > b_type)
		return 1;

	/* If we get here, both a and b have same type, sort by name */

	gtk_tree_model_get(model, a,
		COLUMN_NAME, &a_name,
		-1);

	gtk_tree_model_get(model, b,
		COLUMN_NAME, &b_name,
		-1);

	gint ret = g_strcmp0(a_name, b_name);

	g_free(a_name);
	g_free(b_name);

	return ret;
}

void
rs_profile_selector_set_model_filter(RSProfileSelector *selector, GtkTreeModelFilter *filter)
{
	g_assert(RS_IS_PROFILE_SELECTOR(selector));
	g_assert(GTK_IS_TREE_MODEL_FILTER(filter));

	/* We set up a modify function, to write correct names for the combobox */
	GType types[NUM_COLUMNS] = {G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT};
	gtk_tree_model_filter_set_modify_func(filter, NUM_COLUMNS, types, modify_func, NULL, NULL);

	/* ort the damn thing, we do it this late to avoid sorting the complete list */
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(filter)));
	gtk_tree_sortable_set_default_sort_func(sortable, sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	gtk_combo_box_set_model(GTK_COMBO_BOX(selector), GTK_TREE_MODEL(sortable));
}

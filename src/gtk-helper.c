/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <config.h>
#include "rawstudio.h"
#include "conf_interface.h"
#include "gtk-interface.h"
#include "filename.h"
#include "gtk-helper.h"
#include "rs-cms.h"
#include "rs-preview-widget.h"
#include <gettext.h>
#include <lcms.h>

struct _RS_CONFBOX
{
	GtkWidget *widget;
	GtkListStore *model;
	const gchar *conf_key;
	gpointer user_data;
	void (*callback)(gpointer active, gpointer user_data);
};

static void gui_confbox_changed(GtkComboBox *filetype_combo, gpointer callback_data);
static gboolean gui_confbox_deleted(GtkWidget *widget, GdkEvent *event, gpointer callback_data);
static gboolean gui_confbox_select_value(RS_CONFBOX *confbox, gchar *value);
static void gui_cms_in_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data);
static void gui_cms_di_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data);
static void gui_cms_ex_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data);
static void gui_cms_intent_combobox_changed(GtkComboBox *combobox, gpointer user_data);
static void gui_cms_in_profile_button_clicked(GtkButton *button, gpointer user_data);
static void gui_cms_di_profile_button_clicked(GtkButton *button, gpointer user_data);
static void gui_cms_ex_profile_button_clicked(GtkButton *button, gpointer user_data);
static gboolean rs_gtk_tree_model_count_helper(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static gint rs_gtk_tree_model_count(GtkTreeModel *model);

static const gchar *color_profiles[] = {
	"*.icc", 
	"*.icm", 
	"*.ICC", 
	"*.ICM", 
	NULL
};

enum {
	COMBO_CONF_ID = 0,
	COMBO_TEXT,
	COMBO_PTR,
	COMBO_ROWS,
};

static void
gui_confbox_changed(GtkComboBox *combo, gpointer callback_data)
{
	RS_CONFBOX *confbox = (RS_CONFBOX *) callback_data;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *conf_id;
	gpointer ptr;

	gtk_combo_box_get_active_iter(GTK_COMBO_BOX(confbox->widget), &iter);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(confbox->widget));
	gtk_tree_model_get(model, &iter,
		COMBO_CONF_ID, &conf_id,
		COMBO_PTR, &ptr,
		-1);
	rs_conf_set_string(confbox->conf_key, conf_id);

	if (confbox->callback)
		confbox->callback(ptr, confbox->user_data);
	return;
}

static gboolean
gui_confbox_deleted(GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	RS_CONFBOX *confbox = (RS_CONFBOX *) callback_data;
	gui_confbox_destroy(confbox);

	return(TRUE);
}

gpointer
gui_confbox_get_active(RS_CONFBOX *confbox)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gpointer ptr;

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(confbox->widget), &iter))
	{
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(confbox->widget));
		gtk_tree_model_get(model, &iter,
			COMBO_PTR, &ptr,
			-1);
		return(ptr);
	}
	else
		return(NULL);
}

void
gui_confbox_add_entry(RS_CONFBOX *confbox, const gchar *conf_id, const gchar *text, gpointer *user_data)
{
	GtkTreeIter iter;
	gtk_list_store_append (confbox->model, &iter);
	gtk_list_store_set (confbox->model, &iter,
		COMBO_CONF_ID, conf_id,
		COMBO_TEXT, text, 
		COMBO_PTR, user_data,
		-1);

	return;
}

gboolean
gui_confbox_select_value(RS_CONFBOX *confbox, gchar *value)
{
	gchar *conf_id;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean found = FALSE;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(confbox->widget));
	path = gtk_tree_path_new_first();
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);

	do {
		gtk_tree_model_get(model, &iter, COMBO_CONF_ID, &conf_id, -1);
		if (g_str_equal(conf_id, value))
		{
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(confbox->widget), &iter);
			found = TRUE;
			break;
		}
	} while(gtk_tree_model_iter_next (model, &iter));

	return found;
}

void
gui_confbox_load_conf(RS_CONFBOX *confbox, gchar *default_value)
{
	gchar *value;

	value = rs_conf_get_string(confbox->conf_key);
	if (value)
		if (!gui_confbox_select_value(confbox, value))
			if (!gui_confbox_select_value(confbox, default_value))
		g_free(value);
	return;
}

void
gui_confbox_set_callback(RS_CONFBOX *confbox, gpointer user_data, void (*callback)(gpointer active, gpointer user_data))
{
	confbox->user_data = user_data;
	confbox->callback = callback;
	return;
}

RS_CONFBOX *
gui_confbox_new(const gchar *conf_key)
{
	RS_CONFBOX *confbox;
	GtkCellRenderer *renderer;

	confbox = g_new(RS_CONFBOX, 1);

	confbox->widget = gtk_combo_box_new();
	confbox->model = gtk_list_store_new(COMBO_ROWS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_combo_box_set_model (GTK_COMBO_BOX(confbox->widget), GTK_TREE_MODEL (confbox->model));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (confbox->widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (confbox->widget), renderer,
		"text", COMBO_TEXT, NULL);
	confbox->conf_key = conf_key;
	g_signal_connect ((gpointer) confbox->widget, "changed", G_CALLBACK (gui_confbox_changed), confbox);
	g_signal_connect ((gpointer) confbox->widget, "delete_event", G_CALLBACK(gui_confbox_deleted), confbox);
	confbox->user_data = NULL;
	confbox->callback = NULL;

	return(confbox);
}

void
gui_confbox_destroy(RS_CONFBOX *confbox)
{
	gtk_widget_destroy(confbox->widget);
	g_free(confbox);

	return;
}

GtkWidget *
gui_confbox_get_widget(RS_CONFBOX *confbox)
{
	return(confbox->widget);
}

RS_CONFBOX *
gui_confbox_filetype_new(const gchar *conf_key)
{
	extern RS_FILETYPE *filetypes;
	RS_FILETYPE *filetype = filetypes;
	RS_CONFBOX *confbox;

	confbox = gui_confbox_new(conf_key);

	while(filetype)
	{
		if (filetype->save)
		{
			gui_confbox_add_entry(confbox, filetype->id, filetype->description, (gpointer) filetype);
		}
		filetype = filetype->next;
	}
	gui_confbox_load_conf(confbox, "jpeg");

	return(confbox);
}

void
checkbox_set_conf(GtkToggleButton *togglebutton, gpointer user_data)
{
	const gchar *path = user_data;
	rs_conf_set_boolean(path, togglebutton->active);
	return;
}

GtkWidget *
checkbox_from_conf(const gchar *conf, gchar *label, gboolean default_value)
{
	gboolean check = default_value;
	GtkWidget *checkbox;
	rs_conf_get_boolean(conf, &check);
	checkbox = gtk_check_button_new_with_label(label);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
		check);
	g_signal_connect ((gpointer) checkbox, "toggled",
		G_CALLBACK (checkbox_set_conf), (gpointer) conf);
	return(checkbox);
}

GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private)
{
	GtkWidget *e;
	GtkTooltips *tip;

	tip = gtk_tooltips_new();
	e = gtk_event_box_new();
	gtk_tooltips_set_tip(tip, e, tip_tip, tip_private);
	gtk_widget_show(widget);
	gtk_container_add(GTK_CONTAINER(e), widget);

	return e;
}

void gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private)
{
	GtkTooltips *tip;

	tip = gtk_tooltips_new();
	gtk_tooltips_set_tip(tip, widget, tip_tip, tip_private);
	gtk_widget_show(widget);

	return;
}

void
gui_batch_directory_entry_changed(GtkEntry *entry, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_DIRECTORY, gtk_entry_get_text(entry));
	return;
}

void
gui_batch_filename_entry_changed(GtkComboBox *combobox, gpointer user_data)
{
	rs_conf_set_string(CONF_BATCH_FILENAME, gtk_combo_box_get_active_text(combobox));
	return;
}

void
gui_batch_filetype_combobox_changed(gpointer active, gpointer user_data)
{
	return;
}

void
gui_export_changed_helper(GtkLabel *label)
{
	gchar *parsed = NULL;
	gchar *directory;
	gchar *filename;
	GString *final;
	RS_FILETYPE *filetype;

	directory = rs_conf_get_string(CONF_EXPORT_DIRECTORY);
	filename = rs_conf_get_string(CONF_EXPORT_FILENAME);
	rs_conf_get_filetype(CONF_EXPORT_FILETYPE, &filetype);

	parsed = filename_parse(filename, "filename", 0);

	final = g_string_new("<small>");
	if (directory)
	{
		g_string_append(final, directory);
		g_free(directory);
	}
	g_string_append(final, parsed);
	g_free(parsed);
	g_string_append(final, filetype->ext);
	g_string_append(final, "</small>");

	gtk_label_set_markup(label, final->str);

	g_string_free(final, TRUE);

	return;
}

void
gui_export_directory_entry_changed(GtkEntry *entry, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	rs_conf_set_string(CONF_EXPORT_DIRECTORY, gtk_entry_get_text(entry));

	gui_export_changed_helper(label);

	return;
}

void
gui_export_filename_entry_changed(GtkComboBox *combobox, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL(user_data);
	rs_conf_set_string(CONF_EXPORT_FILENAME, gtk_combo_box_get_active_text(combobox));

	gui_export_changed_helper(label);

	return;
}

static void
cms_enable_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *) user_data;
	rs_conf_set_boolean(CONF_CMS_ENABLED, togglebutton->active);
	rs_cms_enable(rs->cms, togglebutton->active);
	rs_preview_widget_set_cms(RS_PREVIEW_WIDGET(rs->preview), rs_cms_get_transform(rs->cms, PROFILE_DISPLAY));
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

static void
gui_cms_in_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *) user_data;
	gchar *filename;
	rs_conf_set_integer(CONF_CMS_IN_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
	filename = rs_conf_get_cms_profile(RS_CMS_PROFILE_IN);
	rs_cms_set_profile(rs->cms, PROFILE_INPUT, filename);
	g_free(filename);
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

static void
gui_cms_di_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *) user_data;
	gchar *filename;

	rs_conf_set_integer(CONF_CMS_DI_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
	filename = rs_conf_get_cms_profile(RS_CMS_PROFILE_DISPLAY);
	rs_cms_set_profile(rs->cms, PROFILE_DISPLAY, filename);
	g_free(filename);
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

static void
gui_cms_ex_profile_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *) user_data;
	gchar *filename;

	rs_conf_set_integer(CONF_CMS_EX_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
	filename = rs_conf_get_cms_profile(RS_CMS_PROFILE_EXPORT);
	rs_cms_set_profile(rs->cms, PROFILE_EXPORT, filename);
	g_free(filename);
	return;
}

static void
gui_cms_intent_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	RS_BLOB *rs = (RS_BLOB *) user_data;
	gint active = gtk_combo_box_get_active(combobox);
	rs_conf_set_cms_intent(CONF_CMS_INTENT, &active);
	rs_cms_set_intent(rs->cms, active);
	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview));
	return;
}

static gchar *
gui_cms_choose_profile(const gchar *path)
{
	gchar *ret = NULL;
	GtkWidget *fc;
	GtkFileFilter *file_filter_all;
	GtkFileFilter *file_filter_color_profiles;
	gint n;

	fc = gtk_file_chooser_dialog_new (_("Select color profile"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);

	if (path && g_file_test(path, G_FILE_TEST_IS_DIR))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), path);
	else
	{
#define DEAULT_PROFILE_PATH "/usr/share/color/icc"
		if ((g_access(DEAULT_PROFILE_PATH, X_OK)==0)
			&& g_file_test(DEAULT_PROFILE_PATH, G_FILE_TEST_IS_DIR))
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc),
				DEAULT_PROFILE_PATH);
#undef DEAULT_PROFILE_PATH
	}

	file_filter_all = gtk_file_filter_new();
	file_filter_color_profiles = gtk_file_filter_new();

	n=0;
	while(color_profiles[n])
	{
		gtk_file_filter_add_pattern(file_filter_color_profiles, color_profiles[n]);
		n++;
	}

	gtk_file_filter_add_pattern(file_filter_all, "*");

	gtk_file_filter_set_name(file_filter_all, _("All files"));
	gtk_file_filter_set_name(file_filter_color_profiles, _("Color profiles (icc and icm)"));

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_color_profiles);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), file_filter_all);
	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));

		if (rs_cms_is_profile_valid(filename))
			ret = filename;
		else
		{
			GtkWidget *warning = gui_dialog_make_from_text(GTK_STOCK_DIALOG_WARNING,
				_("Not a valid color profile."), 
				_("The file you selected does not appear to be a valid color profile for this version of Rawstudio."));
			GtkWidget *ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
			gtk_dialog_add_action_widget(GTK_DIALOG(warning), ok_button, GTK_RESPONSE_ACCEPT);
			
			gtk_widget_show_all(warning);
			
        	gtk_dialog_run(GTK_DIALOG(warning));
			gtk_widget_destroy(warning);
		}
	}
	gtk_widget_destroy (fc);

	return(ret);
}

static void
gui_cms_in_profile_button_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *combobox = GTK_WIDGET(user_data);
	gchar *filename;

	filename = gui_cms_choose_profile(NULL);

	if (filename)
	{
		GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));
		gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), g_basename(filename));
		rs_conf_add_string_to_list_string(CONF_CMS_IN_PROFILE_LIST, filename);
		rs_conf_set_integer(CONF_CMS_IN_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
		gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), rs_gtk_tree_model_count(model)-1);
	}
	return;
}

static void
gui_cms_di_profile_button_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *combobox = GTK_WIDGET(user_data);
	gchar *filename; 

	filename = gui_cms_choose_profile(NULL);

	if (filename)
	{
		GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));
		gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), g_basename(filename));
		rs_conf_add_string_to_list_string(CONF_CMS_DI_PROFILE_LIST, filename);
		rs_conf_set_integer(CONF_CMS_DI_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
		gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), rs_gtk_tree_model_count(model)-1);
	}
	return;
}

static void
gui_cms_ex_profile_button_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *combobox = GTK_WIDGET(user_data);
	gchar *filename;

	filename = gui_cms_choose_profile(NULL);

	if (filename)
	{
		GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));
		gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), g_basename(filename));
		rs_conf_add_string_to_list_string(CONF_CMS_EX_PROFILE_LIST, filename);
		rs_conf_set_integer(CONF_CMS_EX_PROFILE_SELECTED, gtk_combo_box_get_active(GTK_COMBO_BOX(combobox)));
		gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), rs_gtk_tree_model_count(model)-1);
	}
	return;
}

GtkWidget *
gui_preferences_make_cms_page(RS_BLOB *rs)
{
	gint temp_conf_gint;
	GSList *temp_conf_gslist;
	GSList *temp_new_gslist = NULL;
	gchar *temp_conf_string;
	gboolean cms_enable;

	GtkWidget *cms_page;
	GtkWidget *cms_enable_check;
	GtkWidget *cms_in_profile_hbox;
	GtkWidget *cms_in_profile_label;
	GtkWidget *cms_in_profile_combobox;
	GtkWidget *cms_in_profile_button;
	GtkWidget *cms_di_profile_hbox;
	GtkWidget *cms_di_profile_label;
	GtkWidget *cms_di_profile_combobox;
	GtkWidget *cms_di_profile_button;
	GtkWidget *cms_ex_profile_hbox;
	GtkWidget *cms_ex_profile_label;
	GtkWidget *cms_ex_profile_combobox;
	GtkWidget *cms_ex_profile_button;
	GtkWidget *cms_intent_hbox;
	GtkWidget *cms_intent_label;
	GtkWidget *cms_intent_combobox;

	cms_page = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (cms_page), 6);

	cms_enable_check = gtk_check_button_new_with_label(_("Enable color management (experimental)"));
	if(!rs_conf_get_boolean(CONF_CMS_ENABLED, &cms_enable))
		cms_enable = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cms_enable_check), cms_enable);
	g_signal_connect ((gpointer) cms_enable_check, "toggled", G_CALLBACK (cms_enable_toggled), rs);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_enable_check, FALSE, TRUE, 0);

	cms_in_profile_hbox = gtk_hbox_new(FALSE, 0);
	cms_in_profile_label = gtk_label_new(_("Input profile"));
	gtk_misc_set_alignment(GTK_MISC(cms_in_profile_label), 0.0, 0.5);
	cms_in_profile_combobox = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_in_profile_combobox), _("BuiltInRGBProfile"));
	temp_conf_gslist = rs_conf_get_list_string(CONF_CMS_IN_PROFILE_LIST);
	temp_conf_gint = 0;
	rs_conf_get_integer(CONF_CMS_IN_PROFILE_SELECTED, &temp_conf_gint);

	while(temp_conf_gslist)
	{
		temp_conf_string = (gchar *) temp_conf_gslist->data;
		if (g_file_test(temp_conf_string, G_FILE_TEST_EXISTS))
		{
			cmsHPROFILE color_profile = cmsOpenProfileFromFile(temp_conf_string, "r");
			if (color_profile)
			{
				cmsCloseProfile(color_profile);	
				gtk_combo_box_append_text(GTK_COMBO_BOX(cms_in_profile_combobox), g_basename(temp_conf_string));
				temp_new_gslist = g_slist_append(temp_new_gslist, (gpointer *) temp_conf_string);
			}
		}
		else
			temp_conf_gint = 0;
		temp_conf_gslist = temp_conf_gslist->next;
	}

	rs_conf_set_list_string(CONF_CMS_IN_PROFILE_LIST, temp_new_gslist);
	g_slist_free(temp_new_gslist);
	temp_new_gslist = NULL;
	if (temp_conf_gint < 0)
		temp_conf_gint = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_in_profile_combobox), temp_conf_gint);
	cms_in_profile_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (cms_in_profile_hbox), cms_in_profile_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_in_profile_hbox), cms_in_profile_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_in_profile_hbox), cms_in_profile_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_in_profile_hbox, FALSE, TRUE, 0);

	cms_di_profile_hbox = gtk_hbox_new(FALSE, 0);
	cms_di_profile_label = gtk_label_new(_("Display profile"));
	gtk_misc_set_alignment(GTK_MISC(cms_di_profile_label), 0.0, 0.5);
	cms_di_profile_combobox = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_di_profile_combobox), _("sRGB"));
	temp_conf_gslist = rs_conf_get_list_string(CONF_CMS_DI_PROFILE_LIST);
	temp_conf_gint = 0;
	rs_conf_get_integer(CONF_CMS_DI_PROFILE_SELECTED, &temp_conf_gint);

	while(temp_conf_gslist)
	{
		temp_conf_string = (gchar *) temp_conf_gslist->data;
		if (g_file_test(temp_conf_string, G_FILE_TEST_EXISTS))
		{
			cmsHPROFILE color_profile = cmsOpenProfileFromFile(temp_conf_string, "r");
			if (color_profile)
			{
				cmsCloseProfile(color_profile);	
				gtk_combo_box_append_text(GTK_COMBO_BOX(cms_di_profile_combobox), g_basename(temp_conf_string));
				temp_new_gslist = g_slist_append(temp_new_gslist, (gpointer *) temp_conf_string);
			}
		}
		else
			temp_conf_gint = 0;
		temp_conf_gslist = temp_conf_gslist->next;
	}

	rs_conf_set_list_string(CONF_CMS_DI_PROFILE_LIST, temp_new_gslist);
	g_slist_free(temp_new_gslist);
	temp_new_gslist = NULL;
	if (temp_conf_gint < 0)
		temp_conf_gint = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_di_profile_combobox), temp_conf_gint);
	cms_di_profile_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (cms_di_profile_hbox), cms_di_profile_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_di_profile_hbox), cms_di_profile_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_di_profile_hbox), cms_di_profile_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_di_profile_hbox, FALSE, TRUE, 0);

	cms_ex_profile_hbox = gtk_hbox_new(FALSE, 0);
	cms_ex_profile_label = gtk_label_new(_("Export profile"));
	gtk_misc_set_alignment(GTK_MISC(cms_ex_profile_label), 0.0, 0.5);
	cms_ex_profile_combobox = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_ex_profile_combobox), _("sRGB"));
	temp_conf_gslist = rs_conf_get_list_string(CONF_CMS_EX_PROFILE_LIST);
	temp_conf_gint = 0;
	rs_conf_get_integer(CONF_CMS_EX_PROFILE_SELECTED, &temp_conf_gint);

	while(temp_conf_gslist)
	{
		temp_conf_string = (gchar *) temp_conf_gslist->data;
		if (g_file_test(temp_conf_string, G_FILE_TEST_EXISTS))
		{
			cmsHPROFILE color_profile = cmsOpenProfileFromFile(temp_conf_string, "r");
			if (color_profile)
			{
				cmsCloseProfile(color_profile);	
				gtk_combo_box_append_text(GTK_COMBO_BOX(cms_ex_profile_combobox), g_basename(temp_conf_string));
				temp_new_gslist = g_slist_append(temp_new_gslist, (gpointer *) temp_conf_string);
			}
		}
		else
			temp_conf_gint = 0;
		temp_conf_gslist = temp_conf_gslist->next;
	}

	rs_conf_set_list_string(CONF_CMS_EX_PROFILE_LIST, temp_new_gslist);
	g_slist_free(temp_new_gslist);
	if (temp_conf_gint < 0)
		temp_conf_gint = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_ex_profile_combobox), temp_conf_gint);
	cms_ex_profile_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (cms_ex_profile_hbox), cms_ex_profile_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_ex_profile_hbox), cms_ex_profile_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_ex_profile_hbox), cms_ex_profile_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_ex_profile_hbox, FALSE, TRUE, 0);

	cms_intent_hbox = gtk_hbox_new(FALSE, 0);
	cms_intent_label = gtk_label_new(_("Intent"));
	gtk_misc_set_alignment(GTK_MISC(cms_intent_label), 0.0, 0.5);
	cms_intent_combobox = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Perceptual"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Relative colormetric"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Saturation"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cms_intent_combobox), _("Absolute colormetric"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(cms_intent_combobox), rs_cms_get_intent(rs->cms));
	gtk_box_pack_start (GTK_BOX (cms_intent_hbox), cms_intent_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_intent_hbox), cms_intent_combobox, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (cms_page), cms_intent_hbox, FALSE, TRUE, 0);

	g_signal_connect ((gpointer) cms_in_profile_combobox, "changed",
			G_CALLBACK(gui_cms_in_profile_combobox_changed), rs);
	g_signal_connect ((gpointer) cms_di_profile_combobox, "changed",
			G_CALLBACK(gui_cms_di_profile_combobox_changed), rs);
	g_signal_connect ((gpointer) cms_ex_profile_combobox, "changed",
			G_CALLBACK(gui_cms_ex_profile_combobox_changed), rs);
			
	g_signal_connect ((gpointer) cms_intent_combobox, "changed",
			G_CALLBACK(gui_cms_intent_combobox_changed), rs);
			
	g_signal_connect ((gpointer) cms_in_profile_button, "clicked",
			G_CALLBACK(gui_cms_in_profile_button_clicked), (gpointer) cms_in_profile_combobox);
	g_signal_connect ((gpointer) cms_di_profile_button, "clicked",
			G_CALLBACK(gui_cms_di_profile_button_clicked), (gpointer) cms_di_profile_combobox);
	g_signal_connect ((gpointer) cms_ex_profile_button, "clicked",
			G_CALLBACK(gui_cms_ex_profile_button_clicked), (gpointer) cms_ex_profile_combobox);

	return cms_page;
}

/* copied verbatim from Gimp: app/widgets/gimpdock.c */
gboolean
window_key_press_event (GtkWidget   *widget,
                           GdkEventKey *event)
{
  GtkWindow *window  = GTK_WINDOW (widget);
  GtkWidget *focus   = gtk_window_get_focus (window);
  gboolean   handled = FALSE;

  /* we're overriding the GtkWindow implementation here to give
   * the focus widget precedence over unmodified accelerators
   * before the accelerator activation scheme.
   */

  /* control/alt accelerators get all key events first */
  if (! handled && event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
    handled = gtk_window_activate_key (window, event);

  /* invoke text widgets  */
  if (G_UNLIKELY (GTK_IS_EDITABLE (focus) || GTK_IS_TEXT_VIEW (focus)))
    handled = gtk_window_propagate_key_event (window, event);

  /* invoke focus widget handlers */
  if (! handled)
    handled = gtk_window_propagate_key_event (window, event);

  /* invoke non-(control/alt) accelerators */
  if (! handled && ! (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
    handled = gtk_window_activate_key (window, event);

  /* chain up, bypassing gtk_window_key_press(), to invoke binding set */
  if (! handled)
    handled = GTK_WIDGET_CLASS (g_type_class_peek (g_type_parent (GTK_TYPE_WINDOW)))->key_press_event (widget, event);

  return handled;
}

/**
 * Function to help gtk_menu_popup(), positiones the popup menu below a widget
 * Should be used like this: gtk_menu_popup(GTK_MENU(menu), NULL, NULL, pos_menu_below_widget, widget, 0, GDK_CURRENT_TIME);
 */
void
pos_menu_below_widget (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET (user_data);
    gint origin_x, origin_y;

	gdk_window_get_origin (widget->window, &origin_x, &origin_y);

	*x = origin_x + widget->allocation.x;
	*y = origin_y + widget->allocation.y + widget->allocation.height;
	*push_in = TRUE;
	return;
}

/**
 * Put a GtkFrame around a widget
 * @param widget The widget to frame
 * @param title Title for the frame
 * @param shadowtype A GtkShadowType
 * @return A new GtkFrame
 */
GtkWidget *
gui_framed(GtkWidget *widget, const gchar *title, GtkShadowType shadowtype)
{
	GtkWidget *frame;
	
	frame = gtk_frame_new(title);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), shadowtype);
	gtk_container_add (GTK_CONTAINER (frame), widget);

	return(frame);
}

static gboolean
rs_gtk_tree_model_count_helper(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	(*((gint *)data))++;
	return FALSE;
}

static gint
rs_gtk_tree_model_count(GtkTreeModel *model)
{
	gint n = 0;
	gtk_tree_model_foreach (model, rs_gtk_tree_model_count_helper, &n);
	return n;
}

GtkWidget *
gui_aligned(GtkWidget *widget, const gfloat xalign, const gfloat yalign, const gfloat xscale, const gfloat yscale)
{
	GtkWidget *alignment;

	g_assert(GTK_IS_WIDGET(widget));

	alignment = gtk_alignment_new(xalign, yalign, xscale, yscale);
	gtk_container_add (GTK_CONTAINER (alignment), widget);

	return alignment;
}

/**
 * Build and show a popup-menu
 * @param widget A widget to pop up below or NULL to pop upat mouse pointer
 * @param user_data Pointer to pass to callback
 * @param ... Pairs of gchar labels and callbaks, terminated by -1
 * @return The newly created menu
 */
GtkWidget *
gui_menu_popup(GtkWidget *widget, gpointer user_data, ...)
{
	va_list ap;
	GCallback cb;
	gchar *label;
	GtkWidget *item, *menu = gtk_menu_new();
	gint n = 0;

	va_start(ap, user_data);

	/* Loop through arguments, abort on -1 */
	while (1)
	{
		label = va_arg(ap, gchar *);
		if (GPOINTER_TO_INT(label) == -1) break;
		cb = va_arg(ap, GCallback);
		if (GPOINTER_TO_INT(cb) == -1) break;

		item = gtk_menu_item_new_with_label (label);
		gtk_widget_show (item);
		gtk_menu_attach (GTK_MENU (menu), item, 0, 1, n, n+1); n++;
		g_signal_connect (item, "activate", cb, user_data);
	}

	va_end(ap);

	if (widget)
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, pos_menu_below_widget, widget, 0, GDK_CURRENT_TIME);
	else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);

	return (menu);
}

void
gui_select_theme(RS_THEME theme)
{
	static RS_THEME current_theme = STANDARD_GTK_THEME;
	static gchar **default_rc_files = NULL;
	GStaticMutex lock = G_STATIC_MUTEX_INIT;
	GtkSettings *settings;

	g_static_mutex_lock(&lock);

	/* Copy default RC files */
	if (!default_rc_files)
	{
		gchar **def;
		gint i;
		def = gtk_rc_get_default_files();
		for(i=0;def[i];i++); /* Count */
		default_rc_files = g_new0(gchar *, i+1);
		for(i=0;def[i];i++) /* Copy */
			default_rc_files[i] = g_strdup(def[i]);
	}

	/* Change theme if needed */
	if (theme != current_theme)
	{
		settings = gtk_settings_get_default();
		switch (theme)
		{
			case STANDARD_GTK_THEME:
				gtk_rc_set_default_files(default_rc_files);
				break;
			case RAWSTUDIO_THEME:
				gtk_rc_add_default_file(PACKAGE_DATA_DIR "/" PACKAGE "/rawstudio.gtkrc");
				break;
		}
		current_theme = theme;
		/* Reread everything */
		if (settings)
			gtk_rc_reparse_all_for_settings(settings, TRUE);
	}

	g_static_mutex_unlock(&lock);
}

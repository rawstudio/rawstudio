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

#include <rawstudio.h>
#include "rs-output.h"

G_DEFINE_TYPE (RSOutput, rs_output, G_TYPE_OBJECT)

static void integer_changed(GtkAdjustment *adjustment, gpointer user_data);
static void boolean_changed(GtkToggleButton *togglebutton, gpointer user_data);
static void string_changed(GtkEditable *editable, gpointer user_data);

static void
rs_output_class_init(RSOutputClass *klass)
{
	/* Some sane defaults */
	klass->extension = "";
	klass->display_name = "N/A";
}

static void
rs_output_init(RSOutput *self)
{
}

/**
 * Instantiate a new RSOutput type
 * @param identifier A string representing a type, for example "RSJpegfile"
 * @return A new RSOutput or NULL on failure
 */
RSOutput *
rs_output_new(const gchar *identifier)
{
	RSOutput *output = NULL;

	g_assert(identifier != NULL);

	GType type = g_type_from_name(identifier);

	if (g_type_is_a (type, RS_TYPE_OUTPUT))
		output = g_object_new(type, NULL);
	else
		g_warning("%s is not a RSOutput",identifier);

	if (!RS_IS_OUTPUT(output))
		g_warning("Could not instantiate output of type \"%s\"", identifier);

	return output;
}

/**
 * Get a filename extension as announced by a RSOutput module
 * @param output A RSOutput
 * @return A proposed filename extension excluding the ., this should not be freed.
 */
const gchar *
rs_output_get_extension(RSOutput *output)
{
	g_assert(RS_IS_OUTPUT(output));

	if (RS_OUTPUT_GET_CLASS(output)->extension)
		return RS_OUTPUT_GET_CLASS(output)->extension;
	else
		return "";
}

/**
 * Actually execute the saver
 * @param output A RSOutput
 * @param filter A RSFilter to get image data from
 * @return TRUE on success, FALSE on error
 */
gboolean
rs_output_execute(RSOutput *output, RSFilter *filter)
{
	g_assert(RS_IS_OUTPUT(output));
	g_assert(RS_IS_FILTER(filter));

	if (RS_OUTPUT_GET_CLASS(output)->execute)
		return RS_OUTPUT_GET_CLASS(output)->execute(output, filter);
	else
		return FALSE;
}

/* FIXME: This is a fucking stupid hack to get by until config is moved
 * into librawstudio */
extern gchar *rs_conf_get_string(const gchar *name);
extern gboolean rs_conf_set_string(const gchar *path, const gchar *string);
extern gboolean rs_conf_get_integer(const gchar *name, gint *integer_value);
extern gboolean rs_conf_set_integer(const gchar *name, const gint integer_value);
extern gboolean rs_conf_get_boolean(const gchar *name, gboolean *boolean_value);
extern gboolean rs_conf_set_boolean(const gchar *name, gboolean bool_value);

static void
integer_changed(GtkAdjustment *adjustment, gpointer user_data)
{
	RSOutput *output = RS_OUTPUT(user_data);
	gint value = (gint) gtk_adjustment_get_value(adjustment);
	gchar *name = g_object_get_data(G_OBJECT(adjustment), "spec-name");
	gchar *confpath = g_object_get_data(G_OBJECT(adjustment), "conf-path");

	if (name)
		g_object_set(output, name, value, NULL);

	if (confpath)
		rs_conf_set_integer(confpath, value);
}

static void
boolean_changed(GtkToggleButton *togglebutton, gpointer user_data)
{
	RSOutput *output = RS_OUTPUT(user_data);
	gboolean value = gtk_toggle_button_get_active(togglebutton);
	gchar *name = g_object_get_data(G_OBJECT(togglebutton), "spec-name");
	gchar *confpath = g_object_get_data(G_OBJECT(togglebutton), "conf-path");

	if (name)
		g_object_set(output, name, value, NULL);

	if (confpath)
		rs_conf_set_boolean(confpath, value);
}

static void
string_changed(GtkEditable *editable, gpointer user_data)
{
	RSOutput *output = RS_OUTPUT(user_data);

	const gchar *value = gtk_entry_get_text(GTK_ENTRY(editable));
	gchar *name = g_object_get_data(G_OBJECT(editable), "spec-name");
	gchar *confpath = g_object_get_data(G_OBJECT(editable), "conf-path");

	if (name)
		g_object_set(output, name, value, NULL);

	if (confpath)
		rs_conf_set_string(confpath, value);
}

static void
colorspace_changed(RSColorSpaceSelector *selector, RSColorSpace *color_space, gpointer user_data)
{
	RSOutput *output = RS_OUTPUT(user_data);

	const gchar *name = g_object_get_data(G_OBJECT(selector), "spec-name");
	const gchar *confpath = g_object_get_data(G_OBJECT(selector), "conf-path");

	if (name)
		g_object_set(output, name, color_space, NULL);

	if (confpath)
		rs_conf_set_string(confpath, G_OBJECT_TYPE_NAME(color_space));
}

/**
 * Load parameters from config for a RSOutput
 * @param output A RSOutput
 * @param conf_prefix The prefix to prepend on config-keys.
 */
void
rs_output_set_from_conf(RSOutput *output, const gchar *conf_prefix)
{
	GObjectClass *klass = G_OBJECT_GET_CLASS(output);
	GParamSpec **specs;
	guint n_specs = 0;
	gint i;

	g_assert(RS_IS_OUTPUT(output));
	g_assert(conf_prefix != NULL);

	specs = g_object_class_list_properties(G_OBJECT_CLASS(klass), &n_specs);
	for(i=0; i<n_specs; i++)
	{
		GType type = G_PARAM_SPEC_VALUE_TYPE(specs[i]);
		gchar *confpath = NULL;

		confpath = g_strdup_printf("%s:%s:%s", conf_prefix, G_OBJECT_TYPE_NAME(output), specs[i]->name);

		if (type == RS_TYPE_COLOR_SPACE)
		{
			gchar *str;

			if (confpath && (str = rs_conf_get_string(confpath)))
			{
				RSColorSpace *color_space;
				color_space = rs_color_space_new_singleton(str);
				if (color_space)
					g_object_set(output, specs[i]->name, color_space, NULL);
			}
		}
		else switch (type)
		{
			case G_TYPE_BOOLEAN:
			{
				gboolean boolean = FALSE;
				if (rs_conf_get_boolean(confpath, &boolean))
					g_object_set(output, specs[i]->name, boolean, NULL);
				break;
			}
			case G_TYPE_INT:
			{
				gint integer = 0;
				if (rs_conf_get_integer(confpath, &integer))
					g_object_set(output, specs[i]->name, integer, NULL);
				break;
			}
			case G_TYPE_STRING:
			{
				gchar *str = rs_conf_get_string(confpath);
				if (str)
				{
					g_object_set(output, specs[i]->name, str, NULL);
					g_free(str);
				}
				break;
			}
			default:
				g_assert_not_reached();
				break;
		}
	}
}

/**
 * Build a GtkWidget that can edit parameters of a RSOutput
 * @param output A RSOutput
 * @param conf_prefix If this is non-NULL, the value will be saved in config,
 *                    and reloaded next time.
 * @return A new GtkWidget representing all parameters of output
 */
GtkWidget *
rs_output_get_parameter_widget(RSOutput *output, const gchar *conf_prefix)
{
	GtkWidget *box = gtk_vbox_new(FALSE, 0);
	GObjectClass *klass = G_OBJECT_GET_CLASS(output);
	GParamSpec **specs;
	guint n_specs = 0;
	gint i;
	gchar *str;

	/* Maintain a reference to the RSOutput */
	g_object_ref(output);
	g_object_set_data_full(G_OBJECT(box), "just-for-refcounting", output, g_object_unref);

	/* Iterate through all GParamSpec's and build a GtkWidget representing them */
	specs = g_object_class_list_properties(G_OBJECT_CLASS(klass), &n_specs);
	for(i=0; i<n_specs; i++)
	{
		gchar *confpath = NULL;
		GtkWidget *widget = NULL;

		/* Ignore "filename" for now */
		if (g_str_equal(specs[i]->name, "filename"))
			continue;

		/* Iw the caller has supplied ud with a conf prefix, sync everything
		 * with config system */
		if (conf_prefix)
			confpath = g_strdup_printf("%s:%s:%s", conf_prefix, G_OBJECT_TYPE_NAME(output), specs[i]->name);

		GType type = G_PARAM_SPEC_VALUE_TYPE(specs[i]);

		if (type == GTK_TYPE_WIDGET)
		{
			g_object_get(output, specs[i]->name, &widget, NULL);
		}
		else if (type == RS_TYPE_COLOR_SPACE)
		{
			widget = rs_color_space_selector_new();
			g_object_set_data(G_OBJECT(widget), "spec-name", specs[i]->name);
			g_object_set_data_full(G_OBJECT(widget), "conf-path", confpath, g_free);

			rs_color_space_selector_add_all(RS_COLOR_SPACE_SELECTOR(widget));
			rs_color_space_selector_set_selected_by_name(RS_COLOR_SPACE_SELECTOR(widget), "RSSrgb");

			if (confpath && (str = rs_conf_get_string(confpath)))
			{
				RSColorSpace *color_space;
				color_space = rs_color_space_selector_set_selected_by_name(RS_COLOR_SPACE_SELECTOR(widget), str);
				if (color_space)
					g_object_set(output, specs[i]->name, color_space, NULL);
			}

			g_signal_connect(widget, "colorspace-selected", G_CALLBACK(colorspace_changed), output);
		}
		else switch (type)
		{
			case G_TYPE_BOOLEAN:
			{
				gboolean boolean = FALSE;

				/* Should this be dropped, and then let the user worry about
				 * calling rs_output_set_from_conf()? */
				if (confpath && rs_conf_get_boolean(confpath, &boolean))
					g_object_set(output, specs[i]->name, boolean, NULL);
				else
					g_object_get(output, specs[i]->name, &boolean, NULL);

				widget = gtk_check_button_new_with_label(g_param_spec_get_blurb(specs[i]));
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), boolean);
				g_object_set_data(G_OBJECT(widget), "spec-name", specs[i]->name);
				g_object_set_data_full(G_OBJECT(widget), "conf-path", confpath, g_free);
				g_signal_connect(widget, "toggled", G_CALLBACK(boolean_changed), output);
				break;
			}
			case G_TYPE_INT:
			{
				GtkObject *adj;
				GtkWidget *label;
				GtkWidget *scale;
				GtkWidget *spin;
				gint integer = 0;

				if (confpath && rs_conf_get_integer(confpath, &integer))
					g_object_set(output, specs[i]->name, integer, NULL);

				g_object_get(output, specs[i]->name, &integer, NULL);

				adj = gtk_adjustment_new((gdouble) integer,
					(gdouble) (((GParamSpecInt*)specs[i])->minimum),
					(gdouble) (((GParamSpecInt*)specs[i])->maximum),
					1.0, 10.0, 0.0);
				g_object_set_data(G_OBJECT(adj), "spec-name", specs[i]->name);
				g_object_set_data_full(G_OBJECT(adj), "conf-path", confpath, g_free);
				g_signal_connect(adj, "value-changed", G_CALLBACK(integer_changed), output);

				label = gtk_label_new(g_param_spec_get_blurb(specs[i]));
				scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
				gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
				spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1.0, 0);

				widget = gtk_hbox_new(FALSE, 2);
				gtk_box_pack_start(GTK_BOX(widget), label, FALSE, TRUE, 0);
				gtk_box_pack_start(GTK_BOX(widget), scale, TRUE, TRUE, 0);
				gtk_box_pack_start(GTK_BOX(widget), spin, FALSE, TRUE, 0);
				break;
			}
			case G_TYPE_STRING:
			{
				GtkWidget *label = gtk_label_new(g_param_spec_get_blurb(specs[i]));
				GtkWidget *entry = gtk_entry_new();

				if (confpath && (str = rs_conf_get_string(confpath)))
				{
					g_object_set(output, specs[i]->name, str, NULL);
					g_free(str);
				}

				g_object_get(output, specs[i]->name, &str, NULL);
				if (str)
				{
					gtk_entry_set_text(GTK_ENTRY(entry), str);
					g_free(str);
				}

				g_object_set_data(G_OBJECT(entry), "spec-name", specs[i]->name);
				g_object_set_data_full(G_OBJECT(entry), "conf-path", confpath, g_free);
				g_signal_connect(entry, "changed", G_CALLBACK(string_changed), output);

				widget = gtk_hbox_new(FALSE, 2);
				gtk_box_pack_start(GTK_BOX(widget), label, FALSE, TRUE, 0);
				gtk_box_pack_start(GTK_BOX(widget), entry, TRUE, TRUE, 0);
				break;
			}
			default:
				g_assert_not_reached();
				break;
		}
		gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 3);
	}
	return box;
}

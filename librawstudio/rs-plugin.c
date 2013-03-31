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

#include <gmodule.h>
#include "rs-plugin.h"

struct _RSPlugin {
	GTypeModule parent_instance;

	gchar *filename;
	GModule *library;

	void (*load)(RSPlugin *plugin);
	void (*unload)(RSPlugin *plugin);
};

struct _RSPluginClass {
	GTypeModuleClass  parent_class;
};

enum {
	PROP_0,
	PROP_FILENAME
};

static void rs_plugin_finalize (GObject *object);
static void rs_plugin_get_property (GObject *object, guint param_id, GValue *value, GParamSpec *pspec);
static void rs_plugin_set_property (GObject *object, guint param_id, const GValue *value, GParamSpec *pspec);
static gboolean rs_plugin_load_module (GTypeModule  *gmodule);
static void rs_plugin_unload_module (GTypeModule  *gmodule);

G_DEFINE_TYPE(RSPlugin, rs_plugin, G_TYPE_TYPE_MODULE);

static void
rs_plugin_class_init(RSPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GTypeModuleClass *type_module_class = G_TYPE_MODULE_CLASS(klass);

	object_class->finalize = rs_plugin_finalize;
	object_class->get_property = rs_plugin_get_property;
	object_class->set_property = rs_plugin_set_property;

	type_module_class->load = rs_plugin_load_module;
	type_module_class->unload  = rs_plugin_unload_module;

	g_object_class_install_property(object_class,
		PROP_FILENAME, g_param_spec_string (
			"filename",
			"Filename",
			"The filaname of the plugin",
			NULL,
			G_PARAM_READWRITE |G_PARAM_CONSTRUCT_ONLY)
	);
}

static void
rs_plugin_init(RSPlugin *plugin)
{
	plugin->filename = NULL;
	plugin->library = NULL;
	plugin->load = NULL;
	plugin->unload = NULL;
}

static void
rs_plugin_finalize(GObject *object)
{
	RSPlugin *plugin = RS_PLUGIN(object);

	g_free(plugin->filename);

	G_OBJECT_CLASS(rs_plugin_parent_class)->finalize(object);
}

static void
rs_plugin_get_property(GObject *object, guint param_id, GValue *value, GParamSpec *pspec)
{
	RSPlugin *plugin = RS_PLUGIN(object);

	switch (param_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, plugin->filename);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
rs_plugin_set_property (GObject *object, guint param_id, const GValue *value, GParamSpec *pspec)
{
	RSPlugin *plugin = RS_PLUGIN(object);

	switch (param_id)
	{
		case PROP_FILENAME:
			g_free (plugin->filename);
			plugin->filename = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static gboolean
rs_plugin_load_module(GTypeModule *gmodule)
{
	RSPlugin *plugin;

	g_return_val_if_fail(G_IS_TYPE_MODULE(gmodule), FALSE);

	plugin = RS_PLUGIN(gmodule);

	g_assert(RS_IS_PLUGIN(plugin));
	g_assert(plugin->filename != NULL);

	plugin->library = g_module_open(plugin->filename, 0);

	if (!plugin->library)
	{
		g_printerr ("%s\n", g_module_error ());
		return FALSE;
	}

	if (!g_module_symbol(plugin->library, "rs_plugin_load", (gpointer *) &plugin->load))
	{
		g_printerr ("%s\n", g_module_error ());
		g_module_close (plugin->library);
		return FALSE;
	}

	/* This is not required from plugins - YET! */
	if (!g_module_symbol(plugin->library, "rs_plugin_unload", (gpointer *) &plugin->unload))
		plugin->unload = NULL;

	/* Execute plugin load method */
	plugin->load (plugin);

	/* We don't support unloading of modules - FOR NOW - make sure it never happens */
	g_module_make_resident(plugin->library);

	return TRUE;
}

static void
rs_plugin_unload_module (GTypeModule *gmodule)
{
	RSPlugin *plugin = RS_PLUGIN(gmodule);

	g_assert(G_IS_TYPE_MODULE(gmodule));
	g_assert(RS_IS_PLUGIN(plugin));

	if (plugin->unload)
		plugin->unload (plugin);

	g_module_close (plugin->library);
	plugin->library = NULL;

	plugin->load = NULL;
	plugin->unload = NULL;
}

RSPlugin *
rs_plugin_new (const gchar *filename)
{
	RSPlugin *plugin;	

	g_return_val_if_fail(filename != NULL, NULL);

	plugin = g_object_new(RS_TYPE_PLUGIN, "filename", filename, NULL);

	return plugin;
}

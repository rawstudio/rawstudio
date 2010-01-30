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

#include <gmodule.h>
#include <rawstudio.h>
#include "config.h"
#include "rs-plugin.h"

static GList *plugins = NULL;

/**
 * Load all installed Rawstudio plugins
 */
gint
rs_plugin_manager_load_all_plugins()
{
	gint num = 0;
	gchar *plugin_directory;
	GDir *dir;
	const gchar *filename;
	GTimer *gt = g_timer_new();

	g_assert(g_module_supported());

	plugin_directory = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "plugins", NULL);
	g_debug("Loading modules from %s", plugin_directory);

	dir = g_dir_open(plugin_directory, 0, NULL);

	while(dir && (filename = g_dir_read_name(dir)))
	{
		if (g_str_has_suffix(filename, "." G_MODULE_SUFFIX))
		{
			RSPlugin *plugin;
			gchar *path;

			/* Load the plugin */
			path = g_build_filename(plugin_directory, filename, NULL);
			plugin = rs_plugin_new(path);
			g_free(path);

			g_assert(g_type_module_use(G_TYPE_MODULE(plugin)));
			/* This doesn't work for some reason, GType's blow up */
//			g_type_module_unuse(G_TYPE_MODULE(plugin));

			plugins = g_list_prepend (plugins, plugin);

			g_debug("%s loaded", filename);
			num++;
		}
	}
	g_debug("%d plugins loaded in %.03f second", num, g_timer_elapsed(gt, NULL));


	/* Print some debug info about loaded plugins */
	GType *plugins;
	guint n_plugins, i;
	plugins = g_type_children (RS_TYPE_FILTER, &n_plugins);
	g_debug("%d filters loaded:", n_plugins);
	for (i = 0; i < n_plugins; i++)
	{
		RSFilterClass *klass;
		GParamSpec **specs;
		guint n_specs = 0;
		gint s;
		/* NOTE: Some plugins depend on all classes is initialized before ANY
		 * instance instantiation takes place, it is NOT safe to just remove
		 * the next line! */
		klass = g_type_class_ref(plugins[i]);
		g_debug("* %s: %s", g_type_name(plugins[i]), klass->name);
		specs = g_object_class_list_properties(G_OBJECT_CLASS(klass), &n_specs);
		for(s=0;s<n_specs;s++)
		{
			g_debug("  + \"%s\":\t%s%s%s%s%s%s%s%s [%s]", specs[s]->name,
				(specs[s]->flags & G_PARAM_READABLE) ? " READABLE" : "",
				(specs[s]->flags & G_PARAM_WRITABLE) ? " WRITABLE" : "",
				(specs[s]->flags & G_PARAM_CONSTRUCT) ? " CONSTRUCT" : "",
				(specs[s]->flags & G_PARAM_CONSTRUCT_ONLY) ? " CONSTRUCT_ONLY" : "",
				(specs[s]->flags & G_PARAM_LAX_VALIDATION) ? " LAX_VALIDATION" : "",
				(specs[s]->flags & G_PARAM_STATIC_NAME) ? " STATIC_NAME" : "",
				(specs[s]->flags & G_PARAM_STATIC_NICK) ? " STATIC_NICK" : "",
				(specs[s]->flags & G_PARAM_STATIC_BLURB) ? " STATIC_BLURB" : "",
				g_param_spec_get_blurb(specs[s])
			);
		}
		g_free(specs);
		g_type_class_unref(klass);
	}
	g_free(plugins);

	plugins = g_type_children (RS_TYPE_OUTPUT, &n_plugins);
	g_debug("%d exporters loaded:", n_plugins);
	for (i = 0; i < n_plugins; i++)
	{
		RSOutputClass *klass;
		GParamSpec **specs;
		guint n_specs = 0;
		gint s;
		klass = g_type_class_ref(plugins[i]);
		g_debug("* %s: %s", g_type_name(plugins[i]), klass->display_name);
		specs = g_object_class_list_properties(G_OBJECT_CLASS(klass), &n_specs);
		for(s=0;s<n_specs;s++)
		{
			g_debug("  + \"%s\":\t%s%s%s%s%s%s%s%s [%s]", specs[s]->name,
				(specs[s]->flags & G_PARAM_READABLE) ? " READABLE" : "",
				(specs[s]->flags & G_PARAM_WRITABLE) ? " WRITABLE" : "",
				(specs[s]->flags & G_PARAM_CONSTRUCT) ? " CONSTRUCT" : "",
				(specs[s]->flags & G_PARAM_CONSTRUCT_ONLY) ? " CONSTRUCT_ONLY" : "",
				(specs[s]->flags & G_PARAM_LAX_VALIDATION) ? " LAX_VALIDATION" : "",
				(specs[s]->flags & G_PARAM_STATIC_NAME) ? " STATIC_NAME" : "",
				(specs[s]->flags & G_PARAM_STATIC_NICK) ? " STATIC_NICK" : "",
				(specs[s]->flags & G_PARAM_STATIC_BLURB) ? " STATIC_BLURB" : "",
				g_param_spec_get_blurb(specs[s])
			);
		}
		g_free(specs);
		g_type_class_unref(klass);
	}
	g_free(plugins);

	if (dir)
		g_dir_close(dir);

	g_timer_destroy(gt);

	return num;
}

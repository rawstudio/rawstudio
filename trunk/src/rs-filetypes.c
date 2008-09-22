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

#include "rs-filetypes.h"

static gint tree_sort(gconstpointer a, gconstpointer b);
static gint tree_search_func(gconstpointer a, gconstpointer b);
static gpointer filetype_search(GTree *tree, const gchar *filename, gint *priority);
static void filetype_add_to_tree(GTree *tree, const gchar *extension, const gchar *description, const gpointer func, const gint priority);

static gboolean rs_filetype_is_initialized = FALSE;
static GStaticMutex lock = G_STATIC_MUTEX_INIT;
static GTree *loaders = NULL;
static GTree *meta_loaders = NULL;

typedef struct {
	gchar *extension;
	gchar *description;
	gint priority;
} RSFiletype;

struct search_needle {
	gchar *extension;
	gint *priority;
};

static gint
tree_sort(gconstpointer a, gconstpointer b)
{
	gint extension;
	RSFiletype *type_a = (RSFiletype *) a;
	RSFiletype *type_b = (RSFiletype *) b;

	extension = g_utf8_collate(type_a->extension, type_b->extension);
	if (extension == 0)
		return type_b->priority - type_a->priority;
	else
		return extension;
}

static gint
tree_search_func(gconstpointer a, gconstpointer b)
{
	gint extension;
	RSFiletype *type_a = (RSFiletype *) a;
	struct search_needle *needle = (struct search_needle *) b;
	extension = g_utf8_collate(needle->extension, type_a->extension);

	if (extension == 0)
	{
		if (type_a->priority > *(needle->priority))
		{
			*(needle->priority) = type_a->priority;
			return 0;
		}
		else
			return -1;
	}

	return extension;
}

static gpointer
filetype_search(GTree *tree, const gchar *filename, gint *priority)
{
	gpointer func = NULL;
	const gchar *extension;

	extension = g_strrstr(filename, ".");

	if (extension)
	{
		struct search_needle needle;

		needle.extension = g_utf8_strdown(extension, -1);
		needle.priority = priority;
		g_static_mutex_lock(&lock);
		func = g_tree_search(tree, tree_search_func, &needle);
		g_static_mutex_unlock(&lock);

		g_free(needle.extension);
	}

	return func;
}

static void
filetype_add_to_tree(GTree *tree, const gchar *extension, const gchar *description, const gpointer func, const gint priority)
{
	RSFiletype *filetype = g_new(RSFiletype, 1);

	g_assert(rs_filetype_is_initialized);
	g_assert(tree != NULL);
	g_assert(extension != NULL);
	g_assert(extension[0] == '.');
	g_assert(description != NULL);
	g_assert(func != NULL);
	g_assert(priority > 0);

	filetype->extension = g_strdup(extension);
	filetype->description = g_strdup(description);
	filetype->priority = priority;

	g_static_mutex_lock(&lock);
	g_tree_insert(tree, filetype, func);
	g_static_mutex_unlock(&lock);
}

/**
 * Initialize the RSFiletype subsystem, this MUST be called before any other
 * rs_filetype_*-functions
 */
void
rs_filetype_init()
{
	g_static_mutex_lock(&lock);
	if (rs_filetype_is_initialized)
		return;
	rs_filetype_is_initialized = TRUE;
	loaders = g_tree_new(tree_sort);
	meta_loaders = g_tree_new(tree_sort);
	g_static_mutex_unlock(&lock);
}

/**
 * Register a new image loader
 * @param extension The filename extension including the dot, ie: ".cr2"
 * @param description A human readable description of the file-format/loader
 * @param loader The loader function
 * @param priority A loader priority, lowest is served first.
 */
void
rs_filetype_register_loader(const gchar *extension, const gchar *description, const RSFileLoaderFunc loader, const gint priority)
{
	filetype_add_to_tree(loaders, extension, description, loader, priority);
}

/**
 * Register a new metadata loader
 * @param extension The filename extension including the dot, ie: ".cr2"
 * @param description A human readable description of the file-format/loader
 * @param meta_loader The loader function
 * @param priority A loader priority, lowest is served first.
 */
void
rs_filetype_register_meta_loader(const gchar *extension, const gchar *description, const RSFileMetaLoaderFunc meta_loader, const gint priority)
{
	filetype_add_to_tree(meta_loaders, extension, description, meta_loader, priority);
}

/**
 * Check if we support loading a given extension
 * @param filename A filename or extension to look-up
 */
gboolean
rs_filetype_can_load(const gchar *filename)
{
	gboolean can_load = FALSE;
	gint priority = 0;
	
	g_assert(rs_filetype_is_initialized);
	g_assert(filename != NULL);

	if (filetype_search(loaders, filename, &priority))
		can_load = TRUE;

	return can_load;
}

/**
 * Load an image according to registered loaders
 * @param filename The file to load
 * @param half_size Set to TRUE to avoid preparing image for debayer
 * @return A new RS_IMAGE16 or NULL if the loading failed
 */
RS_IMAGE16 *
rs_filetype_load(const gchar *filename, const gboolean half_size)
{
	RS_IMAGE16 *image = NULL;
	gint priority = 0;
	RSFileLoaderFunc loader;

	g_assert(rs_filetype_is_initialized);
	g_assert(filename != NULL);

	while((loader = filetype_search(loaders, filename, &priority)) && !image)
		image = loader(filename, half_size);

	return image;
}

/**
 * Load metadata from a specified file
 * @param filename The file to load metadata from
 * @param meta A RSMetadata structure to load everything into
 */
void
rs_filetype_meta_load(const gchar *filename, RSMetadata *meta)
{
	gint priority = 0;
	RSFileMetaLoaderFunc loader;

	g_assert(rs_filetype_is_initialized);
	g_assert(filename != NULL);
	g_assert(RS_IS_METADATA(meta));

	while((loader = filetype_search(meta_loaders, filename, &priority)))
	{
		loader(filename, meta);
	}
}

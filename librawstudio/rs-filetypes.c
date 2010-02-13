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

#include "rs-filetypes.h"
#include "rs-metadata.h"

static gint tree_sort(gconstpointer a, gconstpointer b);
static gpointer filetype_search(GTree *tree, const gchar *filename, gint *priority, const RSLoaderFlags flags);
static void filetype_add_to_tree(GTree *tree, const gchar *extension, const gchar *description, const gpointer func, const gint priority, const RSLoaderFlags flags);

static gboolean rs_filetype_is_initialized = FALSE;
static GStaticMutex lock = G_STATIC_MUTEX_INIT;
static GTree *loaders = NULL;
static GTree *meta_loaders = NULL;

typedef struct {
	gchar *extension;
	gchar *description;
	gint priority;
	RSLoaderFlags flags;
} RSFiletype;

struct search_needle {
	gchar *extension;
	gint *priority;
	RSFileLoaderFunc *func;
	RSLoaderFlags flags;
};

static gint
tree_sort(gconstpointer a, gconstpointer b)
{
	gint extension;
	RSFiletype *type_a = (RSFiletype *) a;
	RSFiletype *type_b = (RSFiletype *) b;

	extension = g_utf8_collate(type_a->extension, type_b->extension);
	if (extension == 0)
		return type_a->priority - type_b->priority;
	else
		return extension;
}

static gboolean
filetype_search_traverse(gpointer key, gpointer value, gpointer data)
{
	RSFiletype *type = key;
	RSFileLoaderFunc *func = value;
	struct search_needle *needle = data;

	if (g_utf8_collate(needle->extension, type->extension) == 0)
	{
		if (type->priority > *(needle->priority))
		{
			if (type->flags & needle->flags)
			{
				needle->func = func;
				*(needle->priority) = type->priority;
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gpointer
filetype_search(GTree *tree, const gchar *filename, gint *priority, const RSLoaderFlags flags)
{
	gpointer func = NULL;
	const gchar *extension;

	extension = g_strrstr(filename, ".");

	if (extension)
	{
		struct search_needle needle;

		needle.extension = g_utf8_strdown(extension, -1);
		needle.priority = priority;
		needle.func = NULL;
		needle.flags = flags;

		g_static_mutex_lock(&lock);
		g_tree_foreach(tree, filetype_search_traverse, &needle);
		g_static_mutex_unlock(&lock);

		g_free(needle.extension);
		func = needle.func;
	}

	return func;
}

static void
filetype_add_to_tree(GTree *tree, const gchar *extension, const gchar *description, const gpointer func, const gint priority, const RSLoaderFlags flags)
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
	filetype->flags = flags;

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
 * @param flags Flags describing the loader
 */
void
rs_filetype_register_loader(const gchar *extension, const gchar *description, const RSFileLoaderFunc loader, const gint priority, const RSLoaderFlags flags)
{
	filetype_add_to_tree(loaders, extension, description, loader, priority, flags);
}

/**
 * Register a new metadata loader
 * @param extension The filename extension including the dot, ie: ".cr2"
 * @param description A human readable description of the file-format/loader
 * @param meta_loader The loader function
 * @param priority A loader priority, lowest is served first.
 * @param flags Flags describing the loader
 */
void
rs_filetype_register_meta_loader(const gchar *extension, const gchar *description, const RSFileMetaLoaderFunc meta_loader, const gint priority, const RSLoaderFlags flags)
{
	filetype_add_to_tree(meta_loaders, extension, description, meta_loader, priority, flags);
}

/* FIXME: Port conf abstraction to librawstudio */
extern gboolean rs_conf_get_boolean(const gchar *name, gboolean *boolean_value);

/**
 * Check if we support loading a given extension
 * @param filename A filename or extension to look-up
 */
gboolean
rs_filetype_can_load(const gchar *filename)
{
	gboolean can_load = FALSE;
	gint priority = 0;
	RSLoaderFlags flags = RS_LOADER_FLAGS_RAW;
	gboolean load_8bit = FALSE;

	g_assert(rs_filetype_is_initialized);
	g_assert(filename != NULL);

	rs_conf_get_boolean("open_8bit_images", &load_8bit);
	if (load_8bit)
		flags |= RS_LOADER_FLAGS_8BIT;

	if (filetype_search(loaders, filename, &priority, flags))
		can_load = TRUE;

	return can_load;
}

/**
 * Load an image according to registered loaders
 * @param filename The file to load
 * @return A new RS_IMAGE16 or NULL if the loading failed
 */
RS_IMAGE16 *
rs_filetype_load(const gchar *filename)
{
	RS_IMAGE16 *image = NULL;
	gint priority = 0;
	RSFileLoaderFunc loader;

	g_assert(rs_filetype_is_initialized);
	g_assert(filename != NULL);

	while((loader = filetype_search(loaders, filename, &priority, RS_LOADER_FLAGS_ALL)) && !image)
		image = loader(filename);

	return image;
}

/**
 * Load metadata from a specified file
 * @param service The file to load metadata from OR a servicename (".exif" for example)
 * @param meta A RSMetadata structure to load everything into
 * @param rawfile An open RAWFILE
 * @param offset An offset in the open RAWFILE
 */
void
rs_filetype_meta_load(const gchar *service, RSMetadata *meta, RAWFILE *rawfile, guint offset)
{
	gint priority = 0;
	RSFileMetaLoaderFunc loader;

	g_assert(rs_filetype_is_initialized);
	g_assert(service != NULL);
	g_assert(RS_IS_METADATA(meta));

	if((loader = filetype_search(meta_loaders, service, &priority, RS_LOADER_FLAGS_ALL)))
		loader(service, rawfile, offset, meta);
}

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

#include "rs-dcp-file.h"
#include "rs-profile-factory.h"
#include "rs-profile-factory-model.h"
#include "config.h"
#include "rs-utils.h"

#define PROFILE_FACTORY_DEFAULT_SEARCH_PATH PACKAGE_DATA_DIR "/" PACKAGE "/profiles/"

G_DEFINE_TYPE(RSProfileFactory, rs_profile_factory, G_TYPE_OBJECT)

static void
rs_profile_factory_class_init(RSProfileFactoryClass *klass)
{
}

static void
rs_profile_factory_init(RSProfileFactory *factory)
{
	/* We use G_TYPE_POINTER to store some strings because they should live
	 forever - and we avoid unneeded strdup/free */
	factory->profiles = gtk_list_store_new(FACTORY_MODEL_NUM_COLUMNS, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER);
}

static gboolean
add_icc_profile(RSProfileFactory *factory, const gchar *path)
{
	gboolean readable = FALSE;

	RSIccProfile *profile = rs_icc_profile_new_from_file(path);
	g_assert(RS_IS_ICC_PROFILE(profile));
	if (profile)
	{
		GtkTreeIter iter;

		gtk_list_store_prepend(factory->profiles, &iter);
		gtk_list_store_set(factory->profiles, &iter,
			FACTORY_MODEL_COLUMN_TYPE, FACTORY_MODEL_TYPE_ICC,
			FACTORY_MODEL_COLUMN_PROFILE, profile,
			FACTORY_MODEL_COLUMN_ID, g_path_get_basename(path), /* FIXME */
			-1);

		readable = TRUE;
	}

	return readable;
}

static gboolean
add_dcp_profile(RSProfileFactory *factory, const gchar *path)
{
	gboolean readable = FALSE;

	RSDcpFile *profile = rs_dcp_file_new_from_file(path);
	const gchar *model = rs_dcp_file_get_model(profile);
	if (model)
	{
		GtkTreeIter iter;
		gtk_list_store_prepend(factory->profiles, &iter);
		gtk_list_store_set(factory->profiles, &iter,
			FACTORY_MODEL_COLUMN_TYPE, FACTORY_MODEL_TYPE_DCP,
			FACTORY_MODEL_COLUMN_PROFILE, profile,
			FACTORY_MODEL_COLUMN_MODEL, model,
			FACTORY_MODEL_COLUMN_ID, rs_dcp_get_id(profile),
			-1);
		readable = TRUE;
		rs_tiff_free_data(RS_TIFF(profile));
	}

	return readable;
}

void
rs_profile_factory_load_profiles(RSProfileFactory *factory, const gchar *path, gboolean load_dcp, gboolean load_icc)
{
	const gchar *basename;
	gchar *filename;
	GDir *dir = g_dir_open(path, 0, NULL);

	if (NULL == dir )
		return;

	while((basename = g_dir_read_name(dir)))
	{
		if (basename[0] == '.')
            continue;

		filename = g_build_filename(path, basename, NULL);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
			rs_profile_factory_load_profiles(factory, filename, load_dcp, load_icc);

		else if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		{
			if (load_dcp && (g_str_has_suffix(basename, ".dcp") || g_str_has_suffix(basename, ".DCP")))
				add_dcp_profile(factory, filename);
			else if (load_icc && (
				g_str_has_suffix(basename, ".icc")
				|| g_str_has_suffix(basename, ".ICC")
				|| g_str_has_suffix(basename, ".icm")
				|| g_str_has_suffix(basename, ".ICM")
				))
				add_icc_profile(factory, filename);
		}
		g_free(filename);
	}
	g_dir_close(dir);
}

RSProfileFactory *
rs_profile_factory_new(const gchar *search_path)
{
	RSProfileFactory *factory = g_object_new(RS_TYPE_PROFILE_FACTORY, NULL);
	
	rs_profile_factory_load_profiles(factory, search_path, TRUE, FALSE);

	GtkTreeIter iter;
	gtk_list_store_prepend(factory->profiles, &iter);
	gtk_list_store_set(factory->profiles, &iter,
		FACTORY_MODEL_COLUMN_TYPE, FACTORY_MODEL_TYPE_INFO,
		FACTORY_MODEL_COLUMN_PROFILE, NULL,
		FACTORY_MODEL_COLUMN_ID, "_embedded_image_profile_",
		-1);
	gtk_list_store_prepend(factory->profiles, &iter);
	gtk_list_store_set(factory->profiles, &iter,
		FACTORY_MODEL_COLUMN_TYPE, FACTORY_MODEL_TYPE_SEP,
		-1);
	gtk_list_store_prepend(factory->profiles, &iter);
	gtk_list_store_set(factory->profiles, &iter,
		FACTORY_MODEL_COLUMN_TYPE, FACTORY_MODEL_TYPE_ADD,
		-1);

	return factory;
}

RSProfileFactory *
rs_profile_factory_new_default(void)
{
	static RSProfileFactory *factory = NULL;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!factory)
	{
		factory = rs_profile_factory_new(PROFILE_FACTORY_DEFAULT_SEARCH_PATH);

		const gchar *user_profiles = rs_profile_factory_get_user_profile_directory();
		rs_profile_factory_load_profiles(factory, user_profiles, TRUE, TRUE);
	}
	g_static_mutex_unlock(&lock);

	return factory;
}

const gchar *
rs_profile_factory_get_user_profile_directory(void)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	gchar *directory = NULL;

	g_static_mutex_lock(&lock);
	if (!directory)
		directory = g_strdup_printf("%s/profiles/", rs_confdir_get());
	g_static_mutex_unlock(&lock);

	return directory;
}

gboolean
rs_profile_factory_add_profile(RSProfileFactory *factory, const gchar *path)
{
	g_assert(RS_IS_PROFILE_FACTORY(factory));
	g_assert(path != NULL);
	g_assert(path[0] != '\0');
	g_assert(g_path_is_absolute(path));

	if (g_str_has_suffix(path, ".dcp") || g_str_has_suffix(path, ".DCP"))
		return add_dcp_profile(factory, path);
	if (g_str_has_suffix(path, ".icc") || g_str_has_suffix(path, ".ICC"))
		return add_icc_profile(factory, path);
	if (g_str_has_suffix(path, ".icm") || g_str_has_suffix(path, ".ICM"))
		return add_icc_profile(factory, path);

	return FALSE;
}

static gboolean
visible_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean visible = FALSE;

	gchar *model_needle = (gchar *) data;
	gchar *model_haystack;
	gint type;

	gtk_tree_model_get(model, iter,
		FACTORY_MODEL_COLUMN_MODEL, &model_haystack,
		FACTORY_MODEL_COLUMN_TYPE, &type,
		-1);

	/* The only thing we need to hide is mismatched DCP profiles */
	if (model_needle && model_haystack)
		if ((type == FACTORY_MODEL_TYPE_DCP) && (g_ascii_strcasecmp(model_needle, model_haystack) == 0))
	    	visible = TRUE;

	if (type != FACTORY_MODEL_TYPE_DCP)
		visible = TRUE;

	return visible;
}

GtkTreeModelFilter *
rs_dcp_factory_get_compatible_as_model(RSProfileFactory *factory, const gchar *unique_id)
{
	g_assert(RS_IS_PROFILE_FACTORY(factory));

	GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(factory->profiles), NULL));

	gtk_tree_model_filter_set_visible_func(filter, visible_func, g_strdup(unique_id), g_free);

	return filter;
}

static GSList *
rs_profile_factory_find_from_column(RSProfileFactory *factory, const gchar *id, int column)
{
	RSDcpFile *dcp;
	gchar *model_id;
	GtkTreeIter iter;
	GtkTreeModel *treemodel = GTK_TREE_MODEL(factory->profiles);
	GSList *ret = NULL;

	g_assert(RS_IS_PROFILE_FACTORY(factory));
	if (!id)
		return NULL;

	if (gtk_tree_model_get_iter_first(treemodel, &iter))
		do 
		{
			gtk_tree_model_get(treemodel, &iter, column, &model_id, -1);

			if (id && model_id && 0 == g_ascii_strcasecmp(id, model_id))
			{
				gtk_tree_model_get(treemodel, &iter,
					FACTORY_MODEL_COLUMN_PROFILE, &dcp,
					-1);

				/* FIXME: Deal with ICC */
				g_assert(RS_IS_DCP_FILE(dcp));
				ret = g_slist_append (ret, dcp);

			}
		} while (gtk_tree_model_iter_next(treemodel, &iter));

	return ret;
}

RSDcpFile *
rs_profile_factory_find_from_id(RSProfileFactory *factory, const gchar *id)
{
	RSDcpFile *ret = NULL;
	g_assert(RS_IS_PROFILE_FACTORY(factory));
	GSList *profiles = rs_profile_factory_find_from_column(factory, id, FACTORY_MODEL_COLUMN_ID);
	gint nprofiles = g_slist_length(profiles);
	
	if (nprofiles >= 1)
	{
		ret = profiles->data;
	  if (nprofiles > 1)
			g_warning("Found %d profiles when searching for unique profile: %s. Using the first one.", nprofiles, id);
	}
	g_slist_free(profiles);
	return ret;
}

GSList *
rs_profile_factory_find_from_model(RSProfileFactory *factory, const gchar *id)
{
	g_assert(RS_IS_PROFILE_FACTORY(factory));
	return rs_profile_factory_find_from_column(factory, id, FACTORY_MODEL_COLUMN_MODEL);
}


void 
rs_profile_factory_set_embedded_profile(RSProfileFactory *factory, const RSIccProfile *profile)
{
	GtkTreeIter iter;
	GtkTreeModel *treemodel = GTK_TREE_MODEL(factory->profiles);
	if (gtk_tree_model_get_iter_first(treemodel, &iter))
	{
		do
		{
			gchar *model_id;
			gtk_tree_model_get(treemodel, &iter,
				FACTORY_MODEL_COLUMN_ID, &model_id,
				-1);
			if (model_id && g_str_equal("_embedded_image_profile_", model_id))
			{
				gtk_list_store_set(factory->profiles, &iter,
					FACTORY_MODEL_COLUMN_PROFILE, profile,
					-1);
			}
		} while (gtk_tree_model_iter_next(treemodel, &iter));
	}

}

#include "rs-dcp-file.h"
#include "rs-profile-factory.h"
#include "rs-profile-factory-model.h"
#include "rs-icc-profile.h"
#include "config.h"
#include "rs-utils.h"

#define PROFILE_FACTORY_DEFAULT_SEARCH_PATH PACKAGE_DATA_DIR "/" PACKAGE "/profiles/"

struct _RSProfileFactory {
	GObject parent;

	GtkListStore *profiles;
};

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
	}

	return readable;
}

static void
load_profiles(RSProfileFactory *factory, const gchar *path, gboolean load_dcp, gboolean load_icc)
{
	const gchar *basename;
	gchar *filename;
	GDir *dir = g_dir_open(path, 0, NULL);

	while((dir != NULL) && (basename = g_dir_read_name(dir)))
	{
		if (basename[0] == '.')
            continue;

		filename = g_build_filename(path, basename, NULL);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
			load_profiles(factory, filename, load_dcp, load_icc);

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

}

RSProfileFactory *
rs_profile_factory_new(const gchar *search_path)
{
	RSProfileFactory *factory = g_object_new(RS_TYPE_PROFILE_FACTORY, NULL);

	load_profiles(factory, search_path, TRUE, FALSE);

	GtkTreeIter iter;

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
	GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!factory)
	{
		factory = rs_profile_factory_new(PROFILE_FACTORY_DEFAULT_SEARCH_PATH);

		const gchar *user_profiles = rs_profile_factory_get_user_profile_directory();
		load_profiles(factory, user_profiles, TRUE, TRUE);
	}
	g_static_mutex_unlock(&lock);

	return factory;
}

const gchar *
rs_profile_factory_get_user_profile_directory(void)
{
	GStaticMutex lock = G_STATIC_MUTEX_INIT;
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
	gboolean visible = TRUE;

	gchar *model_needle = (gchar *) data;
	gchar *model_haystack;
	gint type;

	gtk_tree_model_get(model, iter,
		FACTORY_MODEL_COLUMN_MODEL, &model_haystack,
		FACTORY_MODEL_COLUMN_TYPE, &type,
		-1);

	/* The only thing we need to hide is mismatched DCP profiles */
	if (model_needle && model_haystack)
		if ((type == FACTORY_MODEL_TYPE_DCP) && (g_ascii_strcasecmp(model_needle, model_haystack) != 0))
	    	visible = FALSE;

	return visible;
}

GtkTreeModelFilter *
rs_dcp_factory_get_compatible_as_model(RSProfileFactory *factory, const gchar *make, const gchar *model)
{
	g_assert(RS_IS_PROFILE_FACTORY(factory));

	GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(factory->profiles), NULL));

	gtk_tree_model_filter_set_visible_func(filter, visible_func, g_strdup(model), g_free);

	return filter;
}

RSDcpFile *
rs_profile_factory_find_from_id(RSProfileFactory *factory, const gchar *id)
{
	RSDcpFile *ret = NULL;
	RSDcpFile *dcp;
	gchar *model_id;
	GtkTreeIter iter;
	GtkTreeModel *treemodel = GTK_TREE_MODEL(factory->profiles);

	if (gtk_tree_model_get_iter_first(treemodel, &iter))
		do {
			gtk_tree_model_get(treemodel, &iter,
				FACTORY_MODEL_COLUMN_ID, &model_id,
				-1);

			if (id && model_id && g_str_equal(id, model_id))
			{
				gtk_tree_model_get(treemodel, &iter,
					FACTORY_MODEL_COLUMN_PROFILE, &dcp,
					-1);

				/* FIXME: Deal with ICC */
				g_assert(RS_IS_DCP_FILE(dcp));
				if (ret)
					g_warning("WARNING: Duplicate profiles detected in file: %s, for %s, named:%s.\nUnsing last found profile.", rs_tiff_get_filename_nopath(RS_TIFF(dcp)),  rs_dcp_file_get_model(dcp),  rs_dcp_file_get_name(dcp));

				ret = dcp;
			}
		} while (gtk_tree_model_iter_next(treemodel, &iter));

	return ret;
}

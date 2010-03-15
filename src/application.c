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
#include <glib/gstdio.h>
#include <glib.h>
#include <unistd.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include <time.h>
#include <config.h>
#ifndef WIN32
#include <gconf/gconf-client.h>
#endif
#include "application.h"
#include "gtk-interface.h"
#include "gtk-helper.h"
#include "rs-cache.h"
#include "gettext.h"
#include "conf_interface.h"
#include "filename.h"
#include "rs-tiff.h"
#include "rs-arch.h"
#include "rs-batch.h"
#include "rs-cms.h"
#include "rs-store.h"
#include "rs-preview-widget.h"
#include "rs-histogram.h"
#include "rs-photo.h"
#include "rs-exif.h"
#include "rs-library.h"                                                                                                                                    
#include "lensfun.h"
#include "rs-profile-factory-model.h"
#include "rs-profile-camera.h"

static void photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs);
static void photo_profile_changed(RS_PHOTO *photo, gpointer profile, RS_BLOB *rs);

void
rs_free(RS_BLOB *rs)
{
	if (rs->photo)
		g_object_unref(rs->photo);
}

void
rs_set_photo(RS_BLOB *rs, RS_PHOTO *photo)
{
	g_assert(rs != NULL);

	/* Unref old photo if any */
	if (rs->photo)
		g_object_unref(rs->photo);
	rs->photo = NULL;

	/* Set photo in preview-widget */
	rs_preview_widget_set_photo(RS_PREVIEW_WIDGET(rs->preview), photo);

	/* Save photo in blob */
	rs->photo = photo;

	if (rs->photo)
	{
		/* Look up lens */
		RSMetadata *meta = rs_photo_get_metadata(rs->photo);
		RSLensDb *lens_db = rs_lens_db_get_default();
		RSLens *lens = rs_lens_db_lookup_from_metadata(lens_db, meta);

		/* Apply lens information to RSLensfun */
		if (lens)
		{
			rs_filter_set_recursive(rs->filter_end,
				"make", meta->make_ascii,
				"model", meta->model_ascii,
				"lens", lens,
				"focal", (gfloat) meta->focallength,
				"aperture", meta->aperture,
				"tca_kr", rs->photo->settings[rs->current_setting]->tca_kr,
				"tca_kb", rs->photo->settings[rs->current_setting]->tca_kb,
				"vignetting", rs->photo->settings[rs->current_setting]->vignetting,
				NULL);
			g_object_unref(lens);
		}

		g_object_unref(meta);

		rs_filter_set_recursive(rs->filter_end,
			"image", rs->photo->input,
			"filename", rs->photo->filename,
			"rectangle", rs_photo_get_crop(photo),
			"angle", rs_photo_get_angle(photo),
			"orientation", rs->photo->orientation,
			NULL);

		g_signal_connect(G_OBJECT(rs->photo), "spatial-changed", G_CALLBACK(photo_spatial_changed), rs);
		g_signal_connect(G_OBJECT(rs->photo), "profile-changed", G_CALLBACK(photo_profile_changed), rs);
	}
}

static void
photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs)
{
	if (photo == rs->photo)
	{
		/* Update crop and rotate filters */
		rs_filter_set_recursive(rs->filter_end,
			"rectangle", rs_photo_get_crop(photo),
			"angle", rs_photo_get_angle(photo),
			"orientation", rs->photo->orientation,
			NULL);
	}

}

static void
photo_profile_changed(RS_PHOTO *photo, gpointer profile, RS_BLOB *rs)
{
	if (photo == rs->photo)
	{
		if (RS_IS_ICC_PROFILE(profile))
		{
			RSColorSpace *cs = rs_color_space_icc_new_from_icc(profile);

			g_object_set(rs->filter_input, "color-space", cs, NULL);

			/* We unref at once, and the the filter keep the only reference */
			g_object_unref(cs);
		}
		else
		{
			/* If we don't have a specific ICC profile, we will simply assign
			   a Prophoto colorspace to stop RSColorTransform from doing
			   anything - this works because RSDcp is requesting Prophoto. */
			g_object_set(rs->filter_input, "color-space", rs_color_space_new_singleton("RSProphoto"), NULL);
		}
	}
}

gboolean
rs_photo_save(RS_PHOTO *photo, RSOutput *output, gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms)
{
	gfloat actual_scale;
	RSIccProfile *profile = NULL;
	gchar *profile_filename;

	g_assert(RS_IS_PHOTO(photo));
	g_assert(RS_IS_OUTPUT(output));

	RSFilter *finput = rs_filter_new("RSInputImage16", NULL);
	RSFilter *fdemosaic = rs_filter_new("RSDemosaic", finput);
	RSFilter *flensfun = rs_filter_new("RSLensfun", fdemosaic);
	RSFilter *ftransform_input = rs_filter_new("RSColorspaceTransform", flensfun);
	RSFilter *frotate = rs_filter_new("RSRotate",ftransform_input) ;
	RSFilter *fcrop = rs_filter_new("RSCrop", frotate);
	RSFilter *fresample= rs_filter_new("RSResample", fcrop);
	RSFilter *fdcp = rs_filter_new("RSDcp", fresample);
	RSFilter *fdenoise= rs_filter_new("RSDenoise", fdcp);
	RSFilter *ftransform_display = rs_filter_new("RSColorspaceTransform", fdenoise);
	RSFilter *fend = ftransform_display;

	rs_filter_set_recursive(fend,
		"image", photo->input,
		"filename", photo->filename,
		"angle", photo->angle,
		"orientation", photo->orientation,
		"rectangle", photo->crop,
		"settings", photo->settings[snapshot],
		NULL);
	actual_scale = ((gdouble) width / (gdouble) rs_filter_get_width(finput));
	if (0 < width && 0 < height) /* We only wan't to set width and height if they are not -1 */
		rs_filter_set_recursive(fend, "width", width, "height", height, NULL);

	/* Set input ICC profile */
	profile_filename = rs_conf_get_cms_profile(CMS_PROFILE_INPUT);
	if (profile_filename)
	{
		profile = rs_icc_profile_new_from_file(profile_filename);
		g_free(profile_filename);
	}
	
	/* Look up lens */
	RSMetadata *meta = rs_photo_get_metadata(photo);
	RSLensDb *lens_db = rs_lens_db_get_default();
	RSLens *lens = rs_lens_db_lookup_from_metadata(lens_db, meta);

	/* Apply lens information to RSLensfun */
	if (lens)
	{
		rs_filter_set_recursive(fend,
			"make", meta->make_ascii,
			"model", meta->model_ascii,
			"lens", lens,
			"focal", (gfloat) meta->focallength,
			"aperture", meta->aperture,
			"tca_kr", photo->settings[snapshot]->tca_kr,
			"tca_kb", photo->settings[snapshot]->tca_kb,
			"vignetting", photo->settings[snapshot]->vignetting,
			NULL);
		g_object_unref(lens);
	}

	g_object_unref(meta);
	
//	if (!profile)
//		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/generic_camera_profile.icc");
	g_object_set(finput, "icc-profile", profile, NULL);
	g_object_unref(profile);

	/* Set output ICC profile */
	profile_filename = rs_conf_get_cms_profile(CMS_PROFILE_EXPORT);
	if (profile_filename)
	{
		profile = rs_icc_profile_new_from_file(profile_filename);
		g_free(profile_filename);
	}
/*	if (!profile)
		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/sRGB.icc");
	g_object_set(fend, "icc-profile", profile, NULL);
	g_object_unref(profile);*/
		
//	RSFilterResponse *response = rs_filter_get_image8(navigator->cache, request);

	/* actually save */
	g_assert(rs_output_execute(output, fend));
//	g_object_unref(request);

	photo->exported = TRUE;
	rs_cache_save(photo, MASK_ALL);

	/* Set the exported flag */
	rs_store_set_flags(NULL, photo->filename, NULL, NULL, &photo->exported);

	g_object_unref(finput);
	g_object_unref(flensfun);
	g_object_unref(ftransform_input);
	g_object_unref(ftransform_display);
	g_object_unref(fdemosaic);
	g_object_unref(frotate);
	g_object_unref(fcrop);
	g_object_unref(fresample);
	g_object_unref(fdenoise);
	g_object_unref(fdcp);

	return(TRUE);
}

RS_BLOB *
rs_new(void)
{
	RSFilter *cache;

	RS_BLOB *rs;
	rs = g_malloc(sizeof(RS_BLOB));
	rs->settings_buffer = NULL;
	rs->photo = NULL;
	rs->queue = rs_batch_new_queue();
	rs->current_setting = 0;

	/* Build basic filter chain */
	rs->filter_input = rs_filter_new("RSInputImage16", NULL);
	rs->filter_demosaic = rs_filter_new("RSDemosaic", rs->filter_input);
	rs->filter_demosaic_cache = rs_filter_new("RSCache", rs->filter_demosaic);

	/* We need this for 100% zoom */
	g_object_set(rs->filter_demosaic_cache, "ignore-roi", TRUE, NULL);

	rs->filter_lensfun = rs_filter_new("RSLensfun", rs->filter_demosaic_cache);
	rs->filter_rotate = rs_filter_new("RSRotate", rs->filter_lensfun);
	rs->filter_crop = rs_filter_new("RSCrop", rs->filter_rotate);
	cache = rs_filter_new("RSCache", rs->filter_crop);

	rs_filter_set_recursive(rs->filter_input, "color-space", rs_color_space_new_singleton("RSProphoto"), NULL);
	rs->filter_end = cache;

	return(rs);
}

void
rs_white_black_point(RS_BLOB *rs)
{
	if (rs->photo)
	{
		guint hist[4][256];
		gint i = 0;
		gdouble black_threshold = 0.003; // Percent underexposed pixels
		gdouble white_threshold = 0.01; // Percent overexposed pixels
		gdouble blackpoint;
		gdouble whitepoint;
		guint total = 0;
		RSColorTransform *rct;

		rct = rs_color_transform_new();
		rs_color_transform_set_from_settings(rct, rs->photo->settings[rs->current_setting], MASK_ALL ^ MASK_CURVE);
		g_object_unref(rct);

		// calculate black point
		while(i < 256) {
			total += hist[R][i]+hist[G][i]+hist[B][i];
			if ((total/3) > ((250*250*3)/100*black_threshold))
				break;
			i++;
		}
		blackpoint = (gdouble) i / (gdouble) 255;
		
		// calculate white point
		i = 255;
		while(i) {
			total += hist[R][i]+hist[G][i]+hist[B][i];
			if ((total/3) > ((250*250*3)/100*white_threshold))
				break;
			i--;
		}
		whitepoint = (gdouble) i / (gdouble) 255;

		rs_curve_widget_move_knot(RS_CURVE_WIDGET(rs->curve[rs->current_setting]),0,blackpoint,0.0);
		rs_curve_widget_move_knot(RS_CURVE_WIDGET(rs->curve[rs->current_setting]),-1,whitepoint,1.0);
	}
}

gboolean
test_dcp_profile(RSProfileFactory *factory, gchar *make_ascii, gchar *model_ascii)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(factory->profiles);
	gchar *unique = g_strdup(rs_profile_camera_find(make_ascii, model_ascii));
	gchar *temp;

	gtk_tree_model_get_iter_first(model, &iter);
	do {
		gtk_tree_model_get(model, &iter,
				   FACTORY_MODEL_COLUMN_MODEL, &temp,
				   -1);
		if (g_strcmp0(temp, unique) == 0)
		{
			g_free(unique);
			return TRUE;
		}
	} while ( gtk_tree_model_iter_next(model, &iter));
	g_free(unique);
	return FALSE;
}

/**
 * This is a very simple regression test for Rawstudio. Filenames will be read
 * from "testimages" in the current directory, one filename per line, and a
 * small series of tests will be carried out for each filename. Output can be
 * piped to a file for further processing.
 */
void
test()
{
	if (!g_file_test("testimages", G_FILE_TEST_EXISTS))
	{
		printf("File: testimages is missing.\n");
		return;
	}

	gchar *filename, *basename, *next_filename;
	GdkPixbuf *pixbuf;
	GIOStatus status = G_IO_STATUS_NORMAL;
	GIOChannel *io = g_io_channel_new_file("testimages", "r", NULL);
	gint sum, good = 0, bad = 0;

	struct lfDatabase *lensdb = lf_db_new ();
	lf_db_load (lensdb);

	RSProfileFactory *profile_factory = g_object_new(RS_TYPE_PROFILE_FACTORY, NULL);
	rs_profile_factory_load_profiles(profile_factory, PACKAGE_DATA_DIR "/" PACKAGE "/profiles/", TRUE, FALSE);

	printf("basename, load, filetype, thumb, meta, make, a-make, a-model, aperture, iso, s-speed, wb, f-length, lensfun camera, lens min focal, lens max focal, lens max aperture, lens min aperture, lens id, lens identifier, dcp profile\n");
	status = g_io_channel_read_line(io, &filename, NULL, NULL, NULL);
	g_strstrip(filename);

	while (G_IO_STATUS_EOF != status)
	{
		status = g_io_channel_read_line(io, &next_filename, NULL, NULL, NULL);
		g_strstrip(next_filename);
		if (status != G_IO_STATUS_EOF)
			rs_io_idle_prefetch_file(next_filename, -1);
		gboolean filetype_ok = FALSE;
		gboolean load_ok = FALSE;
		gboolean thumbnail_ok = FALSE;
		gboolean load_meta_ok = FALSE;
		gboolean make_ok = FALSE;
		gboolean make_ascii_ok = FALSE;
		gboolean model_ascii_ok = FALSE;
		gboolean aperture_ok = FALSE;
		gboolean iso_ok = FALSE;
		gboolean shutterspeed_ok = FALSE;
		gboolean wb_ok = FALSE;
		gboolean focallength_ok = FALSE;
		gboolean lensfun_camera_ok = FALSE;
		gboolean lens_min_focal_ok = FALSE;
		gboolean lens_max_focal_ok = FALSE;
		gboolean lens_max_aperture_ok = FALSE;
		gboolean lens_min_aperture_ok = FALSE;
		gboolean lens_id_ok = FALSE;
		gboolean lens_identifier_ok = FALSE;
		gboolean dcp_profile_ok = FALSE;

		if (rs_filetype_can_load(filename))
		{
			RS_PHOTO *photo = NULL;
			filetype_ok = TRUE;
			photo = rs_photo_load_from_file(filename);
			if (photo)
			{
				load_ok = TRUE;
				g_object_unref(photo);
			}

			RSMetadata *metadata = rs_metadata_new();
			rs_metadata_load_from_file(metadata, filename);

			load_meta_ok = TRUE;

			if (metadata->make != MAKE_UNKNOWN)
				make_ok = TRUE;
			if (metadata->make_ascii != NULL)
				make_ascii_ok = TRUE;
			if (metadata->model_ascii != NULL)
				model_ascii_ok = TRUE;
			if (metadata->aperture > 0.0)
				aperture_ok = TRUE;
			if (metadata->iso > 0)
				iso_ok = TRUE;
			if (metadata->shutterspeed > 1.0)
				shutterspeed_ok = TRUE;
			if (metadata->cam_mul[0] > 0.1 && metadata->cam_mul[0] != 1.0)
				wb_ok = TRUE;
			if (metadata->focallength > 0.0)
				focallength_ok = TRUE;

			/* FIXME: Port to RSFiletype */
			pixbuf = rs_metadata_get_thumbnail(metadata);
			if (pixbuf)
			{
				thumbnail_ok = TRUE;
				g_object_unref(pixbuf);
			}

			/* Test if camera is known in Lensfun */
			const lfCamera **cameras = lf_db_find_cameras(lensdb, metadata->make_ascii, metadata->model_ascii);
			if (cameras)
				lensfun_camera_ok = TRUE;

			if (metadata->lens_min_focal > 0.0)
				lens_min_focal_ok = TRUE;
			if (metadata->lens_max_focal > 0.0)
				lens_max_focal_ok = TRUE;
			if (metadata->lens_max_aperture > 0.0)
				lens_max_aperture_ok = TRUE;
			if (metadata->lens_min_aperture > 0.0)
				lens_min_aperture_ok = TRUE;
			if (metadata->lens_id > 0.0)
				lens_id_ok = TRUE;
			if (metadata->lens_identifier)
				lens_identifier_ok = TRUE;

			if (test_dcp_profile(profile_factory, metadata->make_ascii, metadata->model_ascii))
				dcp_profile_ok = TRUE;

			g_object_unref(metadata);

		}

		basename = g_path_get_basename(filename);
		printf("%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			basename,
			load_ok,
			filetype_ok,
			thumbnail_ok,
			load_meta_ok,
			make_ok,
			make_ascii_ok,
			model_ascii_ok,
			aperture_ok,
			iso_ok,
			shutterspeed_ok,
			wb_ok,
			focallength_ok,
			lensfun_camera_ok,
			lens_min_focal_ok,
			lens_max_focal_ok,
			lens_max_aperture_ok,
			lens_min_aperture_ok,
			lens_id_ok,
		       lens_identifier_ok,
		       dcp_profile_ok
		);
		sum = load_ok
			+filetype_ok
			+thumbnail_ok
			+load_meta_ok
			+make_ok
			+make_ascii_ok
			+model_ascii_ok
			+aperture_ok
			+iso_ok
			+shutterspeed_ok
			+wb_ok
			+focallength_ok
			+lensfun_camera_ok
			+lens_min_focal_ok
			+lens_max_focal_ok
			+lens_max_aperture_ok
			+lens_min_aperture_ok
			+lens_id_ok
			+lens_identifier_ok
			+dcp_profile_ok;
		good += sum;
		bad += (20-sum);

		g_free(basename);

		g_free(filename);
		filename = next_filename;
	}
	printf("Passed: %d Failed: %d (%d%%)\n", good, bad, (good*100)/(good+bad));
	g_io_channel_shutdown(io, TRUE, NULL);
	exit(0);
}

#if GTK_CHECK_VERSION(2,10,0)
/* Default handler for GtkLinkButton's -copied almost verbatim from Bond2 */
static void runuri(GtkLinkButton *button, const gchar *link, gpointer user_data)
{
#ifdef WIN32
#warning This is untested
	gchar* argv[]= {
		getenv("ComSpec"),
		"/c",
		"start",
		"uri click", /* start needs explicit title incase link has spaces or quotes */
		(gchar*)link,
		NULL
	};
#else
	gchar *argv[]= {
		"gnome-open", /* this feels like cheating! */
		(gchar *) link,
		NULL
		};
#endif
	GError *error = NULL;
	gint status = 0;
	gboolean res;

	res = g_spawn_sync(
		NULL /*PWD*/,
		argv,
		NULL /*envp*/,
		G_SPAWN_SEARCH_PATH, /*flags*/
		NULL, /*setup_func*/
		NULL, /*user data for setup*/
		NULL,
		NULL, /* stdin/out/error */
		&status,
		&error);

	if(!res)
	{
		g_error("%s: %s\n", g_quark_to_string(error->domain), error->message);
		g_error_free(error);
		return ;
	}
}
#endif

/* We use out own reentrant locking for GDK/GTK */

static GStaticRecMutex gdk_lock = G_STATIC_REC_MUTEX_INIT;

static void
rs_gdk_lock()
{
	g_static_rec_mutex_lock (&gdk_lock);
}

static void
rs_gdk_unlock()
{
	g_static_rec_mutex_unlock (&gdk_lock);
}


int
main(int argc, char **argv)
{
	RS_BLOB *rs;
	int optimized = 1;
	gboolean do_test = FALSE;
	int opt;
	gboolean use_system_theme = DEFAULT_CONF_USE_SYSTEM_THEME;
#ifndef WIN32
	GConfClient *client;
#endif

	while ((opt = getopt(argc, argv, "nt")) != -1) {
		switch (opt) {
		case 'n':
			optimized = 0;
			break;
		case 't':
			do_test = TRUE;
			break;
		}
	}

	gdk_threads_set_lock_functions(rs_gdk_lock, rs_gdk_unlock);
	g_thread_init(NULL);
	gdk_threads_init();

	/* Bind default C functions */
	rs_bind_default_functions();

	/* Bind optimized functions if any */
	if (likely(optimized)) {
		rs_bind_optimized_functions();
	}

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	/* Make sure the GType system is initialized */
	g_type_init();

	/* Switch to rawstudio theme before any drawing if needed */
	rs_conf_get_boolean_with_default(CONF_USE_SYSTEM_THEME, &use_system_theme, DEFAULT_CONF_USE_SYSTEM_THEME);
	if (!use_system_theme)
		gui_select_theme(RAWSTUDIO_THEME);

	gtk_init(&argc, &argv);
	check_install();

	rs_filetype_init();

	rs_plugin_manager_load_all_plugins();

#ifndef WIN32
	/* Add our own directory to default GConfClient before anyone uses it */
	client = gconf_client_get_default();
	gconf_client_add_dir(client, "/apps/" PACKAGE, GCONF_CLIENT_PRELOAD_NONE, NULL);
#endif

	rs = rs_new();
	rs->queue->cms = rs->cms = rs_cms_init();

	rs_stock_init();

#if GTK_CHECK_VERSION(2,10,0)
	gtk_link_button_set_uri_hook(runuri,NULL,NULL);
#endif

	if (do_test)
		test();
	else
		gui_init(argc, argv, rs);

	/* This is so fucking evil, but Rawstudio will deadlock in some GTK atexit() function from time to time :-/ */
	_exit(0);
}
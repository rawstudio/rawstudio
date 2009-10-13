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

#include <rawstudio.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include <time.h>
#include <config.h>
#include <gconf/gconf-client.h>
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
#include "rs-preload.h"
#include "rs-library.h"                                                                                                                                    

static void photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs);

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
			g_object_set(rs->filter_lensfun,
				"make", meta->make_ascii,
				"model", meta->model_ascii,
				"lens", lens,
				"focal", (gfloat) meta->focallength,
				"aperture", meta->aperture,
				NULL);
			g_object_unref(lens);
		}
		g_object_set(rs->filter_input,
			"image", rs->photo->input,
			"filename", rs->photo->filename,
			NULL);

		g_object_unref(meta);

		/* Update crop and rotate filters */
		g_object_set(rs->filter_crop, "rectangle", rs_photo_get_crop(photo), NULL);
		g_object_set(rs->filter_rotate, "angle", rs_photo_get_angle(photo), "orientation", rs->photo->orientation, NULL);
		g_object_set(rs->filter_rotate, "angle", rs_photo_get_angle(photo), NULL);

		g_signal_connect(G_OBJECT(rs->photo), "spatial-changed", G_CALLBACK(photo_spatial_changed), rs);
	}
}

static void
photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs)
{
	if (photo == rs->photo)
	{
		/* Update crop and rotate filters */
		g_object_set(rs->filter_crop, "rectangle", rs_photo_get_crop(photo), NULL);
		g_object_set(rs->filter_rotate, "angle", rs_photo_get_angle(photo), "orientation", rs->photo->orientation, NULL);
		g_object_set(rs->filter_rotate, "angle", rs_photo_get_angle(photo), NULL);
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
	RSFilter *frotate = rs_filter_new("RSRotate", fdemosaic);
	RSFilter *fcrop = rs_filter_new("RSCrop", frotate);
	RSFilter *fresample= rs_filter_new("RSResample", fcrop);
	RSFilter *fdenoise= rs_filter_new("RSDenoise", fresample);
	RSFilter *fbasic_render = rs_filter_new("RSBasicRender", fdenoise);
	RSFilter *fend = fbasic_render;

	g_object_set(finput, "image", photo->input, "filename", photo->filename, NULL);
	g_object_set(frotate, "angle", photo->angle, "orientation", photo->orientation, NULL);
	g_object_set(fcrop, "rectangle", photo->crop, NULL);
	actual_scale = ((gdouble) width / (gdouble) rs_filter_get_width(finput));
	if (0 < width && 0 < height) /* We only wan't to set width and height if they are not -1 */
		g_object_set(fresample, "width", width, "height", height, NULL);
	g_object_set(fbasic_render, "settings", photo->settings[snapshot], NULL);
	g_object_set(fdenoise, "settings", photo->settings[snapshot], NULL);

	/* Set input ICC profile */
	profile_filename = rs_conf_get_cms_profile(CMS_PROFILE_INPUT);
	if (profile_filename)
	{
		profile = rs_icc_profile_new_from_file(profile_filename);
		g_free(profile_filename);
	}
	if (!profile)
		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/generic_camera_profile.icc");
	g_object_set(finput, "icc-profile", profile, NULL);
	g_object_unref(profile);

	/* Set output ICC profile */
	profile_filename = rs_conf_get_cms_profile(CMS_PROFILE_EXPORT);
	if (profile_filename)
	{
		profile = rs_icc_profile_new_from_file(profile_filename);
		g_free(profile_filename);
	}
	if (!profile)
		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/sRGB.icc");
	g_object_set(fbasic_render, "icc-profile", profile, NULL);
	g_object_unref(profile);

	/* actually save */
	g_assert(rs_output_execute(output, fend));

	photo->exported = TRUE;
	rs_cache_save(photo, MASK_ALL);

	/* Set the exported flag */
	rs_store_set_flags(NULL, photo->filename, NULL, NULL, &photo->exported);

	g_object_unref(finput);
	g_object_unref(fdemosaic);
	g_object_unref(frotate);
	g_object_unref(fcrop);
	g_object_unref(fresample);
	g_object_unref(fdenoise);
	g_object_unref(fbasic_render);

	return(TRUE);
}

RS_BLOB *
rs_new(void)
{
	RSFilter *cache;
	RSIccProfile *profile = NULL;
	gchar *filename;

	RS_BLOB *rs;
	rs = g_malloc(sizeof(RS_BLOB));
	rs->settings_buffer = NULL;
	rs->photo = NULL;
	rs->queue = rs_batch_new_queue();
	rs->current_setting = 0;

	/* Create library */
	rs->library = rs_library_new();

	/* Build basic filter chain */
	rs->filter_input = rs_filter_new("RSInputImage16", NULL);
	rs->filter_demosaic = rs_filter_new("RSDemosaic", rs->filter_input);
	rs->filter_lensfun = rs_filter_new("RSLensfun", rs->filter_demosaic);
	cache = rs_filter_new("RSCache", rs->filter_lensfun);
	rs->filter_rotate = rs_filter_new("RSRotate", cache);
	rs->filter_crop = rs_filter_new("RSCrop", rs->filter_rotate);
	cache = rs_filter_new("RSCache", rs->filter_crop);

	/* We need this for 100% zoom */
	g_object_set(cache, "ignore-roi", TRUE, NULL);

	rs->filter_end = cache;

	filename = rs_conf_get_cms_profile(CMS_PROFILE_INPUT);
	if (filename)
	{
		profile = rs_icc_profile_new_from_file(filename);
		g_free(filename);
	}
	if (!profile)
		profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/generic_camera_profile.icc");
	g_object_set(rs->filter_input, "icc-profile", profile, NULL);
	g_object_unref(profile);

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

/**
 * This is a very simple regression test for Rawstudio. Filenames will be read
 * from "testimages" in the current directory, one filename per line, and a
 * small series of tests will be carried out for each filename. Output can be
 * piped to a file for further processing.
 */
void
test()
{
	gchar *filename, *basename, *next_filename;
	GdkPixbuf *pixbuf;
	GIOStatus status = G_IO_STATUS_NORMAL;
	GIOChannel *io = g_io_channel_new_file("testimages", "r", NULL);
	gint sum, good = 0, bad = 0;

	printf("basename, load, filetype, thumb, meta, make, a-make, a-model, aperture, iso, s-speed, wb, f-length\n");
	status = g_io_channel_read_line(io, &filename, NULL, NULL, NULL);
	g_strstrip(filename);

	while (G_IO_STATUS_EOF != status)
	{
		status = g_io_channel_read_line(io, &next_filename, NULL, NULL, NULL);
		g_strstrip(next_filename);
		if (status != G_IO_STATUS_EOF)
			rs_preload(next_filename);
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

			RSMetadata *metadata = rs_metadata_new_from_file(filename);

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
			g_object_unref(metadata);

		}

		basename = g_path_get_basename(filename);
		printf("%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
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
			focallength_ok
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
			+focallength_ok;

		good += sum;
		bad += (12-sum);

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
	GConfClient *client;

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

	/* Add our own directory to default GConfClient before anyone uses it */
	client = gconf_client_get_default();
	gconf_client_add_dir(client, "/apps/" PACKAGE, GCONF_CLIENT_PRELOAD_NONE, NULL);

	rs = rs_new();
	rs->queue->cms = rs->cms = rs_cms_init();

	rs_stock_init();

	rs_library_init(rs->library);

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

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

static void photo_settings_changed(RS_PHOTO *photo, RSSettingsMask mask, RS_BLOB *rs);
static void photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs);

RS_FILETYPE *filetypes;

static void
rs_add_filetype(gchar *id, gint filetype, const gchar *ext, gchar *description,
	RS_IMAGE16 *(*load)(const gchar *, gboolean),
	void (*load_meta)(const gchar *, RSMetadata *),
	gboolean (*save)(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms))
{
	RS_FILETYPE *cur = filetypes;
	if (filetypes==NULL)
		cur = filetypes = g_malloc(sizeof(RS_FILETYPE));
	else
	{
		while (cur->next) cur = cur->next;
		cur->next = g_malloc(sizeof(RS_FILETYPE));
		cur = cur->next;
	}
	cur->id = id;
	cur->filetype = filetype;
	cur->ext = ext;
	cur->description = description;
	cur->save = save;
	cur->next = NULL;
	return;
}

static void
rs_init_filetypes(void)
{

	rs_filetype_init();

#define REGISTER_FILETYPE(extension, description, load, meta) do { \
	rs_filetype_register_loader(extension, description, load, 10); \
	rs_filetype_register_meta_loader(extension, description, meta, 10); \
} while(0)

#undef REGISTER_FILETYPE

	/* Old-style savers - FIXME: Port to RSFiletype */
	filetypes = NULL;
	rs_add_filetype("jpeg", FILETYPE_JPEG, ".jpg", _("JPEG (Joint Photographic Experts Group)"),
		NULL, NULL, rs_photo_save);
	rs_add_filetype("png", FILETYPE_PNG, ".png", _("PNG (Portable Network Graphics)"),
		NULL, NULL, rs_photo_save);
	rs_add_filetype("tiff8", FILETYPE_TIFF8, ".tif", _("8-bit TIFF (Tagged Image File Format)"),
		NULL, NULL, rs_photo_save);
	rs_add_filetype("tiff16", FILETYPE_TIFF16, ".tif", _("16-bit TIFF (Tagged Image File Format)"),
		NULL, NULL, rs_photo_save);
	return;
}

void
rs_free(RS_BLOB *rs)
{
	if (rs->photo)
		g_object_unref(rs->photo);
}

static void
photo_settings_changed(RS_PHOTO *photo, RSSettingsMask mask, RS_BLOB *rs)
{
	const gint snapshot = mask>>24;
	mask &= 0x00ffffff;

	/* Is this really safe? */
	if (snapshot != rs->current_setting)
		return;

	if (photo == rs->photo)
	{
		/* Update histogram */
		rs_histogram_set_settings(RS_HISTOGRAM_WIDGET(rs->histogram), rs->photo->settings[rs->current_setting]);

		/* Update histogram in curve */
		rs_curve_draw_histogram(RS_CURVE_WIDGET(rs->curve[rs->current_setting]),
			rs->histogram_dataset,
			rs->photo->settings[rs->current_setting]);
	}
}

static void
photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs)
{
	if (photo == rs->photo)
	{
		/* Update crop and rotate filters */
		g_object_set(rs->filter_crop, "rectangle", rs_photo_get_crop(photo), NULL);
		g_object_set(rs->filter_rotate, "angle", rs_photo_get_angle(photo), NULL);

		/* Update histogram dataset */
		if (rs->histogram_dataset)
			rs_image16_free(rs->histogram_dataset);

		rs->histogram_dataset = rs_image16_transform(photo->input, NULL,
			NULL, NULL, photo->crop, 250, 250,
			TRUE, -1.0f, photo->angle, photo->orientation, NULL);

		rs_histogram_set_image(RS_HISTOGRAM_WIDGET(rs->histogram), rs->histogram_dataset);

		/* Force update of histograms */
		photo_settings_changed(photo, MASK_ALL, rs);
	}
}

void
rs_set_photo(RS_BLOB *rs, RS_PHOTO *photo)
{
	g_assert(rs != NULL);

	/* Unref old photo if any */
	if (rs->photo)
		g_object_unref(rs->photo);
	rs->photo = NULL;

	/* Apply settings from photo */
	rs_settings_copy(photo->settings[0], MASK_ALL, rs->settings[0]);
	rs_settings_copy(photo->settings[1], MASK_ALL, rs->settings[1]);
	rs_settings_copy(photo->settings[2], MASK_ALL, rs->settings[2]);

	/* make sure we're synchronized */
	rs_settings_link(rs->settings[0], photo->settings[0]);
	rs_settings_link(rs->settings[1], photo->settings[1]);
	rs_settings_link(rs->settings[2], photo->settings[2]);

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

		if (lens)
		{
			/* FIXME: Apply to lensfun-filter here */
			g_object_unref(lens);
		}
		g_object_set(rs->filter_input, "image", rs->photo->input, NULL);
		g_object_set(rs->filter_rotate, "angle", rs->photo->angle, "orientation", rs->photo->orientation, NULL);
		g_object_set(rs->filter_crop, "rectangle", rs->photo->crop, NULL);

		g_object_unref(meta);
		g_signal_connect(G_OBJECT(rs->photo), "settings-changed", G_CALLBACK(photo_settings_changed), rs);
		g_signal_connect(G_OBJECT(rs->photo), "spatial-changed", G_CALLBACK(photo_spatial_changed), rs);

		/* Force an update! */
		photo_spatial_changed(rs->photo, rs);
	}
}

void
rs_set_snapshot(RS_BLOB *rs, gint snapshot)
{
	g_assert (rs != NULL);
	g_assert ((snapshot>=0) && (snapshot<=2));

	rs->current_setting = snapshot;

	/* Switch preview widget to the correct snapshot */
	rs_preview_widget_set_snapshot(RS_PREVIEW_WIDGET(rs->preview), 0, snapshot);

	/* Force an update */
	if (rs->photo)
		photo_settings_changed(rs->photo, MASK_ALL|(snapshot<<24), rs);
}

gboolean
rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms)
{
	GdkPixbuf *pixbuf;
	RS_IMAGE16 *rsi;
	RS_IMAGE16 *image16;
	gint quality = 100;
	gboolean uncompressed_tiff = FALSE;
	RSColorTransform *rct;
	void *transform = NULL;
	RS_EXIF_DATA *exif;
	gfloat actual_scale;
	RSOutput *output;

	g_assert(RS_IS_PHOTO(photo));
	g_assert(filename != NULL);

	RSFilter *finput = rs_filter_new("RSInputImage16", NULL);
	RSFilter *fdemosaic = rs_filter_new("RSDemosaic", finput);
	RSFilter *frotate = rs_filter_new("RSRotate", fdemosaic);
	RSFilter *fcrop = rs_filter_new("RSCrop", frotate);
	RSFilter *fresample= rs_filter_new("RSResample", fcrop);
	RSFilter *fsharpen= rs_filter_new("RSSharpen", fresample);

	g_object_set(finput, "image", photo->input, NULL);
	g_object_set(frotate, "angle", photo->angle, "orientation", photo->orientation, NULL);
	g_object_set(fcrop, "rectangle", photo->crop, NULL);
	actual_scale = ((gdouble) width / (gdouble) rs_filter_get_width(finput));
	g_object_set(fsharpen, "amount", actual_scale * photo->settings[snapshot]->sharpen, NULL);
	g_object_set(fresample, "width", width, "height", height, NULL);


	rsi = rs_filter_get_image(fsharpen);

	if (cms)
		transform = rs_cms_get_transform(cms, TRANSFORM_EXPORT);

	/* Initialize color transform */
	rct = rs_color_transform_new();
	rs_color_transform_set_cms_transform(rct, transform);
	rs_color_transform_set_adobe_matrix(rct, &photo->metadata->adobe_coeff);
	rs_color_transform_set_from_settings(rct, photo->settings[snapshot], MASK_ALL);
	rs_color_transform_set_output_format(rct, 8);

	/* actually save */
	switch (filetype)
	{
		case FILETYPE_JPEG:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rs_color_transform_transform(rct, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));

			rs_conf_get_integer(CONF_EXPORT_JPEG_QUALITY, &quality);
			if (quality > 100)
				quality = 100;
			else if (quality < 0)
				quality = 0;

			output = rs_output_new("RSJpegfile");
			g_object_set(output, "filename", filename, "quality", quality, NULL);
			rs_output_execute(output, pixbuf);

			g_object_unref(output);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_PNG:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rs_color_transform_transform(rct, rsi->w, rsi->h, rsi->pixels, rsi->rowstride,
				gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));

			output = rs_output_new("RSPngfile");
			g_object_set(output, "filename", filename, NULL);
			rs_output_execute(output, pixbuf);

			g_object_unref(output);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_TIFF8:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			rs_color_transform_transform(rct, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));

			output = rs_output_new("RSTifffile");
			g_object_set(output, "filename", filename, "uncompressed", uncompressed_tiff, NULL);
			rs_output_execute(output, pixbuf);

			g_object_unref(output);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_TIFF16:
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			image16 = rs_image16_new(rsi->w, rsi->h, 3, 3);
			rs_color_transform_set_output_format(rct, 16);
			rs_color_transform_transform(rct, rsi->w, rsi->h,
				rsi->pixels, rsi->rowstride,
				image16->pixels, image16->rowstride*2);
			g_warning("Port to RSOutput");
			rs_tiff16_save(image16, filename, rs_cms_get_profile_filename(cms, CMS_PROFILE_EXPORT), uncompressed_tiff);
			rs_image16_free(image16);
			break;
	}

	rs_image16_free(rsi);
	g_object_unref(rct);

	photo->exported = TRUE;
	rs_cache_save(photo, MASK_ALL);

	/* Set the exported flag */
	rs_store_set_flags(NULL, photo->filename, NULL, NULL, &photo->exported);

	/* Save exif for JPEG files */
	if (filetype == FILETYPE_JPEG)
	{
		exif = rs_exif_load_from_file(photo->filename);
		if (exif)
		{
			rs_exif_add_to_file(exif, filename);
			rs_exif_free(exif);
		}
	}

	g_object_unref(finput);
	g_object_unref(fdemosaic);
	g_object_unref(frotate);
	g_object_unref(fcrop);
	g_object_unref(fresample);
	g_object_unref(fsharpen);

	return(TRUE);
}

RS_BLOB *
rs_new(void)
{
	RSFilter *cache;

	RS_BLOB *rs;
	guint c;
	rs = g_malloc(sizeof(RS_BLOB));
	rs->histogram_dataset = NULL;
	rs->settings_buffer = NULL;
	rs->photo = NULL;
	rs->queue = rs_batch_new_queue();
	rs->current_setting = 0;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();

	/* Build basic filter chain */
	rs->filter_input = rs_filter_new("RSInputImage16", NULL);
	rs->filter_demosaic = rs_filter_new("RSDemosaic", rs->filter_input);
	cache = rs_filter_new("RSCache", rs->filter_demosaic);
	rs->filter_rotate = rs_filter_new("RSRotate", cache);
	rs->filter_crop = rs_filter_new("RSCrop", rs->filter_rotate);
	cache = rs_filter_new("RSCache", rs->filter_crop);

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
		rs_color_transform_make_histogram(rct, rs->histogram_dataset, hist);
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
	gchar *filename, *basename;
	GdkPixbuf *pixbuf;
	GIOChannel *io = g_io_channel_new_file("testimages", "r", NULL);
	gint sum, good = 0, bad = 0;

	printf("basename, load, filetype, thumb, meta, make, a-make, a-model, aperture, iso, s-speed, wb, f-length\n");
	while (G_IO_STATUS_EOF != g_io_channel_read_line(io, &filename, NULL, NULL, NULL))
	{
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

		g_strstrip(filename);

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

	rs_init_filetypes();
	gtk_init(&argc, &argv);
	check_install();
	rs_plugin_manager_load_all_plugins();

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

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

#include <glib/gstdio.h>
#include <unistd.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include <time.h>
#include <config.h>
#include "rawstudio.h"
#include "gtk-interface.h"
#include "gtk-helper.h"
#include "rs-cache.h"
#include "color.h"
#include "rawfile.h"
#include "tiff-meta.h"
#include "ciff-meta.h"
#include "mrw-meta.h"
#include "x3f-meta.h"
#include "rs-image.h"
#include "gettext.h"
#include "conf_interface.h"
#include "filename.h"
#include "rs-jpeg.h"
#include "rs-tiff.h"
#include "rs-arch.h"
#include "rs-batch.h"
#include "rs-cms.h"
#include "rs-store.h"
#include "rs-color-transform.h"
#include "rs-preview-widget.h"
#include "rs-histogram.h"
#include "rs-curve.h"
#include "rs-photo.h"
#include "rs-math.h"
#include "rs-exif.h"
#include "rs-metadata.h"
#include "rs-filetypes.h"
#include "rs-utils.h"

static void photo_settings_changed(RS_PHOTO *photo, RSSettingsMask mask, RS_BLOB *rs);
static void photo_spatial_changed(RS_PHOTO *photo, RS_BLOB *rs);
static void rs_gdk_load_meta(const gchar *src, RSMetadata *metadata);

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

	/* Raw file formats */
	REGISTER_FILETYPE(".cr2", _("Canon CR2"), rs_image16_open_dcraw,  rs_tiff_load_meta);
	REGISTER_FILETYPE(".crw", _("Canon CIFF"), rs_image16_open_dcraw, rs_ciff_load_meta);
	REGISTER_FILETYPE(".nef", _("Nikon NEF"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".mrw", _("Minolta raw"), rs_image16_open_dcraw, rs_mrw_load_meta);
	REGISTER_FILETYPE(".tif", _("Canon TIFF"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".arw", _("Sony"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".sr2", _("Sony"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".srf", _("Sony"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".kdc", _("Kodak"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".dcr", _("Kodak"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".x3f", _("Sigma"), rs_image16_open_dcraw, rs_x3f_load_meta);
	REGISTER_FILETYPE(".orf", _("Olympus"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".raw", _("Panasonic raw"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".pef", _("Pentax raw"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".dng", _("Adobe Digital negative"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".mef", _("Mamiya"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".3fr", _("Hasselblad"), rs_image16_open_dcraw, rs_tiff_load_meta);
	REGISTER_FILETYPE(".erf", _("Epson"), rs_image16_open_dcraw, rs_tiff_load_meta);

	/* GDK formats */
	REGISTER_FILETYPE(".jpg", _("JPEG (Joint Photographic Experts Group)"), rs_image16_open_gdk, rs_gdk_load_meta);
	REGISTER_FILETYPE(".png", _("PNG (Portable Network Graphics)"), rs_image16_open_gdk, rs_gdk_load_meta);

#undef REGISTER_FILETYPE

	/* TIFF is special - we need higher priority to try raw first */
	rs_filetype_register_loader(".tif", _("8-bit TIFF (Tagged Image File Format)"), rs_image16_open_gdk, 20);
	rs_filetype_register_meta_loader(".tif", _("8-bit TIFF (Tagged Image File Format)"), rs_gdk_load_meta, 20);

	/* Old-style savers - FIXME: Port to RSFiletype */
	filetypes = NULL;
	rs_add_filetype("jpeg", FILETYPE_JPEG, ".jpg", _("JPEG (Joint Photographic Experts Group)"),
		rs_image16_open_gdk, rs_gdk_load_meta, rs_photo_save);
	rs_add_filetype("png", FILETYPE_PNG, ".png", _("PNG (Portable Network Graphics)"),
		rs_image16_open_gdk, rs_gdk_load_meta, rs_photo_save);
	rs_add_filetype("tiff8", FILETYPE_TIFF8, ".tif", _("8-bit TIFF (Tagged Image File Format)"),
		rs_image16_open_gdk, rs_gdk_load_meta, rs_photo_save);
	rs_add_filetype("tiff16", FILETYPE_TIFF16, ".tif", _("16-bit TIFF (Tagged Image File Format)"),
		rs_image16_open_gdk, rs_gdk_load_meta, rs_photo_save);
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
		rs_color_transform_set_from_settings(rs->histogram_transform, rs->photo->settings[rs->current_setting], mask);
		rs_histogram_set_color_transform(RS_HISTOGRAM_WIDGET(rs->histogram), rs->histogram_transform);

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
	RS_IMAGE16 *sharpened;
	RS_IMAGE16 *image16;
	gint quality = 100;
	gboolean uncompressed_tiff = FALSE;
	RSColorTransform *rct;
	void *transform = NULL;
	RS_EXIF_DATA *exif;

	g_assert(RS_IS_PHOTO(photo));
	g_assert(filename != NULL);

	rs_image16_demosaic(photo->input, RS_DEMOSAIC_PPG);

	sharpened = rs_image16_sharpen(photo->input, NULL, photo->settings[snapshot]->sharpen, NULL);

	/* transform and crop */
	rsi = rs_image16_transform(sharpened, NULL,
			NULL, NULL, photo->crop, width, height, keep_aspect, scale,
			photo->angle, photo->orientation, NULL);

	rs_image16_unref(sharpened);

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

			rs_jpeg_save(pixbuf, filename, quality, rs_cms_get_profile_filename(cms, CMS_PROFILE_EXPORT));
			g_object_unref(pixbuf);
			break;
		case FILETYPE_PNG:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rs_color_transform_transform(rct, rsi->w, rsi->h, rsi->pixels, rsi->rowstride,
				gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));
			gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_TIFF8:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			rs_color_transform_transform(rct, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));
			rs_tiff8_save(pixbuf, filename, rs_cms_get_profile_filename(cms, CMS_PROFILE_EXPORT), uncompressed_tiff);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_TIFF16:
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			image16 = rs_image16_new(rsi->w, rsi->h, 3, 3);
			rs_color_transform_set_output_format(rct, 16);
			rs_color_transform_transform(rct, rsi->w, rsi->h,
				rsi->pixels, rsi->rowstride,
				image16->pixels, image16->rowstride*2);
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

	return(TRUE);
}

RS_BLOB *
rs_new(void)
{
	RS_BLOB *rs;
	guint c;
	rs = g_malloc(sizeof(RS_BLOB));
	rs->histogram_dataset = NULL;
	rs->histogram_transform = rs_color_transform_new();
	rs->settings_buffer = NULL;
	rs->photo = NULL;
	rs->queue = rs_batch_new_queue();
	rs->current_setting = 0;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();
	return(rs);
}

void
rs_gdk_load_meta(const gchar *src, RSMetadata *metadata)
{
	metadata->thumbnail = gdk_pixbuf_new_from_file_at_size(src, 128, 128, NULL);
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
			photo = rs_photo_load_from_file(filename, TRUE);
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

	/* Switch to rawstudio theme before any drawing if needed */
	rs_conf_get_boolean_with_default(CONF_USE_SYSTEM_THEME, &use_system_theme, DEFAULT_CONF_USE_SYSTEM_THEME);
	if (!use_system_theme)
		gui_select_theme(RAWSTUDIO_THEME);

	rs_init_filetypes();
	gtk_init(&argc, &argv);
	check_install();

	rs = rs_new();
	rs->queue->cms = rs->cms = rs_cms_init();

	if (do_test)
		test();
	else
		gui_init(argc, argv, rs);

	/* This is so fucking evil, but Rawstudio will deadlock in some GTK atexit() function from time to time :-/ */
	_exit(0);
}

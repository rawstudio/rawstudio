/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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
#include "panasonic.h"
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

static RS_SETTINGS *rs_settings_new();
static void rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd);
static RS_PHOTO *rs_photo_open_dcraw(const gchar *filename, gboolean half_size);
void rs_photo_open_dcraw_apply_black_and_shift_half_size(dcraw_data *raw, RS_PHOTO *photo);
static RS_PHOTO *rs_photo_open_gdk(const gchar *filename, gboolean half_size);
static GdkPixbuf *rs_thumb_gdk(const gchar *src);
static void rs_rect_flip(RS_RECT *in, RS_RECT *out, gint w, gint h);
static void rs_rect_mirror(RS_RECT *in, RS_RECT *out, gint w, gint h);
static void rs_rect_rotate(RS_RECT *in, RS_RECT *out, gint w, gint h, gint quarterturns);

RS_FILETYPE *filetypes;

static void
rs_add_filetype(gchar *id, gint filetype, const gchar *ext, gchar *description,
	RS_PHOTO *(*load)(const gchar *, gboolean),
	GdkPixbuf *(*thumb)(const gchar *),
	void (*load_meta)(const gchar *, RS_METADATA *),
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
	cur->load = load;
	cur->thumb = thumb;
	cur->load_meta = load_meta;
	cur->save = save;
	cur->next = NULL;
	return;
}

static void
rs_init_filetypes(void)
{
	filetypes = NULL;
	rs_add_filetype("cr2", FILETYPE_RAW, ".cr2", _("Canon CR2"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("crw", FILETYPE_RAW, ".crw", _("Canon CIFF"),
		rs_photo_open_dcraw, rs_ciff_load_thumb, rs_ciff_load_meta, NULL);
	rs_add_filetype("nef", FILETYPE_RAW, ".nef", _("Nikon NEF"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("mrw", FILETYPE_RAW, ".mrw", _("Minolta raw"),
		rs_photo_open_dcraw, rs_mrw_load_thumb, rs_mrw_load_meta, NULL);
	rs_add_filetype("cr-tiff", FILETYPE_RAW, ".tif", _("Canon TIFF"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("arw", FILETYPE_RAW, ".arw", _("Sony"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("kdc", FILETYPE_RAW, ".kdc", _("Kodak"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("x3f", FILETYPE_RAW, ".x3f", _("Sigma"),
		rs_photo_open_dcraw, rs_x3f_load_thumb, NULL, NULL);
	rs_add_filetype("orf", FILETYPE_RAW, ".orf", "",
		rs_photo_open_dcraw, rs_tiff_load_thumb, NULL, NULL);
	rs_add_filetype("raw", FILETYPE_RAW, ".raw", _("Panasonic raw"),
		rs_photo_open_dcraw, rs_panasonic_load_thumb, rs_panasonic_load_meta, NULL);
	rs_add_filetype("pef", FILETYPE_RAW, ".pef", _("Pentax raw"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("dng", FILETYPE_RAW, "dng", _("Adobe Digital negative"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("jpeg", FILETYPE_JPEG, ".jpg", _("JPEG (Joint Photographic Experts Group)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	rs_add_filetype("png", FILETYPE_PNG, ".png", _("PNG (Portable Network Graphics)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	rs_add_filetype("tiff8", FILETYPE_TIFF8, ".tif", _("8-bit TIFF (Tagged Image File Format)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	rs_add_filetype("tiff16", FILETYPE_TIFF16, ".tif", _("16-bit TIFF (Tagged Image File Format)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	return;
}

void
rs_update_preview(RS_BLOB *rs)
{
	if (!rs->in_use) return;

	rs_preview_widget_update(RS_PREVIEW_WIDGET(rs->preview));

	/* Update histogram */
	rs_color_transform_set_from_settings(rs->histogram_transform, rs->photo->settings[rs->current_setting], MASK_ALL);
	rs_histogram_set_color_transform(RS_HISTOGRAM_WIDGET(rs->histogram), rs->histogram_transform);

	rs_curve_draw_histogram(RS_CURVE_WIDGET(rs->settings[rs->current_setting]->curve), rs->histogram_dataset, rs->photo->settings[rs->current_setting]);
}

void
rs_reset(RS_BLOB *rs)
{
	gboolean in_use = rs->in_use;
	gint c;
	rs->in_use = FALSE;
	for(c=0;c<3;c++)
		rs_settings_reset(rs->settings[c], MASK_ALL);
	rs->in_use = in_use;
	return;
}

void
rs_free(RS_BLOB *rs)
{
	if (rs->in_use)
	{
		if (rs->photo!=NULL)
			rs_photo_free(rs->photo);
		rs->photo=NULL;
		rs->in_use=FALSE;
	}
}

void
rs_settings_to_rs_settings_double(RS_SETTINGS *rs_settings, RS_SETTINGS_DOUBLE *rs_settings_double)
{
	if (rs_settings==NULL)
		return;
	if (rs_settings_double==NULL)
		return;
	rs_settings_double->exposure = GETVAL(rs_settings->exposure);
	rs_settings_double->saturation = GETVAL(rs_settings->saturation);
	rs_settings_double->hue = GETVAL(rs_settings->hue);
	rs_settings_double->contrast = GETVAL(rs_settings->contrast);
	rs_settings_double->warmth = GETVAL(rs_settings->warmth);
	rs_settings_double->tint = GETVAL(rs_settings->tint);
	rs_curve_widget_get_knots(RS_CURVE_WIDGET(rs_settings->curve), &rs_settings_double->curve_knots, &rs_settings_double->curve_nknots);
	return;
}

void
rs_settings_double_to_rs_settings(RS_SETTINGS_DOUBLE *rs_settings_double, RS_SETTINGS *rs_settings)
{
	if (rs_settings_double==NULL)
		return;
	if (rs_settings==NULL)
		return;
	SETVAL(rs_settings->exposure, rs_settings_double->exposure);
	SETVAL(rs_settings->saturation, rs_settings_double->saturation);
	SETVAL(rs_settings->hue, rs_settings_double->hue);
	SETVAL(rs_settings->contrast, rs_settings_double->contrast);
	SETVAL(rs_settings->warmth, rs_settings_double->warmth);
	SETVAL(rs_settings->tint, rs_settings_double->tint);
	rs_curve_widget_reset(RS_CURVE_WIDGET(rs_settings->curve));
	if (rs_settings_double->curve_nknots>0)
	{
		gint i;
		for(i=0;i<rs_settings_double->curve_nknots;i++)
			rs_curve_widget_add_knot(RS_CURVE_WIDGET(rs_settings->curve), rs_settings_double->curve_knots[i*2+0],
				rs_settings_double->curve_knots[i*2+1]);
	}
	else
	{
		rs_curve_widget_add_knot(RS_CURVE_WIDGET(rs_settings->curve), 0.0f, 0.0f);
		rs_curve_widget_add_knot(RS_CURVE_WIDGET(rs_settings->curve), 1.0f, 1.0f);
	}
	return;
}

void
rs_settings_reset(RS_SETTINGS *rss, guint mask)
{
	if (mask & MASK_EXPOSURE)
		gtk_adjustment_set_value((GtkAdjustment *) rss->exposure, 0.0);
	if (mask & MASK_SATURATION)
		gtk_adjustment_set_value((GtkAdjustment *) rss->saturation, 1.0);
	if (mask & MASK_HUE)
		gtk_adjustment_set_value((GtkAdjustment *) rss->hue, 0.0);
	if (mask & MASK_CONTRAST)
		gtk_adjustment_set_value((GtkAdjustment *) rss->contrast, 1.0);
	if (mask & MASK_WARMTH)
		gtk_adjustment_set_value((GtkAdjustment *) rss->warmth, 0.0);
	if (mask & MASK_TINT)
		gtk_adjustment_set_value((GtkAdjustment *) rss->tint, 0.0);
	if (mask & MASK_CURVE)
	{
		rs_curve_widget_reset(RS_CURVE_WIDGET(rss->curve));
		rs_curve_widget_add_knot(RS_CURVE_WIDGET(rss->curve), 0.0f, 0.0f);
		rs_curve_widget_add_knot(RS_CURVE_WIDGET(rss->curve), 1.0f, 1.0f);
	}
	return;
}

static RS_SETTINGS *
rs_settings_new(void)
{
	RS_SETTINGS *rss;
	rss = g_malloc(sizeof(RS_SETTINGS));
	rss->exposure = gtk_adjustment_new(0.0, -3.0, 3.0, 0.1, 0.5, 0.0);
	rss->saturation = gtk_adjustment_new(1.0, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->hue = gtk_adjustment_new(0.0, -180.0, 180.0, 0.1, 30.0, 0.0);
	rss->contrast = gtk_adjustment_new(1.0, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->warmth = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	rss->tint = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	rss->curve = rs_curve_widget_new();
	return(rss);
}

RS_SETTINGS_DOUBLE *
rs_settings_double_new(void)
{
	RS_SETTINGS_DOUBLE *rssd;
	rssd = g_malloc(sizeof(RS_SETTINGS_DOUBLE));
	rssd->exposure = 0.0;
	rssd->saturation = 1.0;
	rssd->hue = 0.0;
	rssd->contrast = 1.0;
	rssd->warmth = 0.0;
	rssd->tint = 0.0;
	rssd->curve_nknots = 0;
	rssd->curve_knots = NULL;
	return rssd;
}

void
rs_settings_double_copy(RS_SETTINGS_DOUBLE *in, RS_SETTINGS_DOUBLE *out, gint mask)
{
	g_assert(in);
	g_assert(out);
	if (mask & MASK_EXPOSURE)
		out->exposure = in->exposure;
	if (mask & MASK_SATURATION)
		out->saturation = in->saturation;
	if (mask & MASK_HUE)
		out->hue = in->hue;
	if (mask & MASK_CONTRAST)
		out->contrast = in->contrast;
	if (mask & MASK_WARMTH)
		out->warmth = in->warmth;
	if (mask & MASK_TINT)
		out->tint = in->tint;
	if (mask & MASK_CURVE)
	{

		if (in->curve_nknots>1)
		{
			gint i;

			out->curve_nknots = in->curve_nknots;
			if (out->curve_knots)
				g_free(out->curve_knots);
			out->curve_knots = g_new(gfloat, out->curve_nknots*2);
			
			for(i=0;i<in->curve_nknots*2;i++)
				out->curve_knots[i] = in->curve_knots[i];
		}
	}
	return;
}

static void
rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd)
{
	g_free(rssd);
	return;
}

RS_METADATA *
rs_metadata_new(void)
{
	RS_METADATA *metadata;
	gint i;
	metadata = g_malloc(sizeof(RS_METADATA));
	metadata->make = MAKE_UNKNOWN;
	metadata->make_ascii = NULL;
	metadata->model_ascii = NULL;
	metadata->orientation = 0;
	metadata->aperture = -1.0;
	metadata->iso = 0;
	metadata->shutterspeed = 1.0;
	metadata->thumbnail_start = 0;
	metadata->thumbnail_length = 0;
	metadata->preview_start = 0;
	metadata->preview_length = 0;
	metadata->preview_planar_config = 0;
	metadata->preview_width = 0;
	metadata->preview_height = 0;
	metadata->cam_mul[0] = -1.0;
	metadata->contrast = -1.0;
	metadata->saturation = -1.0;
	metadata->sharpness = -1.0;
	metadata->color_tone = -1.0;
	metadata->focallength = -1;
	for(i=0;i<4;i++)
		metadata->cam_mul[i] = 1.0f;
	matrix4_identity(&metadata->adobe_coeff);
	metadata->data = NULL;
	return(metadata);
}

void
rs_metadata_free(RS_METADATA *metadata)
{
	if (metadata->make_ascii)
		g_free(metadata->make_ascii);
	if (metadata->model_ascii)
		g_free(metadata->model_ascii);
	g_free(metadata);
	return;
}

void
rs_metadata_normalize_wb(RS_METADATA *meta)
{
	gdouble div;
	if ((meta->cam_mul[1]+meta->cam_mul[3])!=0.0)
	{
		div = 2/(meta->cam_mul[1]+meta->cam_mul[3]);
		meta->cam_mul[0] *= div;
		meta->cam_mul[1] = 1.0;
		meta->cam_mul[2] *= div;
		meta->cam_mul[3] = 1.0;
	}
	return;
}

RS_PHOTO *
rs_photo_new()
{
	guint c;
	RS_PHOTO *photo;
	photo = g_malloc(sizeof(RS_PHOTO));
	photo->filename = NULL;
	if (!photo) return(NULL);
	photo->input = NULL;
	ORIENTATION_RESET(photo->orientation);
	photo->priority = PRIO_U;
	photo->metadata = rs_metadata_new();
	for(c=0;c<3;c++)
		photo->settings[c] = rs_settings_double_new();
	photo->crop = NULL;
	photo->angle = 0.0;
	photo->exported = FALSE;
	return(photo);
}

void
rs_photo_flip(RS_PHOTO *photo)
{
	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_flip(photo->crop, photo->crop, w, h);
	}
	ORIENTATION_FLIP(photo->orientation);
}

void
rs_photo_mirror(RS_PHOTO *photo)
{
	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_mirror(photo->crop, photo->crop, w, h);
	}
	ORIENTATION_MIRROR(photo->orientation);
}

void
rs_photo_rotate(RS_PHOTO *photo, gint quarterturns, gdouble angle)
{
	gint n;
	photo->angle += angle;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_rotate(photo->crop, photo->crop, w, h, quarterturns);
	}

	for(n=0;n<quarterturns;n++)
		ORIENTATION_90(photo->orientation);

	return;

}

void
rs_photo_set_crop(RS_PHOTO *photo, RS_RECT *crop)
{
	if (photo->crop)
		g_free(photo->crop);
	photo->crop = NULL;

	if (crop)
	{
		photo->crop = g_new(RS_RECT, 1);
		*photo->crop = *crop;
	}
}

RS_RECT *
rs_photo_get_crop(RS_PHOTO *photo)
{
	return photo->crop;
}

gboolean
rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gboolean keep_aspect, gdouble scale, gint snapshot, RS_CMS *cms)
{
	GdkPixbuf *pixbuf;
	RS_IMAGE16 *rsi;
	RS_IMAGE8 *image8;
	RS_IMAGE16 *image16;
	gint quality = 100;
	gboolean uncompressed_tiff = FALSE;
	RS_COLOR_TRANSFORM *rct;
	void *transform = NULL;

	/* transform and crop */
	rsi = rs_image16_transform(photo->input, NULL,
			NULL, NULL, photo->crop, width, height, keep_aspect, scale,
			photo->angle, photo->orientation, NULL);

	if (cms)
		transform = rs_cms_get_transform(cms, TRANSFORM_EXPORT);

	/* Initialize color transform */
	rct = rs_color_transform_new();
	rs_color_transform_set_from_settings(rct, photo->settings[snapshot], MASK_ALL);
	rs_color_transform_set_output_format(rct, 8);
	rs_color_transform_set_cms_transform(rct, transform);

	/* actually save */
	switch (filetype)
	{
		case FILETYPE_JPEG:
			image8 = rs_image8_new(rsi->w, rsi->h, 3, 3);
			rct->transform(rct, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, image8->pixels, image8->rowstride);

			rs_conf_get_integer(CONF_EXPORT_JPEG_QUALITY, &quality);
			if (quality > 100)
				quality = 100;
			else if (quality < 0)
				quality = 0;

			rs_jpeg_save(image8, filename, quality, rs_cms_get_profile_filename(cms, PROFILE_EXPORT));
			rs_image8_free(image8);
			break;
		case FILETYPE_PNG:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rct->transform(rct, rsi->w, rsi->h, rsi->pixels, rsi->rowstride,
				gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));
			gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_TIFF8:
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			image8 = rs_image8_new(rsi->w, rsi->h, 3, 3);
			rct->transform(rct, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, image8->pixels, image8->rowstride);
			rs_tiff8_save(image8, filename, rs_cms_get_profile_filename(cms, PROFILE_EXPORT), uncompressed_tiff);
			rs_image8_free(image8);
			break;
		case FILETYPE_TIFF16:
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			image16 = rs_image16_new(rsi->w, rsi->h, 3, 3);
			rs_color_transform_set_output_format(rct, 16);
			rct->transform(rct, rsi->w, rsi->h,
				rsi->pixels, rsi->rowstride,
				image16->pixels, image16->rowstride*2);
			rs_tiff16_save(image16, filename, rs_cms_get_profile_filename(cms, PROFILE_EXPORT), uncompressed_tiff);
			rs_image16_free(image16);
			break;
	}

	rs_image16_free(rsi);
	rs_color_transform_free(rct);

	photo->exported = TRUE;
	rs_cache_save(photo);

	/* Set the exported flag */
	rs_store_set_flags(NULL, photo->filename, NULL, NULL, &photo->exported);
	return(TRUE);
}

void
rs_photo_free(RS_PHOTO *photo)
{
	guint c;
	if (!photo) return;
	g_free(photo->filename);
	if (photo->metadata)
	{
		rs_metadata_free(photo->metadata);
		photo->metadata = NULL;
	}
	if (photo->input)
	{
		rs_image16_free(photo->input);
		photo->input = NULL;
	}
	for(c=0;c<3;c++)
		rs_settings_double_free(photo->settings[c]);
	if (photo->crop)
	{
		g_free(photo->crop);
		photo->crop = NULL;
	}
	g_free(photo);
	return;
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
	rs->in_use = FALSE;
	rs->photo = NULL;
	rs->queue = rs_batch_new_queue();
	rs->current_setting = 0;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();
	return(rs);
}

void
rs_photo_close(RS_PHOTO *photo)
{
	if (!photo) return;
	rs_cache_save(photo);
	return;
}

static RS_PHOTO *
rs_photo_open_dcraw(const gchar *filename, gboolean half_size)
{
	dcraw_data *raw;
	RS_PHOTO *photo=NULL;

	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);
		photo = rs_photo_new(NULL);

		if (half_size)
		{
			photo->input = rs_image16_new(raw->raw.width, raw->raw.height, raw->raw.colors, 4);
			rs_photo_open_dcraw_apply_black_and_shift_half_size(raw, photo);
		}
		else
		{
			photo->input = rs_image16_new(raw->raw.width*2, raw->raw.height*2, raw->raw.colors, 4);
			rs_photo_open_dcraw_apply_black_and_shift(raw, photo);
		}

		photo->input->filters = raw->filters;
		photo->input->fourColorFilters = raw->fourColorFilters;
		photo->filename = g_strdup(filename);
		dcraw_close(raw);
	}
	g_free(raw);
	return(photo);
}

void
rs_photo_open_dcraw_apply_black_and_shift_half_size(dcraw_data *raw, RS_PHOTO *photo)
{
	gushort *dst, *src;
	gint row, col;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	for(row=0;row<(raw->raw.height);row++)
	{
		src = (gushort *) raw->raw.image + row * raw->raw.width * 4;
		dst = GET_PIXEL(photo->input, 0, row);
		col = raw->raw.width;
		while(col--)
		{
			register gint r, g, b, g2;
			r  = *src++ - raw->black;
			g  = *src++ - raw->black;
			b  = *src++ - raw->black;
			g2 = *src++ - raw->black;
			r  = MAX(0, r);
			g  = MAX(0, g);
			b  = MAX(0, b);
			g2 = MAX(0, g2);
			*dst++ = (gushort)( r<<shift);
			*dst++ = (gushort)( g<<shift);
			*dst++ = (gushort)( b<<shift);
			*dst++ = (gushort)(g2<<shift);
		}
	}
}

/* Function pointer. Initiliazed by arch binder */
void
(*rs_photo_open_dcraw_apply_black_and_shift)(dcraw_data *raw, RS_PHOTO *photo);

void
rs_photo_open_dcraw_apply_black_and_shift_c(dcraw_data *raw, RS_PHOTO *photo)
{
	gushort *dst1, *dst2, *src;
	gint row, col;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	for(row=0;row<(raw->raw.height*2);row+=2)
	{
		src = (gushort *) raw->raw.image + row/2 * raw->raw.width * 4;
		dst1 = GET_PIXEL(photo->input, 0, row);
		dst2 = GET_PIXEL(photo->input, 0, row+1);
		col = raw->raw.width;
		while(col--)
		{
			register gint r, g, b, g2;
			r  = *src++ - raw->black;
			g  = *src++ - raw->black;
			b  = *src++ - raw->black;
			g2 = *src++ - raw->black;
			r  = MAX(0, r);
			g  = MAX(0, g);
			b  = MAX(0, b);
			g2 = MAX(0, g2);
			*dst1++ = (gushort)( r<<shift);
			*dst1++ = (gushort)( g<<shift);
			*dst1++ = (gushort)( b<<shift);
			*dst1++ = (gushort)(g2<<shift);
			*dst1++ = (gushort)( r<<shift);
			*dst1++ = (gushort)( g<<shift);
			*dst1++ = (gushort)( b<<shift);
			*dst1++ = (gushort)(g2<<shift);
			*dst2++ = (gushort)( r<<shift);
			*dst2++ = (gushort)( g<<shift);
			*dst2++ = (gushort)( b<<shift);
			*dst2++ = (gushort)(g2<<shift);
			*dst2++ = (gushort)( r<<shift);
			*dst2++ = (gushort)( g<<shift);
			*dst2++ = (gushort)( b<<shift);
			*dst2++ = (gushort)(g2<<shift);
		}
	}
}

#if defined (__i386__) || defined (__x86_64__)
void
rs_photo_open_dcraw_apply_black_and_shift_mmx(dcraw_data *raw, RS_PHOTO *photo)
{
	char b[8];
	volatile gushort *sub = (gushort *) b;
	void *srcoffset;
	void *destoffset;
	guint x;
	guint y;
	gushort *src = (gushort*)raw->raw.image;
	volatile gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	sub[0] = raw->black;
	sub[1] = raw->black;
	sub[2] = raw->black;
	sub[3] = raw->black;

	for (y=0; y<(raw->raw.height*2); y++)
	{
		destoffset = (void*) (photo->input->pixels + y*photo->input->rowstride);
		srcoffset = (void*) (src + y/2 * raw->raw.width * photo->input->pixelsize);
		x = raw->raw.width;
		asm volatile (
			"mov %3, %%"REG_a"\n\t" /* copy x to %eax */
			"movq (%2), %%mm7\n\t" /* put black in %mm7 */
			"movq (%4), %%mm6\n\t" /* put shift in %mm6 */
			".p2align 4,,15\n"
			"load_raw_inner_loop:\n\t"
			"movq (%1), %%mm0\n\t" /* load source */
			"movq 8(%1), %%mm1\n\t"
			"movq 16(%1), %%mm2\n\t"
			"movq 24(%1), %%mm3\n\t"
			"psubusw %%mm7, %%mm0\n\t" /* subtract black */
			"psubusw %%mm7, %%mm1\n\t"
			"psubusw %%mm7, %%mm2\n\t"
			"psubusw %%mm7, %%mm3\n\t"
			"psllw %%mm6, %%mm0\n\t" /* bitshift */
			"psllw %%mm6, %%mm1\n\t"
			"psllw %%mm6, %%mm2\n\t"
			"psllw %%mm6, %%mm3\n\t"
			"movq %%mm0, (%0)\n\t" /* write destination (twice) */
			"movq %%mm0, 8(%0)\n\t"
			"movq %%mm1, 16(%0)\n\t"
			"movq %%mm1, 24(%0)\n\t"
			"movq %%mm2, 32(%0)\n\t"
			"movq %%mm2, 40(%0)\n\t"
			"movq %%mm3, 48(%0)\n\t"
			"movq %%mm3, 56(%0)\n\t"
			"sub $4, %%"REG_a"\n\t"
			"add $64, %0\n\t"
			"add $32, %1\n\t"
			"cmp $3, %%"REG_a"\n\t"
			"jg load_raw_inner_loop\n\t"
			"cmp $1, %%"REG_a"\n\t"
			"jb load_raw_inner_done\n\t"
			".p2align 4,,15\n"
			"load_raw_leftover:\n\t"
			"movq (%1), %%mm0\n\t" /* leftover pixels */
			"psubusw %%mm7, %%mm0\n\t"
			"psllw %%mm6, %%mm0\n\t"
			"movq %%mm0, (%0)\n\t"
			"sub $1, %%"REG_a"\n\t"
			"cmp $0, %%"REG_a"\n\t"
			"jg load_raw_leftover\n\t"
			"load_raw_inner_done:\n\t"
			"emms\n\t" /* clean up */
			: "+r" (destoffset), "+r" (srcoffset)
			: "r" (sub), "r" ((gulong)x), "r" (&shift)
			: "%"REG_a
			);
	}
}
#endif

RS_FILETYPE *
rs_filetype_get(const gchar *filename, gboolean load)
{
	RS_FILETYPE *filetype = filetypes;
	gchar *iname;
	gint n;
	gboolean load_gdk = FALSE;
	rs_conf_get_boolean(CONF_LOAD_GDK, &load_gdk);
	iname = g_ascii_strdown(filename,-1);
	n = 0;
	while(filetype)
	{
		if (g_str_has_suffix(iname, filetype->ext))
		{
			if ((!load) || (filetype->load))
			{
				if (filetype->filetype == FILETYPE_RAW)
					return(filetype);
				else if ((filetype->filetype != FILETYPE_RAW) && (load_gdk))
					return(filetype);
				break;
			}
		}
		filetype = filetype->next;
	}
	g_free(iname);
	return(NULL);
}

static RS_PHOTO *
rs_photo_open_gdk(const gchar *filename, gboolean half_size)
{
	RS_PHOTO *photo=NULL;
	GdkPixbuf *pixbuf;
	guchar *pixels;
	gint rowstride;
	gint width, height;
	gint row,col,n,res, src, dest;
	gdouble nd;
	gushort gammatable[256];
	gint alpha=0;
	if ((pixbuf = gdk_pixbuf_new_from_file(filename, NULL)))
	{
		photo = rs_photo_new();
		for(n=0;n<256;n++)
		{
			nd = ((gdouble) n) / 255.0;
			res = (gint) (pow(nd, GAMMA) * 65535.0);
			_CLAMP65535(res);
			gammatable[n] = res;
		}
		rowstride = gdk_pixbuf_get_rowstride(pixbuf);
		pixels = gdk_pixbuf_get_pixels(pixbuf);
		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);
		if (gdk_pixbuf_get_has_alpha(pixbuf))
			alpha = 1;
		photo->input = rs_image16_new(width, height, 3, 4);
		for(row=0;row<photo->input->h;row++)
		{
			dest = row * photo->input->rowstride;
			src = row * rowstride;
			for(col=0;col<photo->input->w;col++)
			{
				photo->input->pixels[dest++] = gammatable[pixels[src++]];
				photo->input->pixels[dest++] = gammatable[pixels[src++]];
				photo->input->pixels[dest++] = gammatable[pixels[src++]];
				photo->input->pixels[dest++] = gammatable[pixels[src-2]];
				src+=alpha;
			}
		}
		g_object_unref(pixbuf);
		photo->filename = g_strdup(filename);
	}
	return(photo);
}

gchar *
rs_confdir_get()
{
	static gchar *dir = NULL;

	if (!dir)
	{
		GString *gs = g_string_new(g_get_home_dir());
		g_string_append(gs, G_DIR_SEPARATOR_S);
		g_string_append(gs, ".rawstudio/");
		dir = gs->str;
		g_string_free(gs, FALSE);
	}

	g_mkdir_with_parents(dir, 00755);

	return dir;
}

gchar *
rs_dotdir_get(const gchar *filename)
{
	gchar *ret;
	gchar *directory;
	GString *dotdir;
	gboolean dotdir_is_local = FALSE;
	rs_conf_get_boolean(CONF_CACHEDIR_IS_LOCAL, &dotdir_is_local);

	directory = g_path_get_dirname(filename);
	if (dotdir_is_local)
	{
		dotdir = g_string_new(g_get_home_dir());
		dotdir = g_string_append(dotdir, G_DIR_SEPARATOR_S);
		dotdir = g_string_append(dotdir, DOTDIR);
		dotdir = g_string_append(dotdir, G_DIR_SEPARATOR_S);
		dotdir = g_string_append(dotdir, directory);
	}
	else
	{
		dotdir = g_string_new(directory);
		dotdir = g_string_append(dotdir, G_DIR_SEPARATOR_S);
		dotdir = g_string_append(dotdir, DOTDIR);
	}

	if (!g_file_test(dotdir->str, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
	{
		if (g_mkdir_with_parents(dotdir->str, 0700) != 0)
			ret = NULL;
		else
			ret = dotdir->str;
	}
	else
		ret = dotdir->str;
	g_free(directory);
	g_string_free(dotdir, FALSE);
	return (ret);
}

gchar *
rs_thumb_get_name(const gchar *src)
{
	gchar *ret=NULL;
	gchar *dotdir, *filename;
	GString *out;
	dotdir = rs_dotdir_get(src);
	filename = g_path_get_basename(src);
	if (dotdir)
	{
		out = g_string_new(dotdir);
		out = g_string_append(out, G_DIR_SEPARATOR_S);
		out = g_string_append(out, filename);
		out = g_string_append(out, ".thumb.png");
		ret = out->str;
		g_string_free(out, FALSE);
		g_free(dotdir);
	}
	g_free(filename);
	return(ret);
}

static GdkPixbuf *
rs_thumb_gdk(const gchar *src)
{
	GdkPixbuf *pixbuf=NULL;
	gchar *thumbname;

	thumbname = rs_thumb_get_name(src);

	if (thumbname)
	{
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
		}
		else
		{
			pixbuf = gdk_pixbuf_new_from_file_at_size(src, 128, 128, NULL);
			gdk_pixbuf_save(pixbuf, thumbname, "png", NULL, NULL);
			g_free(thumbname);
		}
	}
	else
		pixbuf = gdk_pixbuf_new_from_file_at_size(src, 128, 128, NULL);
	return(pixbuf);
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
		RS_COLOR_TRANSFORM *rct;

		rct = rs_color_transform_new();
		rs_color_transform_set_from_settings(rct, rs->photo->settings[rs->current_setting], MASK_ALL ^ MASK_CURVE);
		rs_color_transform_make_histogram(rct, rs->histogram_dataset, hist);
		rs_color_transform_free(rct);

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

		rs_curve_widget_move_knot(RS_CURVE_WIDGET(rs->settings[rs->current_setting]->curve),0,blackpoint,0.0);
		rs_curve_widget_move_knot(RS_CURVE_WIDGET(rs->settings[rs->current_setting]->curve),-1,whitepoint,1.0);
	}
}

void
rs_set_wb_auto(RS_BLOB *rs)
{
	gint row, col, x, y, c, val;
	gint sum[8];
	gdouble pre_mul[4];
	gdouble dsum[8];

	if(unlikely(!rs->photo)) return;

	for (c=0; c < 8; c++)
		dsum[c] = 0.0;

	for (row=0; row < rs->photo->input->h-7; row += 8)
		for (col=0; col < rs->photo->input->w-7; col += 8)
		{
			memset (sum, 0, sizeof sum);
			for (y=row; y < row+8; y++)
				for (x=col; x < col+8; x++)
					for(c=0;c<4;c++)
					{
						val = rs->photo->input->pixels[y*rs->photo->input->rowstride+x*4+c];
						if (!val) continue;
						if (val > 65100)
							goto skip_block; /* I'm sorry mom */
						sum[c] += val;
						sum[c+4]++;
					}
			for (c=0; c < 8; c++)
				dsum[c] += sum[c];
skip_block:
							continue;
		}
	for(c=0;c<4;c++)
		if (dsum[c])
			pre_mul[c] = dsum[c+4] / dsum[c];
	rs_set_wb_from_mul(rs, pre_mul);
	return;
}

void
rs_set_wb_from_color(RS_BLOB *rs, gdouble r, gdouble g, gdouble b)
{
	gdouble warmth, tint;
	warmth = (b-r)/(r+b); /* r*(1+warmth) = b*(1-warmth) */
	tint = -g/(r+r*warmth)+2.0; /* magic */
	rs_set_wb(rs, warmth, tint);
	return;
}

void
rs_set_wb_from_mul(RS_BLOB *rs, gdouble *mul)
{
	int c;
	gdouble max=0.0, warmth, tint;

	for (c=0; c < 4; c++)
		if (max < mul[c])
			max = mul[c];

	for(c=0;c<4;c++)
		mul[c] /= max;

	mul[R] *= (1.0/mul[G]);
	mul[B] *= (1.0/mul[G]);
	mul[G] = 1.0;
	mul[G2] = 1.0;

	tint = (mul[B] + mul[R] - 4.0)/-2.0;
	warmth = (mul[R]/(2.0-tint))-1.0;
	rs_set_wb(rs, warmth, tint);
	return;
}

void
rs_set_wb(RS_BLOB *rs, gfloat warmth, gfloat tint)
{
	gboolean in_use = rs->in_use;
	rs->in_use = FALSE;
	SETVAL(rs->settings[rs->current_setting]->warmth, warmth);
	rs->in_use = in_use;
	SETVAL(rs->settings[rs->current_setting]->tint, tint);
	return;
}

void
rs_apply_settings_from_double(RS_SETTINGS *rss, RS_SETTINGS_DOUBLE *rsd, gint mask)
{
	if (mask & MASK_EXPOSURE)
		SETVAL(rss->exposure,rsd->exposure);
	if (mask & MASK_SATURATION)
		SETVAL(rss->saturation,rsd->saturation);
	if (mask & MASK_HUE)
		SETVAL(rss->hue,rsd->hue);
	if (mask & MASK_CONTRAST)
		SETVAL(rss->contrast,rsd->contrast);
	if (mask & MASK_WARMTH)
		SETVAL(rss->warmth,rsd->warmth);
	if (mask & MASK_TINT)
		SETVAL(rss->tint,rsd->tint);
	if (mask & MASK_CURVE) {
		rs_curve_widget_reset(RS_CURVE_WIDGET(rss->curve));

		if (rsd->curve_nknots>1)
		{
			gint i;
			for(i=0;i<rsd->curve_nknots;i++)
				rs_curve_widget_add_knot(RS_CURVE_WIDGET(rss->curve), 
										rsd->curve_knots[i*2+0],
										rsd->curve_knots[i*2+1]
										);
		}
		else
		{
			rs_curve_widget_add_knot(RS_CURVE_WIDGET(rss->curve), 0.0f, 0.0f);
			rs_curve_widget_add_knot(RS_CURVE_WIDGET(rss->curve), 1.0f, 1.0f);
		}
	}
	return;
}

void
rs_rect_scale(RS_RECT *in, RS_RECT *out, gdouble scale)
{
	out->x1 = (gint) (((gdouble) in->x1)*scale);
	out->x2 = (gint) (((gdouble) in->x2)*scale);
	out->y1 = (gint) (((gdouble) in->y1)*scale);
	out->y2 = (gint) (((gdouble) in->y2)*scale);
	return;
}

void
rs_rect_union(RS_RECT *a, RS_RECT *b, RS_RECT *destination)
{
	destination->x1 = (a->x1 > b->x1) ? a->x1 : b->x1;
	destination->y1 = (a->y1 > b->y1) ? a->y1 : b->y1;
	destination->x2 = (a->x2 < b->x2) ? a->x2 : b->x2;
	destination->y2 = (a->y2 < b->y2) ? a->y2 : b->y2;
	return;
}

void
rs_rect_normalize(RS_RECT *in, RS_RECT *out)
{
	gint n;
	gint x1,y1;
	gint x2,y2;

	x1 = in->x2;
	x2 = in->x1;
	y1 = in->y1;
	y2 = in->y2;

	if (x1>x2)
	{
		n = x1;
		x1 = x2;
		x2 = n;
	}
	if (y1>y2)
	{
		n = y1;
		y1 = y2;
		y2 = n;
	}

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
}

static void
rs_rect_flip(RS_RECT *in, RS_RECT *out, gint w, gint h)
{
	gint x1,y1;
	gint x2,y2;

	x1 = in->x1;
	x2 = in->x2;
	y1 = h - in->y2 - 1;
	y2 = h - in->y1 - 1;

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
	rs_rect_normalize(out, out);

	return;
}

static void
rs_rect_mirror(RS_RECT *in, RS_RECT *out, gint w, gint h)
{
	gint x1,y1;
	gint x2,y2;

	x1 = w - in->x2 - 1;
	x2 = w - in->x1 - 1;
	y1 = in->y1;
	y2 = in->y2;

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
	rs_rect_normalize(out, out);

	return;
}

static void
rs_rect_rotate(RS_RECT *in, RS_RECT *out, gint w, gint h, gint quarterturns)
{
	gint x1,y1;
	gint x2,y2;

	x1 = in->x2;
	x2 = in->x1;
	y1 = in->y1;
	y2 = in->y2;

	switch(quarterturns)
	{
		case 1:
			x1 = h - in->y1-1;
			x2 = h - in->y2-1;
			y1 = in->x1;
			y2 = in->x2;
			break;
		case 2:
			x1 = w - in->x1 - 1;
			x2 = w - in->x2 - 1;
			y1 = h - in->y1 - 1;
			y2 = h - in->y2 - 1;
			break;
		case 3:
			x1 = in->y1;
			x2 = in->y2;
			y1 = w - in->x1 - 1;
			y2 = w - in->x2 - 1;
			break;
	}

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
	rs_rect_normalize(out, out);

	return;
}

void
check_install()
{
#define TEST_FILE_ACCESS(path) do { if (g_access(path, R_OK)!=0) g_debug("Cannot access %s\n", path);} while (0)
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/overlay_priority1.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/overlay_priority2.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/overlay_priority3.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/overlay_deleted.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/overlay_exported.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_flip.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_mirror.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_90.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_180.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR "/pixmaps/rawstudio/transform_270.png");

#undef TEST_FILE_ACCESS
}

int
main(int argc, char **argv)
{
	RS_BLOB *rs;
	int optimized = 1;
	int opt;

	while ((opt = getopt(argc, argv, "n")) != -1) {
		switch (opt) {
		case 'n':
			optimized = 0;
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
	rs_init_filetypes();
	gtk_init(&argc, &argv);
	check_install();

	rs = rs_new();
	rs->queue->cms = rs->cms = rs_cms_init();
	gui_init(argc, argv, rs);

	/* This is so fucking evil, but Rawstudio will deadlock in some GTK atexit() function from time to time :-/ */
	_exit(0);
}

gboolean
rs_shutdown(GtkWidget *dummy1, GdkEvent *dummy2, RS_BLOB *rs)
{
	rs_photo_close(rs->photo);
	rs_conf_set_integer(CONF_LAST_PRIORITY_PAGE, gtk_notebook_get_current_page(GTK_NOTEBOOK(rs->store)));
	gtk_main_quit();
	return(TRUE);
}

#if !GLIB_CHECK_VERSION(2,8,0)

/* Include our own g_mkdir_with_parents() in case of old glib.
Copied almost verbatim from glib-2.10.0/glib/gfileutils.c */
int
g_mkdir_with_parents (const gchar *pathname,
		      int          mode)
{
  gchar *fn, *p;

  if (pathname == NULL || *pathname == '\0')
    {
      return -1;
    }

  fn = g_strdup (pathname);

  if (g_path_is_absolute (fn))
    p = (gchar *) g_path_skip_root (fn);
  else
    p = fn;

  do
    {
      while (*p && !G_IS_DIR_SEPARATOR (*p))
	p++;
      
      if (!*p)
	p = NULL;
      else
	*p = '\0';
      
      if (!g_file_test (fn, G_FILE_TEST_EXISTS))
	{
	  if (g_mkdir (fn, mode) == -1)
	    {
	      g_free (fn);
	      return -1;
	    }
	}
      else if (!g_file_test (fn, G_FILE_TEST_IS_DIR))
	{
	  g_free (fn);
	  return -1;
	}
      if (p)
	{
	  *p++ = G_DIR_SEPARATOR;
	  while (*p && G_IS_DIR_SEPARATOR (*p))
	    p++;
	}
    }
  while (p);

  g_free (fn);

  return 0;
}

/* Include our own g_access() in case of old glib.
Copied almost verbatim from glib-2.12.13/glib/gstdio.h */
int
g_access (const gchar *filename,
	  int          mode)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      
      if (wfilename == NULL)
	{
	  return -1;
	}

      retval = _waccess (wfilename, mode);

      g_free (wfilename);

      return retval;
    }
  else
    {    
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;

      if (cp_filename == NULL)
	{
	  return -1;
	}

      retval = access (cp_filename, mode);

      g_free (cp_filename);

      return retval;
    }
#else
  return access (filename, mode);
#endif
}

#endif

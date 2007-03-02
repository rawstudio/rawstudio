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
#include "matrix.h"
#include "rawstudio.h"
#include "rs-crop.h"
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
#include "rs-render.h"
#include "rs-arch.h"
#include "rs-batch.h"
#include "rs-cms.h"

static void update_scaled(RS_BLOB *rs, gboolean force);
static inline void rs_render_mask(guchar *pixels, guchar *mask, guint length);
static gboolean rs_render_idle(RS_BLOB *rs);
static void rs_render_overlay(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, guchar *out, gint out_rowstride,
	guchar *mask, gint mask_rowstride, RS_CMS *cms);
static RS_SETTINGS *rs_settings_new();
static RS_SETTINGS_DOUBLE *rs_settings_double_new();
static void rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd);
static RS_PHOTO *rs_photo_open_dcraw(const gchar *filename);
static RS_PHOTO *rs_photo_open_gdk(const gchar *filename);
static GdkPixbuf *rs_thumb_gdk(const gchar *src);
static gboolean rs_mark_roi_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
static void rs_rect_flip(RS_RECT *in, RS_RECT *out, gint w, gint h);
static void rs_rect_mirror(RS_RECT *in, RS_RECT *out, gint w, gint h);
static void rs_rect_rotate(RS_RECT *in, RS_RECT *out, gint w, gint h, gint quarterturns);

RS_FILETYPE *filetypes;

static void
rs_add_filetype(gchar *id, gint filetype, const gchar *ext, gchar *description,
	RS_PHOTO *(*load)(const gchar *),
	GdkPixbuf *(*thumb)(const gchar *),
	void (*load_meta)(const gchar *, RS_METADATA *),
	gboolean (*save)(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gdouble scale, RS_CMS *cms))
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
	rs_add_filetype("cr2", FILETYPE_RAW, "cr2", _("Canon CR2"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("crw", FILETYPE_RAW, "crw", _("Canon CIFF"),
		rs_photo_open_dcraw, rs_ciff_load_thumb, rs_ciff_load_meta, NULL);
	rs_add_filetype("nef", FILETYPE_RAW, "nef", _("Nikon NEF"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("mrw", FILETYPE_RAW, "mrw", _("Minolta raw"),
		rs_photo_open_dcraw, rs_mrw_load_thumb, rs_mrw_load_meta, NULL);
	rs_add_filetype("cr-tiff", FILETYPE_RAW, "tif", _("Canon TIFF"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("arw", FILETYPE_RAW, "arw", _("Sony"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("kdc", FILETYPE_RAW, "kdc", _("Kodak"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta, NULL);
	rs_add_filetype("x3f", FILETYPE_RAW, "x3f", _("Sigma"),
		rs_photo_open_dcraw, rs_x3f_load_thumb, NULL, NULL);
	rs_add_filetype("orf", FILETYPE_RAW, "orf", "",
		rs_photo_open_dcraw, rs_tiff_load_thumb, NULL, NULL);
	rs_add_filetype("raw", FILETYPE_RAW, "raw", _("Panasonic raw"),
		rs_photo_open_dcraw, rs_panasonic_load_thumb, rs_panasonic_load_meta, NULL);
	rs_add_filetype("pef", FILETYPE_RAW, "pef", _("Pentax raw"),
		rs_photo_open_dcraw, rs_tiff_load_thumb, NULL, NULL);
	rs_add_filetype("jpeg", FILETYPE_JPEG, "jpg", _("JPEG (Joint Photographic Experts Group)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	rs_add_filetype("png", FILETYPE_PNG, "png", _("PNG (Portable Network Graphics)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	rs_add_filetype("tiff8", FILETYPE_TIFF8, "tif", _("8-bit TIFF (Tagged Image File Format)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	rs_add_filetype("tiff16", FILETYPE_TIFF16, "tif", _("16-bit TIFF (Tagged Image File Format)"),
		rs_photo_open_gdk, rs_thumb_gdk, NULL, rs_photo_save);
	return;
}

static void
update_scaled(RS_BLOB *rs, gboolean force)
{
	/* scale if needed */
	if ((rs->preview_scale != GETVAL(rs->scale)) || force)
	{
		gdouble scale;
		rs->preview_scale = GETVAL(rs->scale);

		if (rs->zoom_to_fit)
			scale = -1.0;
		else
			scale = rs->preview_scale;

		if (rs->photo->scaled)
			rs_image16_free(rs->photo->scaled);
		rs->photo->scaled = rs_image16_transform(rs->photo->input, NULL,
			&rs->photo->affine, &rs->photo->inverse_affine, rs->photo->crop,
			rs->preview_width-12, rs->preview_height-12, TRUE,
			scale, rs->photo->angle, rs->photo->orientation);
		matrix3_affine_transform_point_int(&rs->photo->affine,
			rs->roi.x1, rs->roi.y1,
			&rs->roi_scaled.x1, &rs->roi_scaled.y1);
		matrix3_affine_transform_point_int(&rs->photo->affine,
			rs->roi.x2, rs->roi.y2,
			&rs->roi_scaled.x2, &rs->roi_scaled.y2);
		rs->preview_done = TRUE; /* stop rs_render_idle() */
	}

	/* allocate 8 bit buffers if needed */
	if (!rs_image16_8_cmp_size(rs->photo->scaled, rs->photo->preview))
	{
		rs_image8_free(rs->photo->preview);
		rs->photo->preview = rs_image8_new(rs->photo->scaled->w, rs->photo->scaled->h, 3, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->photo->scaled->w, rs->photo->scaled->h);
	}

	/* allocate mask-buffer if needed */
	if (!rs_image16_8_cmp_size(rs->photo->scaled, rs->photo->mask))
	{
		rs_image8_free(rs->photo->mask);
		rs->photo->mask = rs_image8_new(rs->photo->scaled->w, rs->photo->scaled->h, 1, 1);
	}
	return;
}

inline void
rs_photo_prepare(RS_PHOTO *photo)
{
	matrix4_identity(&photo->mat);
#ifdef USE_ADOBE_COEFF
	matrix4_multiply(&photo->mat, &photo->metadata->adobe_coeff, &photo->mat);
#endif
	matrix4_color_exposure(&photo->mat, photo->settings[photo->current_setting]->exposure);

	photo->pre_mul[R] = (1.0+photo->settings[photo->current_setting]->warmth)
		*(2.0-photo->settings[photo->current_setting]->tint);
	photo->pre_mul[G] = 1.0;
	photo->pre_mul[B] = (1.0-photo->settings[photo->current_setting]->warmth)
		*(2.0-photo->settings[photo->current_setting]->tint);
	photo->pre_mul[G2] = 1.0;

	matrix4_color_saturate(&photo->mat, photo->settings[photo->current_setting]->saturation);
	matrix4_color_hue(&photo->mat, photo->settings[photo->current_setting]->hue);
	matrix4_to_matrix4int(&photo->mat, &photo->mati);
}

void
update_preview(RS_BLOB *rs, gboolean update_table, gboolean update_scale)
{
	if(unlikely(!rs->photo)) return;

	if (update_table)
		rs_render_previewtable(rs->photo->settings[rs->photo->current_setting]->contrast);
	update_scaled(rs, update_scale);
	rs_photo_prepare(rs->photo);
	update_preview_region(rs, rs->preview_exposed, TRUE);

	/* Reset histogram_table */
	if (GTK_WIDGET_VISIBLE(rs->histogram_image))
	{
		memset(rs->histogram_table, 0x00, sizeof(guint)*3*256);
		rs_render_histogram_table(rs->photo, rs->histogram_dataset, (guint *) rs->histogram_table);
		update_histogram(rs);
	}

	rs->preview_done = FALSE;
	rs->preview_idle_render_lastrow = 0;
	if (!rs->preview_idle_render)
	{
		rs->preview_idle_render = TRUE;
		gui_set_busy(TRUE);
		g_idle_add((GSourceFunc) rs_render_idle, rs);
	}

	return;
}	

void
update_preview_region(RS_BLOB *rs, RS_RECT *region, gboolean force_render)
{
	guchar *pixels;
	gushort *in;
	gint w, h;
	extern GdkPixmap *blitter;
	GdkGC *gc = rs->preview_drawingarea->style->fg_gc[GTK_WIDGET_STATE (rs->preview_drawingarea)];

	if (unlikely(!rs->in_use)) return;

	_CLAMP(region->x2, rs->photo->scaled->w-1);
	_CLAMP(region->y2, rs->photo->scaled->h-1);
	w = region->x2-region->x1+1;
	h = region->y2-region->y1+1;

	/* evil hack to fix crash after zoom */
	if (unlikely(region->y2 < region->y1)) /* FIXME: this is not good */
		return;
	if (unlikely(region->x2 < region->x1))
		return;

	pixels = rs->photo->preview->pixels+(region->y1*rs->photo->preview->rowstride
		+ region->x1*rs->photo->preview->pixelsize);
	in = rs->photo->scaled->pixels+(region->y1*rs->photo->scaled->rowstride
		+ region->x1*rs->photo->scaled->pixelsize);

	if (likely((!rs->preview_done) || force_render))
	{
		if (unlikely(rs->show_exposure_overlay))
		{
			guchar *mask = rs->photo->mask->pixels+(region->y1*rs->photo->mask->rowstride
				+region->x1*rs->photo->mask->pixelsize);
			rs_render_overlay(rs->photo, w, h, in, rs->photo->scaled->rowstride,
				pixels, rs->photo->preview->rowstride,
				mask, rs->photo->mask->rowstride, rs->cms);
		}
		else
			rs_render(rs->photo, w, h, in, rs->photo->scaled->rowstride,
				pixels, rs->photo->preview->rowstride,
				rs_cms_get_transform(rs->cms, TRANSFORM_DISPLAY));

		if (unlikely(rs->mark_roi))
		{
			guchar *buffer;
			gint srcoffset, dstoffset;
			gint row=h-1, col;

			buffer = g_malloc(h * w * rs->photo->preview->pixelsize);
			while(row--) /* render pixels outside ROI */
			{
				col = w * rs->photo->preview->pixelsize;
				dstoffset = col * row;
				srcoffset = rs->photo->preview->rowstride * (region->y1+row);
				while(col--)
				{
					buffer[dstoffset] = ((rs->photo->preview->pixels[srcoffset]+63)*3)>>3;
					srcoffset++;
					dstoffset++;
				}
			}
			gdk_draw_rgb_image(rs->preview_backing_notroi, gc, /* not ROI */
				region->x1, region->y1, w, h,
				GDK_RGB_DITHER_NONE, buffer, w * rs->photo->preview->pixelsize);
			g_free(buffer);
		}
	}

	if (unlikely(rs->mark_roi))
	{
		static PangoLayout *text_layout = NULL;
		gint text_width;
		gint text_height;
		static GString *text = NULL;
		gint x1, x2, y1, y2;
		extern GdkGC *dashed;
		extern GdkGC *grid;
		x1 = rs->roi_scaled.x1;
		y1 = rs->roi_scaled.y1;
		x2 = rs->roi_scaled.x2;
		y2 = rs->roi_scaled.y2;

		if (unlikely(!text))
			text = g_string_new("");

		if (unlikely(!text_layout))
			text_layout = gtk_widget_create_pango_layout(rs->preview_drawingarea, "");

		g_string_printf(text, "%d x %d", rs->roi.x2-rs->roi.x1+1, rs->roi.y2-rs->roi.y1+1);
		pango_layout_set_text(text_layout, text->str, -1);
		pango_layout_get_pixel_size(text_layout, &text_width, &text_height);

		pixels = rs->photo->preview->pixels+(rs->roi_scaled.y1*rs->photo->preview->rowstride
			+ rs->roi_scaled.x1*rs->photo->preview->pixelsize);
		/* draw all our stuff to blit-buffer */
		gdk_draw_drawable(blitter, gc, /* not ROI */
			rs->preview_backing_notroi,
			region->x1, region->y1,
			region->x1, region->y1,
			region->x2-region->x1+1,
			region->y2-region->y1+1+text_height+2);
		if (likely(((rs->roi.x2-rs->roi.x1)*(rs->roi.y2-rs->roi.y1)) > 0))
		{
			gdk_draw_rgb_image(blitter, gc, /* ROI */
				rs->roi_scaled.x1, rs->roi_scaled.y1,
				rs->roi_scaled.x2-rs->roi_scaled.x1,
				rs->roi_scaled.y2-rs->roi_scaled.y1,
				GDK_RGB_DITHER_NONE, pixels, rs->photo->preview->rowstride);
			gdk_draw_rectangle(blitter, dashed, FALSE,
				rs->roi_scaled.x1, rs->roi_scaled.y1,
				rs->roi_scaled.x2-rs->roi_scaled.x1-1,
				rs->roi_scaled.y2-rs->roi_scaled.y1-1);
			
			if (rs->photo->scaled->h-text_height-4 > y2)
			{
				gdk_draw_layout(blitter, dashed,
					rs->roi_scaled.x1+(rs->roi_scaled.x2-rs->roi_scaled.x1-text_width)/2,
					rs->roi_scaled.y2+2,
					text_layout);
			}
			else
			{
				gdk_draw_layout(blitter, dashed,
					rs->roi_scaled.x1+(rs->roi_scaled.x2-rs->roi_scaled.x1-text_width)/2,
					rs->roi_scaled.y2-text_height-2,
					text_layout);
			}
		}

/*
		We should support all these:
		http://powerretouche.com/Divine_proportion_tutorial.htm
*/
		switch(rs->roi_grid)
		{
			case ROI_GRID_NONE:
				break;
			case ROI_GRID_GOLDEN:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				/* vertical */
				golden = ((x2-x1)/goldenratio);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, t, y1, t, y2);
				t = (x2-golden);
				gdk_draw_line(blitter, grid, t, y1, t, y2);

				/* horizontal */
				golden = ((y2-y1)/goldenratio);

				t = (y1+golden);
				gdk_draw_line(blitter, grid, x1, t, x2, t);
				t = (y2-golden);
				gdk_draw_line(blitter, grid, x1, t, x2, t);
				break;
			}

			case ROI_GRID_THIRDS:
			{
				gint t;

				/* vertical */
				t = ((x2-x1+1)/3*1+x1);
				gdk_draw_line(blitter, grid, t, y1, t, y2);
				t = ((x2-x1+1)/3*2+x1);
				gdk_draw_line(blitter, grid, t, y1, t, y2);

				/* horizontal */
				t = ((y2-y1+1)/3*1+y1);
				gdk_draw_line(blitter, grid, x1, t, x2, t);
				t = ((y2-y1+1)/3*2+y1);
				gdk_draw_line(blitter, grid, x1, t, x2, t);
				break;
			}

			case ROI_GRID_GOLDEN_TRIANGLES1:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x1, y1, x2, y2);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x1, y2, t, y1);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x2, y1, t, y2);
				break;
			}

			case ROI_GRID_GOLDEN_TRIANGLES2:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x2, y1, x1, y2);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x1, y1, t, y2);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x2, y2, t, y1);
				break;
			}

			case ROI_GRID_HARMONIOUS_TRIANGLES1:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x1, y1, x2, y2);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x1, y2, t, y1);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x2, y1, t, y2);
				break;
			}

			case ROI_GRID_HARMONIOUS_TRIANGLES2:
			{
				gdouble goldenratio = ((1+sqrt(5))/2);
				gint t, golden;

				golden = ((x2-x1)/goldenratio);

				gdk_draw_line(blitter, grid, x1, y2, x2, y1);

				t = (x1+golden);
				gdk_draw_line(blitter, grid, x1, y1, t, y2);

				t = (x2-golden);
				gdk_draw_line(blitter, grid, x2, y2, t, y1);
				break;
			}
		}
		/* blit to screen */
		gdk_draw_drawable(rs->preview_drawingarea->window, gc, blitter,
			region->x1, region->y1,
			region->x1, region->y1,
			w, h+text_height+2);
	}
	else
	{
		gdk_draw_rgb_image(rs->preview_drawingarea->window, gc,
			region->x1, region->y1, w, h, GDK_RGB_DITHER_NONE,
			pixels, rs->photo->preview->rowstride);
	}
	return;
}

inline void
rs_render_mask(guchar *pixels, guchar *mask, guint length)
{
	guchar *pixel = pixels;
	while(length--)
	{
		*mask = 0x0;
		if (pixel[R] == 255)
			*mask |= MASK_OVER;
		else if (pixel[G] == 255)
			*mask |= MASK_OVER;
		else if (pixel[B] == 255)
			*mask |= MASK_OVER;
		else if ((pixel[R] < 2 && pixel[G] < 2) && pixel[B] < 2)
			*mask |= MASK_UNDER;
		pixel+=3;
		mask++;
	}
	return;
}

static gboolean
rs_render_idle(RS_BLOB *rs)
{
	gint row;
	gushort *in;
	guchar *out, *mask;

	if (rs->in_use && (!rs->preview_done))
		for(row=rs->preview_idle_render_lastrow; row<rs->photo->scaled->h; row++)
		{
			in = rs->photo->scaled->pixels + row*rs->photo->scaled->rowstride;
			out = rs->photo->preview->pixels + row*rs->photo->preview->rowstride;

			if (unlikely(rs->show_exposure_overlay))
			{
				mask = rs->photo->mask->pixels + row*rs->photo->mask->rowstride;
				rs_render_overlay(rs->photo, rs->photo->scaled->w, 1, in, rs->photo->scaled->rowstride,
					out, rs->photo->preview->rowstride,
					mask, rs->photo->mask->rowstride, rs->cms);
			}
			else
				rs_render(rs->photo, rs->photo->scaled->w, 1, in, rs->photo->scaled->rowstride,
					out, rs->photo->preview->rowstride, rs_cms_get_transform(rs->cms, TRANSFORM_DISPLAY));
	
			gdk_draw_rgb_image(rs->preview_backing,
				rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL], 0, row,
				rs->photo->scaled->w, 1, GDK_RGB_DITHER_NONE, out,
				rs->photo->preview->rowstride);
			if (rs->mark_roi)
			{
				gint size = rs->photo->preview->rowstride;
				guchar *buffer;
		
				buffer = g_malloc(size);
				while(size--) /* render backing store for pixels outside ROI */
					buffer[size] = ((out[size]+63)*3)>>3;
				gdk_draw_rgb_image(rs->preview_backing_notroi,
					rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL], 0, row,
					rs->photo->scaled->w, 1, GDK_RGB_DITHER_NONE, buffer,
					rs->photo->preview->rowstride);
				g_free(buffer);
			}
			rs->preview_idle_render_lastrow=row+1;
			if (gtk_events_pending()) return(TRUE);
		}
	rs->preview_idle_render_lastrow = 0;
	rs->preview_done = TRUE;
	rs->preview_idle_render = FALSE;
	gui_set_busy(FALSE);
	return(FALSE);
}

static void
rs_render_overlay(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, guchar *out, gint out_rowstride,
	guchar *mask, gint mask_rowstride, RS_CMS *cms)
{
	gint y,x;
	gint maskoffset, destoffset;
	rs_render(photo, width, height, in, in_rowstride, out, out_rowstride, rs_cms_get_transform(cms, TRANSFORM_DISPLAY));
	for(y=0 ; y<height ; y++)
	{
		destoffset = y * out_rowstride;
		maskoffset = y * mask_rowstride;
		rs_render_mask(out+destoffset, mask+maskoffset, width);
		for(x=0 ; x<width ; x++)
		{
			if (mask[maskoffset] & MASK_OVER)
			{
				out[destoffset+R] = 255;
				out[destoffset+B] = 0;
				out[destoffset+G] = 0;
			}
			if (mask[maskoffset] & MASK_UNDER)
			{
				out[destoffset+R] = 0;
				out[destoffset+B] = 255;
				out[destoffset+G] = 0;
			}
			maskoffset++;
			destoffset+=3;
		}
	}
	return;
}

void
rs_reset(RS_BLOB *rs)
{
	gboolean in_use = rs->in_use;
	gint c;
	rs->in_use = FALSE;
	rs->preview_scale = 0;
	for(c=0;c<3;c++)
		rs_settings_reset(rs->settings[c], MASK_ALL);
	rs->in_use = in_use;
	update_preview(rs, TRUE, FALSE);
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
	return(rss);
}

static RS_SETTINGS_DOUBLE
*rs_settings_double_new(void)
{
	RS_SETTINGS_DOUBLE *rssd;
	rssd = g_malloc(sizeof(RS_SETTINGS_DOUBLE));
	rssd->exposure = 0.0;
	rssd->saturation = 1.0;
	rssd->hue = 0.0;
	rssd->contrast = 1.0;
	rssd->warmth = 0.0;
	rssd->tint = 0.0;
	return rssd;
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
	metadata->cam_mul[0] = -1.0;
	metadata->contrast = -1.0;
	metadata->saturation = -1.0;
	metadata->sharpness = -1.0;
	metadata->color_tone = -1.0;
	metadata->focallength = -1;
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
	photo->active = FALSE;
	if (!photo) return(NULL);
	photo->input = NULL;
	photo->scaled = NULL;
	photo->preview = NULL;
	photo->mask = NULL;
	ORIENTATION_RESET(photo->orientation);
	photo->current_setting = 0;
	photo->priority = PRIO_U;
	photo->metadata = rs_metadata_new();
	for(c=0;c<3;c++)
		photo->settings[c] = rs_settings_double_new();
	photo->crop = NULL;
	photo->angle = 0.0;
	matrix3_identity(&photo->inverse_affine);
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

gboolean
rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype, gint width, gint height, gdouble scale, RS_CMS *cms)
{
	GdkPixbuf *pixbuf;
	RS_IMAGE16 *rsi;
	RS_IMAGE8 *image8;
	RS_IMAGE16 *image16;
	gint quality = 100;
	gboolean uncompressed_tiff = FALSE;

	/* transform and crop */
	rsi = rs_image16_transform(photo->input, NULL,
			NULL, NULL, photo->crop, width, height, FALSE, scale,
			photo->angle, photo->orientation);

	/* actually save */
	switch (filetype)
	{
		case FILETYPE_JPEG:
			image8 = rs_image8_new(rsi->w, rsi->h, 3, 3);
			rs_render(photo, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, image8->pixels, image8->rowstride,
				rs_cms_get_transform(cms, TRANSFORM_EXPORT));

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
			rs_render(photo, rsi->w, rsi->h, rsi->pixels, rsi->rowstride,
				gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf),
				rs_cms_get_transform(cms, TRANSFORM_EXPORT));
			gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
			g_object_unref(pixbuf);
			break;
		case FILETYPE_TIFF8:
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			image8 = rs_image8_new(rsi->w, rsi->h, 3, 3);
			rs_render(photo, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, image8->pixels, image8->rowstride,
				rs_cms_get_transform(cms, TRANSFORM_EXPORT));
			rs_tiff8_save(image8, filename, rs_cms_get_profile_filename(cms, PROFILE_EXPORT), uncompressed_tiff);
			rs_image8_free(image8);
			break;
		case FILETYPE_TIFF16:
			rs_conf_get_boolean(CONF_EXPORT_TIFF_UNCOMPRESSED, &uncompressed_tiff);
			image16 = rs_image16_new(rsi->w, rsi->h, 3, 3);
			rs_render16(photo, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, image16->pixels, image16->rowstride,
				rs_cms_get_transform(cms, TRANSFORM_EXPORT16));
			rs_tiff16_save(image16, filename, rs_cms_get_profile_filename(cms, PROFILE_EXPORT), uncompressed_tiff);
			rs_image16_free(image16);
			break;
	}
	if (photo->orientation)
		rs_image16_free(rsi);
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
	if (photo->scaled)
	{
		rs_image16_free(photo->scaled);
		photo->scaled = NULL;
	}
	if (photo->preview)
	{
		rs_image8_free(photo->preview);
		photo->preview = NULL;
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
	rs->scale = gtk_adjustment_new(0.5, 0.1, 1.0, 0.01, 0.1, 0.0);
	g_signal_connect(G_OBJECT(rs->scale), "value_changed",
		G_CALLBACK(update_scale_callback), rs);
	rs->histogram_dataset = NULL;
	rs->preview_exposed = (RS_RECT *) g_malloc(sizeof(RS_RECT));
	rs->preview_backing = NULL;
	rs->preview_backing_notroi = NULL;
	rs->preview_done = FALSE;
	rs->preview_idle_render = FALSE;
	rs->settings_buffer = NULL;
	rs->in_use = FALSE;
	rs->mark_roi = FALSE;
	rs->roi_grid = ROI_GRID_NONE;
	rs->show_exposure_overlay = FALSE;
	rs->photo = NULL;
	rs->queue = rs_batch_new_queue();
	rs->zoom_to_fit = TRUE;
	rs->current_setting = 0;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();
	return(rs);
}

void
rs_zoom_to_fit(RS_BLOB *rs)
{
	rs->zoom_to_fit = TRUE;
	update_preview(rs, FALSE, TRUE);
}

void
rs_photo_close(RS_PHOTO *photo)
{
	if (!photo) return;
	rs_cache_save(photo);
	photo->active = FALSE;
	return;
}

static RS_PHOTO *
rs_photo_open_dcraw(const gchar *filename)
{
	dcraw_data *raw;
	RS_PHOTO *photo=NULL;

	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);
		photo = rs_photo_new(NULL);
		photo->input = rs_image16_new(raw->raw.width, raw->raw.height, 4, 4);

		rs_photo_open_dcraw_apply_black_and_shift(raw, photo);

		photo->filename = g_strdup(filename);
		dcraw_close(raw);
		photo->active = TRUE;
	}
	g_free(raw);
	return(photo);
}

/* Function pointer. Initiliazed by arch binder */
void
(*rs_photo_open_dcraw_apply_black_and_shift)(dcraw_data *raw, RS_PHOTO *photo);

void
rs_photo_open_dcraw_apply_black_and_shift_c(dcraw_data *raw, RS_PHOTO *photo)
{
	guint srcoffset;
	guint destoffset;
	guint x;
	guint y;
	gushort *src = (gushort*)raw->raw.image;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	for (y=0; y<raw->raw.height; y++)
	{
		destoffset = y*photo->input->rowstride;
		srcoffset = y * raw->raw.width * 4;
		for (x=0; x<raw->raw.width; x++)
		{
			register gint r, g, b, g2;
			r  = src[srcoffset++] - raw->black;
			g  = src[srcoffset++] - raw->black;
			b  = src[srcoffset++] - raw->black;
			g2 = src[srcoffset++] - raw->black;
			r  = MAX(0, r);
			g  = MAX(0, g);
			b  = MAX(0, b);
			g2 = MAX(0, g2);
			photo->input->pixels[destoffset++] = (gushort)( r<<shift);
			photo->input->pixels[destoffset++] = (gushort)( g<<shift);
			photo->input->pixels[destoffset++] = (gushort)( b<<shift);
			photo->input->pixels[destoffset++] = (gushort)(g2<<shift);
		}
	}
}

#if defined (__i386__) || defined (__x86_64__)
void
rs_photo_open_dcraw_apply_black_and_shift_mmx(dcraw_data *raw, RS_PHOTO *photo)
{
	char b[8];
	gushort *sub = (gushort *) b;
	void *srcoffset;
	void *destoffset;
	guint x;
	guint y;
	gushort *src = (gushort*)raw->raw.image;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	sub[0] = raw->black;
	sub[1] = raw->black;
	sub[2] = raw->black;
	sub[3] = raw->black;

	for (y=0; y<raw->raw.height; y++)
	{
		destoffset = (void*) (photo->input->pixels + y*photo->input->rowstride);
		srcoffset = (void*) (src + y * photo->input->w * photo->input->pixelsize);
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
			"movq %%mm0, (%0)\n\t" /* write destination */
			"movq %%mm1, 8(%0)\n\t"
			"movq %%mm2, 16(%0)\n\t"
			"movq %%mm3, 24(%0)\n\t"
			"sub $4, %%"REG_a"\n\t"
			"add $32, %0\n\t"
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
rs_photo_open_gdk(const gchar *filename)
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
rs_set_wb_from_pixels(RS_BLOB *rs, gint x, gint y)
{
	gint offset, row, col;
	gdouble r=0.0, g=0.0, b=0.0;

	for(row=0; row<3; row++)
	{
		for(col=0; col<3; col++)
		{
			offset = (y+row-1)*rs->photo->scaled->rowstride
				+ (x+col-1)*rs->photo->scaled->pixelsize;
			r += ((gdouble) rs->photo->scaled->pixels[offset+R])/65535.0;
			g += ((gdouble) rs->photo->scaled->pixels[offset+G])/65535.0;
			b += ((gdouble) rs->photo->scaled->pixels[offset+B])/65535.0;
			if (rs->photo->scaled->channels==4)
				g += ((gdouble) rs->photo->scaled->pixels[offset+G2])/65535.0;
				
		}
	}
	r /= 9;
	g /= 9;
	b /= 9;
	if (rs->photo->scaled->channels==4)
		g /= 2;
	rs_set_wb_from_color(rs, r, g, b);
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
rs_render_pixel_to_srgb(RS_BLOB *rs, gint x, gint y, guchar *dest)
{
	gushort *pixel;
	if (x>(rs->photo->scaled->w-1))
		x = rs->photo->scaled->w-1;
	if (y>(rs->photo->scaled->h-1))
		y = rs->photo->scaled->h-1;
	pixel = &rs->photo->scaled->pixels[y*rs->photo->scaled->rowstride
		+ x*rs->photo->scaled->pixelsize];
	rs_render_pixel(rs->photo, pixel, dest, rs_cms_get_transform(rs->cms, TRANSFORM_SRGB));
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
rs_roi_orientation(RS_BLOB *rs)
{
	gint x1,x2,y1,y2;
	const gint rot = (rs->photo->orientation&3)%4;
	x1 = rs->roi.x1;
	y1 = rs->roi.y1;
	x2 = rs->roi.x2;
	y2 = rs->roi.y2;

	/* rotate and flip */
	switch(rot)
	{
		case 1:
			x1 = rs->roi.y1;
			y1 = rs->photo->input->h - rs->roi.x1;
			x2 = rs->roi.y2;
			y2 = rs->photo->input->h - rs->roi.x2;
			break;
		case 2:
			x1 = rs->photo->input->w - rs->roi.x2;
			y1 = rs->photo->input->h - rs->roi.y2;
			x2 = rs->photo->input->w - rs->roi.x1;
			y2 = rs->photo->input->h - rs->roi.y1;
			break;
		case 3:
			x1 = rs->photo->input->w - rs->roi.y1;
			y1 = rs->roi.x1;
			x2 = rs->photo->input->w - rs->roi.y2;
			y2 = rs->roi.x2;
			break;
	}

	if (rs->photo->orientation&4)
	{
		y1 = rs->photo->input->h-y1;
		y2 = rs->photo->input->h-y2;
	}

	rs->roi.x1 = x1;
	rs->roi.y1 = y1;
	rs->roi.x2 = x2;
	rs->roi.y2 = y2;

	/* normalize */
	if (rs->roi.x1 > rs->roi.x2)
		SWAP(rs->roi.x1, rs->roi.x2);
	if (rs->roi.y1 > rs->roi.y2)
		SWAP(rs->roi.y1, rs->roi.y2);
	return;
}

static gboolean
rs_mark_roi_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs)
{
	if (rs->preview_backing_notroi)
		g_object_unref(rs->preview_backing_notroi);
	rs->preview_backing_notroi = gdk_pixmap_new(rs->preview_drawingarea->window,
		rs->preview_drawingarea->allocation.width,
		rs->preview_drawingarea->allocation.height, -1);
	return(FALSE);
}

void
rs_mark_roi(RS_BLOB *rs, gboolean mark)
{
	static gint configure;

	if (mark && (!rs->mark_roi))
	{
		if (rs->preview_backing_notroi)
			g_object_unref(rs->preview_backing_notroi);
		rs->preview_backing_notroi = gdk_pixmap_new(rs->preview_drawingarea->window,
			rs->preview_drawingarea->allocation.width,
			rs->preview_drawingarea->allocation.height, -1);
		rs->mark_roi = TRUE;
		configure = g_signal_connect (GTK_OBJECT (rs->preview_drawingarea), "configure-event",
			GTK_SIGNAL_FUNC (rs_mark_roi_configure), rs);
	}
	else if (rs->mark_roi)
	{
		g_signal_handler_disconnect(rs->preview_drawingarea, configure);
		if (rs->preview_backing_notroi)
			g_object_unref(rs->preview_backing_notroi);
		rs->preview_backing_notroi = NULL;
		rs->mark_roi = FALSE;
	}
}

int
main(int argc, char **argv)
{
	RS_BLOB *rs;

	/* Bind default C functions */
	rs_bind_default_functions();

	/* Bind optimized functions if any */
	rs_bind_optimized_functions();

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
	rs_init_filetypes();
	gtk_init(&argc, &argv);
	rs = rs_new();
	rs->queue->cms = rs->cms = rs_cms_init();
	gui_init(argc, argv, rs);
	return(0);
}

gboolean
rs_shutdown(GtkWidget *dummy1, GdkEvent *dummy2, RS_BLOB *rs)
{
	rs_photo_close(rs->photo);
	gtk_main_quit();
	return(TRUE);
}

/* Inlclude our own g_mkdir_with_parents() in case of old glib.
Copied almost verbatim from glib-2.10.0/glib/gfileutils.c */

#if !GLIB_CHECK_VERSION(2,8,0)
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
#endif

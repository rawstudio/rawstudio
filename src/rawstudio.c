/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include <time.h>
#include <config.h>
#include <lcms.h>
#include "dcraw_api.h"
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "gtk-helper.h"
#include "rs-cache.h"
#include "color.h"
#include "rawfile.h"
#include "tiff-meta.h"
#include "ciff-meta.h"
#include "mrw-meta.h"
#include "rs-image.h"
#include "gettext.h"
#include "conf_interface.h"
#include "filename.h"
#include "rs-jpeg.h"
#include "rs-render.h"

#define cpuid(n) \
  a = b = c = d = 0x0; \
  asm( \
  	"cpuid" \
  	: "=eax" (a), "=ebx" (b), "=ecx" (c), "=edx" (d) : "0" (n) \
	)

guint cpuflags = 0;
gushort loadtable[65536];

static cmsHPROFILE genericLoadProfile = NULL;
static cmsHPROFILE genericRGBProfile = NULL;
static cmsHPROFILE workProfile = NULL;

static cmsHTRANSFORM loadTransform = NULL;
static cmsHTRANSFORM displayTransform = NULL;
static cmsHTRANSFORM exportTransform = NULL;

inline void rs_photo_prepare(RS_PHOTO *photo);
void update_scaled(RS_BLOB *rs);
inline void rs_render_mask(guchar *pixels, guchar *mask, guint length);
gboolean rs_render_idle(RS_BLOB *rs);
void rs_render_overlay(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride,
	guchar *mask, gint mask_rowstride);
RS_SETTINGS *rs_settings_new();
void rs_settings_free(RS_SETTINGS *rss);
RS_SETTINGS_DOUBLE *rs_settings_double_new();
void rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd);
RS_PHOTO *rs_photo_open_dcraw(const gchar *filename);
RS_PHOTO *rs_photo_open_gdk(const gchar *filename);
GdkPixbuf *rs_thumb_grt(const gchar *src);
GdkPixbuf *rs_thumb_gdk(const gchar *src);
static guchar *mycms_pack_rgb_b(void *info, register WORD wOut[], register LPBYTE output);
static guchar *mycms_unroll_rgb_w(void *info, register WORD wIn[], register LPBYTE accum);
static guchar *mycms_unroll_rgb_w_loadtable(void *info, register WORD wIn[], register LPBYTE accum);
static guchar *mycms_unroll_rgb4_w(void *info, register WORD wIn[], register LPBYTE accum);
static guchar *mycms_unroll_rgb4_w_loadtable(void *info, register WORD wIn[], register LPBYTE accum);
static guchar *mycms_pack_rgb4_w(void *info, register WORD wOut[], register LPBYTE output);
static gboolean dotdir_is_local = FALSE;
static gboolean load_gdk = FALSE;

void
rs_local_cachedir(gboolean new_value)
{
	dotdir_is_local = new_value;
	return;
}

void
rs_load_gdk(gboolean new_value)
{
	load_gdk = new_value;
	return;
}

static RS_FILETYPE filetypes[] = {
	{".cr2", FILETYPE_RAW, rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta},
	{".crw", FILETYPE_RAW, rs_photo_open_dcraw, rs_ciff_load_thumb, rs_ciff_load_meta},
	{".nef", FILETYPE_RAW, rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta},
	{".mrw", FILETYPE_RAW, rs_photo_open_dcraw, rs_mrw_load_thumb, rs_mrw_load_meta},
	{".tif", FILETYPE_RAW, rs_photo_open_dcraw, rs_tiff_load_thumb, rs_tiff_load_meta},
	{".orf", FILETYPE_RAW, rs_photo_open_dcraw, rs_tiff_load_thumb, NULL},
	{".raw", FILETYPE_RAW, rs_photo_open_dcraw, NULL, NULL},
	{".jpg", FILETYPE_GDK, rs_photo_open_gdk, rs_thumb_gdk, NULL},
	{".png", FILETYPE_GDK, rs_photo_open_gdk, rs_thumb_gdk, NULL},
	{NULL, 0, NULL}
};

void
make_gammatable16(gushort *table, gdouble gamma)
{
	gint n;
	const gdouble gammavalue = (1.0/gamma);
	gdouble nd;
	gint res;

	for (n=0;n<0x10000;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		nd = pow(nd, gammavalue);
		res = (gint) (nd*65535.0);
		_CLAMP65535(res);
		table[n] = res;
	}
	return;
}

void
update_scaled(RS_BLOB *rs)
{
	/* scale if needed */
	if (rs->preview_scale != GETVAL(rs->scale))
	{
		rs->preview_scale = GETVAL(rs->scale);
		if (rs->photo->scaled)
			rs_image16_free(rs->photo->scaled);
		rs->photo->scaled = rs_image16_scale(rs->photo->input, NULL, rs->preview_scale);
		rs->preview_done = TRUE; /* stop rs_render_idle() */
	}

	/* transform if needed */
	if (rs->photo->orientation != rs->photo->scaled->orientation)
		rs_image16_orientation(rs->photo->scaled, rs->photo->orientation);

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
update_preview(RS_BLOB *rs, gboolean update_table)
{
	if(unlikely(!rs->photo)) return;

	if (update_table)
		rs_render_previewtable(rs->photo->settings[rs->photo->current_setting]->contrast);
	update_scaled(rs);
	rs_photo_prepare(rs->photo);
	update_preview_region(rs, rs->preview_exposed);

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
update_preview_region(RS_BLOB *rs, RS_RECT *region)
{
	guchar *pixels;
	gushort *in;
	gint x1 = region->x1;
	gint y1 = region->y1;
	gint x2 = region->x2;
	gint y2 = region->y2;

	if (unlikely(!rs->in_use)) return;

	_CLAMP(x2, rs->photo->scaled->w);
	_CLAMP(y2, rs->photo->scaled->h);

	/* evil hack to fix crash after zoom */
	if (unlikely(y2<y1)) /* FIXME: this is not good */
		return;

	pixels = rs->photo->preview->pixels+(y1*rs->photo->preview->rowstride+x1*rs->photo->preview->pixelsize);
	in = rs->photo->scaled->pixels+(y1*rs->photo->scaled->rowstride+x1*rs->photo->scaled->pixelsize);
	if (unlikely(rs->show_exposure_overlay))
	{
		guchar *mask = rs->photo->mask->pixels+(y1*rs->photo->mask->rowstride+x1*rs->photo->mask->pixelsize);
		rs_render_overlay(rs->photo, x2-x1, y2-y1, in, rs->photo->scaled->rowstride,
			rs->photo->scaled->pixelsize, pixels, rs->photo->preview->rowstride,
			mask, rs->photo->mask->rowstride);
	}
	else
		rs_render(rs->photo, x2-x1, y2-y1, in, rs->photo->scaled->rowstride,
			rs->photo->scaled->pixelsize, pixels, rs->photo->preview->rowstride, displayTransform);
	gdk_draw_rgb_image(rs->preview_drawingarea->window,
		rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL],
		x1, y1, x2-x1, y2-y1,
		GDK_RGB_DITHER_NONE, pixels, rs->photo->preview->rowstride);
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

gboolean
rs_run_batch_idle(RS_QUEUE *queue)
{
	RS_QUEUE_ELEMENT *e;
	RS_PHOTO *photo=NULL;
	RS_FILETYPE *filetype;
	gchar *parsed_filename = NULL;
	GString *savefile = NULL;
	GString *savedir = NULL;

	while((e = batch_get_next_in_queue(queue)))
	{
		if ((filetype = rs_filetype_get(e->path_file, TRUE)))
		{
			photo = filetype->load(e->path_file);
			if (photo)
			{
				if (queue->directory[0] == '/')
					savedir = g_string_new(queue->directory);
				else
				{
					savedir = g_string_new(g_dirname(photo->filename));
					g_string_append(savedir, "/");
					g_string_append(savedir, queue->directory);
					if (savedir->str[((savedir->len)-1)] != '/')
						g_string_append(savedir, "/");
				}
				g_mkdir_with_parents(savedir->str, 00755);

				savefile = g_string_new(savedir->str);
				g_string_free(savedir, TRUE);
				
				g_string_append(savefile, queue->filename);
				g_string_append(savefile, ".");

				if (queue->filetype == FILETYPE_JPEG)
					g_string_append(savefile, "jpg");
				else if (queue->filetype == FILETYPE_PNG)
					g_string_append(savefile, "png");
				else
					g_string_append(savefile, "jpg");				

				parsed_filename = filename_parse(savefile->str, photo);
				g_string_free(savefile, TRUE);

				rs_cache_load(photo);
				rs_photo_prepare(photo);
				rs_photo_save(photo, parsed_filename, queue->filetype, NULL); /* FIXME: profile */
				g_free(parsed_filename);
				rs_photo_close(photo);
				rs_photo_free(photo);
			}
		}

		batch_remove_element_from_queue(queue, e);
		if (gtk_events_pending()) return(TRUE);
		/* FIXME: It leaves this function and never comes back (hangs rawstudio)*/
	}
	return(FALSE);
}

gboolean
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
					rs->photo->scaled->pixelsize, out, rs->photo->preview->rowstride,
					mask, rs->photo->mask->rowstride);
			}
			else
				rs_render(rs->photo, rs->photo->scaled->w, 1, in, rs->photo->scaled->rowstride,
					rs->photo->scaled->pixelsize, out, rs->photo->preview->rowstride, displayTransform);
	
			gdk_draw_rgb_image(rs->preview_backing,
				rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL], 0, row,
				rs->photo->scaled->w, 1, GDK_RGB_DITHER_NONE, out,
				rs->photo->preview->rowstride);
			rs->preview_idle_render_lastrow=row+1;
			if (gtk_events_pending()) return(TRUE);
		}
	rs->preview_idle_render_lastrow = 0;
	rs->preview_done = TRUE;
	rs->preview_idle_render = FALSE;
	gui_set_busy(FALSE);
	return(FALSE);
}

void
rs_render_overlay(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride,
	guchar *mask, gint mask_rowstride)
{
	gint y,x;
	gint maskoffset, destoffset;
	rs_render(photo, width, height, in, in_rowstride, in_channels, out, out_rowstride, displayTransform);
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
	rs->in_use = FALSE;
	rs->preview_scale = 0;
	gint c;
	for(c=0;c<3;c++)
		rs_settings_reset(rs->settings[c], MASK_ALL);
	rs->in_use = in_use;
	update_preview(rs, TRUE);
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

RS_SETTINGS *
rs_settings_new()
{
	RS_SETTINGS *rss;
	rss = g_malloc(sizeof(RS_SETTINGS));
	rss->exposure = gtk_adjustment_new(0.0, -3.0, 3.0, 0.1, 0.5, 0.0);
	rss->saturation = gtk_adjustment_new(1.0, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->hue = gtk_adjustment_new(0.0, 0.0, 360.0, 0.1, 30.0, 0.0);
	rss->contrast = gtk_adjustment_new(1.0, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->warmth = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	rss->tint = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	return(rss);
}

void
rs_settings_free(RS_SETTINGS *rss)
{
	if (rss!=NULL)
	{
		g_object_unref(rss->exposure);
		g_object_unref(rss->saturation);
		g_object_unref(rss->hue);
		g_object_unref(rss->contrast);
		g_object_unref(rss->warmth);
		g_object_unref(rss->tint);
		g_free(rss);
	}
	return;
}

RS_SETTINGS_DOUBLE
*rs_settings_double_new()
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

void
rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd)
{
	g_free(rssd);
	return;
}

RS_METADATA *
rs_metadata_new()
{
	RS_METADATA *metadata;
	metadata = g_malloc(sizeof(RS_METADATA));
	metadata->make = MAKE_UNKNOWN;
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
	return(metadata);
}

void
rs_metadata_free(RS_METADATA *metadata)
{
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
	return(photo);
}

gboolean
rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype, const gchar *profile_filename)
{
	GdkPixbuf *pixbuf;
	RS_IMAGE16 *rsi;
	RS_IMAGE8 *image8;
	gint quality = 100;

	if (photo->orientation)
	{
		rsi = rs_image16_copy(photo->input);
		rs_image16_orientation(rsi, photo->orientation);
	}
	else
		rsi = photo->input;

	/* actually save */
	switch (filetype)
	{
		case FILETYPE_JPEG:
			image8 = rs_image8_new(rsi->w, rsi->h, 3, 3);
			rs_render(photo, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, rsi->channels,
				image8->pixels, image8->rowstride,
				exportTransform);

			rs_conf_get_integer(CONF_EXPORT_JPEG_QUALITY, &quality);
			if (quality > 100)
				quality = 100;
			else if (quality < 0)
				quality = 0;

			rs_jpeg_save(image8, filename, quality, profile_filename);
			rs_image8_free(image8);
			break;
		case FILETYPE_PNG:
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);
			rs_render(photo, rsi->w, rsi->h, rsi->pixels,
				rsi->rowstride, rsi->channels,
				gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf),
				exportTransform);
			gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);
			g_object_unref(pixbuf);
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
	g_free(photo);
	return;
}

RS_BLOB *
rs_new()
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
	rs->preview_done = FALSE;
	rs->preview_idle_render = FALSE;
	rs->settings_buffer = NULL;
	rs->in_use = FALSE;
	rs->show_exposure_overlay = FALSE;
	rs->photo = NULL;
	rs->queue = batch_new_queue();
	rs->zoom_to_fit = TRUE;
	rs->loadProfile = NULL;
	rs->displayProfile = NULL;
	rs->exportProfile = NULL;
	rs->exportProfileFilename = NULL;
	rs->current_setting = 0;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();
	return(rs);
}

void
rs_zoom_to_fit(RS_BLOB *rs)
{
	gdouble scalex, scaley;

	if (rs->photo)
	{
		if (rs->photo->orientation & 1)
		{
			scalex = ((gdouble) rs->preview_width / (gdouble) rs->photo->input->h)*0.99;
			scaley = ((gdouble) rs->preview_height / (gdouble) rs->photo->input->w)*0.99;
		}
		else
		{
			scalex = ((gdouble) rs->preview_width / (gdouble) rs->photo->input->w)*0.99;
			scaley = ((gdouble) rs->preview_height / (gdouble) rs->photo->input->h)*0.99;
		}
		if (scalex < scaley)
			SETVAL(rs->scale, scalex);
		else
			SETVAL(rs->scale, scaley);
	}
	rs->zoom_to_fit = TRUE;
}

void
rs_photo_close(RS_PHOTO *photo)
{
	if (!photo) return;
	rs_cache_save(photo);
	photo->active = FALSE;
	return;
}

RS_PHOTO *
rs_photo_open_dcraw(const gchar *filename)
{
	dcraw_data *raw;
	RS_PHOTO *photo=NULL;
	gushort *src;
	guint x,y;
	guint srcoffset, destoffset;
	gint64 shift;
	gushort *buffer;

	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);
		photo = rs_photo_new();
		shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);
		photo->input = rs_image16_new(raw->raw.width, raw->raw.height, 3, 4);
		src  = (gushort *) raw->raw.image;
		buffer = g_malloc(raw->raw.width*4*sizeof(gushort));
#ifdef __i386__
		if (cpuflags & _MMX)
		{
			char b[8];
			gushort *sub = (gushort *) b;
			sub[0] = raw->black;
			sub[1] = raw->black;
			sub[2] = raw->black;
			sub[3] = raw->black;
			for (y=0; y<raw->raw.height; y++)
			{
				destoffset = (guint) buffer;//(photo->input->pixels + y*photo->input->rowstride);
				srcoffset = (guint) (src + y * photo->input->w * photo->input->pixelsize);
				x = raw->raw.width;
				asm volatile (
					"movl %3, %%eax\n\t" /* copy x to %eax */
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
					"sub $4, %%eax\n\t"
					"add $32, %0\n\t"
					"add $32, %1\n\t"
					"cmp $3, %%eax\n\t"
					"jg load_raw_inner_loop\n\t"
					"cmp $1, %%eax\n\t"
					"jb load_raw_inner_done\n\t"
					".p2align 4,,15\n"
					"load_raw_leftover:\n\t"
					"movq (%1), %%mm0\n\t" /* leftover pixels */
					"psubusw %%mm7, %%mm0\n\t"
					"psllw $4, %%mm0\n\t"
					"movq %%mm0, (%0)\n\t"
					"sub $1, %%eax\n\t"
					"cmp $0, %%eax\n\t"
					"jg load_raw_leftover\n\t"
					"load_raw_inner_done:\n\t"
					"emms\n\t" /* clean up */
					: "+r" (destoffset), "+r" (srcoffset)
					: "r" (sub), "r" (x), "r" (&shift)
					: "%eax"
				);
				cmsDoTransform(loadTransform, buffer,
					(photo->input->pixels + y*photo->input->rowstride),
					raw->raw.width);
			}
		}
		else
#endif
		{
			for (y=0; y<raw->raw.height; y++)
			{
				destoffset = 0;
				srcoffset = y * raw->raw.width * 4;
				for (x=0; x<raw->raw.width; x++)
				{
					register gint r,g,b;
					r = (src[srcoffset++] - raw->black)<<shift;
					g = (src[srcoffset++] - raw->black)<<shift;
					b = (src[srcoffset++] - raw->black)<<shift;
					g += ((src[srcoffset++] - raw->black)<<shift);
					g = g>>1;
					_CLAMP65535_TRIPLET(r, g, b);
					buffer[destoffset++] = r;
					buffer[destoffset++] = g;
					buffer[destoffset++] = b;
				}
				cmsDoTransform(loadTransform, buffer,
					photo->input->pixels+y*photo->input->rowstride,
					raw->raw.width);
			}
		}
		photo->filename = g_strdup(filename);
		dcraw_close(raw);
		photo->active = TRUE;
		g_free(buffer);
	}
	g_free(raw);
	return(photo);
}

RS_FILETYPE *
rs_filetype_get(const gchar *filename, gboolean load)
{
	RS_FILETYPE *filetype = NULL;
	gchar *iname;
	gint n;
	iname = g_ascii_strdown(filename,-1);
	n = 0;
	while(filetypes[n].ext)
	{
		if (g_str_has_suffix(iname, filetypes[n].ext))
		{
			if ((!load) || (filetypes[n].load))
			{
				if (filetypes[n].filetype == FILETYPE_RAW)
					filetype = &filetypes[n];
				else if ((filetypes[n].filetype == FILETYPE_GDK) && (load_gdk))
					filetype = &filetypes[n];
				break;
			}
		}
		n++;
	}
	g_free(iname);
	return(filetype);
}

RS_PHOTO *
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

	directory = g_path_get_dirname(filename);
	if (dotdir_is_local)
	{
		dotdir = g_string_new(g_get_home_dir());
		dotdir = g_string_append(dotdir, "/");
		dotdir = g_string_append(dotdir, DOTDIR);
		dotdir = g_string_append(dotdir, "/");
		dotdir = g_string_append(dotdir, directory);
	}
	else
	{
		dotdir = g_string_new(directory);
		dotdir = g_string_append(dotdir, "/");
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
		out = g_string_append(out, "/");
		out = g_string_append(out, filename);
		out = g_string_append(out, ".thumb.png");
		ret = out->str;
		g_string_free(out, FALSE);
		g_free(dotdir);
	}
	g_free(filename);
	return(ret);
}

GdkPixbuf *
rs_thumb_grt(const gchar *src)
{
	GdkPixbuf *pixbuf=NULL;
	gchar *in, *argv[6];
	gchar *tmp=NULL;
	gint tmpfd;
	gchar *thumbname;
	static gboolean grt_warning_shown = FALSE;

	thumbname = rs_thumb_get_name(src);

	if (thumbname)
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
			return(pixbuf);
		}

	if (!thumbname)
	{
		tmpfd = g_file_open_tmp("XXXXXX", &tmp, NULL);
		thumbname = tmp;
		close(tmpfd);
	}

	if (g_file_test("/usr/bin/gnome-raw-thumbnailer", G_FILE_TEST_IS_EXECUTABLE))
	{
		in = g_filename_to_uri(src, NULL, NULL);

		if (in)
		{
			argv[0] = "/usr/bin/gnome-raw-thumbnailer";
			argv[1] = "-s";
			argv[2] = "128";
			argv[3] = in;
			argv[4] = thumbname;
			argv[5] = NULL;
			g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(in);
		}
	}
	else if (!grt_warning_shown)
	{
		gui_dialog_simple("Warning", "gnome-raw-thumbnailer needed for RAW thumbnails.");
		grt_warning_shown = TRUE;
	}

	if (tmp)
		g_unlink(tmp);
	g_free(thumbname);
	return(pixbuf);
}

GdkPixbuf *
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
	SETVAL(rs->settings[rs->photo->current_setting]->warmth, warmth);
	rs->in_use = in_use;
	SETVAL(rs->settings[rs->photo->current_setting]->tint, tint);
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

gchar *
rs_get_profile(gint type)
{
	gchar *ret = NULL;
	gint selected = 0;
	if (type == RS_CMS_PROFILE_IN)
	{
		rs_conf_get_integer(CONF_CMS_IN_PROFILE_SELECTED, &selected);
		if (selected > 0)
			ret = rs_conf_get_nth_string_from_list_string(CONF_CMS_IN_PROFILE_LIST, --selected);
	}
	else if (type == RS_CMS_PROFILE_DISPLAY)
	{
		rs_conf_get_integer(CONF_CMS_DI_PROFILE_SELECTED, &selected);
		if (selected > 0)
			ret = rs_conf_get_nth_string_from_list_string(CONF_CMS_DI_PROFILE_LIST, --selected);
	} 
	else if (type == RS_CMS_PROFILE_EXPORT)
	{
		rs_conf_get_integer(CONF_CMS_EX_PROFILE_SELECTED, &selected);
		if (selected > 0)
			ret = rs_conf_get_nth_string_from_list_string(CONF_CMS_EX_PROFILE_LIST, --selected);
	}

	return ret;
}

gboolean
rs_cms_is_profile_valid(const gchar *path)
{
	gboolean ret = FALSE;
	cmsHPROFILE profile;

	if (path)
	{
		profile = cmsOpenProfileFromFile(path, "r");
		if (profile)
		{
			cmsCloseProfile(profile);
			ret = TRUE;
		}
	}
	return(ret);
}

gdouble
rs_cms_guess_gamma(void *transform)
{
	gushort buffer[36];
	gint n;
	gint lin = 0;
	gint g045 = 0;
	gdouble gamma = 1.0;

	gushort table_lin[] = {
		6553,   6553,  6553,
		13107, 13107, 13107,
		19661, 19661, 19661,
		26214, 26214, 26214,
		32768, 32768, 32768,
		39321, 39321, 39321,
		45875, 45875, 45875,
		52428, 52428, 52428,
		58981, 58981, 58981
	};
	const gushort table_g045[] = {
		  392,   392,   392,
		 1833,  1833,  1833,
		 4514,  4514,  4514,
		 8554,  8554,  8554,
		14045, 14045, 14045,
		21061, 21061, 21061,
		29665, 29665, 29665,
		39913, 39913, 39913,
		51855, 51855, 51855
	};
	cmsDoTransform(loadTransform, table_lin, buffer, 9);
	for (n=0;n<9;n++)
	{
		lin += abs(buffer[n*4]-table_lin[n*3]);
		lin += abs(buffer[n*4+1]-table_lin[n*3+1]);
		lin += abs(buffer[n*4+2]-table_lin[n*3+2]);
		g045 += abs(buffer[n*4]-table_g045[n*3]);
		g045 += abs(buffer[n*4+1]-table_g045[n*3+1]);
		g045 += abs(buffer[n*4+2]-table_g045[n*3+2]);
	}
	if (g045 < lin)
		gamma = 2.2;

	return(gamma);
}

void
rs_cms_prepare_transforms(RS_BLOB *rs)
{
	gdouble gamma;
	if (rs->cms_enabled)
	{
		
		if (rs->loadProfile)
		{
			if (loadTransform)
				cmsDeleteTransform(loadTransform);
			loadTransform = cmsCreateTransform(rs->loadProfile, TYPE_RGB_16,
				workProfile, TYPE_RGB_16, rs->cms_intent, 0);
		}
		else
			loadTransform = cmsCreateTransform(genericLoadProfile, TYPE_RGB_16,
				workProfile, TYPE_RGB_16, rs->cms_intent, 0);
	}
	else
	{
		if (loadTransform)
			cmsDeleteTransform(loadTransform);
		loadTransform = cmsCreateTransform(workProfile, TYPE_RGB_16,
				workProfile, TYPE_RGB_16, rs->cms_intent, 0);
	}

	cmsSetUserFormatters(loadTransform, TYPE_RGB_16, mycms_unroll_rgb_w, TYPE_RGB_16, mycms_pack_rgb4_w);
	gamma = rs_cms_guess_gamma(loadTransform);
	if (gamma != 1.0)
	{
		make_gammatable16(loadtable, gamma);
		if (cpuflags & _MMX)
			cmsSetUserFormatters(loadTransform, TYPE_RGB_16, mycms_unroll_rgb4_w_loadtable, TYPE_RGB_16, mycms_pack_rgb4_w);
		else
			cmsSetUserFormatters(loadTransform, TYPE_RGB_16, mycms_unroll_rgb_w_loadtable, TYPE_RGB_16, mycms_pack_rgb4_w);
	}
	else
		if (cpuflags & _MMX)
			cmsSetUserFormatters(loadTransform, TYPE_RGB_16, mycms_unroll_rgb4_w, TYPE_RGB_16, mycms_pack_rgb4_w);
		else
			cmsSetUserFormatters(loadTransform, TYPE_RGB_16, mycms_unroll_rgb_w, TYPE_RGB_16, mycms_pack_rgb4_w);

	if (rs->displayProfile)
	{
		if (displayTransform)
			cmsDeleteTransform(displayTransform);
		displayTransform = cmsCreateTransform(workProfile, TYPE_RGB_16,
			rs->displayProfile, TYPE_RGB_8, rs->cms_intent, 0);
	}
	else
	{
		if (displayTransform)
			cmsDeleteTransform(displayTransform);
		displayTransform = cmsCreateTransform(workProfile, TYPE_RGB_16,
			genericRGBProfile, TYPE_RGB_8, rs->cms_intent, 0);
	}
	cmsSetUserFormatters(displayTransform, TYPE_RGB_16, mycms_unroll_rgb_w, TYPE_RGB_8, mycms_pack_rgb_b);

	if (rs->exportProfile)
	{
		if (exportTransform)
			cmsDeleteTransform(exportTransform);
		exportTransform = cmsCreateTransform(workProfile, TYPE_RGB_16,
			rs->exportProfile, TYPE_RGB_8, rs->cms_intent, 0);
	}
	else
	{
		if (exportTransform)
			cmsDeleteTransform(exportTransform);
		exportTransform = cmsCreateTransform(workProfile, TYPE_RGB_16,
			genericRGBProfile, TYPE_RGB_8, rs->cms_intent, 0);
	}
	cmsSetUserFormatters(exportTransform, TYPE_RGB_16, mycms_unroll_rgb_w, TYPE_RGB_8, mycms_pack_rgb_b);

	rs_render_select(rs->cms_enabled);
	return;
}

void
rs_cms_init(RS_BLOB *rs)
{
	gchar *custom_cms_in_profile;
	gchar *custom_cms_display_profile;
	gchar *custom_cms_export_profile;
	cmsCIExyY D65;
	LPGAMMATABLE gamma[3];
	cmsCIExyYTRIPLE AdobeRGBPrimaries = {
		{0.6400, 0.3300, 0.297361},
		{0.2100, 0.7100, 0.627355},
		{0.1500, 0.0600, 0.075285}};
	cmsCIExyYTRIPLE genericLoadPrimaries = { /* sRGB primaries */
		{0.64, 0.33, 0.212656},
		{0.115, 0.826, 0.724938},
		{0.157, 0.018, 0.016875}};

	cmsErrorAction(LCMS_ERROR_IGNORE);
	cmsWhitePointFromTemp(6504, &D65);
	gamma[0] = gamma[1] = gamma[2] = cmsBuildGamma(2,1.0);

	/* set up builtin profiles */
	workProfile = cmsCreateRGBProfile(&D65, &AdobeRGBPrimaries, gamma);
	genericRGBProfile = cmsCreate_sRGBProfile();
	genericLoadProfile = cmsCreateRGBProfile(&D65, &genericLoadPrimaries, gamma);

	custom_cms_in_profile = rs_get_profile(RS_CMS_PROFILE_IN);
	if (custom_cms_in_profile)
	{
		rs->loadProfile = cmsOpenProfileFromFile(custom_cms_in_profile, "r");
		g_free(custom_cms_in_profile);
	}

	custom_cms_display_profile = rs_get_profile(RS_CMS_PROFILE_DISPLAY);
	if (custom_cms_display_profile)
	{
		rs->displayProfile = cmsOpenProfileFromFile(custom_cms_display_profile, "r");
		g_free(custom_cms_display_profile);
	}

	custom_cms_export_profile = rs_get_profile(RS_CMS_PROFILE_EXPORT);
	if (custom_cms_export_profile)
	{
		rs->exportProfile = cmsOpenProfileFromFile(custom_cms_export_profile, "r");
		if (rs->exportProfile)
			rs->exportProfileFilename = custom_cms_export_profile;
		else
			g_free(custom_cms_export_profile);
	}

	rs->cms_intent = INTENT_PERCEPTUAL; /* default intent */
	rs_conf_get_cms_intent(CONF_CMS_INTENT, &rs->cms_intent);

	rs->cms_enabled = FALSE;
	rs_conf_get_boolean(CONF_CMS_ENABLED, &rs->cms_enabled);
	rs_cms_prepare_transforms(rs);
	return;
}

static guchar *
mycms_pack_rgb_b(void *info, register WORD wOut[], register LPBYTE output)
{
	*output++ = RGB_16_TO_8(wOut[0]);
	*output++ = RGB_16_TO_8(wOut[1]);
	*output++ = RGB_16_TO_8(wOut[2]);
	return(output);
}

static guchar *
mycms_unroll_rgb_w(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = *(LPWORD) accum; accum+= 2;
	wIn[1] = *(LPWORD) accum; accum+= 2;
	wIn[2] = *(LPWORD) accum; accum+= 2;
	return(accum);
}

static guchar *
mycms_unroll_rgb_w_loadtable(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = loadtable[*(LPWORD) accum]; accum+= 2;
	wIn[1] = loadtable[*(LPWORD) accum]/2; accum+= 2;
	wIn[2] = loadtable[*(LPWORD) accum]; accum+= 2;
	return(accum);
}

static guchar *
mycms_unroll_rgb4_w(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = *(LPWORD) accum; accum+= 2;
	wIn[1] = *(LPWORD) accum; accum+= 2;
	wIn[2] = *(LPWORD) accum; accum+= 4;
	return(accum);
}

static guchar *
mycms_unroll_rgb4_w_loadtable(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = loadtable[*(LPWORD) accum]; accum+= 2;
	wIn[1] = loadtable[*(LPWORD) accum]; accum+= 2;
	wIn[2] = loadtable[*(LPWORD) accum]; accum+= 4;
	return(accum);
}

static guchar *
mycms_pack_rgb4_w(void *info, register WORD wOut[], register LPBYTE output)
{
#ifdef __i386__
	asm volatile (
		"movl (%1), %%ebx\n\t" /* read R G */
		"movw 4(%1), %%cx\n\t" /* read B */
		"movl %%ebx, (%0)\n\t" /* write R G */
		"andl $0x0000FFFF, %%ecx\n\t" /* clean B (Is this necessary?) */
		"andl $0xFFFF0000, %%ebx\n\t" /* G, remove R */
		"orl %%ebx, %%ecx\n\t" /* B G */
		"movl %%ecx, 4(%0)\n\t" /* write B G */
		"addl $8, %0\n\t" /* output+=8 */
	: "+r" (output)
	: "r" (wOut)
	: "%ebx", "%ecx"
	);
#else
	*(LPWORD) output = wOut[0]; output+= 2;
	*(LPWORD) output = wOut[1]; output+= 2;
	*(LPWORD) output = wOut[2]; output+= 2;
	*(LPWORD) output = wOut[1]; output+= 2;
#endif
	return(output);
}

int
main(int argc, char **argv)
{
#ifdef __i386__
	guint a,b,c,d;
	asm(
		"pushfl\n\t"
		"popl %%eax\n\t"
		"movl %%eax, %%ebx\n\t"
		"xorl $0x00200000, %%eax\n\t"
		"pushl %%eax\n\t"
		"popfl\n\t"
		"pushfl\n\t"
		"popl %%eax\n\t"
		"cmpl %%eax, %%ebx\n\t"
		"je notfound\n\t"
		"movl $1, %0\n\t"
		"notfound:\n\t"
		: "=r" (a)
		:
		: "eax", "ebx"
		);
	if (a)
	{
		cpuid(0x1);
		if(d&0x00800000) cpuflags |= _MMX;
		if(d&0x2000000) cpuflags |= _SSE;
		if(d&0x8000) cpuflags |= _CMOV;
		cpuid(0x80000001);
		if(d&0x80000000) cpuflags |= _3DNOW;
	}
#endif
#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
	RS_BLOB *rs;

	gtk_init(&argc, &argv);
	rs = rs_new();
	rs_cms_init(rs);
	rs_conf_get_boolean(CONF_CACHEDIR_IS_LOCAL, &dotdir_is_local);
	rs_conf_get_boolean(CONF_LOAD_GDK, &load_gdk);
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

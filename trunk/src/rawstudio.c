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


#define cpuid(n) \
  a = b = c = d = 0x0; \
  asm( \
  	"cpuid" \
  	: "=eax" (a), "=ebx" (b), "=ecx" (c), "=edx" (d) : "0" (n) \
	)

guint cpuflags = 0;
guchar previewtable[65536];

void update_previewtable(const double gamma, const double contrast);
inline void rs_photo_prepare(RS_PHOTO *photo, gdouble gamma);
void update_scaled(RS_BLOB *rs);
inline void rs_render_mask(guchar *pixels, guchar *mask, guint length);
gboolean rs_render_idle(RS_BLOB *rs);
void rs_render_overlay(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride,
	guchar *mask, gint mask_rowstride);
inline void rs_histogram_update_table(RS_BLOB *rs, RS_IMAGE16 *input, guint *table);
RS_SETTINGS *rs_settings_new();
void rs_settings_free(RS_SETTINGS *rss);
RS_SETTINGS_DOUBLE *rs_settings_double_new();
void rs_settings_double_free(RS_SETTINGS_DOUBLE *rssd);
RS_PHOTO *rs_photo_open_dcraw(const gchar *filename);
RS_PHOTO *rs_photo_open_gdk(const gchar *filename);
GdkPixbuf *rs_thumb_grt(const gchar *src);
GdkPixbuf *rs_thumb_gdk(const gchar *src);
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
update_previewtable(const gdouble gamma, const gdouble contrast)
{
	gint n;
	gdouble nd;
	gint res;
	double gammavalue;
	const double postadd = 0.5 - (contrast/2.0);
	gammavalue = (1.0/gamma);

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		res = (gint) ((pow(nd, gammavalue)*contrast+postadd) * 255.0);
		_CLAMP255(res);
		previewtable[n] = res;
	}
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
rs_photo_prepare(RS_PHOTO *photo, gdouble gamma)
{
	update_previewtable(gamma, photo->settings[photo->current_setting]->contrast);
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
update_preview(RS_BLOB *rs)
{
	if(unlikely(!rs->photo)) return;

	update_scaled(rs);
	rs_photo_prepare(rs->photo, rs->gamma);
	update_preview_region(rs, rs->preview_exposed);

	/* Reset histogram_table */
	if (GTK_WIDGET_VISIBLE(rs->histogram_image))
	{
		memset(rs->histogram_table, 0x00, sizeof(guint)*3*256);
		rs_histogram_update_table(rs, rs->histogram_dataset, (guint *) rs->histogram_table);
		update_histogram(rs);
	}

	rs->preview_done = FALSE;
	rs->preview_idle_render_lastrow = 0;
	if (!rs->preview_idle_render)
	{
		rs->preview_idle_render = TRUE;
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
			rs->photo->scaled->pixelsize, pixels, rs->photo->preview->rowstride);
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
				rs_photo_prepare(photo, 2.2);
				rs_photo_save(photo, parsed_filename, queue->filetype);
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
					rs->photo->scaled->pixelsize, out, rs->photo->preview->rowstride);
	
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
	return(FALSE);
}

void
rs_render_overlay(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride,
	guchar *mask, gint mask_rowstride)
{
	gint y,x;
	gint maskoffset, destoffset;
	rs_render(photo, width, height, in, in_rowstride, in_channels, out, out_rowstride);
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

inline void
rs_render(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride)
{
#ifdef __i386__
	if (cpuflags & _SSE)
	{
		register gint r,g,b;
		gint destoffset;
		gint col;
		gfloat top[4] align(16) = {65535.0, 65535.0, 65535.0, 65535.0};
		gfloat mat[12] align(16) = {
		photo->mat.coeff[0][0],
		photo->mat.coeff[1][0],
		photo->mat.coeff[2][0],
		0.0,
		photo->mat.coeff[0][1],
		photo->mat.coeff[1][1],
		photo->mat.coeff[2][1],
		0.0,
		photo->mat.coeff[0][2],
		photo->mat.coeff[1][2],
		photo->mat.coeff[2][2],
		0.0 };
		asm volatile (
			"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
			"movaps (%0), %%xmm3\n\t" /* matrix */
			"movaps 16(%0), %%xmm4\n\t"
			"movaps 32(%0), %%xmm5\n\t"
			"movaps (%1), %%xmm6\n\t" /* top */
			"pxor %%mm7, %%mm7\n\t" /* 0x0 */
			:
			: "r" (mat), "r" (top), "r" (photo->pre_mul)
			: "memory"
		);
		while(height--)
		{
			destoffset = height * out_rowstride;
			col = width;
			gushort *s = in + height * in_rowstride;
			while(col--)
			{
				asm volatile (
					/* load */
					"movq (%3), %%mm0\n\t" /* R | G | B | G2 */
					"movq %%mm0, %%mm1\n\t" /* R | G | B | G2 */
					"punpcklwd %%mm7, %%mm0\n\t" /* R | G */
					"punpckhwd %%mm7, %%mm1\n\t" /* B | G2 */
					"cvtpi2ps %%mm1, %%xmm0\n\t" /* B | G2 | ? | ? */
					"shufps $0x4E, %%xmm0, %%xmm0\n\t" /* ? | ? | B | G2 */
					"cvtpi2ps %%mm0, %%xmm0\n\t" /* R | G | B | G2 */

					"mulps %%xmm2, %%xmm0\n\t"
					"maxps %%xmm7, %%xmm0\n\t"
					"minps %%xmm6, %%xmm0\n\t"

					"movaps %%xmm0, %%xmm1\n\t"
					"shufps $0x0, %%xmm0, %%xmm1\n\t"
					"mulps %%xmm3, %%xmm1\n\t"
					"addps %%xmm1, %%xmm7\n\t"

					"movaps %%xmm0, %%xmm1\n\t"
					"shufps $0x55, %%xmm1, %%xmm1\n\t"
					"mulps %%xmm4, %%xmm1\n\t"
					"addps %%xmm1, %%xmm7\n\t"

					"movaps %%xmm0, %%xmm1\n\t"
					"shufps $0xAA, %%xmm1, %%xmm1\n\t"
					"mulps %%xmm5, %%xmm1\n\t"
					"addps %%xmm7, %%xmm1\n\t"

					"xorps %%xmm7, %%xmm7\n\t"
					"minps %%xmm6, %%xmm1\n\t"
					"maxps %%xmm7, %%xmm1\n\t"

					"cvtss2si %%xmm1, %0\n\t"
					"shufps $0xF9, %%xmm1, %%xmm1\n\t"
					"cvtss2si %%xmm1, %1\n\t"
					"shufps $0xF9, %%xmm1, %%xmm1\n\t"
					"cvtss2si %%xmm1, %2\n\t"
					: "=r" (r), "=r" (g), "=r" (b)
					: "r" (s)
					: "memory"
				);
				out[destoffset++] = previewtable[r];
				out[destoffset++] = previewtable[g];
				out[destoffset++] = previewtable[b];
				s += 4;
			}
		}
		asm volatile("emms\n\t");
	}
	else if (cpuflags & _3DNOW)
	{
		gint destoffset;
		gint col;
		register gint r=0,g=0,b=0;
		gfloat mat[12] align(8);
		gfloat top[2] align(8);
		mat[0] = photo->mat.coeff[0][0];
		mat[1] = photo->mat.coeff[0][1]*0.5;
		mat[2] = photo->mat.coeff[0][2];
		mat[3] = photo->mat.coeff[0][1]*0.5;
		mat[4] = photo->mat.coeff[1][0];
		mat[5] = photo->mat.coeff[1][1]*0.5;
		mat[6] = photo->mat.coeff[1][2];
		mat[7] = photo->mat.coeff[1][1]*0.5;
		mat[8] = photo->mat.coeff[2][0];
		mat[9] = photo->mat.coeff[2][1]*0.5;
		mat[10] = photo->mat.coeff[2][2];
		mat[11] = photo->mat.coeff[2][1]*0.5;
		top[0] = 65535.0;
		top[1] = 65535.0;
		asm volatile (
			"femms\n\t"
			"pxor %%mm7, %%mm7\n\t" /* 0x0 */
			"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
			"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
			"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
			:
			: "r" (&photo->pre_mul), "r" (&top)
		);
		while(height--)
		{
			destoffset = height * out_rowstride;
			col = width;
			gushort *s = in + height * in_rowstride;
			while(col--)
			{
				asm volatile (
					/* pre multiply */
					"movq (%0), %%mm0\n\t" /* R | G | B | G2 */
					"movq %%mm0, %%mm1\n\t" /* R | G | B | G2 */
					"punpcklwd %%mm7, %%mm0\n\t" /* R, G */
					"punpckhwd %%mm7, %%mm1\n\t" /* B, G2 */
					"pi2fd %%mm0, %%mm0\n\t" /* to float */
					"pi2fd %%mm1, %%mm1\n\t"
					"pfmul %%mm2, %%mm0\n\t" /* pre_mul[R]*R | pre_mul[G]*G */
					"pfmul %%mm3, %%mm1\n\t" /* pre_mul[B]*B | pre_mul[G2]*G2 */
					"pfmin %%mm6, %%mm0\n\t"
					"pfmin %%mm6, %%mm1\n\t"
					"pfmax %%mm7, %%mm0\n\t"
					"pfmax %%mm7, %%mm1\n\t"

					"add $8, %0\n\t" /* increment offset */

					/* red */
					"movq (%4), %%mm4\n\t" /* mat[0] | mat[1] */
					"movq 8(%4), %%mm5\n\t" /* mat[2] | mat[3] */
					"pfmul %%mm0, %%mm4\n\t" /* R*[0] | G*[1] */
					"pfmul %%mm1, %%mm5\n\t" /* B*[2] | G2*[3] */
					"pfadd %%mm4, %%mm5\n\t" /* R*[0] + B*[2] | G*[1] + G2*[3] */
					"pfacc %%mm5, %%mm5\n\t" /* R*[0] + B*[2] + G*[1] + G2*[3] | ? */
					"pfmin %%mm6, %%mm5\n\t"
					"pfmax %%mm7, %%mm5\n\t"
					"pf2id %%mm5, %%mm5\n\t" /* to integer */
					"movd %%mm5, %1\n\t" /* write r */

					/* green */
					"movq 16(%4), %%mm4\n\t"
					"movq 24(%4), %%mm5\n\t"
					"pfmul %%mm0, %%mm4\n\t"
					"pfmul %%mm1, %%mm5\n\t"
					"pfadd %%mm4, %%mm5\n\t"
					"pfacc %%mm5, %%mm5\n\t"
					"pfmin %%mm6, %%mm5\n\t"
					"pfmax %%mm7, %%mm5\n\t"
					"pf2id %%mm5, %%mm5\n\t"
					"movd %%mm5, %2\n\t"

					/* blue */
					"movq 32(%4), %%mm4\n\t"
					"movq 40(%4), %%mm5\n\t"
					"pfmul %%mm0, %%mm4\n\t"
					"pfmul %%mm1, %%mm5\n\t"
					"pfadd %%mm4, %%mm5\n\t"
					"pfacc %%mm5, %%mm5\n\t"
					"pfmin %%mm6, %%mm5\n\t"
					"pfmax %%mm7, %%mm5\n\t"
					"pf2id %%mm5, %%mm5\n\t"
					"movd %%mm5, %3\n\t"
					: "+r" (s), "+r" (r), "+r" (g), "+r" (b)
					: "r" (&mat)
				);
				out[destoffset++] = previewtable[r];
				out[destoffset++] = previewtable[g];
				out[destoffset++] = previewtable[b];
			}
		}
		asm volatile ("femms\n\t");
	}
	else
#endif
	{
		gint srcoffset, destoffset;
		register gint x,y;
		register gint r,g,b;
		gint rr,gg,bb;
		gint pre_mul[4];
		for(x=0;x<4;x++)
			pre_mul[x] = (gint) (photo->pre_mul[x]*128.0);
		for(y=0 ; y<height ; y++)
		{
			destoffset = y * out_rowstride;
			srcoffset = y * in_rowstride;
			for(x=0 ; x<width ; x++)
			{
				rr = (in[srcoffset+R]*pre_mul[R])>>7;
				gg = (in[srcoffset+G]*pre_mul[G])>>7;
				bb = (in[srcoffset+B]*pre_mul[B])>>7;
				_CLAMP65535_TRIPLET(rr,gg,bb);
				r = (rr*photo->mati.coeff[0][0]
					+ gg*photo->mati.coeff[0][1]
					+ bb*photo->mati.coeff[0][2])>>MATRIX_RESOLUTION;
				g = (rr*photo->mati.coeff[1][0]
					+ gg*photo->mati.coeff[1][1]
					+ bb*photo->mati.coeff[1][2])>>MATRIX_RESOLUTION;
				b = (rr*photo->mati.coeff[2][0]
					+ gg*photo->mati.coeff[2][1]
					+ bb*photo->mati.coeff[2][2])>>MATRIX_RESOLUTION;
				_CLAMP65535_TRIPLET(r,g,b);
				out[destoffset++] = previewtable[r];
				out[destoffset++] = previewtable[g];
				out[destoffset++] = previewtable[b];
				srcoffset+=in_channels;
			}
		}
	}
	return;
}

inline void
rs_histogram_update_table(RS_BLOB *rs, RS_IMAGE16 *input, guint *table)
{
	gint y,x;
	gint srcoffset;
	gint r,g,b,rr,gg,bb;
	gushort *in;
	gint pre_mul[4];

	if (unlikely(input==NULL)) return;

	for(x=0;x<4;x++)
		pre_mul[x] = (gint) (rs->photo->pre_mul[x]*128.0);
	in	= input->pixels;
	for(y=0 ; y<input->h ; y++)
	{
		srcoffset = y * input->rowstride;
		for(x=0 ; x<input->w ; x++)
		{
			rr = (in[srcoffset+R]*pre_mul[R])>>7;
			gg = (in[srcoffset+G]*pre_mul[G])>>7;
			bb = (in[srcoffset+B]*pre_mul[B])>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*rs->photo->mati.coeff[0][0]
				+ gg*rs->photo->mati.coeff[0][1]
				+ bb*rs->photo->mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*rs->photo->mati.coeff[1][0]
				+ gg*rs->photo->mati.coeff[1][1]
				+ bb*rs->photo->mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*rs->photo->mati.coeff[2][0]
				+ gg*rs->photo->mati.coeff[2][1]
				+ bb*rs->photo->mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			table[previewtable[r]]++;
			table[256+previewtable[g]]++;
			table[512+previewtable[b]]++;
			srcoffset+=input->pixelsize;
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
	update_preview(rs);
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
rs_photo_save(RS_PHOTO *photo, const gchar *filename, gint filetype)
{
	GdkPixbuf *pixbuf;
	RS_IMAGE16 *rsi;

	if (photo->orientation)
	{
		rsi = rs_image16_copy(photo->input);
		rs_image16_orientation(rsi, photo->orientation);
	}
	else
		rsi = photo->input;
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rsi->w, rsi->h);

	rs_render(photo, rsi->w, rsi->h, rsi->pixels,
		rsi->rowstride, rsi->channels,
		gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf));

	/* actually save */
	switch (filetype)
	{
		case FILETYPE_JPEG:
			gui_save_jpg(pixbuf, (gchar *) filename);
			break;
		case FILETYPE_PNG:
			gui_save_png(pixbuf, (gchar *) filename);
			break;
	}
	if (photo->orientation)
		rs_image16_free(rsi);
	g_object_unref(pixbuf);
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
	rs->gamma = 0.0;
	rs_conf_get_double(CONF_GAMMAVALUE, &rs->gamma);
	if(rs->gamma < 0.1)
	{
		rs->gamma = 2.2;
		rs_conf_set_double(CONF_GAMMAVALUE,rs->gamma);
	}
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

	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);
		photo = rs_photo_new();
		shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);
		photo->input = rs_image16_new(raw->raw.width, raw->raw.height, 4, 4);
		src  = (gushort *) raw->raw.image;
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
				destoffset = (guint) (photo->input->pixels + y*photo->input->rowstride);
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
			}
		}
		else
#endif
		{
			for (y=0; y<raw->raw.height; y++)
			{
				destoffset = y*photo->input->rowstride;
				srcoffset = y*photo->input->w*photo->input->pixelsize;
				for (x=0; x<raw->raw.width; x++)
				{
					register gint r,g,b;
					r = (src[srcoffset++] - raw->black)<<shift;
					g = (src[srcoffset++] - raw->black)<<shift;
					b = (src[srcoffset++] - raw->black)<<shift;
					_CLAMP65535_TRIPLET(r, g, b);
					photo->input->pixels[destoffset++] = r;
					photo->input->pixels[destoffset++] = g;
					photo->input->pixels[destoffset++] = b;
					g = (src[srcoffset++] - raw->black)<<shift;
					_CLAMP65535(g);
					photo->input->pixels[destoffset++] = g;
				}
			}
		}
		photo->filename = g_strdup(filename);
		dcraw_close(raw);
		photo->active = TRUE;
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
			res = (gint) (pow(nd, 2.2) * 65535.0);
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
	rs_conf_get_boolean(CONF_CACHEDIR_IS_LOCAL, &dotdir_is_local);
	rs_conf_get_boolean(CONF_LOAD_GDK, &load_gdk);
	gui_init(argc, argv);
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

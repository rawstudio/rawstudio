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

#include "rs-photo.h"
#include "rs-image.h"
#include "color.h"
#include "rs-cache.h"

static void rs_photo_class_init (RS_PHOTOClass *klass);
void rs_photo_open_dcraw_apply_black_and_shift_half_size(dcraw_data *raw, RS_PHOTO *photo);

G_DEFINE_TYPE (RS_PHOTO, rs_photo, G_TYPE_OBJECT);

enum {
	SPATIAL_CHANGED,
	SETTINGS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void
rs_photo_dispose (GObject *obj)
{
	RS_PHOTO *self = (RS_PHOTO *)obj;

	if (self->dispose_has_run)
		return;
	self->dispose_has_run = TRUE;

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
rs_photo_finalize (GObject *obj)
{
	gint c;
	RS_PHOTO *photo = (RS_PHOTO *)obj;

	if (photo->filename)
		g_free(photo->filename);

	if (photo->metadata)
		rs_metadata_free(photo->metadata);

	if (photo->input)
		g_object_unref(photo->input);

	for(c=0;c<3;c++)
		rs_settings_double_free(photo->settings[c]);
	if (photo->crop)
		g_free(photo->crop);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
rs_photo_class_init (RS_PHOTOClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = rs_photo_dispose;
	gobject_class->finalize = rs_photo_finalize;

	signals[SETTINGS_CHANGED] = g_signal_new ("settings-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	signals[SPATIAL_CHANGED] = g_signal_new ("spatial-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	parent_class = g_type_class_peek_parent (klass);
}

static void
rs_photo_init (RS_PHOTO *photo)
{
	guint c;

	photo->filename = NULL;
	photo->input = NULL;
	ORIENTATION_RESET(photo->orientation);
	photo->priority = PRIO_U;
	photo->metadata = rs_metadata_new();
	for(c=0;c<3;c++)
		photo->settings[c] = rs_settings_double_new();
	photo->crop = NULL;
	photo->angle = 0.0;
	photo->exported = FALSE;
}

/**
 * Allocates a new RS_PHOTO
 * @return A new RS_PHOTO
 */
RS_PHOTO *
rs_photo_new()
{
	RS_PHOTO *photo;

	photo = g_object_new(RS_TYPE_PHOTO, NULL);

	return photo;
}

/**
 * Rotates a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param quarterturns How many quarters to turn
 * @param angle The angle in degrees (360 is whole circle) to turn the image
 */
void
rs_photo_rotate(RS_PHOTO *photo, const gint quarterturns, const gdouble angle)
{
	gint n;

	if (!photo) return;

	photo->angle += angle;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_rotate(photo->crop, photo->crop, w, h, quarterturns);
	}

	for(n=0;n<quarterturns;n++)
		ORIENTATION_90(photo->orientation);

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Sets a new crop of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param crop The new crop or NULL to remove previous cropping
 */
void
rs_photo_set_crop(RS_PHOTO *photo, const RS_RECT *crop)
{
	if (!photo) return;

	if (photo->crop)
		g_free(photo->crop);
	photo->crop = NULL;

	if (crop)
	{
		photo->crop = g_new(RS_RECT, 1);
		*photo->crop = *crop;
	}

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Gets the crop of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return The crop as a RS_RECT or NULL if the photo is uncropped
 */
RS_RECT *
rs_photo_get_crop(RS_PHOTO *photo)
{
	if (!photo) return NULL;

	return photo->crop;
}

/* Macro to create functions for changing single parameters */
#define RS_PHOTO_SET_GDOUBLE_VALUE(setting) \
void \
rs_photo_set_##setting(RS_PHOTO *photo, const gint snapshot, const gdouble value) \
{ \
	if (!photo) return; \
	g_return_if_fail ((snapshot>=0) && (snapshot<=2)); \
	photo->settings[snapshot]->setting = value; \
	g_signal_emit(photo, signals[SETTINGS_CHANGED], 0, NULL); \
}

RS_PHOTO_SET_GDOUBLE_VALUE(exposure)
RS_PHOTO_SET_GDOUBLE_VALUE(saturation)
RS_PHOTO_SET_GDOUBLE_VALUE(hue)
RS_PHOTO_SET_GDOUBLE_VALUE(contrast)
RS_PHOTO_SET_GDOUBLE_VALUE(warmth)
RS_PHOTO_SET_GDOUBLE_VALUE(tint)

#undef RS_PHOTO_SET_GDOUBLE_VALUE

/**
 * Apply settings to a RS_PHOTO from a RS_SETTINGS_DOUBLE
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param rs_settings_double The settings to apply
 * @param mask A mask for defining which settings to apply
 */
void
rs_photo_apply_settings_double(RS_PHOTO *photo, const gint snapshot, const RS_SETTINGS_DOUBLE *rs_settings_double, const gint mask)
{
	if (!photo) return;
	if (!rs_settings_double) return;
	g_return_if_fail ((snapshot>=0) && (snapshot<=2));

	rs_settings_double_copy(rs_settings_double, photo->settings[snapshot], mask);

	g_signal_emit(photo, signals[SETTINGS_CHANGED], 0, NULL);
}

/**
 * Flips a RS_PHOTO
 * @param photo A RS_PHOTO
 */
void
rs_photo_flip(RS_PHOTO *photo)
{
	if (!photo) return;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_flip(photo->crop, photo->crop, w, h);
	}
	ORIENTATION_FLIP(photo->orientation);

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Mirrors a RS_PHOTO
 * @param photo A RS_PHOTO
 */
void
rs_photo_mirror(RS_PHOTO *photo)
{
	if (!photo) return;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_mirror(photo->crop, photo->crop, w, h);
	}
	ORIENTATION_MIRROR(photo->orientation);

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Closes a RS_PHOTO - this basically means saving cache
 * @param photo A RS_PHOTO
 */
void
rs_photo_close(RS_PHOTO *photo)
{
	if (!photo) return;

	rs_cache_save(photo);
}

/**
 * Open a photo using the dcraw-engine
 * @param filename The filename to open
 * @param half_size Open in half size - without NN-demosaic
 * @return The newly created RS_PHOTO or NULL on error
 */
RS_PHOTO *
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

/**
 * Open a photo using the GDK-engine
 * @param filename The filename to open
 * @param half_size Does nothing
 * @return The newly created RS_PHOTO or NULL on error
 */
RS_PHOTO *
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

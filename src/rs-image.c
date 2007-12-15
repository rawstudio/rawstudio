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

#include <gtk/gtk.h>
#include <string.h>
#define _ISOC9X_SOURCE 1 /* lrint() */
#define _ISOC99_SOURCE 1
#define	__USE_ISOC9X 1
#define	__USE_ISOC99 1
#include <math.h> /* floor() */
#include "color.h"
#include "rs-math.h"
#include "rawstudio.h"
#include "rs-image.h"

struct struct_program {
	gint offset;
	gint scale;
};

static void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
static void rs_image16_mirror(RS_IMAGE16 *rsi);
static void rs_image16_flip(RS_IMAGE16 *rsi);
inline static void rs_image16_nearest(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);
inline static void rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);
inline static void rs_image16_bicubic(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);
static void rs_image8_realloc(RS_IMAGE8 *rsi, const guint width, const guint height, const guint channels, const guint pixelsize);
inline gushort topright(gushort *in, struct struct_program *program, gint divisor);
inline gushort top(gushort *in, struct struct_program *program, gint divisor);
inline gushort topleft(gushort *in, struct struct_program *program, gint divisor);
inline gushort right(gushort *in, struct struct_program *program, gint divisor);
inline gushort left(gushort *in, struct struct_program *program, gint divisor);
inline gushort bottomright(gushort *in, struct struct_program *program, gint divisor);
inline gushort bottom(gushort *in, struct struct_program *program, gint divisor);
inline gushort bottomleft(gushort *in, struct struct_program *program, gint divisor);

GStaticMutex giant_spinlock = G_STATIC_MUTEX_INIT;

static void rs_image16_class_init (RS_IMAGE16Class *klass);

G_DEFINE_TYPE (RS_IMAGE16, rs_image16, G_TYPE_OBJECT);

enum {
	PIXELDATA_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void
rs_image16_dispose (GObject *obj)
{
	RS_IMAGE16 *self = (RS_IMAGE16 *)obj;

	if (self->dispose_has_run)
		return;
	self->dispose_has_run = TRUE;

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
rs_image16_finalize (GObject *obj)
{
	RS_IMAGE16 *self = (RS_IMAGE16 *)obj;

	g_free(self->pixels);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
rs_image16_class_init (RS_IMAGE16Class *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = rs_image16_dispose;
	gobject_class->finalize = rs_image16_finalize;

	signals[PIXELDATA_CHANGED] = g_signal_new ("pixeldata-changed",
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
rs_image16_init (RS_IMAGE16 *self)
{
	ORIENTATION_RESET(self->orientation);
	self->filters = 0;
	self->preview = FALSE;
	self->pixels = NULL;
}

void
rs_image8_ref(RS_IMAGE8 *image)
{
	if (!image) return;

	g_static_mutex_lock(&giant_spinlock);
	image->reference_count++;
	g_static_mutex_unlock(&giant_spinlock);
}

void
rs_image8_unref(RS_IMAGE8 *image)
{
	if (!image) return;

	g_static_mutex_lock(&giant_spinlock);
	image->reference_count--;
	if (image->reference_count < 1)
	{
		g_free(image->pixels);
		g_free(image);
	}
	g_static_mutex_unlock(&giant_spinlock);
}

void
rs_image16_orientation(RS_IMAGE16 *rsi, const gint orientation)
{
	const gint rot = ((orientation&3)-(rsi->orientation&3)+8)%4;

	rs_image16_rotate(rsi, rot);
	if (((rsi->orientation)&4)^((orientation)&4))
		rs_image16_flip(rsi);

	return;
}

static void
rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns)
{
	gint width, height, pitch;
	gint y,x;
	gint offset,destoffset;
	gushort *swap;

	quarterturns %= 4;

	rs_image16_ref(rsi);
	switch (quarterturns)
	{
		case 1:
			width = rsi->h;
			height = rsi->w;
			pitch = PITCH(width);
			swap = (gushort *) g_malloc(pitch*height*sizeof(gushort)*rsi->pixelsize);
			for(y=0; y<rsi->h; y++)
			{
				offset = y * rsi->rowstride;
				for(x=0; x<rsi->w; x++)
				{
					destoffset = (width-1-y+pitch*x)*rsi->pixelsize;
					swap[destoffset+R] = rsi->pixels[offset+R];
					swap[destoffset+G] = rsi->pixels[offset+G];
					swap[destoffset+B] = rsi->pixels[offset+B];
					if (rsi->pixelsize==4)
						swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=rsi->pixelsize;
				}
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			rsi->rowstride = pitch * rsi->pixelsize;
			ORIENTATION_90(rsi->orientation);
			break;
		case 2:
			rs_image16_flip(rsi);
			rs_image16_mirror(rsi);
			break;
		case 3:
			width = rsi->h;
			height = rsi->w;
			pitch = PITCH(width);
			swap = (gushort *) g_malloc(pitch*height*sizeof(gushort)*4);
			for(y=0; y<rsi->h; y++)
			{
				offset = y*rsi->rowstride;
				destoffset = ((height-1)*pitch + y)*rsi->pixelsize;
				for(x=0; x<rsi->w; x++)
				{
					swap[destoffset+R] = rsi->pixels[offset+R];
					swap[destoffset+G] = rsi->pixels[offset+G];
					swap[destoffset+B] = rsi->pixels[offset+B];
					if (rsi->pixelsize==4)
						swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=rsi->pixelsize;
					destoffset -= pitch*rsi->pixelsize;
				}
				offset += rsi->pitch*rsi->pixelsize;
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			rsi->rowstride = pitch * rsi->pixelsize;
			ORIENTATION_270(rsi->orientation);
			break;
		default:
			break;
	}
	rs_image16_unref(rsi);
	return;
}

static void
rs_image16_mirror(RS_IMAGE16 *rsi)
{
	gint row,col;
	gint offset,destoffset;

	rs_image16_ref(rsi);

	for(row=0;row<rsi->h;row++)
	{
		offset = row*rsi->rowstride;
		destoffset = (row*rsi->pitch+rsi->w-1)*rsi->pixelsize;
		for(col=0;col<rsi->w/2;col++)
		{
			SWAP(rsi->pixels[offset+R], rsi->pixels[destoffset+R]);
			SWAP(rsi->pixels[offset+G], rsi->pixels[destoffset+G]);
			SWAP(rsi->pixels[offset+B], rsi->pixels[destoffset+B]);
			if (rsi->pixelsize==4)
				SWAP(rsi->pixels[offset+G2], rsi->pixels[destoffset+G2]);
			offset+=rsi->pixelsize;
			destoffset-=rsi->pixelsize;
		}
	}
	ORIENTATION_MIRROR(rsi->orientation);

	rs_image16_unref(rsi);
}

static void
rs_image16_flip(RS_IMAGE16 *rsi)
{
	gint row;
	const gint linel = rsi->rowstride*sizeof(gushort);
	gushort *tmp = (gushort *) g_malloc(linel);

	rs_image16_ref(rsi);

	for(row=0;row<rsi->h/2;row++)
	{
		memcpy(tmp,
			rsi->pixels + row * rsi->rowstride, linel);
		memcpy(rsi->pixels + row * rsi->rowstride,
			rsi->pixels + (rsi->h-1-row) * rsi->rowstride, linel);
		memcpy(rsi->pixels + (rsi->h-1-row) * rsi->rowstride, tmp, linel);
	}
	g_free(tmp);
	ORIENTATION_FLIP(rsi->orientation);

	rs_image16_unref(rsi);

	return;
}

static void inline
rs_image16_preview(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y)
{
	const gint nx = (lrint(x)&0xfffffff8)+4;
	const gint ny = (lrint(y)&0xfffffff8)+4;
	const gushort *a = GET_PIXEL(in, nx, ny);

	if (unlikely((nx>(in->w-1))||(ny>(in->h-1))))
		return;
	else if (unlikely(nx<0) || unlikely(ny<0))
		return;

	out[R] = a[R];
	out[G] = a[G];
	out[B] = a[B];
}

static void inline
rs_image16_nearest(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y)
{
	const gint nx = lrint(x);
	const gint ny = lrint(y);
	const gushort *a = &in->pixels[ny*in->rowstride + nx*in->pixelsize];
	out[R] = a[R];
	out[G] = a[G];
	out[B] = a[B];
	out[G2] = a[G2];
}

static void inline
rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y)
{
	gint fx = floor(x);
	gint fy = floor(y);
	const gint nextx = (fx<(in->w-1)) * in->pixelsize;
	const gint nexty = (fy<(in->h-1));
	gushort *a, *b, *c, *d;
	gdouble diffx, diffy;
	gdouble aw, bw, cw, dw;

	if (unlikely((fx>(in->w-1))||(fy>(in->h-1))))
		return;
	else if (unlikely(fx<0))
	{
		if (likely(fx<-1))
			return;
		else
		{
			fx = 0;
			x = 0.0;
		}
	}

	if (unlikely(fy<0))
	{
		if (likely(fy<-1))
			return;
		else
		{
			fy = 0;
			y = 0.0;
		}
	}

	/* find four cornerpixels */
	a = &in->pixels[fy*in->rowstride + fx*in->pixelsize];
	b = a + nextx;
	c = &in->pixels[(fy+nexty)*in->rowstride + fx*in->pixelsize];
	d = c + nextx;

	/* calculate weightings */
	diffx = x - ((gdouble) fx); /* x distance from a */
	diffy = y - ((gdouble) fy); /* y distance from a */
	aw = (1.0-diffx) * (1.0-diffy);
	bw = diffx       * (1.0-diffy);
	cw = (1.0-diffx) * diffy;
	dw = diffx       * diffy;

	out[R]  = (gushort) (a[R]*aw  + b[R]*bw  + c[R]*cw  + d[R]*dw);
	out[G]  = (gushort) (a[G]*aw  + b[G]*bw  + c[G]*cw  + d[G]*dw);
	out[B]  = (gushort) (a[B]*aw  + b[B]*bw  + c[B]*cw  + d[B]*dw);
	out[G2] = 0;
	return;
}

inline static gushort
cubicweight(gushort a1, gushort a2, gushort a3, gushort a4, gdouble t)
{
	int p = (a4 - a3) - (a1 - a2);
	int q = (a1 - a2) - p;
	int r = a3 - a1;
	int s = a2;
	gdouble tSqrd = t * t;

	return (p * (tSqrd * t)) + (q * tSqrd) + (r * t) + s;
} 


inline static void 
rs_image16_bicubic(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y)
{
	gint fx = floor(x);
	gint fy = floor(y);
	const gint nextx = (fx<(in->w-1)) * in->pixelsize;
	const gint nexty = (fy<(in->h-1));
	gushort* pp;
	int i, j;
	
	if (unlikely((fx>(in->w-1))||(fy>(in->h-1))))
		return;
	else if (unlikely(fx<0))
	{
		if (likely(fx<-1))
			return;
		else
		{
			fx = 0;
			x = 0.0;
		}
	}

	if (unlikely(fy<0))
	{
		if (likely(fy<-1))
			return;
		else
		{
			fy = 0;
			y = 0.0;
		}
	}
	gdouble dx = x - fx;
	gdouble dy = y - fy;

	gushort wr[4];
	gushort wg[4];
	gushort wb[4];

	gint safty = 0;
	for(j = 0; j < 4; ++j)
	{
		if(unlikely((fy+j+1)>in->h))
		{
			safty = in->h - (fy+j+1);
		}
		pp = &in->pixels[(fy+j*nexty + safty)*in->rowstride + fx*in->pixelsize];

		gushort ar[4];
		gushort ag[4];
		gushort ab[4];

		for(i = 0; i < 4; ++i)
		{
			ar[i] = pp[R];
			ag[i] = pp[G];
			ab[i] = pp[B];

			pp += nextx; 
		} 

		wr[j] = cubicweight(ar[0],ar[1],ar[2],ar[3],dx);
		wg[j] = cubicweight(ag[0],ag[1],ag[2],ag[3],dx);
		wb[j] = cubicweight(ab[0],ab[1],ab[2],ab[3],dx);
	}
	out[R]  = cubicweight(wr[0],wr[1],wr[2],wr[3],dy);
	out[G]  = cubicweight(wg[0],wg[1],wg[2],wg[3],dy);
	out[B]  = cubicweight(wb[0],wb[1],wb[2],wb[3],dy);
	out[G2] = 0;
	return;
}

		
void
rs_image16_transform_getwh(RS_IMAGE16 *in, RS_RECT *crop, gdouble angle, gint orientation, gint *w, gint *h)
{
	RS_MATRIX3 mat;
	gdouble minx, miny;
	gdouble maxx, maxy;

	matrix3_identity(&mat);

	/* rotate straighten-angle + orientation-angle */
	matrix3_affine_rotate(&mat, angle+(orientation&3)*90.0);

	/* flip if needed */
	if (orientation&4)
		matrix3_affine_scale(&mat, 1.0, -1.0);

	/* translate into positive x,y*/
	matrix3_affine_get_minmax(&mat, &minx, &miny, &maxx, &maxy, 0.0, 0.0, (gdouble) in->w, (gdouble) in->h);
	matrix3_affine_translate(&mat, -minx, -miny);

	/* get width and height used for calculating scale */
	*w = lrint(maxx - minx);
	*h = lrint(maxy - miny);

	/* apply crop if needed */
	if (crop)
	{
		/* calculate cropped width and height */
		*w = abs(crop->x2 - crop->x1 + 1);
		*h = abs(crop->y2 - crop->y1 + 1);
		/* translate non-cropped area into negative x,y*/
		matrix3_affine_translate(&mat, ((gdouble) -crop->x1), ((gdouble) -crop->y1));
	}

	return;
}

/**
 * Transforms an RS_IMAGE16
 * @param in An input image
 * @param out An output image or NULL
 * @param affine Will be set to forward affine matrix if not NULL.
 * @param inverse_affine Will be set to inverse affine matrix if not NULL.
 * @param crop Crop to apply or NULL
 * @param width Output width or -1
 * @param height Output height or -1
 * @param keep_aspect if set to TRUE aspect will be locked
 * @param scale How much to scale the image (0.01 - 2.0)
 * @param angle Rotation angle in degrees
 * @param orientation The orientation
 * @param actual_scale The resulting scale or NULL
 * @return A new RS_IMAGE16 or out
 */
RS_IMAGE16 *
rs_image16_transform(RS_IMAGE16 *in, RS_IMAGE16 *out, RS_MATRIX3 *affine, RS_MATRIX3 *inverse_affine,
	RS_RECT *crop, gint width, gint height, gboolean keep_aspect, gdouble scale, gdouble angle, gint orientation, gdouble *actual_scale)
{
	RS_MATRIX3 mat;
	gdouble xscale=1.0, yscale=1.0;
	gdouble x, y;
	gdouble minx, miny;
	gdouble maxx, maxy;
	gdouble w, h;
	gint row, col;
	gint destoffset;

	rs_image16_ref(in);

	matrix3_identity(&mat);

	/* rotate straighten-angle + orientation-angle */
	matrix3_affine_rotate(&mat, angle+(orientation&3)*90.0);

	/* flip if needed */
	if (orientation&4)
		matrix3_affine_scale(&mat, 1.0, -1.0);

	/* translate into positive x,y*/
	matrix3_affine_get_minmax(&mat, &minx, &miny, &maxx, &maxy, 0.0, 0.0, (gdouble) (in->w-1), (gdouble) (in->h-1));
	matrix3_affine_translate(&mat, -minx, -miny);

	/* get width and height used for calculating scale */
	w = maxx - minx + 1.0;
	h = maxy - miny + 1.0;

	/* apply crop if needed */
	if (crop)
	{
		/* calculate cropped width and height */
		w = (gdouble) abs(crop->x2 - crop->x1);
		h = (gdouble) abs(crop->y2 - crop->y1);
		/* translate non-cropped area into negative x,y*/
		matrix3_affine_translate(&mat, ((gdouble) -crop->x1), ((gdouble) -crop->y1));
	}

	/* calculate scale */
	if (scale > 0.0)
		xscale = yscale = scale;
	else
	{
		if (width > 0)
		{
			xscale = ((gdouble)width)/w;
			if (height<1)
				yscale = xscale;
		}
		if (height > 0)
		{
			yscale = ((gdouble)height)/h;
			if (width<1)
				xscale = yscale;
		}
		if ((width>0) && (height>0) && keep_aspect)
		{
			if ((h*xscale)>((gdouble) height))
				xscale = yscale;
			if ((w*yscale)>((gdouble) width))
				yscale = xscale;
		}
	}

	/* scale */
	matrix3_affine_scale(&mat, xscale, yscale);

	/* Write back the actual scaling if requested */
	if (actual_scale)
		*actual_scale = (xscale+yscale)/2.0;

	/* apply scaling to our previously calculated width and height */
	w *= xscale;
	h *= yscale;

	/* calculate inverse affine (without rotation and orientation) */
	if (inverse_affine)
	{
		matrix3_identity(inverse_affine);
		if (crop)
			matrix3_affine_translate(inverse_affine, ((gdouble) -crop->x1), ((gdouble) -crop->y1));
		matrix3_affine_scale(inverse_affine, xscale, yscale);
		matrix3_affine_invert(inverse_affine);
	}
	if (affine)
	{
		matrix3_identity(affine);
		if (crop)
			matrix3_affine_translate(affine, ((gdouble) -crop->x1), ((gdouble) -crop->y1));
		matrix3_affine_scale(affine, xscale, yscale);
	}

	if (out==NULL)
		out = rs_image16_new(lrint(w), lrint(h), 3, 4);
	else
		g_assert((out->w>=((gint)w)) && (out->h>=((gint)h)));

	rs_image16_ref(out);
	/* we use the inverse matrix for this */
	matrix3_affine_invert(&mat);

	for(row=0;row<out->h;row++)
	{
		gdouble foox = ((gdouble)row) * mat.coeff[1][0] + mat.coeff[2][0];
		gdouble fooy = ((gdouble)row) * mat.coeff[1][1] + mat.coeff[2][1];
		destoffset = row * out->rowstride;
		for(col=0;col<out->w;col++,destoffset += out->pixelsize)
		{
			x = ((gdouble)col)*mat.coeff[0][0] + foox;
			y = ((gdouble)col)*mat.coeff[0][1] + fooy;

			if (in->preview)
				rs_image16_preview(in, &out->pixels[destoffset], x, y);
			else
				rs_image16_bilinear(in, &out->pixels[destoffset], x, y);
		}
	}

	rs_image16_unref(out);
	rs_image16_unref(in);

	return(out);
}

RS_IMAGE16 *
rs_image16_scale_double(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale)
{
	gint x,y;
	gint destoffset, srcoffset, rowoffset;

	gint x1, x2, y1, y2;
	gdouble diffx, diffy;

	if ( scale == 1.0 ){
		rs_image16_ref(in);
		out = rs_image16_copy(in);
		rs_image16_unref(in);
		return(out);
	}

	rs_image16_ref(in);

	scale = 1 / scale;

	if (out==NULL)
		out = rs_image16_new((int)(in->w/scale), (int)(in->h/scale), in->channels, in->pixelsize);
	else
		g_assert(out->w == (int)(in->w/scale));

	rs_image16_ref(out);

	if ( scale >= 1.0 ){ // Cheap downscale
		for(y=0; y!=out->h; y++)
		{
			rowoffset = (gint)(y*scale) * in->rowstride;
			destoffset = y*out->rowstride;
			for(x=0; x!=out->w; x++)
			{
				srcoffset = (lrint(x*scale)*in->pixelsize)+rowoffset;
				out->pixels[destoffset++] = in->pixels[srcoffset++];
				out->pixels[destoffset++] = in->pixels[srcoffset++];
				out->pixels[destoffset++] = in->pixels[srcoffset++];
				if (in->pixelsize==4)
					out->pixels[destoffset++] = in->pixels[srcoffset++];
			}
		}
	}else{ // Upscale
		gint index11, index12, index21, index22;
		gushort col11, col12, col21, col22, col;
		gint rowoffset1, rowoffset2;
		gdouble w11, w12, w21, w22;

		for(y=0; y!=out->h; y++)
		{
			destoffset = y*out->rowstride;
			diffy = y * scale - floor(y*scale);
		
			y1 = y * scale;
			y2 = y1 + 1;
		
			if (unlikely(y2 >= in->h)) y2 = in->h - 1;
			
			rowoffset1 = y1*in->rowstride;
			rowoffset2 = y2*in->rowstride;

			for(x=0; x!=out->w; x++)
			{
				diffx = x * scale - floor(x*scale);

				// Calc weights of each pixel
				w11 = (1.0 - diffx)*(1.0 - diffy);
				w12 = ((1.0 - diffx)*diffy);
				w21 = (diffx*(1.0 - diffy));
				w22 = (diffx*diffy);
	
				x1 = x * scale;
				x2 = x1 + 1;

				if (unlikely(x2 >= in->w)) x2 = in->w - 1;
				
				// Red
				index11 = (x1<<2)+rowoffset1;
				index12 = (x1<<2)+rowoffset2;
				index21 = (x2<<2)+rowoffset1;
				index22 = (x2<<2)+rowoffset2;

				col11 = in->pixels[index11];
				col12 = in->pixels[index12];
				col21 = in->pixels[index21];
				col22 = in->pixels[index22];

				col = (gushort)(col11*w11 + col21*w21 + col12*w12 + col22*w22);

				out->pixels[destoffset++] = col; 

				// Green
				index11++;
				index12++;
				index21++;
				index22++;

				col11 = in->pixels[index11];
				col12 = in->pixels[index12];
				col21 = in->pixels[index21];
				col22 = in->pixels[index22];

				col = (gushort)(col11*w11 + col21*w21 + col12*w12 + col22*w22);
				
				out->pixels[destoffset++] = col; 
				
				// Blue
				index11++;
				index12++;
				index21++;
				index22++;

				col11 = in->pixels[index11];
				col12 = in->pixels[index12];
				col21 = in->pixels[index21];
				col22 = in->pixels[index22];

				col = (gushort)(col11*w11 + col21*w21 + col12*w12 + col22*w22);
				
				out->pixels[destoffset++] = col; 
				
				// Green2
				index11++;
				index12++;
				index21++;
				index22++;

				col11 = in->pixels[index11];
				col12 = in->pixels[index12];
				col21 = in->pixels[index21];
				col22 = in->pixels[index22];

				col = (gushort)(col11*w11 + col21*w21 + col12*w12 + col22*w22);
				
				out->pixels[destoffset++] = col; 
				

			}
		}

	}
	rs_image16_unref(in);
	rs_image16_unref(out);
	return(out);
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize)
{
	RS_IMAGE16 *rsi;
	rsi = g_object_new(RS_TYPE_IMAGE16, NULL);
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * pixelsize;
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	rsi->pixels = g_new0(gushort, rsi->h*rsi->rowstride);
	return(rsi);
}

RS_IMAGE8 *
rs_image8_new(const guint width, const guint height, const guint channels, const guint pixelsize)
{
	RS_IMAGE8 *rsi;

	rsi = (RS_IMAGE8 *) g_malloc(sizeof(RS_IMAGE8));
	rsi->w = width;
	rsi->h = height;
	ORIENTATION_RESET(rsi->orientation);
	rsi->rowstride = PITCH(width) * pixelsize;
	rsi->pixels = g_new0(guchar, rsi->h*rsi->rowstride);
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	rsi->reference_count = 0;
	rs_image8_ref(rsi);

	return(rsi);
}

static void
rs_image8_realloc(RS_IMAGE8 *rsi, const guint width, const guint height, const guint channels, const guint pixelsize)
{
	if (!rsi) return;

	rs_image8_ref(rsi);

	/* Do we actually differ? */
	if ((rsi->w != width) || (rsi->h != height) || (rsi->channels != channels) || (rsi->pixelsize != pixelsize))
	{
		/* Free the old pixels */
		g_free(rsi->pixels);

		/* Fill in new values */
		rsi->w = width;
		rsi->h = height;
		rsi->rowstride = PITCH(width) * pixelsize;
		rsi->pixels = g_new0(guchar, rsi->h*rsi->rowstride);
		rsi->channels = channels;
		rsi->pixelsize = pixelsize;
	}

	rs_image8_unref(rsi);
}

/**
 * Renders an exposure map on top of an RS_IMAGE8 with 3 channels
 * @param image A RS_IMAGE8
 * @param only_row A single row to render or -1 to render all
 */
void
rs_image8_render_exposure_mask(RS_IMAGE8 *image, gint only_row)
{
	gint row, col;
	gint start;
	gint stop;

	g_assert(image != NULL);
	g_assert(image->channels == 3);

	rs_image8_ref(image);
	if ((only_row > -1) && (only_row < image->h))
	{
		start = only_row;
		stop = only_row + 1;
	}
	else
	{
		start = 0;
		stop = image->h;
	}

	for(row=start;row<stop;row++)
	{
		/* Get start pixel of row */
		guchar *pixel = GET_PIXEL(image, 0, row);

		for(col=0;col<image->w;col++)
		{
			/* Catch pixels overexposed and color them red */
			if ((pixel[R]==0xFF) || (pixel[G]==0xFF) || (pixel[B]==0xFF))
			{
				*pixel++ = 0xFF;
				*pixel++ = 0x00;
				*pixel++ = 0x00;
			}
			/* Color underexposed pixels blue */
			else if ((pixel[R]<2) && (pixel[G]<2) && (pixel[B]<2))
			{
				*pixel++ = 0x00;
				*pixel++ = 0x00;
				*pixel++ = 0xFF;
			}
			else
				pixel += 3;
		}
	}
	rs_image8_unref(image);
}

/**
 * Renders a shaded image
 * @param in The input image
 * @param out The output image or NULL
 * @return The shaded image
 */
RS_IMAGE8 *
rs_image8_render_shaded(RS_IMAGE8 *in, RS_IMAGE8 *out)
{
	gint size;

	if (!in) return NULL;

	rs_image8_ref(in);

	if (!out)
		out = rs_image8_new(in->w, in->h, in->channels, in->pixelsize);
	else
		rs_image8_realloc(out, in->w, in->h, in->channels, in->pixelsize);

	rs_image8_ref(out);

	size = in->h * in->rowstride;
	while(size--)
		out->pixels[size] = ((in->pixels[size]+63)*3)>>3; /* Magic shade formula :) */

	rs_image8_unref(out);
	rs_image8_unref(in);
	return out;
}

RS_IMAGE16 *
rs_image16_copy(RS_IMAGE16 *in)
{
	RS_IMAGE16 *out;
	rs_image16_ref(in);
	out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);
	memcpy(out->pixels, in->pixels, in->rowstride*in->h*2);
	rs_image16_unref(in);
	return(out);
}

inline gushort
topleft(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[4].offset)) * program[0].scale)
		+ ((*(in+program[4].offset)) * program[1].scale)
		+ ((*(in+program[5].offset)) * program[2].scale)
		+ ((*(in+program[4].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[5].offset)) * program[5].scale)
		+ ((*(in+program[7].offset)) * program[6].scale)
		+ ((*(in+program[7].offset)) * program[7].scale)
		+ ((*(in+program[8].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
top(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[3].offset)) * program[0].scale)
		+ ((*(in+program[4].offset)) * program[1].scale)
		+ ((*(in+program[5].offset)) * program[2].scale)
		+ ((*(in+program[3].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[5].offset)) * program[5].scale)
		+ ((*(in+program[6].offset)) * program[6].scale)
		+ ((*(in+program[7].offset)) * program[7].scale)
		+ ((*(in+program[8].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
topright(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[3].offset)) * program[0].scale)
		+ ((*(in+program[4].offset)) * program[1].scale)
		+ ((*(in+program[4].offset)) * program[2].scale)
		+ ((*(in+program[3].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[4].offset)) * program[5].scale)
		+ ((*(in+program[6].offset)) * program[6].scale)
		+ ((*(in+program[7].offset)) * program[7].scale)
		+ ((*(in+program[7].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
left(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[1].offset)) * program[0].scale)
		+ ((*(in+program[1].offset)) * program[1].scale)
		+ ((*(in+program[2].offset)) * program[2].scale)
		+ ((*(in+program[4].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[5].offset)) * program[5].scale)
		+ ((*(in+program[7].offset)) * program[6].scale)
		+ ((*(in+program[7].offset)) * program[7].scale)
		+ ((*(in+program[8].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
right(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[0].offset)) * program[0].scale)
		+ ((*(in+program[1].offset)) * program[1].scale)
		+ ((*(in+program[1].offset)) * program[2].scale)
		+ ((*(in+program[3].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[4].offset)) * program[5].scale)
		+ ((*(in+program[6].offset)) * program[6].scale)
		+ ((*(in+program[7].offset)) * program[7].scale)
		+ ((*(in+program[7].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
bottomleft(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[1].offset)) * program[0].scale)
		+ ((*(in+program[1].offset)) * program[1].scale)
		+ ((*(in+program[2].offset)) * program[2].scale)
		+ ((*(in+program[4].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[5].offset)) * program[5].scale)
		+ ((*(in+program[4].offset)) * program[6].scale)
		+ ((*(in+program[4].offset)) * program[7].scale)
		+ ((*(in+program[5].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
bottom(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[0].offset)) * program[0].scale)
		+ ((*(in+program[1].offset)) * program[1].scale)
		+ ((*(in+program[2].offset)) * program[2].scale)
		+ ((*(in+program[3].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[5].offset)) * program[5].scale)
		+ ((*(in+program[3].offset)) * program[6].scale)
		+ ((*(in+program[4].offset)) * program[7].scale)
		+ ((*(in+program[5].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
bottomright(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[0].offset)) * program[0].scale)
		+ ((*(in+program[1].offset)) * program[1].scale)
		+ ((*(in+program[1].offset)) * program[2].scale)
		+ ((*(in+program[3].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[4].offset)) * program[5].scale)
		+ ((*(in+program[3].offset)) * program[6].scale)
		+ ((*(in+program[4].offset)) * program[7].scale)
		+ ((*(in+program[4].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

inline gushort
everything(gushort *in, struct struct_program *program, gint divisor)
{
	gint temp;
	temp = ((*(in+program[0].offset)) * program[0].scale)
		+ ((*(in+program[1].offset)) * program[1].scale)
		+ ((*(in+program[2].offset)) * program[2].scale)
		+ ((*(in+program[3].offset)) * program[3].scale)
		+ ((*(in+program[4].offset)) * program[4].scale)
		+ ((*(in+program[5].offset)) * program[5].scale)
		+ ((*(in+program[6].offset)) * program[6].scale)
		+ ((*(in+program[7].offset)) * program[7].scale)
		+ ((*(in+program[8].offset)) * program[8].scale);
	temp /= divisor;
	_CLAMP65535(temp);
	return(temp);
}

RS_IMAGE16
*rs_image16_convolve(RS_IMAGE16 *input, RS_IMAGE16 *output, RS_MATRIX3 *matrix, gfloat scaler)
{
	gint col, row, c;
	gushort *in, *out;
	gint divisor = (gint) (scaler * 256.0);
	struct struct_program *program;
	
	program = (struct struct_program *) g_new(struct struct_program, 9);
	program[0].offset = -input->rowstride-input->pixelsize;
	program[1].offset = -input->rowstride;
	program[2].offset = -input->rowstride+input->pixelsize;
	program[3].offset = -input->pixelsize;
	program[4].offset = 0;
	program[5].offset = input->pixelsize;
	program[6].offset = input->rowstride-input->pixelsize;
	program[7].offset = input->rowstride;
	program[8].offset = input->rowstride+input->pixelsize;
	program[0].scale = (gint) (matrix->coeff[0][0]*256.0);
	program[1].scale = (gint) (matrix->coeff[0][1]*256.0);
	program[2].scale = (gint) (matrix->coeff[0][2]*256.0);
	program[3].scale = (gint) (matrix->coeff[1][0]*256.0);
	program[4].scale = (gint) (matrix->coeff[1][1]*256.0);
	program[5].scale = (gint) (matrix->coeff[1][2]*256.0);
	program[6].scale = (gint) (matrix->coeff[2][0]*256.0);
	program[7].scale = (gint) (matrix->coeff[2][1]*256.0);
	program[8].scale = (gint) (matrix->coeff[2][2]*256.0);
	
	if (!output || ((output->w != input->w) || (output->h != input->h))) {
		return NULL;
	}
	
	/* top left */
	in = GET_PIXEL(input, 0, 0);
	out = GET_PIXEL(output, 0, 0);
	for (c=0;c<input->pixelsize;c++)
	{
		*out = topleft(in, program, divisor);
		in += 1;
		out += 1;
	}
	/* top */
	in = GET_PIXEL(input, 1, 0);
	out = GET_PIXEL(output, 1, 0);
	for(col=1; col<input->w-1; col++)
	{
		for (c=0;c<input->pixelsize;c++)
		{
			*out = top(in, program, divisor);
			in += 1;
			out += 1;
		}
	}
	/* top right */
	in = GET_PIXEL(input, input->w-1, 0);
	out = GET_PIXEL(output, input->w-1, 0);
	for (c=0;c<input->pixelsize;c++)
	{
		*out = topright(in, program, divisor);
		in += 1;
		out += 1;
	}
	/* left border */
	for(row=1; row<input->h-1; row++)
	{
		in = GET_PIXEL(input, 0, row);
		out = GET_PIXEL(output, 0, row);
		for (c=0;c<input->pixelsize;c++)
		{
			*out = left(in, program, divisor);
			in += 1;
			out += 1;
		}
	}
	/* right border */
	for(row=1; row<input->h-1; row++)
	{
		in = GET_PIXEL(input, input->w-1, row);
		out = GET_PIXEL(output, output->w-1, row);
		for (c=0;c<input->pixelsize;c++)
		{
			*out = right(in, program, divisor);
			in += 1;
			out += 1;
		}
	}
	/* bottom left */
	in = GET_PIXEL(input, 0, input->h-1);
	out = GET_PIXEL(output, 0, input->h-1);
	for (c=0;c<input->pixelsize;c++)
	{
		*out = bottomleft(in, program, divisor);
		in += 1;
		out += 1;
	}
	/* bottom */
	in = GET_PIXEL(input, 1, output->h-1);
	out = GET_PIXEL(output, 1, output->h-1);
	for(col=1; col<input->w-1; col++)
	{
		for (c=0;c<input->pixelsize;c++)
		{
			*out = bottom(in, program, divisor);
			in += 1;
			out += 1;
		}
	}
	/* bottom right */
	in = GET_PIXEL(input, input->w-1, input->h-1);
	out = GET_PIXEL(output, output->w-1, input->h-1);

	for (c=0;c<input->pixelsize;c++)
	{
		*out = bottomright(in, program, divisor);
		in += 1;
		out += 1;
	}
	/* everything but borders */
	for(row=1; row<input->h-1; row++)
	{
		in = GET_PIXEL(input, 1, row);
		out = GET_PIXEL(output, 1, row);
		for(col=1; col<input->w-1; col++)
		{
			for (c=0;c<input->pixelsize;c++)
			{
				gint temp;
				temp = ((*(in+program[0].offset)) * program[0].scale)
				+ ((*(in+program[1].offset)) * program[1].scale)
				+ ((*(in+program[2].offset)) * program[2].scale)
				+ ((*(in+program[3].offset)) * program[3].scale)
				+ ((*(in+program[4].offset)) * program[4].scale)
				+ ((*(in+program[5].offset)) * program[5].scale)
				+ ((*(in+program[6].offset)) * program[6].scale)
				+ ((*(in+program[7].offset)) * program[7].scale)
				+ ((*(in+program[8].offset)) * program[8].scale);
				
				temp /= divisor;
				*out = _CLAMP65535(temp);
				in += 1;
				out += 1;
			}
		}
	}
	return output;
}


/**
 * Returns a single pixel from a RS_IMAGE16
 * @param image A RS_IMAGE16
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @param extend_edges Tries to extend edges beyond image borders if TRUE
 */
inline gushort *
rs_image16_get_pixel(RS_IMAGE16 *image, gint x, gint y, gboolean extend_edges)
{
	gushort *pixel = NULL;

	if (image)
	{
		if (extend_edges)
		{
			if (x >= image->w)
				x = image->w-1;
			else if (x < 0)
				x = 0;
			if (y >= image->h)
				y = image->h-1;
			else if (y < 0)
				y = 0;
		}

		/* Return pixel if inside image */
		if ((x>=0) && (y>=0) && (x<image->w) && (y<image->h))
			pixel = &image->pixels[y*image->rowstride + x*image->pixelsize];
	}

	return pixel;
}

gboolean
rs_image16_8_cmp_size(RS_IMAGE16 *a, RS_IMAGE8 *b)
{
	gboolean ret = TRUE;
	rs_image16_ref(a);
	rs_image8_ref(b);
	if (!a || !b)
		ret = FALSE;
	if (a->w != b->w)
		ret = FALSE;
	if (a->h != b->h)
		ret = FALSE;
	rs_image16_unref(a);
	rs_image8_unref(b);
	return ret;
}

size_t
rs_image16_get_footprint(RS_IMAGE16 *image)
{
	return image->h*image->rowstride*sizeof(short) + sizeof(RS_IMAGE16);
}

RS_IMAGE16
*rs_image16_sharpen(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble amount)
{
	amount = pow(amount,2)/50;
	
	gdouble corner = (amount/1.4)*-1.0;
	gdouble regular = (amount)*-1.0;
	gdouble center = ((corner*4+regular*4)*-1.0)+1.0;
	
	RS_MATRIX3 sharpen = {	{ { corner, regular, corner },
							{ regular, center, regular },
							{ corner, regular, corner } } };
	if (!out)
		out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);

	rs_image16_convolve(in, out, &sharpen, 1);

	return out;
}

/**
 * Copies an RS_IMAGE16, making it double size in the process
 * @param in The input image
 * @param out The output image or NULL
 */
RS_IMAGE16 *
rs_image16_copy_double(RS_IMAGE16 *in, RS_IMAGE16 *out)
{
	gint row,col;
	guint64 *i, *o1, *o2;
	guint64 tmp;
	if (!in) return NULL;
	if (!out)
		out = rs_image16_new(in->w*2, in->h*2, in->channels, in->pixelsize);

	out->filters = in->filters;
	out->fourColorFilters = in->fourColorFilters;

	rs_image16_ref(in);
	rs_image16_ref(out);
	for(row=0;row<(out->h-1);row++)
	{
		i = (guint64 *) GET_PIXEL(in, 0, row/2);
		o1 = (guint64 *) GET_PIXEL(out, 0, row);
		o2 = (guint64 *) GET_PIXEL(out, 0, row+1);
		col = in->w;
		while(col--)
		{
			tmp = *i;
			*o1++ = tmp;
			*o1++ = tmp;
			*o2++ = tmp;
			*o2++ = tmp;
			i++;
		}
	}
	rs_image16_unref(in);
	rs_image16_unref(out);

	return out;
}

/*
The rest of this file is pretty much copied verbatim from dcraw/ufraw
*/

#define FORCC for (c=0; c < colors; c++)

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2
 */
#define FC(row,col) \
	(int)(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

#define BAYER(row,col) \
        image[((row) >> shrink)*iwidth + ((col) >> shrink)][FC(row,col)]
#define CLIP(x) LIM(x,0,65535)
#define LIM(x,min,max) MAX(min,MIN(x,max))

static int
fc_INDI (const unsigned filters, const int row, const int col)
{
  static const char filter[16][16] =
  { { 2,1,1,3,2,3,2,0,3,2,3,0,1,2,1,0 },
    { 0,3,0,2,0,1,3,1,0,1,1,2,0,3,3,2 },
    { 2,3,3,2,3,1,1,3,3,1,2,1,2,0,0,3 },
    { 0,1,0,1,0,2,0,2,2,0,3,0,1,3,2,1 },
    { 3,1,1,2,0,1,0,2,1,3,1,3,0,1,3,0 },
    { 2,0,0,3,3,2,3,1,2,0,2,0,3,2,2,1 },
    { 2,3,3,1,2,1,2,1,2,1,1,2,3,0,0,1 },
    { 1,0,0,2,3,0,0,3,0,3,0,3,2,1,2,3 },
    { 2,3,3,1,1,2,1,0,3,2,3,0,2,3,1,3 },
    { 1,0,2,0,3,0,3,2,0,1,1,2,0,1,0,2 },
    { 0,1,1,3,3,2,2,1,1,3,3,0,2,1,3,2 },
    { 2,3,2,0,0,1,3,0,2,0,1,2,3,0,1,0 },
    { 1,3,1,2,3,2,3,2,0,2,0,1,1,0,3,0 },
    { 0,2,0,3,1,0,0,1,1,3,3,2,3,2,2,1 },
    { 2,1,3,2,3,1,2,1,0,3,0,2,0,2,0,2 },
    { 0,3,1,0,0,2,0,3,2,1,3,1,1,3,1,3 } };

  if (filters != 1) return FC(row,col);
  /* Assume that we are handling the Leaf CatchLight with
   * top_margin = 8; left_margin = 18; */
//  return filter[(row+top_margin) & 15][(col+left_margin) & 15];
  return filter[(row+8) & 15][(col+18) & 15];
}

static void
border_interpolate_INDI (RS_IMAGE16 *image, const unsigned filters, int colors, int border)
{
  int row, col, y, x, f, c, sum[8];

  for (row=0; row < image->h; row++)
    for (col=0; col < image->w; col++) {
      if (col==border && row >= border && row < image->h-border)
	col = image->w-border;
      memset (sum, 0, sizeof sum);
      for (y=row-1; y != row+2; y++)
	for (x=col-1; x != col+2; x++)
	  if (y >= 0 && y < image->h && x >= 0 && x < image->w) {
	    f = fc_INDI(filters, y, x);
	    sum[f] += image->pixels[y*image->rowstride+x*4+f];
	    sum[f+4]++;
	  }
      f = fc_INDI(filters,row,col);
      for (c=0; c < colors; c++)
		  if (c != f && sum[c+4])
	image->pixels[row*image->rowstride+col*4+c] = sum[c] / sum[c+4];
    }
}

static void
lin_interpolate_INDI(RS_IMAGE16 *image, const unsigned filters, const int colors) /*UF*/
{
	int code[16][16][32], *ip, sum[4];
	int c, i, x, y, row, col, shift, color;
	ushort *pix;

	border_interpolate_INDI(image, filters, colors, 1);

	for (row=0; row < 16; row++)
		for (col=0; col < 16; col++)
		{
			ip = code[row][col];
			memset (sum, 0, sizeof sum);
			for (y=-1; y <= 1; y++)
				for (x=-1; x <= 1; x++)
				{
					shift = (y==0) + (x==0);
					if (shift == 2)
						continue;
					color = fc_INDI(filters,row+y,col+x);
					*ip++ = (image->pitch*y + x)*4 + color;
					*ip++ = shift;
					*ip++ = color;
					sum[color] += 1 << shift;
				}
				FORCC
					if (c != fc_INDI(filters,row,col))
					{
						*ip++ = c;
						*ip++ = sum[c];
					}
		}
	for (row=1; row < image->h-1; row++)
		for (col=1; col < image->w-1; col++)
		{
			pix = GET_PIXEL(image, col, row);
			ip = code[row & 15][col & 15];
			memset (sum, 0, sizeof sum);
			for (i=8; i--; ip+=3)
				sum[ip[2]] += pix[ip[0]] << ip[1];
			for (i=colors; --i; ip+=2)
				pix[ip[0]] = sum[ip[0]] / ip[1];
		}
}

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
#define UT(c1, c2, c3, g1, g3) \
  CLIP((long)(((g1 +g3) >> 1) +((c2-c1 +c2-c3) >> 3)))

#define UT1(v1, v2, v3, c1, c3) \
  CLIP((long)(v2 +((c1 +c3 -v1 -v3) >> 1)))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define ULIM(x,y,z) ((y) < (z) ? LIM(x,y,z) : LIM(x,z,y))

static void
ppg_interpolate_INDI(RS_IMAGE16 *image, const unsigned filters, const int colors)
{
  ushort (*pix)[4];            // Pixel matrix
  ushort g2, c1, c2, cc1, cc2; // Simulated green and color
  int    row, col, diff[2], guess[2], c, d, i;
  int    dir[5]  = { 1, image->pitch, -1, -image->pitch, 1 };
  int    g[2][4] = {{ -1 -2*image->pitch, -1 +2*image->pitch,  1 -2*image->pitch, 1 +2*image->pitch },
                    { -2 -image->pitch,    2 -image->pitch,   -2 +image->pitch,   2 +image->pitch   }};

  border_interpolate_INDI (image, filters, colors, 4);

  // Fill in the green layer with gradients from RGB color pattern simulation
  for (row=3; row < image->h-4; row++) {
    for (col=3+(FC(row,3) & 1), c=FC(row,col); col < image->w-4; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);

      // Horizontaly and verticaly
      for (i=0; d=dir[i], i < 2; i++) {

        // Simulate RGB color pattern
        guess[i] = UT (pix[-2*d][c], pix[0][c], pix[2*d][c],
                       pix[-d][1], pix[d][1]);
        g2       = UT (pix[0][c], pix[2*d][c], pix[4*d][c],
                       pix[d][1], pix[3*d][1]);
        c1       = UT1(pix[-2*d][1], pix[-d][1], guess[i],
                       pix[-2*d][c], pix[0][c]);
        c2       = UT1(guess[i], pix[d][1], g2,
                       pix[0][c], pix[2*d][c]);
        cc1      = UT (pix[g[i][0]][1], pix[-d][1], pix[g[i][1]][1],
                       pix[-1-image->pitch][2-c], pix[1-image->pitch][2-c]);
        cc2      = UT (pix[g[i][2]][1],  pix[d][1], pix[g[i][3]][1],
                       pix[-1+image->pitch][2-c], pix[1+image->pitch][2-c]);

        // Calculate gradient with RGB simulated color
        diff[i]  = ((ABS(pix[-d][1] -pix[-3*d][1]) +
                     ABS(pix[0][c]  -pix[-2*d][c]) +
                     ABS(cc1        -cc2)          +
                     ABS(pix[0][c]  -pix[2*d][c])  +
                     ABS(pix[d][1]  -pix[3*d][1])) * 2 / 3) +
                     ABS(guess[i]   -pix[-d][1])   +
                     ABS(pix[0][c]  -c1)           +
                     ABS(pix[0][c]  -c2)           +
                     ABS(guess[i]   -pix[d][1]);
      }

      // Then, select the best gradient
      d = dir[diff[0] > diff[1]];
      pix[0][1] = ULIM(guess[diff[0] > diff[1]], pix[-d][1], pix[d][1]);
    }
  }

  // Calculate red and blue for each green pixel
  for (row=1; row < image->h-1; row++)
    for (col=1+(FC(row,2) & 1), c=FC(row,col+1); col < image->w-1; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);
      for (i=0; (d=dir[i]) > 0; c=2-c, i++)
        pix[0][c] = UT1(pix[-d][1], pix[0][1], pix[d][1],
                        pix[-d][c], pix[d][c]);
    }

  // Calculate blue for red pixels and vice versa
  for (row=1; row < image->h-1; row++)
    for (col=1+(FC(row,1) & 1), c=2-FC(row,col); col < image->w-1; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);
      for (i=0; (d=dir[i]+dir[i+1]) > 0; i++) {
        diff[i]  = ABS(pix[-d][c] - pix[d][c]) +
                   ABS(pix[-d][1] - pix[d][1]);
        guess[i] = UT1(pix[-d][1], pix[0][1], pix[d][1],
                       pix[-d][c], pix[d][c]);
      }
      pix[0][c] = CLIP(guess[diff[0] > diff[1]]);
    }
}

/**
 * Demosaics a RS_IMAGE16
 * @param image The image to demosaic, this MUST be preprocessed, ie. doubled in size
 * @param demosaic The demosaic algorithm to use
 * @return FALSE if the image was not suited for demosaic, TRUE if we succeed
 */
gboolean
rs_image16_demosaic(RS_IMAGE16 *image, RS_DEMOSAIC demosaic)
{
	gint row,col;
	gushort *pixel;
	guint filters;

	/* Do some sanity checks on input */
	if (!image)
		return FALSE;
	if (image->filters == 0)
		return FALSE;
	if (image->fourColorFilters == 0)
		return FALSE;
	if (image->channels != 4)
		return FALSE;

	rs_image16_ref(image);

	/* Magic - Ask Dave ;) */
	filters = image->filters;
	filters &= ~((filters & 0x55555555) << 1);

	/* Populate new image with bayer data */
	for(row=0; row<image->h; row++)
	{
		pixel = GET_PIXEL(image, 0, row);
		for(col=0;col<image->w;col++)
		{
			pixel[fc_INDI(filters, row, col)] = pixel[fc_INDI(image->fourColorFilters, row, col)];
			pixel += image->pixelsize;
		}
	}

	/* Switch ourself to three channels after populating */
	image->channels = 3;

	/* Do the actual demosaic */
	switch (demosaic)
	{
		case RS_DEMOSAIC_BILINEAR:
			lin_interpolate_INDI(image, filters, 3);
			break;
		default:
		case RS_DEMOSAIC_PPG:
			ppg_interpolate_INDI(image, filters, 3);
			break;
	}

	image->filters = 0;
	image->fourColorFilters = 0;
	image->preview = FALSE;

	rs_image16_unref(image);
	g_signal_emit(image, signals[PIXELDATA_CHANGED], 0, NULL);
	return TRUE;
}

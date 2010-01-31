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

#ifdef WIN32 /* Win32 _aligned_malloc */
#include <malloc.h>
#include <stdio.h>
#endif

#include <rawstudio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#define _ISOC9X_SOURCE 1 /* lrint() */
#define _ISOC99_SOURCE 1
#define	__USE_ISOC9X 1
#define	__USE_ISOC99 1
#include <math.h> /* floor() */
#include "rs-math.h"
#include "rs-image16.h"

#define PITCH(width) ((((width)+15)/16)*16)
#define SWAP( a, b ) a ^= b ^= a ^= b

static void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
static void rs_image16_mirror(RS_IMAGE16 *rsi);
static void rs_image16_flip(RS_IMAGE16 *rsi);
inline static void rs_image16_nearest(RS_IMAGE16 *in, gushort *out, gint x, gint y);
inline static void rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gint x, gint y);
inline static void rs_image16_bicubic(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);

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

	if (self->pixels && (self->pixels_refcount == 1))
		free(self->pixels);

	self->pixels_refcount--;

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
	self->pixels_refcount = 0;
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
#ifdef WIN32
			_aligned_free(rsi->pixels);
#else
			g_free(rsi->pixels);
#endif
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
#ifdef WIN32
			_aligned_free(rsi->pixels);
#else
			g_free(rsi->pixels);
#endif
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
	g_signal_emit(rsi, signals[PIXELDATA_CHANGED], 0, NULL);
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

	g_signal_emit(rsi, signals[PIXELDATA_CHANGED], 0, NULL);
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

	g_signal_emit(rsi, signals[PIXELDATA_CHANGED], 0, NULL);
	rs_image16_unref(rsi);

	return;
}

static void inline
rs_image16_preview(RS_IMAGE16 *in, gushort *out, gint x, gint y)
{
	const gint nx = ((x>>11)<<3)+4;
	const gint ny = ((y>>11)<<3)+4;

	if (unlikely((nx>=(in->w))||(ny>=(in->h))))
		return;
	else if (unlikely(nx<0) || unlikely(ny<0))
		return;
	
	const gushort *a = GET_PIXEL(in, nx, ny);

	out[R] = a[R];
	out[G] = a[G];
	out[B] = a[B];
}

static void inline
rs_image16_nearest(RS_IMAGE16 *in, gushort *out, gint x, gint y)
{
	const gint nx = x>>8;
	const gint ny = y>>8;
	const gushort *a = &in->pixels[ny*in->rowstride + nx*in->pixelsize];
	out[R] = a[R];
	out[G] = a[G];
	out[B] = a[B];
	out[G2] = a[G2];
}

static void inline
rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gint x, gint y)
{
	gint fx = x>>8;
	gint fy = y>>8;
	gint nextx;
	gint nexty;
	
	gushort *a, *b, *c, *d;
	gint diffx, diffy, inv_diffx, inv_diffy;
	gint aw, bw, cw, dw;

	if (unlikely((fx>(in->w-1))||(fy>(in->h-1))))
		return;
	else if (unlikely(fx<0))
	{
		if (likely(fx<-1))
			return;
		else
		{
			fx = 0;
			x = 0;
		}
	}

	if (unlikely(fy<0))
	{
		if (likely(fy<-1))
			return;
		else
		{
			fy = 0;
			y = 0;
		}
	}
	
	if (fx < in->w-1)
		nextx = in->pixelsize;
	else
		nextx = 0;
	
	if (fy < in->h-1)
		nexty = in->rowstride;
	else
		nexty = 0;
	
	fx *= in->pixelsize;
	fy *= in->rowstride;
	
	/* find four cornerpixels */
	a = &in->pixels[fy + fx];
	b = a + nextx;
	c = &in->pixels[fy+nexty + fx];
	d = c + nextx;

	/* calculate weightings */
	diffx = x&0xff; /* x distance from a */
	diffy = y&0xff; /* y distance fromy a */
	inv_diffx = 256 - diffx; /* inverse x distance from a */
	inv_diffy = 256 - diffy; /* inverse y distance from a */
	
	aw = (inv_diffx * inv_diffy) >> 1;  /* Weight is now 0.15 fp */
	bw = (diffx * inv_diffy) >> 1;
	cw = (inv_diffx * diffy) >> 1;
	dw = (diffx * diffy) >> 1;

	out[R]  = (gushort) ((a[R]*aw  + b[R]*bw  + c[R]*cw  + d[R]*dw + 16384) >> 15 );
	out[G]  = (gushort) ((a[G]*aw  + b[G]*bw  + c[G]*cw  + d[G]*dw + 16384) >> 15 );
	out[B]  = (gushort) ((a[B]*aw  + b[B]*bw  + c[B]*cw  + d[B]*dw + 16384) >> 15 );
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
	gint fixed_x, fixed_y;

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
			fixed_x = (int)(256.0f * x);
			fixed_y = (int)(256.0f * y);
			if (in->preview)
				rs_image16_preview(in, &out->pixels[destoffset], fixed_x, fixed_y);
			else
				rs_image16_bilinear(in, &out->pixels[destoffset], fixed_x, fixed_y);
		}
	}

	g_signal_emit(out, signals[PIXELDATA_CHANGED], 0, NULL);
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
		out = rs_image16_copy(in, TRUE);
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
	g_signal_emit(out, signals[PIXELDATA_CHANGED], 0, NULL);
	rs_image16_unref(in);
	rs_image16_unref(out);
	return(out);
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize)
{
	gint ret;
	RS_IMAGE16 *rsi;

	g_assert(width < 65536);
	g_assert(height < 65536);

	g_assert(width > 0);
	g_assert(height > 0);

	g_assert(channels > 0);
	g_assert(pixelsize >= channels);

	rsi = g_object_new(RS_TYPE_IMAGE16, NULL);
	rsi->w = width;
	rsi->h = height;
	rsi->rowstride = PITCH(width * pixelsize);
	rsi->pitch = rsi->rowstride / pixelsize;
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	rsi->filters = 0;

	/* Allocate actual pixels */
#ifdef WIN32
	rsi->pixels = _aligned_malloc(rsi->h*rsi->rowstride * sizeof(gushort), 16); 
	if (rsi->pixels == NULL)
#else
	ret = posix_memalign((void **) &rsi->pixels, 16, rsi->h*rsi->rowstride * sizeof(gushort));
	if (ret > 0)
#endif
	{
		rsi->pixels = NULL;
		g_object_unref(rsi);
		return NULL;
	}
	rsi->pixels_refcount = 1;

	/* Verify alignment */
	g_assert((GPOINTER_TO_INT(rsi->pixels) % 16) == 0);
	g_assert((rsi->rowstride % 16) == 0);

	return(rsi);
}

/**
 * Initializes a new RS_IMAGE16 with pixeldata from @input.
 * @note Pixeldata is NOT copied to new RS_IMAGE16.
 * @param input A RS_IMAGE16
 * @param rectangle A GdkRectangle describing the area to subframe
 * @return A new RS_IMAGE16 with a refcount of 1, the image can be bigger
 *         than rectangle to retain 16 byte alignment.
 */
RS_IMAGE16 *
rs_image16_new_subframe(RS_IMAGE16 *input, GdkRectangle *rectangle)
{
	RS_IMAGE16 *output;
	gint width, height;
	gint x, y;

	g_assert(RS_IS_IMAGE16(input));
	g_assert(rectangle->x >= 0);
	g_assert(rectangle->y >= 0);
	g_assert(rectangle->width > 0);
	g_assert(rectangle->height > 0);

	g_assert(rectangle->width <= input->w);
	g_assert(rectangle->height <= input->h);

	g_assert((rectangle->width + rectangle->x) <= input->w);
	g_assert((rectangle->height + rectangle->y) <= input->h);

	output = g_object_new(RS_TYPE_IMAGE16, NULL);

	/* Align x to 16 byte boundary */
	x = rectangle->x - (rectangle->x & 0x3);
	x = CLAMP(x, 0, input->w-1);

	y = CLAMP(rectangle->y, 0, input->h-1);

	width = CLAMP(rectangle->width + (rectangle->x & 0x3), 1, input->w - x);
	height = CLAMP(rectangle->height, 1, input->h - y);

	output->w = width;
	output->h = height;
	output->rowstride = input->rowstride;
	output->pitch = input->pitch;
	output->channels = input->channels;
	output->pixelsize = input->pixelsize;
	output->filters = input->filters;

	output->pixels = GET_PIXEL(input, x, y);
	output->pixels_refcount = input->pixels_refcount + 1;

	/* Some sanity checks */
	g_assert(output->w <= input->w);
	g_assert(output->h <= input->h);

	g_assert(output->w > 0);
	g_assert(output->h > 0);

	g_assert(output->w >= rectangle->width);
	g_assert(output->h >= rectangle->height);

	g_assert((output->w - 4) <= rectangle->width);

	/* Verify alignment */
	g_assert((GPOINTER_TO_INT(output->pixels) % 16) == 0);
	g_assert((output->rowstride % 16) == 0);

	return output;
}

/**
 * Renders an exposure map on top of an GdkPixbuf with 3 channels
 * @param pixbuf A GdkPixbuf
 * @param only_row A single row to render or -1 to render all
 */
void
gdk_pixbuf_render_exposure_mask(GdkPixbuf *pixbuf, gint only_row)
{
	gint row, col;
	gint start;
	gint stop;
	gint height;
	gint width;

	g_assert(GDK_IS_PIXBUF(pixbuf));
	g_assert(gdk_pixbuf_get_n_channels(pixbuf) == 3);

	g_object_ref(pixbuf);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

	if ((only_row > -1) && (only_row < height))
	{
		start = only_row;
		stop = only_row + 1;
	}
	else
	{
		start = 0;
		stop = height;
	}

	for(row=start;row<stop;row++)
	{
		/* Get start pixel of row */
		guchar *pixel = GET_PIXBUF_PIXEL(pixbuf, 0, row);

		for(col=0;col<width;col++)
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
	g_object_unref(pixbuf);
}

RS_IMAGE16 *
rs_image16_copy(RS_IMAGE16 *in, gboolean copy_pixels)
{
	RS_IMAGE16 *out;
	rs_image16_ref(in);
	out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);
	if (copy_pixels)
		memcpy(out->pixels, in->pixels, in->rowstride*in->h*2);
	rs_image16_unref(in);
	return(out);
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

size_t
rs_image16_get_footprint(RS_IMAGE16 *image)
{
	return image->h*image->rowstride*sizeof(short) + sizeof(RS_IMAGE16);
}

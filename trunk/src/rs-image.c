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

#define PITCH(width) ((((width)+15)/16)*16)
#define SWAP( a, b ) a ^= b ^= a ^= b

struct struct_program {
	gint divisor;
	gint scale[9];
};

static void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
static void rs_image16_mirror(RS_IMAGE16 *rsi);
static void rs_image16_flip(RS_IMAGE16 *rsi);
inline static void rs_image16_nearest(RS_IMAGE16 *in, gushort *out, gint x, gint y);
inline static void rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gint x, gint y);
inline static void rs_image16_bicubic(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);
inline gushort topright(gushort *in, struct struct_program *program, gint divisor);
inline gushort top(gushort *in, struct struct_program *program, gint divisor);
inline gushort topleft(gushort *in, struct struct_program *program, gint divisor);
inline gushort right(gushort *in, struct struct_program *program, gint divisor);
inline gushort left(gushort *in, struct struct_program *program, gint divisor);
inline gushort bottomright(gushort *in, struct struct_program *program, gint divisor);
inline gushort bottom(gushort *in, struct struct_program *program, gint divisor);
inline gushort bottomleft(gushort *in, struct struct_program *program, gint divisor);
static void rs_image16_open_dcraw_apply_black_and_shift_half_size(dcraw_data *raw, RS_IMAGE16 *image);

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
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * pixelsize;
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	rsi->pixels = g_new0(gushort, rsi->h*rsi->rowstride);
	return(rsi);
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

void
convolve_line(RS_IMAGE16 *input, RS_IMAGE16 *output, guint line, struct struct_program *program)
{
	gint size;
	gint col;
	gint accu;
	gushort *line0, *line1, *line2, *dest;

	g_assert(line >= 0);
	g_assert(line < input->h);

	line0 = GET_PIXEL(input, 0, line-1);
	line1 = GET_PIXEL(input, 0, line);
	line2 = GET_PIXEL(input, 0, line+1);
	dest = GET_PIXEL(output, 0, line);

	/* special case for first line */
	if (line == 0)
		line0 = line1;

	/* special case for last line */
	else if (line == (input->h-1))
		line2 = line1;

	/* special case for first pixel */
	for (col = 0; col < input->pixelsize; col++)
	{
		accu
			= program->scale[0] * *line0
			+ program->scale[1] * *line0
			+ program->scale[2] * *(line0+input->pixelsize)
			+ program->scale[3] * *line1
			+ program->scale[4] * *line1
			+ program->scale[5] * *(line1+input->pixelsize)
			+ program->scale[6] * *line2
			+ program->scale[7] * *line2
			+ program->scale[8] * *(line2+input->pixelsize);
		accu /= program->divisor;
		_CLAMP65535(accu);
		*dest = accu;
		line0++; line1++; line2++; dest++;
	}

	size = (input->w-1)*input->pixelsize;

	for(col = input->pixelsize; col < size; col++)
	{
		accu
			= program->scale[0] * *(line0-input->pixelsize)
			+ program->scale[1] * *line0
			+ program->scale[2] * *(line0+input->pixelsize)
			+ program->scale[3] * *(line1-input->pixelsize)
			+ program->scale[4] * *line1
			+ program->scale[5] * *(line1+input->pixelsize)
			+ program->scale[6] * *(line2-input->pixelsize)
			+ program->scale[7] * *line2
			+ program->scale[8] * *(line2+input->pixelsize);
		accu /= program->divisor;
		_CLAMP65535(accu);
		*dest = accu;
		line0++; line1++; line2++; dest++;
	}

	/* special case for last pixel */
	for (col = size; col < input->w*input->pixelsize; col++)
	{
		accu
			= program->scale[0] * *(line0-input->pixelsize)
			+ program->scale[1] * *line0
			+ program->scale[2] * *line0
			+ program->scale[3] * *(line1-input->pixelsize)
			+ program->scale[4] * *line1
			+ program->scale[5] * *line1
			+ program->scale[6] * *(line2-input->pixelsize)
			+ program->scale[7] * *line2
			+ program->scale[8] * *line2;
		accu /= program->divisor;
		_CLAMP65535(accu);
		*dest = accu;
		line0++; line1++; line2++; dest++;
	}
}

/**
 * Concolve a RS_IMAGE16 using a 3x3 kernel
 * @param input The input image
 * @param output The output image
 * @param matrix A 3x3 convolution kernel
 * @param scaler The result will be scaled like this: convolve/scaler
 * @return output image for convenience
 */
RS_IMAGE16 *
rs_image16_convolve(RS_IMAGE16 *input, RS_IMAGE16 *output, RS_MATRIX3 *matrix, gfloat scaler, gboolean *abort)
{
	gint row;
	struct struct_program *program;

	g_assert(RS_IS_IMAGE16(input));
	g_assert(RS_IS_IMAGE16(output));
	g_assert(((output->w == input->w) && (output->h == input->h)));

	rs_image16_ref(input);
	rs_image16_ref(output);

	/* Make the integer based convolve program */
	program = (struct struct_program *) g_new(struct struct_program, 1);
	program->scale[0] = (gint) (matrix->coeff[0][0]*256.0);
	program->scale[1] = (gint) (matrix->coeff[0][1]*256.0);
	program->scale[2] = (gint) (matrix->coeff[0][2]*256.0);
	program->scale[3] = (gint) (matrix->coeff[1][0]*256.0);
	program->scale[4] = (gint) (matrix->coeff[1][1]*256.0);
	program->scale[5] = (gint) (matrix->coeff[1][2]*256.0);
	program->scale[6] = (gint) (matrix->coeff[2][0]*256.0);
	program->scale[7] = (gint) (matrix->coeff[2][1]*256.0);
	program->scale[8] = (gint) (matrix->coeff[2][2]*256.0);
	program->divisor = (gint) (scaler * 256.0);

	for(row = 0; row < input->h; row++)
	{
		convolve_line(input, output, row, program);
		if (abort && *abort) goto abort;
	}

	g_signal_emit(output, signals[PIXELDATA_CHANGED], 0, NULL);

abort:
	g_free(program);
	rs_image16_unref(input);
	rs_image16_unref(output);

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

size_t
rs_image16_get_footprint(RS_IMAGE16 *image)
{
	return image->h*image->rowstride*sizeof(short) + sizeof(RS_IMAGE16);
}

RS_IMAGE16
*rs_image16_sharpen(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble amount, gboolean *abort)
{
	amount = amount/5;
	
	gdouble corner = (amount/1.4)*-1.0;
	gdouble regular = (amount)*-1.0;
	gdouble center = ((corner*4+regular*4)*-1.0)+1.0;
	
	RS_MATRIX3 sharpen = {	{ { corner, regular, corner },
							{ regular, center, regular },
							{ corner, regular, corner } } };
	if (!out)
		out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);

	rs_image16_convolve(in, out, &sharpen, 1, abort);

	return out;
}

/**
 * Copies an RS_IMAGE16, making it double size in the process
 * @param in The input image
 * @param out The output image or NULL
 */
RS_IMAGE16 *
(*rs_image16_copy_double)(RS_IMAGE16 *in, RS_IMAGE16 *out); /* Initialized by arch binder */

RS_IMAGE16 *
rs_image16_copy_double_c(RS_IMAGE16 *in, RS_IMAGE16 *out)
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
	g_signal_emit(out, signals[PIXELDATA_CHANGED], 0, NULL);
	rs_image16_unref(in);
	rs_image16_unref(out);

	return out;
}

#if defined (__i386__) || defined (__x86_64__)
RS_IMAGE16 *
rs_image16_copy_double_mmx(RS_IMAGE16 *in, RS_IMAGE16 *out)
{
	gint row;
	void *i, *o1, *o2;
	if (!in) return NULL;
	if (!out)
		out = rs_image16_new(in->w*2, in->h*2, in->channels, in->pixelsize);

	out->filters = in->filters;
	out->fourColorFilters = in->fourColorFilters;

	rs_image16_ref(in);
	rs_image16_ref(out);
	for(row=0;row<(out->h-1);row++)
	{
		i = (void *) GET_PIXEL(in, 0, row/2);
		o1 = (void *) GET_PIXEL(out, 0, row);
		o2 = (void *) GET_PIXEL(out, 0, row+1);
		asm volatile (
			"mov %3, %%"REG_a"\n\t" /* copy col to %eax */

			".p2align 4,,15\n"
			"rs_image16_copy_double_mmx_inner_loop:\n\t"
			"movq (%0), %%mm0\n\t" /* load source */
			"movq 8(%0), %%mm1\n\t"
			"movq 16(%0), %%mm2\n\t"
			"movq 24(%0), %%mm3\n\t"
			"movq %%mm0, (%1)\n\t" /* write destination (twice) */
			"movq %%mm0, 8(%1)\n\t"
			"movq %%mm1, 16(%1)\n\t"
			"movq %%mm1, 24(%1)\n\t"
			"movq %%mm2, 32(%1)\n\t"
			"movq %%mm2, 40(%1)\n\t"
			"movq %%mm3, 48(%1)\n\t"
			"movq %%mm3, 56(%1)\n\t"
			"movq %%mm0, (%2)\n\t"
			"movq %%mm0, 8(%2)\n\t"
			"movq %%mm1, 16(%2)\n\t"
			"movq %%mm1, 24(%2)\n\t"
			"movq %%mm2, 32(%2)\n\t"
			"movq %%mm2, 40(%2)\n\t"
			"movq %%mm3, 48(%2)\n\t"
			"movq %%mm3, 56(%2)\n\t"
			"sub $4, %%"REG_a"\n\t"
			"add $32, %0\n\t"
			"add $64, %1\n\t"
			"add $64, %2\n\t"
			"cmp $3, %%"REG_a"\n\t"
			"jg rs_image16_copy_double_mmx_inner_loop\n\t"
			"cmp $1, %%"REG_a"\n\t"
			"jb rs_image16_copy_double_mmx_inner_done\n\t"

			"rs_image16_copy_double_mmx_leftover:\n\t"
			"movq (%0), %%mm0\n\t" /* leftover pixels */
			"movq %%mm0, (%1)\n\t"
			"movq %%mm0, 8(%1)\n\t"
			"movq %%mm0, (%2)\n\t"
			"movq %%mm0, 8(%2)\n\t"
			"sub $1, %%"REG_a"\n\t"
			"add $32, %0\n\t"
			"add $64, %1\n\t"
			"add $64, %2\n\t"
			"cmp $0, %%"REG_a"\n\t"
			"jg rs_image16_copy_double_mmx_leftover\n\t"

			"rs_image16_copy_double_mmx_inner_done:\n\t"
			"emms\n\t" /* clean up */
			: "+r" (i), "+r" (o1), "+r" (o2)
			: "r" ((gulong)in->w)
			: "%"REG_a
			);
	}
	g_signal_emit(out, signals[PIXELDATA_CHANGED], 0, NULL);
	rs_image16_unref(in);
	rs_image16_unref(out);

	return out;
}
#endif /* defined (__i386__) || defined (__x86_64__) */

/**
 * Open an image using the dcraw-engine
 * @param filename The filename to open
 * @param half_size Open in half size - without NN-demosaic
 * @return The newly created RS_IMAGE16 or NULL on error
 */
RS_IMAGE16 *
rs_image16_open_dcraw(const gchar *filename, gboolean half_size)
{
	dcraw_data *raw;
	RS_IMAGE16 *image = NULL;

	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);

		if (half_size)
		{
			image = rs_image16_new(raw->raw.width, raw->raw.height, raw->raw.colors, 4);
			rs_image16_open_dcraw_apply_black_and_shift_half_size(raw, image);
		}
		else
		{
			image = rs_image16_new(raw->raw.width*2, raw->raw.height*2, raw->raw.colors, 4);
			rs_image16_open_dcraw_apply_black_and_shift(raw, image);
		}

		image->filters = raw->filters;
		image->fourColorFilters = raw->fourColorFilters;
		dcraw_close(raw);
	}
	else
	{
		/* Try to fall back to GDK loader for TIFF-files */
		gchar *ifilename = g_ascii_strdown(filename, -1);
		if (g_str_has_suffix(ifilename, ".tif"))
			image = rs_image16_open_gdk(filename, half_size);
		g_free(ifilename);
	}
	g_free(raw);
	return(image);
}

static void
rs_image16_open_dcraw_apply_black_and_shift_half_size(dcraw_data *raw, RS_IMAGE16 *image)
{
	gushort *dst, *src;
	gint row, col;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	for(row=0;row<(raw->raw.height);row++)
	{
		src = (gushort *) raw->raw.image + row * raw->raw.width * 4;
		dst = GET_PIXEL(image, 0, row);
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
(*rs_image16_open_dcraw_apply_black_and_shift)(dcraw_data *raw, RS_IMAGE16 *image);

void
rs_image16_open_dcraw_apply_black_and_shift_c(dcraw_data *raw, RS_IMAGE16 *image)
{
	gushort *dst1, *dst2, *src;
	gint row, col;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	for(row=0;row<(raw->raw.height*2);row+=2)
	{
		src = (gushort *) raw->raw.image + row/2 * raw->raw.width * 4;
		dst1 = GET_PIXEL(image, 0, row);
		dst2 = GET_PIXEL(image, 0, row+1);
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
rs_image16_open_dcraw_apply_black_and_shift_mmx(dcraw_data *raw, RS_IMAGE16 *image)
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
		destoffset = (void*) (image->pixels + y*image->rowstride);
		srcoffset = (void*) (src + y/2 * raw->raw.width * image->pixelsize);
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
			"jz load_raw_inner_done\n\t"
			".p2align 4,,15\n"
			"load_raw_leftover:\n\t"
			"movq (%1), %%mm0\n\t" /* leftover pixels */
			"psubusw %%mm7, %%mm0\n\t"
			"psllw %%mm6, %%mm0\n\t"
			"movq %%mm0, (%0)\n\t"
			"movq %%mm0, 8(%0)\n\t"
			"add $16, %0\n\t"
			"add $8, %1\n\t"
			"dec %%"REG_a"\n\t"
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
 * Open an image using the GDK-engine
 * @param filename The filename to open
 * @param half_size Does nothing
 * @return The newly created RS_IMAGE16 or NULL on error
 */
RS_IMAGE16 *
rs_image16_open_gdk(const gchar *filename, gboolean half_size)
{
	RS_IMAGE16 *image = NULL;
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
		image = rs_image16_new(width, height, 3, 4);
		for(row=0;row<image->h;row++)
		{
			dest = row * image->rowstride;
			src = row * rowstride;
			for(col=0;col<image->w;col++)
			{
				image->pixels[dest++] = gammatable[pixels[src++]];
				image->pixels[dest++] = gammatable[pixels[src++]];
				image->pixels[dest++] = gammatable[pixels[src++]];
				image->pixels[dest++] = gammatable[pixels[src-2]];
				src+=alpha;
			}
		}
		g_object_unref(pixbuf);
	}
	return(image);
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

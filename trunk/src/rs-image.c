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
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "rs-image.h"

static void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
static void rs_image16_mirror(RS_IMAGE16 *rsi);
static void rs_image16_flip(RS_IMAGE16 *rsi);
inline static void rs_image16_nearest(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);
inline static void rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gdouble x, gdouble y);

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
	return;
}

static void
rs_image16_mirror(RS_IMAGE16 *rsi)
{
	gint row,col;
	gint offset,destoffset;

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
}

static void
rs_image16_flip(RS_IMAGE16 *rsi)
{
	gint row;
	const gint linel = rsi->rowstride*sizeof(gushort);
	gushort *tmp = (gushort *) g_malloc(linel);

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
	return;
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
			fx = 0;
			x = 0.0;
	}

	if (unlikely(fy<0))
	{
		if (likely(fy<-1))
			return;
		else
			fy = 0;
			y = 0.0;
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
		*w = abs(crop->x2 - crop->x1);
		*h = abs(crop->y2 - crop->y1);
		/* translate non-cropped area into negative x,y*/
		matrix3_affine_translate(&mat, ((gdouble) -crop->x1), ((gdouble) -crop->y1));
	}

	return;
}

RS_IMAGE16 *
rs_image16_transform(RS_IMAGE16 *in, RS_IMAGE16 *out, RS_MATRIX3 *affine, RS_MATRIX3 *inverse_affine,
	RS_RECT *crop, gint width, gint height, gboolean keep_aspect, gdouble scale, gdouble angle, gint orientation)
{
	RS_MATRIX3 mat;
	gdouble xscale=1.0, yscale=1.0;
	gdouble x, y;
	gdouble minx, miny;
	gdouble maxx, maxy;
	gdouble w, h;
	gint row, col;
	gint destoffset;

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

			rs_image16_bilinear(in, &out->pixels[destoffset], x, y);
		}
	}
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
		out = rs_image16_copy(in);
		return(out);
	}

	scale = 1 / scale;

	if (out==NULL)
		out = rs_image16_new((int)(in->w/scale), (int)(in->h/scale), in->channels, in->pixelsize);
	else
		g_assert(out->w == (int)(in->w/scale));

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
	return(out);
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize)
{
	RS_IMAGE16 *rsi;
	rsi = (RS_IMAGE16 *) g_malloc(sizeof(RS_IMAGE16));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * pixelsize;
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	ORIENTATION_RESET(rsi->orientation);
	rsi->pixels = (gushort *) g_malloc(sizeof(gushort)*rsi->h*rsi->rowstride);
	return(rsi);
}

void
rs_image16_free(RS_IMAGE16 *rsi)
{
	if (rsi!=NULL)
	{
		g_assert(rsi->pixels!=NULL);
		g_free(rsi->pixels);
		g_assert(rsi!=NULL);
		g_free(rsi);
	}
	return;
}

RS_IMAGE8 *
rs_image8_new(const guint width, const guint height, const guint channels, const guint pixelsize)
{
	RS_IMAGE8 *rsi;
	GdkVisual *vis;

	rsi = (RS_IMAGE8 *) g_malloc(sizeof(RS_IMAGE8));
	rsi->image = NULL;
	rsi->w = width;
	rsi->h = height;
	ORIENTATION_RESET(rsi->orientation);
	if ((channels==3) && (pixelsize==4))
	{
		vis = gdk_visual_get_system();
		rsi->image = gdk_image_new(GDK_IMAGE_FASTEST, vis, width, height);
		rsi->pixels = rsi->image->mem;
		rsi->rowstride = rsi->image->bpl;
		rsi->channels = channels;
		rsi->pixelsize = rsi->image->bpp;
	}
	else
	{
		rsi->image = NULL;
		rsi->rowstride = PITCH(width) * pixelsize;
		rsi->pixels = (guchar *) g_malloc(sizeof(guchar)*rsi->h*rsi->rowstride);
		rsi->channels = channels;
		rsi->pixelsize = pixelsize;
	}
	return(rsi);
}

void
rs_image8_free(RS_IMAGE8 *rsi)
{
	if (rsi!=NULL)
	{
		if (rsi->image)
			g_object_unref(rsi->image);
		else
			g_free(rsi->pixels);
		g_free(rsi);
	}
	return;
}

RS_IMAGE16 *
rs_image16_copy(RS_IMAGE16 *in)
{
	RS_IMAGE16 *out;
	out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);
	memcpy(out->pixels, in->pixels, in->rowstride*in->h*2);
	return(out);
}

gboolean
rs_image16_8_cmp_size(RS_IMAGE16 *a, RS_IMAGE8 *b)
{
	if (!a || !b)
		return(FALSE);
	if (a->w != b->w)
		return(FALSE);
	if (a->h != b->h)
		return(FALSE);
	return(TRUE);
}

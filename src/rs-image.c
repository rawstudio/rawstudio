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
#include <string.h>
#include <math.h> /* floor() */
#include "color.h"
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "rs-image.h"

void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
void rs_image16_mirror(RS_IMAGE16 *rsi);
void rs_image16_flip(RS_IMAGE16 *rsi);

void
rs_image16_orientation(RS_IMAGE16 *rsi, const gint orientation)
{
	const gint rot = ((orientation&3)-(rsi->orientation&3)+8)%4;

	rs_image16_rotate(rsi, rot);
	if (((rsi->orientation)&4)^((orientation)&4))
		rs_image16_flip(rsi);

	return;
}

void
rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns)
{
	gint width, height, pitch;
	gint y,x;
	gint offset,destoffset;
	gushort *swap;

	quarterturns %= 4;

	g_assert(rsi->pixelsize==4);

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
					swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=4;
				}
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			rsi->rowstride = pitch * 4;
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
				destoffset = ((height-1)*pitch + y)*4;
				for(x=0; x<rsi->w; x++)
				{
					swap[destoffset+R] = rsi->pixels[offset+R];
					swap[destoffset+G] = rsi->pixels[offset+G];
					swap[destoffset+B] = rsi->pixels[offset+B];
					swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=4;
					destoffset -= pitch*4;
				}
				offset += rsi->pitch*4;
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			rsi->rowstride = pitch * 4;
			ORIENTATION_270(rsi->orientation);
			break;
		default:
			break;
	}
	return;
}

void
rs_image16_mirror(RS_IMAGE16 *rsi)
{
	gint row,col;
	gint offset,destoffset;
	g_assert(rsi->pixelsize==4);
	for(row=0;row<rsi->h;row++)
	{
		offset = row*rsi->rowstride;
		destoffset = (row*rsi->pitch+rsi->w-1)*4;
		for(col=0;col<rsi->w/2;col++)
		{
			SWAP(rsi->pixels[offset+R], rsi->pixels[destoffset+R]);
			SWAP(rsi->pixels[offset+G], rsi->pixels[destoffset+G]);
			SWAP(rsi->pixels[offset+B], rsi->pixels[destoffset+B]);
			SWAP(rsi->pixels[offset+G2], rsi->pixels[destoffset+G2]);
			offset+=4;
			destoffset-=4;
		}
	}
	ORIENTATION_MIRROR(rsi->orientation);
}

void
rs_image16_flip(RS_IMAGE16 *rsi)
{
	gint row;
	const gint linel = rsi->rowstride*sizeof(gushort);
	gushort *tmp = (gushort *) g_malloc(linel);

	g_assert(rsi->pixelsize==4);
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

inline gushort 
nearestnabor_interpolation_function ( gint x, gint y, gdouble scale, RS_IMAGE16 *in, RS_IMAGE16 *out, gint col_offset ){

	return in->pixels[(int)(x*scale)*4+(int)(y*scale)*in->rowstride+col_offset];
}	

inline gushort 
/*bilinear_*/interpolation_function ( gint x, gint y, gdouble scale, RS_IMAGE16 *in, RS_IMAGE16 *out, gint col_offset ){
	
	gint x1, x2, y1, y2;
	gdouble diffx, diffy;
	
	diffx = x * scale - floor(x*scale);
	diffy = y * scale - floor(y*scale);
	
	x1 = x * scale;
	y1 = y * scale;
	x2 = x1 + 1;
	y2 = y1 + 1;


	if( x2 > out->w ) x2 = out->w;
	if( y2 > out->h ) y2 = out->h;

	gushort col11 = in->pixels[x1*4+y1*in->rowstride+col_offset];
	gushort col12 = in->pixels[x1*4+y2*in->rowstride+col_offset];
	gushort col21 = in->pixels[x2*4+y1*in->rowstride+col_offset];
	gushort col22 = in->pixels[x2*4+y2*in->rowstride+col_offset];

	return (gushort)(col11*((1.0 - diffx)*(1.0 - diffy)) + col21*(diffx*(1.0 - diffy)) + col12*((1.0 - diffx)*diffy) + col22*(diffx*diffy));
}

inline gushort 
primitive_interpolation_function ( gint x, gint y, gdouble scale, RS_IMAGE16 *in, RS_IMAGE16 *out, gint col_offset ){
	
	gint x1, x2, y1, y2;
	
	x1 = x * scale;
	y1 = y * scale;
	x2 = x1 + 1;
	y2 = y1 + 1;

	if( x2 > out->w ) x2 = out->w;
	if( y2 > out->h ) y2 = out->h;

	gushort col11 = in->pixels[x1*4+y1*in->rowstride+col_offset];
	if ( x2-x1 == 0 ) return col11;
	if ( y2-y1 == 0 ) return col11;
	gushort col12 = in->pixels[x1*4+y2*in->rowstride+col_offset];
	gushort col21 = in->pixels[x2*4+y1*in->rowstride+col_offset];
	gushort col22 = in->pixels[x2*4+y2*in->rowstride+col_offset];

	gushort col1 = (col11 + (col12 - col11)/(x2-x1));
	gushort col2 = (col21 + (col22 - col21)/(x2-x1));
	return col1 + (col2 - col1)/(y2 - y1) ;
}


RS_IMAGE16 *
rs_image16_scale_double(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale)
{
	gint x,y;
	gint destoffset, srcoffset;
	
	scale = 1 / scale;

	g_assert(in->pixelsize==4);

	if (out==NULL)
		out = rs_image16_new((int)(in->w/scale), (int)(in->h/scale), in->channels, 4);
	else
	{
		g_assert(out->w == (int)(in->w/scale));
		g_assert(out->pixelsize==4);
	}

	if ( scale >= 1.0 ){ // Cheap downscale
		for(y=0; y!=out->h; y++)
		{
			destoffset = y*out->rowstride;
			for(x=0; x!=out->w; x++)
			{
				srcoffset = (int)(x*scale)*4+(int)(y*scale)*in->rowstride; 
				out->pixels[destoffset+R] = in->pixels[srcoffset+R];
				out->pixels[destoffset+G] = in->pixels[srcoffset+G];
				out->pixels[destoffset+B] = in->pixels[srcoffset+B];
				out->pixels[destoffset+G2] = in->pixels[srcoffset+G2];

				destoffset += 4;
			}
		}
	}else{ // Upscale
		for(y=0; y!=out->h; y++)
		{
			destoffset = y*out->rowstride;
			for(x=0; x!=out->w; x++)
			{
				out->pixels[destoffset+R] = interpolation_function(x,y,scale,in,out,R); 
				out->pixels[destoffset+G] = interpolation_function(x,y,scale,in,out,G); 
				out->pixels[destoffset+B] = interpolation_function(x,y,scale,in,out,B); 
				out->pixels[destoffset+G2] = interpolation_function(x,y,scale,in,out,G2); 

				destoffset += 4;
			}
		}

	}
	return(out);
}

RS_IMAGE16 *
rs_image16_scale_int(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale)
{
	gint x,y;
	gint destoffset, srcoffset;
	gint iscale;

	g_assert(in->pixelsize==4);

	iscale = (int) floor(scale);
	if (iscale<1) iscale=1;

	if (out==NULL)
		out = rs_image16_new(in->w/iscale, in->h/iscale, in->channels, 4);
	else
	{
		g_assert(out->w == (in->w/iscale));
		g_assert(out->pixelsize==4);
	}

	for(y=0; y<out->h; y++)
	{
		destoffset = y*out->rowstride;
		srcoffset = y*iscale*in->rowstride;
		for(x=0; x<out->w; x++)
		{
			out->pixels[destoffset+R] = in->pixels[srcoffset+R];
			out->pixels[destoffset+G] = in->pixels[srcoffset+G];
			out->pixels[destoffset+B] = in->pixels[srcoffset+B];
			out->pixels[destoffset+G2] = in->pixels[srcoffset+G2];
			destoffset += 4;
			srcoffset += iscale*4;
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
	rsi = (RS_IMAGE8 *) g_malloc(sizeof(RS_IMAGE8));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * pixelsize;
	rsi->channels = channels;
	rsi->pixelsize = pixelsize;
	ORIENTATION_RESET(rsi->orientation);
	rsi->pixels = (guchar *) g_malloc(sizeof(guchar)*rsi->h*rsi->rowstride);
	return(rsi);
}

void
rs_image8_free(RS_IMAGE8 *rsi)
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

RS_IMAGE16 *
rs_image16_copy(RS_IMAGE16 *rsi)
{
	RS_IMAGE16 *ret;
	ret = rs_image16_new(rsi->w, rsi->h, rsi->channels, rsi->pixelsize);
	memcpy(ret->pixels, rsi->pixels, rsi->rowstride*rsi->h*2);
	return(ret);
}

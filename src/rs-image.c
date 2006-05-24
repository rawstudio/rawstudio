#include <gtk/gtk.h>
#include <string.h>
#include "color.h"
#include "matrix.h"
#include "rawstudio.h"
#include "rs-image.h"

void rs_image16_debug(RS_IMAGE16 *rsi);
void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
void rs_image16_mirror(RS_IMAGE16 *rsi);
void rs_image16_flip(RS_IMAGE16 *rsi);

void
rs_image16_debug(RS_IMAGE16 *rsi)
{
	print_debug_line("rsi: %d\n", (gint) rsi, (rsi!=NULL));
	print_debug_line("rsi->w: %d\n", rsi->w, ((rsi->w<5000)&&(rsi->w>0)));
	print_debug_line("rsi->h: %d\n", rsi->h, ((rsi->h<5000)&&(rsi->h>0)));
	print_debug_line("rsi->pitch: %d\n", rsi->pitch, (rsi->pitch == PITCH(rsi->w)));
	print_debug_line("rsi->rowstride: %d\n", rsi->rowstride, (rsi->rowstride == (PITCH(rsi->w)*rsi->channels)));
	print_debug_line("rsi->channels: %d\n", rsi->channels, ((rsi->channels<5)&&(rsi->channels>2)));
	print_debug_line("rsi->orientation: %d\n", rsi->orientation, ((rsi->channels<8)&&(rsi->channels>=0)));
	printf("\n");
	return;
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
	extern guint cpuflags;
	g_assert(rsi->pixelsize==4);
#ifdef __i386__
	if (cpuflags & _MMX)
	{
		gint row,col;
		void *src, *dest;
		for(row=0;row<rsi->h/2;row++)
		{
			src = rsi->pixels + row * rsi->rowstride;
			dest = rsi->pixels + (rsi->h - row - 1) * rsi->rowstride;
			for(col=0;col<rsi->w*rsi->pixelsize*2;col+=16)
			{
				asm volatile (
					"movq (%0), %%mm0\n\t"
					"movq (%1), %%mm1\n\t"
					"movq 8(%0), %%mm2\n\t"
					"movq 8(%1), %%mm3\n\t"
					"movq %%mm0, (%1)\n\t"
					"movq %%mm1, (%0)\n\t"
					"movq %%mm2, 8(%1)\n\t"
					"movq %%mm3, 8(%0)\n\t"
					"add $16, %0\n\t"
					"add $16, %1\n\t"
					: "+r" (src), "+r" (dest)
				);
			}
		}
		asm volatile ("emms");
	}
	else
#endif
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
	}
	ORIENTATION_FLIP(rsi->orientation);
	return;
}

RS_IMAGE16 *
rs_image16_scale(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale)
{
	gint x,y;
	gint destoffset, srcoffset;
	gint iscale;

	g_assert(in->pixelsize==4);

	iscale = (int) scale;
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

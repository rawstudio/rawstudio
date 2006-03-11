#include <gtk/gtk.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include "dcraw_api.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "color.h"
#include "matrix.h"

#define GETVAL(adjustment) \
	gtk_adjustment_get_value((GtkAdjustment *) adjustment)
#define SETVAL(adjustment, value) \
	gtk_adjustment_set_value((GtkAdjustment *) adjustment, value)

guchar previewtable[65536];

void
update_previewtable(RS_BLOB *rs, const gdouble gamma, const gdouble contrast)
{
	gint n;
	gdouble nd;
	gint res;
	static double gammavalue;
	if (gammavalue == (contrast/gamma)) return;
	gammavalue = contrast/gamma;

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		res = (gint) (pow(nd, gammavalue) * 255.0);
		_CLAMP255(res);
		previewtable[n] = res;
	}
}

void
rs_debug(RS_BLOB *rs)
{
	printf("rs: %d\n", (guint) rs);
	printf("rs->input: %d\n", (guint) rs->input);
	printf("rs->preview: %d\n", (guint) rs->preview);
	if(rs->input!=NULL)
	{
		printf("rs->input->w: %d\n", rs->input->w);
		printf("rs->input->h: %d\n", rs->input->h);
		printf("rs->input->pitch: %d\n", rs->input->pitch);
		printf("rs->input->channels: %d\n", rs->input->channels);
		printf("rs->input->pixels: %d\n", (guint) rs->input->pixels);
	}
	if(rs->preview!=NULL)
	{
		printf("rs->preview->w: %d\n", rs->preview->w);
		printf("rs->preview->h: %d\n", rs->preview->h);
		printf("rs->preview->pitch: %d\n", rs->preview->pitch);
		printf("rs->preview_scale: %d\n", rs->preview_scale);
		printf("rs->preview->pixels: %d\n", (guint) rs->preview->pixels);
	}
	printf("\n");
	return;
}

void
update_scaled(RS_BLOB *rs)
{
	guint y,x;
	guint srcoffset, destoffset;

	guint width, height;
	const guint scale = GETVAL(rs->scale);

	width=rs->input->w/scale;
	height=rs->input->h/scale;
	
	if (!rs->in_use) return;

	if (rs->preview==NULL)
	{
		rs->preview = rs_image16_new(width, height, rs->input->channels);
		rs->preview_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
	}

	/* 16 bit downscaled */
	if (rs->preview_scale != GETVAL(rs->scale)) /* do we need to? */
	{
		rs->preview_scale = GETVAL(rs->scale);
		rs_image16_free(rs->preview);
		rs->preview = rs_image16_new(width, height, rs->input->channels);
		for(y=0; y<rs->preview->h; y++)
		{
			destoffset = y*rs->preview->pitch*rs->preview->channels;
			srcoffset = y*rs->preview_scale*rs->input->pitch*rs->preview->channels;
			for(x=0; x<rs->preview->w; x++)
			{
				rs->preview->pixels[destoffset+R] = rs->input->pixels[srcoffset+R];
				rs->preview->pixels[destoffset+G] = rs->input->pixels[srcoffset+G];
				rs->preview->pixels[destoffset+B] = rs->input->pixels[srcoffset+B];
				if (rs->input->channels==4) rs->preview->pixels[destoffset+G2] = rs->input->pixels[srcoffset+G2];
				destoffset += rs->preview->channels;
				srcoffset += rs->preview_scale*rs->preview->channels;
			}
		}
		rs->preview_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->preview->w, rs->preview->h);
		gtk_image_set_from_pixbuf(rs->preview_image, rs->preview_pixbuf);
		g_object_unref(rs->preview_pixbuf);
	}
	else
	{
		if (rs->preview->w != gdk_pixbuf_get_width(rs->preview_pixbuf))
		{
			rs->preview_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rs->preview->w, rs->preview->h);
			gtk_image_set_from_pixbuf(rs->preview_image, rs->preview_pixbuf);
			g_object_unref(rs->preview_pixbuf);
		}
	}
	return;
}

void
update_preview(RS_BLOB *rs)
{
	RS_MATRIX4 mat;
	RS_MATRIX4Int mati;
	gint rowstride, x, y, srcoffset, destoffset;
	register gint r,g,b;
	guchar *pixels;

	if(!rs->in_use) return;

	SETVAL(rs->scale, floor(GETVAL(rs->scale))); // we only do integer scaling
	update_scaled(rs);
	update_previewtable(rs, GETVAL(rs->gamma), GETVAL(rs->contrast));
	matrix4_identity(&mat);
	matrix4_color_exposure(&mat, GETVAL(rs->exposure));
	matrix4_color_mixer(&mat, GETVAL(rs->rgb_mixer[R]), GETVAL(rs->rgb_mixer[G]), GETVAL(rs->rgb_mixer[B]));
	matrix4_color_saturate(&mat, GETVAL(rs->saturation));
	matrix4_color_hue(&mat, GETVAL(rs->hue));
	matrix4_to_matrix4int(&mat, &mati);

	pixels = gdk_pixbuf_get_pixels(rs->preview_pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(rs->preview_pixbuf);
	memset(rs->histogram_table, 0x00, sizeof(guint)*3*256); // reset histogram
	for(y=0 ; y<rs->preview->h ; y++)
	{
		srcoffset = y * rs->preview->pitch * rs->preview->channels;
		destoffset = y * rowstride;
		for(x=0 ; x<rs->preview->w ; x++)
		{
			r = (rs->preview->pixels[srcoffset+R]*mati.coeff[0][0]
				+ rs->preview->pixels[srcoffset+G]*mati.coeff[0][1]
				+ rs->preview->pixels[srcoffset+B]*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rs->preview->pixels[srcoffset+R]*mati.coeff[1][0]
				+ rs->preview->pixels[srcoffset+G]*mati.coeff[1][1]
				+ rs->preview->pixels[srcoffset+B]*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rs->preview->pixels[srcoffset+R]*mati.coeff[2][0]
				+ rs->preview->pixels[srcoffset+G]*mati.coeff[2][1]
				+ rs->preview->pixels[srcoffset+B]*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			pixels[destoffset] = previewtable[r];
			rs->histogram_table[R][pixels[destoffset++]]++;
			pixels[destoffset] = previewtable[g];
			rs->histogram_table[G][pixels[destoffset++]]++;
			pixels[destoffset] = previewtable[b];
			rs->histogram_table[B][pixels[destoffset++]]++;
			srcoffset+=rs->preview->channels; /* increment srcoffset by rs->preview->pixels */
		}
	}
	update_histogram(rs);
	gtk_image_set_from_pixbuf(rs->preview_image, rs->preview_pixbuf);
	return;
}	

void
rs_reset(RS_BLOB *rs)
{
	guint c;
	gtk_adjustment_set_value((GtkAdjustment *) rs->exposure, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rs->gamma, 2.2);
	gtk_adjustment_set_value((GtkAdjustment *) rs->saturation, 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rs->hue, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rs->contrast, 1.0);
	for(c=0;c<3;c++)
		gtk_adjustment_set_value((GtkAdjustment *) rs->rgb_mixer[c], rs->raw->pre_mul[c]);
	rs->preview_scale = 0;
	return;
}

void
rs_free_raw(RS_BLOB *rs)
{
	dcraw_close(rs->raw);
	g_free(rs->raw);
	rs->raw = NULL;
}

void
rs_free(RS_BLOB *rs)
{
	if (rs->in_use)
	{
		g_free(rs->input->pixels);
		rs->input->pixels=0;
		rs->input->w=0;
		rs->input->h=0;
		if (rs->raw!=NULL)
			rs_free_raw(rs);
		if (rs->input!=NULL)
			rs_image16_free(rs->input);
		if (rs->preview!=NULL)
			rs_image16_free(rs->preview);
		rs->input=NULL;
		rs->preview=NULL;
		rs->in_use=FALSE;
	}
}

void
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
			swap = (gushort *) g_malloc(pitch*height*sizeof(gushort)*rsi->channels);
			for(y=0; y<rsi->h; y++)
			{
				offset = y * rsi->pitch * rsi->channels;
				for(x=0; x<rsi->w; x++)
				{
					destoffset = (width-1-y+pitch*x)*rsi->channels;
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
			break;
		case 2:
			rs_image16_flip(rsi);
			rs_image16_mirror(rsi);
			break;
		case 3:
			width = rsi->h;
			height = rsi->w;
			pitch = PITCH(width);
			swap = (gushort *) g_malloc(pitch*height*sizeof(gushort)*rsi->channels);
			for(y=0; y<rsi->h; y++)
			{
				offset = y*rsi->pitch*rsi->channels;
				destoffset = ((height-1)*pitch + y)*rsi->channels;
				for(x=0; x<rsi->w; x++)
				{
					swap[destoffset+R] = rsi->pixels[offset+R];
					swap[destoffset+G] = rsi->pixels[offset+G];
					swap[destoffset+B] = rsi->pixels[offset+B];
					swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=4;
					destoffset -= pitch*rsi->channels;
				}
				offset += rsi->pitch*rsi->channels;
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			break;
		default:
			g_assert_not_reached();
			break;
	}
	return;
}

void
rs_image16_mirror(RS_IMAGE16 *rsi)
{
	gint row,col;
#ifdef __MMX__
	gushort *src, *dest;
	for(row=0;row<rsi->h;row++)
	{
		src = rsi->pixels+row*rsi->pitch*rsi->channels;
		dest = rsi->pixels+(row*rsi->pitch+rsi->w-1)*rsi->channels;
		for(col=0;col<rsi->w/2;col++)
		{
			asm volatile (
				"movq (%0), %%mm0\n\t"
				"movq (%1), %%mm1\n\t"
				"movq %%mm0, (%1)\n\t"
				"movq %%mm1, (%0)\n\t"
				"add $8, %0\n\t"
				"sub $8, %1\n\t"
				: "+r" (src), "+r" (dest)
				:
				: "%mm0", "%mm1"
			);
		}
	}
	asm volatile("emms\n\t");
#else
	gint offset,destoffset;
	for(row=0;row<rsi->h;row++)
	{
		offset = row*rsi->pitch*rsi->channels;
		destoffset = (row*rsi->pitch+rsi->w-1)*rsi->channels;
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
#endif
}

void
rs_image16_flip(RS_IMAGE16 *rsi)
{
#ifdef __MMX__
	gint row,col;
	void *src, *dest;
	for(row=0;row<rsi->h/2;row++)
	{
		src = rsi->pixels + row * rsi->pitch*rsi->channels;
		dest = rsi->pixels + (rsi->h - row - 1) * rsi->pitch*rsi->channels;
		for(col=0;col<rsi->w*rsi->channels*2;col+=16)
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
				:
				: "%mm0", "%mm1", "%mm2", "%mm3"
			);
		}
	}
	asm volatile ("emms");
#else
	gint row;
	const gint linel = rsi->pitch*rsi->channels*sizeof(gushort);
	gushort *tmp = (gushort *) g_malloc(linel);
	for(row=0;row<rsi->h/2;row++)
	{
		memcpy(tmp,
			rsi->pixels + row * rsi->pitch * rsi->channels, linel);
		memcpy(rsi->pixels + row * rsi->pitch * rsi->channels,
			rsi->pixels + (rsi->h-1-row) * rsi->pitch * rsi->channels, linel);
		memcpy(rsi->pixels + (rsi->h-1-row) * rsi->pitch * rsi->channels,
			tmp, linel);
	}
	g_free(tmp);
#endif
	return;
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels)
{
	RS_IMAGE16 *rsi;
	rsi = (RS_IMAGE16 *) g_malloc(sizeof(RS_IMAGE16));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->channels = channels;
	rsi->pixels = (gushort *) g_malloc(sizeof(gushort)*rsi->h*rsi->pitch*rsi->channels);
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
rs_image8_new(const guint width, const guint height, const guint channels)
{
	RS_IMAGE8 *rsi;
	rsi = (RS_IMAGE8 *) g_malloc(sizeof(RS_IMAGE8));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->channels = channels;
	rsi->pixels = (guchar *) g_malloc(sizeof(guchar)*rsi->h*rsi->pitch*rsi->channels);
	return(rsi);
}

void
rs_image8_free(RS_IMAGE16 *rsi)
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

RS_BLOB *
rs_new()
{
	RS_BLOB *rs;
	guint c;
	rs = g_malloc(sizeof(RS_BLOB));

	rs->exposure = make_adj(rs, 0.0, -2.0, 2.0, 0.1, 0.5);
	rs->gamma = make_adj(rs, 2.2, 0.0, 3.0, 0.1, 0.5);
	rs->saturation = make_adj(rs, 1.0, 0.0, 3.0, 0.1, 0.5);
	rs->hue = make_adj(rs, 0.0, 0.0, 360.0, 0.5, 30.0);
	rs->contrast = make_adj(rs, 1.0, 0.0, 3.0, 0.1, 0.1);
	rs->scale = make_adj(rs, 2.0, 1.0, 5.0, 1.0, 1.0);
	for(c=0;c<3;c++)
		rs->rgb_mixer[c] = make_adj(rs, 0.0, 0.0, 5.0, 0.1, 0.5);
	rs->raw = NULL;
	rs->input = NULL;
	rs->preview = NULL;
	rs->in_use = FALSE;
	return(rs);
}

void
rs_load_raw_from_file(RS_BLOB *rs, const gchar *filename)
{
	dcraw_data *raw;
	gushort *src;
	guint x,y;
	guint srcoffset, destoffset;

	if (rs->raw!=NULL) rs_free_raw(rs);
	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	dcraw_open(raw, (char *) filename);
	dcraw_load_raw(raw);
	rs_image16_free(rs->input); /*FIXME: free preview */
	rs->input = NULL;
	rs->input = rs_image16_new(raw->raw.width, raw->raw.height, 4);
	rs->raw = raw;
	src  = (gushort *) rs->raw->raw.image;
#ifdef __MMX__
	char b[8];
	gushort *sub = (gushort *) b;
	sub[0] = rs->raw->black;
	sub[1] = rs->raw->black;
	sub[2] = rs->raw->black;
	sub[3] = rs->raw->black;
	for (y=0; y<rs->raw->raw.height; y++)
	{
		destoffset = (guint) (rs->input->pixels + y*rs->input->pitch * rs->input->channels);
		srcoffset = (guint) (src + y * rs->input->w * rs->input->channels);
		x = rs->raw->raw.width;
		asm volatile (
			"movl %3, %%eax\n\t" /* copy x to %eax */
			"movq (%2), %%mm7\n\t" /* put black in %mm7 */
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
			"psllw $4, %%mm0\n\t" /* bitshift */
			"psllw $4, %%mm1\n\t"
			"psllw $4, %%mm2\n\t"
			"psllw $4, %%mm3\n\t"
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
			: "r" (sub), "r" (x)
			: "%eax", "%mm0", "%mm1"
		);
#else
	for (y=0; y<rs->raw->raw.height; y++)
	{
		destoffset = y*rs->input->pitch*rs->input->channels;
		srcoffset = y*rs->input->w*rs->input->channels;
		for (x=0; x<rs->raw->raw.width; x++)
		{
			register gint r,g,b;
			r = (src[srcoffset++] - rs->raw->black)<<4;
			g = (src[srcoffset++] - rs->raw->black)<<4;
			b = (src[srcoffset++] - rs->raw->black)<<4;
			_CLAMP65535_TRIPLET(r, g, b);
			rs->input->pixels[destoffset++] = r;
			rs->input->pixels[destoffset++] = g;
			rs->input->pixels[destoffset++] = b;

			if (rs->input->channels==4)
			{
				g = (src[srcoffset++] - rs->raw->black)<<4;
				_CLAMP65535(g);
				rs->input->pixels[destoffset++] = g;
			}
		}
#endif
	}
	rs->in_use=TRUE;
	rs->filename = filename;
	update_preview(rs);
	return;
}

int
main(int argc, char **argv)
{
	gui_init(argc, argv);
	return(0);
}

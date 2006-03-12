#include <gtk/gtk.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "color.h"

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
print_debug_line(const char *format, const gint value, const gboolean a)
{
	if (!a)
		printf("\033[31m");
	else
		printf("\033[33m");
	printf(format, value);
	printf("\033[0m");
}

void
rs_image16_debug(RS_IMAGE16 *rsi)
{
	print_debug_line("rsi: %d\n", (gint) rsi, (rsi!=NULL));
	print_debug_line("rsi->w: %d\n", rsi->w, ((rsi->w<5000)&&(rsi->w>0)));
	print_debug_line("rsi->h: %d\n", rsi->h, ((rsi->h<5000)&&(rsi->h>0)));
	print_debug_line("rsi->pitch: %d\n", rsi->pitch, (rsi->pitch == PITCH(rsi->w)));
	print_debug_line("rsi->rowstride: %d\n", rsi->rowstride, (rsi->rowstride == (PITCH(rsi->w)*rsi->channels)));
	print_debug_line("rsi->channels: %d\n", rsi->channels, ((rsi->channels<5)&&(rsi->channels>2)));
	print_debug_line("rsi->direction: %d\n", rsi->direction, ((rsi->channels<8)&&(rsi->channels>=0)));
	printf("\n");
	return;
}

void
rs_debug(RS_BLOB *rs)
{
	printf("rs: %d\n", (guint) rs);
	printf("rs->input: %d\n", (guint) rs->input);
	printf("rs->scaled: %d\n", (guint) rs->scaled);
	if(rs->input!=NULL)
	{
		printf("rs->input->w: %d\n", rs->input->w);
		printf("rs->input->h: %d\n", rs->input->h);
		printf("rs->input->pitch: %d\n", rs->input->pitch);
		printf("rs->input->channels: %d\n", rs->input->channels);
		printf("rs->input->pixels: %d\n", (guint) rs->input->pixels);
	}
	if(rs->scaled!=NULL)
	{
		printf("rs->scaled->w: %d\n", rs->scaled->w);
		printf("rs->scaled->h: %d\n", rs->scaled->h);
		printf("rs->scaled->pitch: %d\n", rs->scaled->pitch);
		printf("rs->preview_scale: %d\n", rs->preview_scale);
		printf("rs->scaled->pixels: %d\n", (guint) rs->scaled->pixels);
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

	if (rs->scaled==NULL)
	{
		rs->scaled = rs_image16_new(width, height, rs->input->channels);
		rs->preview = rs_image8_new(width, height, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, width, height);
	}

	/* 16 bit downscaled */
	if (rs->preview_scale != GETVAL(rs->scale)) /* do we need to? */
	{
		rs->preview_scale = GETVAL(rs->scale);
		rs_image16_free(rs->scaled);
		rs->scaled = rs_image16_new(width, height, rs->input->channels);
		for(y=0; y<rs->scaled->h; y++)
		{
			destoffset = y*rs->scaled->pitch*rs->scaled->channels;
			srcoffset = y*rs->preview_scale*rs->input->pitch*rs->scaled->channels;
			for(x=0; x<rs->scaled->w; x++)
			{
				rs->scaled->pixels[destoffset+R] = rs->input->pixels[srcoffset+R];
				rs->scaled->pixels[destoffset+G] = rs->input->pixels[srcoffset+G];
				rs->scaled->pixels[destoffset+B] = rs->input->pixels[srcoffset+B];
				if (rs->input->channels==4) rs->scaled->pixels[destoffset+G2] = rs->input->pixels[srcoffset+G2];
				destoffset += rs->scaled->channels;
				srcoffset += rs->preview_scale*rs->scaled->channels;
			}
		}
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->scaled->w, rs->scaled->h);
	}

	if (rs->direction != rs->scaled->direction)
		rs_image16_direction(rs->scaled, rs->direction);

	if (rs->scaled->w != rs->preview->w)
	{
		rs_image8_free(rs->preview);
		rs->preview = rs_image8_new(rs->scaled->w, rs->scaled->h, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->scaled->w, rs->scaled->h);
	}
	return;
}

void
update_preview(RS_BLOB *rs)
{
	RS_MATRIX4 mat;

	if(!rs->in_use) return;

	SETVAL(rs->scale, floor(GETVAL(rs->scale))); // we only do integer scaling
	update_scaled(rs);
	update_previewtable(rs, GETVAL(rs->gamma), GETVAL(rs->contrast));
	matrix4_identity(&mat);
	matrix4_color_exposure(&mat, GETVAL(rs->exposure));
	matrix4_color_mixer(&mat, GETVAL(rs->rgb_mixer[R]), GETVAL(rs->rgb_mixer[G]), GETVAL(rs->rgb_mixer[B]));
	matrix4_color_saturate(&mat, GETVAL(rs->saturation));
	matrix4_color_hue(&mat, GETVAL(rs->hue));
	matrix4_to_matrix4int(&mat, &rs->mati);

	/* FIXME: histogram broken! */
	memset(rs->histogram_table, 0x00, sizeof(guint)*3*256); // reset histogram

	update_preview_region(rs, rs->preview_exposed->x, rs->preview_exposed->y,
		rs->preview_exposed->w, rs->preview_exposed->h);
	return;
}	

void
update_preview_region(RS_BLOB *rs, gint rx, gint ry, gint rw, gint rh)
{
	guchar *pixels;
	gushort *in;

	if (!rs->in_use) return;
	if (rx > rs->preview->w) return;
	if (ry > rs->preview->h) return;
	if ((rx + rw) > rs->preview->w) rw = rs->preview->w-rx;
	if ((ry + rh) > rs->preview->h) rh = rs->preview->h-ry;

	pixels = rs->preview->pixels+(ry*rs->preview->rowstride+rx*rs->preview->channels);
	in = rs->scaled->pixels+(ry*rs->scaled->rowstride+rx*rs->scaled->channels);
	rs_render(rs->mati, rw, rh, in, rs->scaled->rowstride,
		rs->scaled->channels, pixels, rs->preview->rowstride);
	gdk_draw_rgb_image(rs->preview_drawingarea->window, rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL],
		rx, ry, rw, rh,
		GDK_RGB_DITHER_NONE, pixels, rs->preview->rowstride);
	return;
}

inline void
rs_render(RS_MATRIX4Int mati, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride)
{
	int srcoffset, destoffset;
	register int x,y;
	register int r,g,b;
	for(y=0 ; y<height ; y++)
	{
		destoffset = y * out_rowstride;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			r = (in[srcoffset+R]*mati.coeff[0][0]
				+ in[srcoffset+G]*mati.coeff[0][1]
				+ in[srcoffset+B]*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (in[srcoffset+R]*mati.coeff[1][0]
				+ in[srcoffset+G]*mati.coeff[1][1]
				+ in[srcoffset+B]*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (in[srcoffset+R]*mati.coeff[2][0]
				+ in[srcoffset+G]*mati.coeff[2][1]
				+ in[srcoffset+B]*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			out[destoffset++] = previewtable[r];
			out[destoffset++] = previewtable[g];
			out[destoffset++] = previewtable[b];
			srcoffset+=in_channels;
		}
	}
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
	DIRECTION_RESET(rs->direction);
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
		if (rs->scaled!=NULL)
			rs_image16_free(rs->scaled);
		rs->input=NULL;
		rs->scaled=NULL;
		rs->in_use=FALSE;
	}
}

void
rs_image16_direction(RS_IMAGE16 *rsi, const gint direction)
{
	const gint rot = ((direction&3)-(rsi->direction&3)+8)%4;

	rs_image16_rotate(rsi, rot);
	if (((rsi->direction)&4)^((direction)&4))
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
			rsi->rowstride = pitch * rsi->channels;
			DIRECTION_90(rsi->direction);
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
			rsi->rowstride = pitch * rsi->channels;
			DIRECTION_270(rsi->direction);
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
	DIRECTION_MIRROR(rsi->direction);
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
	DIRECTION_FLIP(rsi->direction);
	return;
}

RS_IMAGE16 *
rs_image16_scale(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale)
{
	gint x,y;
	gint destoffset, srcoffset;
	gint iscale;

	iscale = (int) scale;

	if (out==NULL)
		out = rs_image16_new(in->w/iscale, in->h/iscale, in->channels);
	else
	{
		g_assert(in->channels == out->channels);
		g_assert(out->w == (in->w/iscale));
	}

	for(y=0; y<out->h; y++)
	{
		destoffset = y*out->pitch*out->channels;
		srcoffset = y*iscale*in->pitch*out->channels;
		for(x=0; x<out->w; x++)
		{
			out->pixels[destoffset+R] = in->pixels[srcoffset+R];
			out->pixels[destoffset+G] = in->pixels[srcoffset+G];
			out->pixels[destoffset+B] = in->pixels[srcoffset+B];
			if (in->channels==4)
				out->pixels[destoffset+G2] = in->pixels[srcoffset+G2];
			destoffset += out->channels;
			srcoffset += iscale*out->channels;
		}
	}
	return(out);
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels)
{
	RS_IMAGE16 *rsi;
	rsi = (RS_IMAGE16 *) g_malloc(sizeof(RS_IMAGE16));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * channels;
	rsi->channels = channels;
	DIRECTION_RESET(rsi->direction);
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
	rsi->rowstride = rsi->pitch * channels;
	rsi->channels = channels;
	DIRECTION_RESET(rsi->direction);
	rsi->pixels = (guchar *) g_malloc(sizeof(guchar)*rsi->h*rsi->pitch*rsi->channels);
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
	rs->scaled = NULL;
	rs->preview = NULL;
	DIRECTION_RESET(rs->direction);
	rs->preview_exposed = (RS_RECT *) g_malloc(sizeof(RS_RECT));
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
	rs_image16_free(rs->input); rs->input = NULL;
	rs_image16_free(rs->scaled); rs->scaled = NULL;
	rs_image8_free(rs->preview); rs->preview = NULL;
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
	rs_reset(rs);
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

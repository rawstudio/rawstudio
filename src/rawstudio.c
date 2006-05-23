#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include <time.h>
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "rs-cache.h"
#include "color.h"
#include "tiff-meta.h"

#define cpuid(n) \
  a = b = c = d = 0x0; \
  asm( \
  	"cpuid" \
  	: "=eax" (a), "=ebx" (b), "=ecx" (c), "=edx" (d) : "0" (n) \
	)

guint cpuflags = 0;
guchar previewtable[65536];

static RS_FILETYPE filetypes[] = {
	{"cr2", rs_load_raw_from_file, rs_tiff_load_thumb, rs_tiff_load_meta},
	{"crw", rs_load_raw_from_file, rs_thumb_grt, NULL},
	{"nef", rs_load_raw_from_file, rs_thumb_grt, NULL},
	{"mrw", rs_load_raw_from_file, rs_thumb_grt, NULL},
	{"tif", rs_load_raw_from_file, rs_thumb_grt, rs_tiff_load_meta},
	{"orf", rs_load_raw_from_file, rs_thumb_grt, NULL},
	{"raw", rs_load_raw_from_file, NULL, NULL},
	{"jpg", rs_load_gdk, rs_thumb_gdk, NULL},
	{NULL, NULL}
};

void
update_previewtable(RS_BLOB *rs, const gdouble gamma, const gdouble contrast)
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
	print_debug_line("rsi->orientation: %d\n", rsi->orientation, ((rsi->channels<8)&&(rsi->channels>=0)));
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
	guint width, height;
	const guint scale = GETVAL(rs->scale);

	width=rs->input->w/scale;
	height=rs->input->h/scale;
	
	if (!rs->in_use) return;

	if (rs->scaled==NULL)
	{
		rs->scaled = rs_image16_new(width, height, rs->input->channels, 4);
		rs->preview = rs_image8_new(width, height, 3, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, width, height);
	}

	/* 16 bit downscaled */
	if (rs->preview_scale != GETVAL(rs->scale)) /* do we need to? */
	{
		rs->preview_scale = GETVAL(rs->scale);
		rs_image16_free(rs->scaled);
		rs->scaled = rs_image16_new(width, height, rs->input->channels, 4);
		rs_image16_scale(rs->input, rs->scaled, rs->preview_scale);
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->scaled->w, rs->scaled->h);
	}

	if (rs->orientation != rs->scaled->orientation)
		rs_image16_orientation(rs->scaled, rs->orientation);

	if (rs->scaled->w != rs->preview->w)
	{
		rs_image8_free(rs->preview);
		rs->preview = rs_image8_new(rs->scaled->w, rs->scaled->h, 3, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->scaled->w, rs->scaled->h);
	}
	return;
}

void
update_preview(RS_BLOB *rs)
{
	if(unlikely(!rs->in_use)) return;

	SETVAL(rs->scale, floor(GETVAL(rs->scale))); /* we only do integer scaling */
	update_scaled(rs);
	update_previewtable(rs, GETVAL(rs->settings[rs->current_setting]->gamma),
		GETVAL(rs->settings[rs->current_setting]->contrast));
	matrix4_identity(&rs->mat);
	matrix4_color_exposure(&rs->mat, GETVAL(rs->settings[rs->current_setting]->exposure));
	matrix4_color_mixer(&rs->mat, GETVAL(rs->settings[rs->current_setting]->rgb_mixer[R]),
		GETVAL(rs->settings[rs->current_setting]->rgb_mixer[G]),
		GETVAL(rs->settings[rs->current_setting]->rgb_mixer[B]));

	rs->pre_mul[R] = (1.0+GETVAL(rs->settings[rs->current_setting]->warmth))
		*(2.0-GETVAL(rs->settings[rs->current_setting]->tint));
	rs->pre_mul[G] = 1.0;
	rs->pre_mul[B] = (1.0-GETVAL(rs->settings[rs->current_setting]->warmth))
		*(2.0-GETVAL(rs->settings[rs->current_setting]->tint));
	rs->pre_mul[G2] = 1.0;

	matrix4_color_saturate(&rs->mat, GETVAL(rs->settings[rs->current_setting]->saturation));
	matrix4_color_hue(&rs->mat, GETVAL(rs->settings[rs->current_setting]->hue));
	matrix4_to_matrix4int(&rs->mat, &rs->mati);
	update_preview_region(rs, rs->preview_exposed->x1, rs->preview_exposed->y1,
		rs->preview_exposed->x2, rs->preview_exposed->y2);

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
update_preview_region(RS_BLOB *rs, gint x1, gint y1, gint x2, gint y2)
{
	guchar *pixels;
	gushort *in;

	if (unlikely(!rs->in_use)) return;

	_CLAMP(x2, rs->scaled->w);
	_CLAMP(y2, rs->scaled->h);

	pixels = rs->preview->pixels+(y1*rs->preview->rowstride+x1*rs->preview->pixelsize);
	in = rs->scaled->pixels+(y1*rs->scaled->rowstride+x1*rs->scaled->pixelsize);
	rs_render(rs, x2-x1, y2-y1, in, rs->scaled->rowstride,
		rs->scaled->pixelsize, pixels, rs->preview->rowstride);
	gdk_draw_rgb_image(rs->preview_drawingarea->window, rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL],
		x1, y1, x2-x1, y2-y1,
		GDK_RGB_DITHER_NONE, pixels, rs->preview->rowstride);
	return;
}

gboolean
rs_render_idle(RS_BLOB *rs)
{
	gint row;
	gushort *in;
	guchar *out;

	if (rs->in_use && (!rs->preview_done))
		for(row=rs->preview_idle_render_lastrow; row<rs->scaled->h; row++)
		{
			in = rs->scaled->pixels + row*rs->scaled->rowstride;
			out = rs->preview->pixels + row*rs->preview->rowstride;
			rs_render(rs, rs->scaled->w, 1, in, rs->scaled->rowstride,
				rs->scaled->pixelsize, out, rs->preview->rowstride);
			gdk_draw_rgb_image(rs->preview_backing,
				rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL], 0, row,
				rs->scaled->w, 1, GDK_RGB_DITHER_NONE, out,
				rs->preview->rowstride);
			rs->preview_idle_render_lastrow=row+1;
			if (gtk_events_pending()) return(TRUE);
		}
	rs->preview_idle_render_lastrow = 0;
	rs->preview_done = TRUE;
	rs->preview_idle_render = FALSE;
	return(FALSE);
}

inline void
rs_render(RS_BLOB *rs, gint width, gint height, gushort *in,
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
		rs->mat.coeff[0][0],
		rs->mat.coeff[1][0],
		rs->mat.coeff[2][0],
		0.0,
		rs->mat.coeff[0][1],
		rs->mat.coeff[1][1],
		rs->mat.coeff[2][1],
		0.0,
		rs->mat.coeff[0][2],
		rs->mat.coeff[1][2],
		rs->mat.coeff[2][2],
		0.0 };
		asm volatile (
			"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
			"movaps (%0), %%xmm3\n\t" /* matrix */
			"movaps 16(%0), %%xmm4\n\t"
			"movaps 32(%0), %%xmm5\n\t"
			"movaps (%1), %%xmm6\n\t" /* top */
			"pxor %%mm7, %%mm7\n\t" /* 0x0 */
			:
			: "r" (mat), "r" (top), "r" (rs->pre_mul)
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
		mat[0] = rs->mat.coeff[0][0];
		mat[1] = rs->mat.coeff[0][1]*0.5;
		mat[2] = rs->mat.coeff[0][2];
		mat[3] = rs->mat.coeff[0][1]*0.5;
		mat[4] = rs->mat.coeff[1][0];
		mat[5] = rs->mat.coeff[1][1]*0.5;
		mat[6] = rs->mat.coeff[1][2];
		mat[7] = rs->mat.coeff[1][1]*0.5;
		mat[8] = rs->mat.coeff[2][0];
		mat[9] = rs->mat.coeff[2][1]*0.5;
		mat[10] = rs->mat.coeff[2][2];
		mat[11] = rs->mat.coeff[2][1]*0.5;
		top[0] = 65535.0;
		top[1] = 65535.0;
		asm volatile (
			"femms\n\t"
			"pxor %%mm7, %%mm7\n\t" /* 0x0 */
			"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
			"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
			"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
			:
			: "r" (&rs->pre_mul), "r" (&top)
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
			pre_mul[x] = (gint) (rs->pre_mul[x]*128.0);
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
				r = (rr*rs->mati.coeff[0][0]
					+ gg*rs->mati.coeff[0][1]
					+ bb*rs->mati.coeff[0][2])>>MATRIX_RESOLUTION;
				g = (rr*rs->mati.coeff[1][0]
					+ gg*rs->mati.coeff[1][1]
					+ bb*rs->mati.coeff[1][2])>>MATRIX_RESOLUTION;
				b = (rr*rs->mati.coeff[2][0]
					+ gg*rs->mati.coeff[2][1]
					+ bb*rs->mati.coeff[2][2])>>MATRIX_RESOLUTION;
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
		pre_mul[x] = (gint) (rs->pre_mul[x]*128.0);
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
			r = (rr*rs->mati.coeff[0][0]
				+ gg*rs->mati.coeff[0][1]
				+ bb*rs->mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*rs->mati.coeff[1][0]
				+ gg*rs->mati.coeff[1][1]
				+ bb*rs->mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*rs->mati.coeff[2][0]
				+ gg*rs->mati.coeff[2][1]
				+ bb*rs->mati.coeff[2][2])>>MATRIX_RESOLUTION;
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
	gint c;
	rs->preview_scale = 0;
	rs->priority = PRIO_U;
	ORIENTATION_RESET(rs->orientation);
	for(c=0;c<3;c++)
		rs_settings_reset(rs->settings[c]);
	return;
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
		if (rs->input!=NULL)
			rs_image16_free(rs->input);
		if (rs->scaled!=NULL)
			rs_image16_free(rs->scaled);
		if (rs->metadata!=NULL)
			g_free(rs->metadata);
		rs->input=NULL;
		rs->scaled=NULL;
		rs->metadata=NULL;
		rs->in_use=FALSE;
	}
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

void
rs_settings_reset(RS_SETTINGS *rss)
{
	gtk_adjustment_set_value((GtkAdjustment *) rss->exposure, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->gamma, 2.2);
	gtk_adjustment_set_value((GtkAdjustment *) rss->saturation, 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->hue, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->rgb_mixer[0], 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->rgb_mixer[1], 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->rgb_mixer[2], 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->contrast, 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->warmth, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->tint, 0.0);
	return;
}

RS_SETTINGS *
rs_settings_new()
{
	RS_SETTINGS *rss;
	rss = g_malloc(sizeof(RS_SETTINGS));
	rss->exposure = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	rss->gamma = gtk_adjustment_new(2.2, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->saturation = gtk_adjustment_new(1.0, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->hue = gtk_adjustment_new(0.0, 0.0, 360.0, 0.1, 30.0, 0.0);
	rss->rgb_mixer[0] = gtk_adjustment_new(1.0, 0.0, 5.0, 0.1, 0.5, 0.0);
	rss->rgb_mixer[1] = gtk_adjustment_new(1.0, 0.0, 5.0, 0.1, 0.5, 0.0);
	rss->rgb_mixer[2] = gtk_adjustment_new(1.0, 0.0, 5.0, 0.1, 0.5, 0.0);
	rss->contrast = gtk_adjustment_new(1.0, 0.0, 2.0, 0.1, 0.5, 0.0);
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
		g_object_unref(rss->gamma);
		g_object_unref(rss->saturation);
		g_object_unref(rss->hue);
		g_object_unref(rss->rgb_mixer[0]);
		g_object_unref(rss->rgb_mixer[1]);
		g_object_unref(rss->rgb_mixer[2]);
		g_object_unref(rss->contrast);
		g_object_unref(rss->warmth);
		g_object_unref(rss->tint);
		g_free(rss);
	}
	return;
}

RS_BLOB *
rs_new()
{
	RS_BLOB *rs;
	guint c;
	rs = g_malloc(sizeof(RS_BLOB));
	rs->scale = gtk_adjustment_new(2.0, 1.0, 5.0, 1.0, 1.0, 0.0);
	gtk_signal_connect(GTK_OBJECT(rs->scale), "value_changed",
		G_CALLBACK(update_preview_callback), rs);
	rs->input = NULL;
	rs->scaled = NULL;
	rs->preview = NULL;
	rs->histogram_dataset = NULL;
	ORIENTATION_RESET(rs->orientation);
	rs->preview_exposed = (RS_RECT *) g_malloc(sizeof(RS_RECT));
	rs->preview_backing = NULL;
	rs->preview_done = FALSE;
	rs->preview_idle_render = FALSE;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();
	rs->current_setting = 0;
	rs->priority = PRIO_U;
	rs->metadata = g_malloc(sizeof(RS_METADATA));
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
	gint64 shift;

	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		rs->in_use=FALSE;
		dcraw_load_raw(raw);
		shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);
		rs_image16_free(rs->input); rs->input = NULL;
		rs_image16_free(rs->scaled); rs->scaled = NULL;
		rs_image16_free(rs->histogram_dataset); rs->histogram_dataset = NULL;
		rs_image8_free(rs->preview); rs->preview = NULL;
		rs->input = rs_image16_new(raw->raw.width, raw->raw.height, 4, 4);
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
				destoffset = (guint) (rs->input->pixels + y*rs->input->rowstride);
				srcoffset = (guint) (src + y * rs->input->w * rs->input->pixelsize);
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
				destoffset = y*rs->input->rowstride;
				srcoffset = y*rs->input->w*rs->input->pixelsize;
				for (x=0; x<raw->raw.width; x++)
				{
					register gint r,g,b;
					r = (src[srcoffset++] - raw->black)<<shift;
					g = (src[srcoffset++] - raw->black)<<shift;
					b = (src[srcoffset++] - raw->black)<<shift;
					_CLAMP65535_TRIPLET(r, g, b);
					rs->input->pixels[destoffset++] = r;
					rs->input->pixels[destoffset++] = g;
					rs->input->pixels[destoffset++] = b;
					g = (src[srcoffset++] - raw->black)<<shift;
					_CLAMP65535(g);
					rs->input->pixels[destoffset++] = g;
				}
			}
		}
		rs->histogram_dataset = rs_image16_scale(rs->input, NULL,
			rs->input->w/HISTOGRAM_DATASET_WIDTH);
		for(x=0;x<4;x++)
			rs->pre_mul[x] = raw->pre_mul[x];
		rs->filename = filename;
		dcraw_close(raw);
	}
	g_free(raw);
	return;
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
				filetype = &filetypes[n];
				break;
			}
		}
		n++;
	}
	g_free(iname);
	return(filetype);
}

void
rs_load_gdk(RS_BLOB *rs, const gchar *filename)
{
	GdkPixbuf *pixbuf;
	guchar *pixels;
	gint rowstride;
	gint width, height;
	gint row,col,n,res, src, dest;
	gdouble nd;
	gushort gammatable[256];
	if ((pixbuf = gdk_pixbuf_new_from_file(filename, NULL)))
	{
		for(n=0;n<256;n++)
		{
			nd = ((gdouble) n) / 255.0;
			res = (gint) (pow(nd, 2.2) * 65535.0);
			_CLAMP65535(res);
			gammatable[n] = res;
		}
		rs_image16_free(rs->input); rs->input = NULL;
		rs_image16_free(rs->scaled); rs->scaled = NULL;
		rs_image16_free(rs->histogram_dataset); rs->histogram_dataset = NULL;
		rs_image8_free(rs->preview); rs->preview = NULL;
		rowstride = gdk_pixbuf_get_rowstride(pixbuf);
		pixels = gdk_pixbuf_get_pixels(pixbuf);
		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);
		rs->input = rs_image16_new(width, height, 3, 4);
		for(row=0;row<rs->input->h;row++)
		{
			dest = row * rs->input->rowstride;
			src = row * rowstride;
			for(col=0;col<rs->input->w;col++)
			{
				rs->input->pixels[dest++] = gammatable[pixels[src++]];
				rs->input->pixels[dest++] = gammatable[pixels[src++]];
				rs->input->pixels[dest++] = gammatable[pixels[src++]];
				rs->input->pixels[dest++] = gammatable[pixels[src-2]];
			}
		}
		g_object_unref(pixbuf);
		rs->histogram_dataset = rs_image16_scale(rs->input, NULL,
			rs->input->w/HISTOGRAM_DATASET_WIDTH);
		for(n=0;n<4;n++)
			rs->pre_mul[n] = 1.0;
		rs->filename = filename;
	}
	return;
}

gchar *
rs_dotdir_get(const gchar *filename)
{
	gchar *ret;
	gchar *directory;
	GString *dotdir;

	directory = g_path_get_dirname(filename);

	dotdir = g_string_new(directory);
	dotdir = g_string_append(dotdir, "/");
	dotdir = g_string_append(dotdir, DOTDIR);

	if (!g_file_test(dotdir->str, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
	{
		if (g_mkdir(dotdir->str, 0700) != 0)
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
rs_set_warmth_auto(RS_BLOB *rs)
{
	gint row, col, x, y, c, val;
	gint sum[8];
	gdouble pre_mul[4];
	gdouble dsum[8], dmax;
	gfloat tint, warmth;

	for (row=0; row < rs->input->h-7; row += 8)
		for (col=0; col < rs->input->w-7; col += 8)
		{
			memset (sum, 0, sizeof sum);
			for (y=row; y < row+8; y++)
				for (x=col; x < col+8; x++)
					for(c=0;c<4;c++)
					{
						val = rs->input->pixels[y*rs->input->rowstride+x*4+c];
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

	dmax = 0.0;
	for (c=0; c < 4; c++)
		if (dmax < pre_mul[c])
			dmax = pre_mul[c];

	for(c=0;c<4;c++)
		pre_mul[c] /= dmax;

	pre_mul[R] *= (1.0/pre_mul[G]);
	pre_mul[B] *= (1.0/pre_mul[G]);
	pre_mul[G] = 1.0;
	pre_mul[G2] = 1.0;

	tint = (pre_mul[B] + pre_mul[R] - 4.0)/-2.0;
	warmth = (pre_mul[R]/(2.0-tint))-1.0;
	SETVAL(rs->settings[rs->current_setting]->tint, tint);
	SETVAL(rs->settings[rs->current_setting]->warmth, warmth);

	return;
}

void
rs_set_warmth_from_color(RS_BLOB *rs, gint x, gint y)
{
	gint offset, row, col;
	gdouble r=0.0, g=0.0, b=0.0;
	gdouble warmth, tint;

	for(row=0; row<3; row++)
	{
		for(col=0; col<3; col++)
		{
			offset = (y+row-1)*rs->scaled->rowstride
				+ (x+col-1)*rs->scaled->pixelsize;
			r += ((gdouble) rs->scaled->pixels[offset+R])/65535.0;
			g += ((gdouble) rs->scaled->pixels[offset+G])/65535.0;
			b += ((gdouble) rs->scaled->pixels[offset+B])/65535.0;
			if (rs->scaled->channels==4)
				g += ((gdouble) rs->scaled->pixels[offset+G2])/65535.0;
				
		}
	}
	r /= 9;
	g /= 9;
	b /= 9;
	if (rs->scaled->channels==4)
		g /= 2;
	warmth = (b-r)/(r+b); /* r*(1+warmth) = b*(1-warmth) */
	tint = -g/(r+r*warmth)+2.0; /* magic */
	SETVAL(rs->settings[rs->current_setting]->warmth, warmth);
	SETVAL(rs->settings[rs->current_setting]->tint, tint);
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
	gui_init(argc, argv);
	return(0);
}

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
#include <lcms.h>
#include "color.h"
#include "rs-render.h"
#include "matrix.h"

/* CMS or no CMS dependant function pointers - initialized by
 * rs_render_select(bool) */
DEFINE_RENDER(*rs_render);
DEFINE_RENDER16(*rs_render16);
void
(*rs_render_pixel)(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gushort *in, guchar *out, void *profile);
void
rs_render_pixel_cms(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gushort *in, guchar *out, void *profile);
void
rs_render_pixel_nocms(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gushort *in, guchar *out, void *profile);

void
rs_render_select(gboolean cms)
{
	if (cms)
	{
		rs_render = rs_render_cms;
		rs_render16 = rs_render16_cms;
		rs_render_pixel = rs_render_pixel_cms;
	}
	else
	{
		rs_render = rs_render_nocms;
		rs_render16 = rs_render16_nocms;
		rs_render_pixel = rs_render_pixel_nocms;
	}

	return;
}

void
rs_render_previewtable(const gdouble contrast, gfloat *curve, guchar *table8, gushort *table16)
{
	static const gdouble rec65535 = (1.0f / 65535.0f);
	register gint n;
	gdouble nd;
	register gint res;
	double gammavalue;
	const double postadd = 0.5 - (contrast/2.0);
	gammavalue = (1.0/GAMMA);

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) * rec65535;
		nd = pow(nd, gammavalue);

		if (likely(curve))
			nd = (gdouble) curve[((gint) (nd*65535.0f))];

		nd = nd*contrast+postadd;

		res = (gint) (nd*255.0);
		_CLAMP255(res);
		if (likely(table8))
			table8[n] = res;

		if (likely(table16))
		{
			nd = pow(nd, GAMMA);
			res = (gint) (nd*65535.0);
			_CLAMP65535(res);
			table16[n] = res;
		}
	}
}

/* Function pointers - initialized by arch binders */
DEFINE_RENDER(*rs_render_cms);
DEFINE_RENDER16(*rs_render16_cms);

/* Default Implementation */
DEFINE_RENDER(rs_render_cms_c)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	matrix4_to_matrix4int(matrix, &mati);
	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (pre_mul[x]*128.0);
	for(y=0 ; y<height ; y++)
	{
		destoffset = 0;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			rr = (in[srcoffset+R]*pre_muli[R])>>7;
			gg = (in[srcoffset+G]*pre_muli[G])>>7;
			bb = (in[srcoffset+B]*pre_muli[B])>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*mati.coeff[0][0]
				+ gg*mati.coeff[0][1]
				+ bb*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			buffer[destoffset++] = table[r];
			buffer[destoffset++] = table[g];
			buffer[destoffset++] = table[b];
			srcoffset+=4;
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+y * out_rowstride, width);
	}
	g_free(buffer);
	return;
}

DEFINE_RENDER16(rs_render16_cms_c)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	matrix4_to_matrix4int(matrix, &mati);
	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (pre_mul[x]*128.0);
	for(y=0 ; y<height ; y++)
	{
		destoffset = 0;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			rr = (in[srcoffset+R]*pre_muli[R]+64)>>7;
			gg = (in[srcoffset+G]*pre_muli[G]+64)>>7;
			bb = (in[srcoffset+B]*pre_muli[B]+64)>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*mati.coeff[0][0]
				+ gg*mati.coeff[0][1]
				+ bb*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			buffer[destoffset++] = table16[r];
			buffer[destoffset++] = table16[g];
			buffer[destoffset++] = table16[b];
			srcoffset+=4;
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+y * out_rowstride, width);
	}
	g_free(buffer);
	return;
}

#if defined (__i386__) || defined (__x86_64__)
DEFINE_RENDER(rs_render_cms_sse)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	register glong r,g,b;
	gint destoffset;
	gint col;
	gfloat top[4] align(16) = {65535.0, 65535.0, 65535.0, 65535.0};
	gfloat mat[12] align(16) = {
		matrix->coeff[0][0],
		matrix->coeff[1][0],
		matrix->coeff[2][0],
		0.0,
		matrix->coeff[0][1],
		matrix->coeff[1][1],
		matrix->coeff[2][1],
		0.0,
		matrix->coeff[0][2],
		matrix->coeff[1][2],
		matrix->coeff[2][2],
		0.0 };
	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		:
		: "r" (mat), "r" (top), "r" (pre_mul)
		: "memory"
	);
	while(height--)
	{
		destoffset = 0;
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
			buffer[destoffset++] = table16[r];
			buffer[destoffset++] = table16[g];
			buffer[destoffset++] = table16[b];
			s += 4;
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+height * out_rowstride, width);
	}
	asm volatile("emms\n\t");
	g_free(buffer);
	return;
}

DEFINE_RENDER(rs_render_cms_3dnow)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint destoffset;
	gint col;
	register glong r=0,g=0,b=0;
	gfloat mat[12] align(8);
	gfloat top[2] align(8);
	mat[0] = matrix->coeff[0][0];
	mat[1] = matrix->coeff[0][1];
	mat[2] = matrix->coeff[0][2];
	mat[3] = 0.0;
	mat[4] = matrix->coeff[1][0];
	mat[5] = matrix->coeff[1][1];
	mat[6] = matrix->coeff[1][2];
	mat[7] = 0.0;
	mat[8] = matrix->coeff[2][0];
	mat[9] = matrix->coeff[2][1];
	mat[10] = matrix->coeff[2][2];
	mat[11] = 0.0;
	top[0] = 65535.0;
	top[1] = 65535.0;
	asm volatile (
		"femms\n\t"
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
		"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
		"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
		:
		: "r" (pre_mul), "r" (&top)
	);
	while(height--)
	{
		destoffset = 0;
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
			buffer[destoffset++] = table16[r];
			buffer[destoffset++] = table16[g];
			buffer[destoffset++] = table16[b];
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+height * out_rowstride, width);
	}
	asm volatile ("femms\n\t");
	g_free(buffer);
	return;
}
#endif

/* No CMS renderer pointer functions - initialized by arch binder */
DEFINE_RENDER(*rs_render_nocms);
DEFINE_RENDER16(*rs_render16_nocms);

DEFINE_RENDER(rs_render_nocms_c)
{
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	matrix4_to_matrix4int(matrix, &mati);
	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (pre_mul[x]*128.0);
	for(y=0 ; y<height ; y++)
	{
		guchar *d = out + y * out_rowstride;
		destoffset = 0;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			rr = (in[srcoffset+R]*pre_muli[R]+64)>>7;
			gg = (in[srcoffset+G]*pre_muli[G]+64)>>7;
			bb = (in[srcoffset+B]*pre_muli[B]+64)>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*mati.coeff[0][0]
				+ gg*mati.coeff[0][1]
				+ bb*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			d[destoffset++] = table[r];
			d[destoffset++] = table[g];
			d[destoffset++] = table[b];
			srcoffset+=4;
		}
	}
	return;
}

DEFINE_RENDER16(rs_render16_nocms_c)
{
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	matrix4_to_matrix4int(matrix, &mati);
	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (pre_mul[x]*128.0);
	for(y=0 ; y<height ; y++)
	{
		gushort *d = out + y * out_rowstride;
		destoffset = 0;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			rr = (in[srcoffset+R]*pre_muli[R])>>7;
			gg = (in[srcoffset+G]*pre_muli[G])>>7;
			bb = (in[srcoffset+B]*pre_muli[B])>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*mati.coeff[0][0]
				+ gg*mati.coeff[0][1]
				+ bb*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			d[destoffset++] = table16[r];
			d[destoffset++] = table16[g];
			d[destoffset++] = table16[b];
			srcoffset+=4;
		}
	}
	return;
}

#if defined (__i386__) || defined (__x86_64__)
DEFINE_RENDER(rs_render_nocms_sse)
{
	register glong r,g,b;
	gint destoffset;
	gint col;
	gfloat top[4] align(16) = {65535.0, 65535.0, 65535.0, 65535.0};
	gfloat mat[12] align(16) = {
		matrix->coeff[0][0],
		matrix->coeff[1][0],
		matrix->coeff[2][0],
		0.0,
		matrix->coeff[0][1],
		matrix->coeff[1][1],
		matrix->coeff[2][1],
		0.0,
		matrix->coeff[0][2],
		matrix->coeff[1][2],
		matrix->coeff[2][2],
		0.0 };
	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		:
		: "r" (mat), "r" (top), "r" (pre_mul)
		: "memory"
	);
	while(height--)
	{
		destoffset = 0;
		col = width;
		gushort *s = in + height * in_rowstride;
		guchar *d = out + height * out_rowstride;
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
			d[destoffset++] = table[r];
			d[destoffset++] = table[g];
			d[destoffset++] = table[b];
			s += 4;
		}
	}
	asm volatile("emms\n\t");
	return;
}

DEFINE_RENDER(rs_render_nocms_3dnow)
{
	gint destoffset;
	gint col;
	register glong r=0,g=0,b=0;
	gfloat mat[12] align(8);
	gfloat top[2] align(8);
	mat[0] = matrix->coeff[0][0];
	mat[1] = matrix->coeff[0][1];
	mat[2] = matrix->coeff[0][2];
	mat[3] = 0.0;
	mat[4] = matrix->coeff[1][0];
	mat[5] = matrix->coeff[1][1];
	mat[6] = matrix->coeff[1][2];
	mat[7] = 0.0;
	mat[8] = matrix->coeff[2][0];
	mat[9] = matrix->coeff[2][1];
	mat[10] = matrix->coeff[2][2];
	mat[11] = 0.0;
	top[0] = 65535.0;
	top[1] = 65535.0;
	asm volatile (
		"femms\n\t"
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
		"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
		"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
		:
		: "r" (pre_mul), "r" (&top)
	);
	while(height--)
	{
		destoffset = 0;
		col = width;
		gushort *s = in + height * in_rowstride;
		guchar *d = out + height * out_rowstride;
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
			d[destoffset++] = table[r];
			d[destoffset++] = table[g];
			d[destoffset++] = table[b];
		}
	}
	asm volatile ("femms\n\t");

	return;
}

#endif /* __i386__ || __x86_64__ */

void
rs_render_pixel_cms(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gushort *in, guchar *out, void *profile)
{
	gushort buffer[3];
	gfloat rr, gg, bb;
	gint r,g,b;

	rr = ((gfloat) in[R]) * pre_mul[R];
	gg = ((gfloat) in[G]) * pre_mul[G];
	bb = ((gfloat) in[B]) * pre_mul[B];

	if (rr>65535.0)
		rr = 65535.0;
	else if (rr<0.0)
		rr = 0.0;
	if (gg>65535.0)
		gg = 65535.0;
	else if (gg<0.0)
		gg = 0.0;
	if (bb>65535.0)
		bb = 65535.0;
	else if (bb<0.0)
		bb = 0.0;

	r = rr*matrix->coeff[0][0]
		+ gg*matrix->coeff[0][1]
		+ bb*matrix->coeff[0][2];
	g = rr*matrix->coeff[1][0]
		+ gg*matrix->coeff[1][1]
		+ bb*matrix->coeff[1][2];
	b = rr*matrix->coeff[2][0]
		+ gg*matrix->coeff[2][1]
		+ bb*matrix->coeff[2][2];
	_CLAMP65535_TRIPLET(r,g,b);
	buffer[R] = table16[r];
	buffer[G] = table16[g];
	buffer[B] = table16[b];
	cmsDoTransform((cmsHPROFILE) profile, buffer, out, 1);
	return;
}

void
rs_render_pixel_nocms(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gushort *in, guchar *out, void *profile)
{
	gfloat rr, gg, bb;
	gint r,g,b;

	rr = ((gfloat) in[R]) * pre_mul[R];
	gg = ((gfloat) in[G]) * pre_mul[G];
	bb = ((gfloat) in[B]) * pre_mul[B];

	if (rr>65535.0)
		rr = 65535.0;
	else if (rr<0.0)
		rr = 0.0;
	if (gg>65535.0)
		gg = 65535.0;
	else if (gg<0.0)
		gg = 0.0;
	if (bb>65535.0)
		bb = 65535.0;
	else if (bb<0.0)
		bb = 0.0;

	r = rr*matrix->coeff[0][0]
		+ gg*matrix->coeff[0][1]
		+ bb*matrix->coeff[0][2];
	g = rr*matrix->coeff[1][0]
		+ gg*matrix->coeff[1][1]
		+ bb*matrix->coeff[1][2];
	b = rr*matrix->coeff[2][0]
		+ gg*matrix->coeff[2][1]
		+ bb*matrix->coeff[2][2];
	_CLAMP65535_TRIPLET(r,g,b);
	out[R] = table[r];
	out[G] = table[g];
	out[B] = table[b];
	return;
}

/* Function pointer - initialiazed by arch binders */
void
(*rs_render_histogram_table)(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, RS_IMAGE16 *input, guint *output) __rs_optimized;

/* Default implementation */
void
rs_render_histogram_table_c(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, RS_IMAGE16 *input, guint *output)
{
	gint y,x;
	gint srcoffset;
	gint r,g,b,rr,gg,bb;
	gushort *in;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	matrix4_to_matrix4int(matrix, &mati);
	if (unlikely(input==NULL)) return;

	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (pre_mul[x]*128.0);
	in	= input->pixels;
	for(y=0 ; y<input->h ; y++)
	{
		srcoffset = y * input->rowstride;
		for(x=0 ; x<input->w ; x++)
		{
			rr = (in[srcoffset+R]*pre_muli[R])>>7;
			gg = (in[srcoffset+G]*pre_muli[G])>>7;
			bb = (in[srcoffset+B]*pre_muli[B])>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*mati.coeff[0][0]
				+ gg*mati.coeff[0][1]
				+ bb*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			output[table[r]]++;
			output[256+table[g]]++;
			output[512+table[b]]++;
			srcoffset+=input->pixelsize;
		}
	}
	return;
}

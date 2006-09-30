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
#include <lcms.h>
#include "color.h"
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "rs-render.h"

#define DECL_RENDER(func) \
	void func(RS_PHOTO *photo, gint width, gint height, gushort *in, \
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile);

DECL_RENDER(rs_render_cms)
DECL_RENDER(rs_render_nocms)

#if defined (__i386__) || defined (__x86_64__)
DECL_RENDER(rs_render_cms_sse)
DECL_RENDER(rs_render_cms_3dnow)
#endif

#if defined (__i386__) || defined (__x86_64__)
DECL_RENDER(rs_render_nocms_sse)
DECL_RENDER(rs_render_nocms_3dnow)
#endif

void rs_render_histogram_table_c(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table);

#if defined (__i386__) || defined (__x86_64__)
void rs_render_histogram_table_cmov(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table);
#endif

guchar previewtable8[65536];
gushort previewtable16[65536];

void
rs_render_select(gboolean cms)
{
#if defined (__i386__) ||  defined (__x86_64__)
	extern gint cpuflags;
#endif

	rs_render_previewtable(1.0); /* make sure previewtables are ready */

	if (cms)
	{
#if defined (__i386__) || defined (__x86_64__)
		if (cpuflags & _SSE)
			rs_render = rs_render_cms_sse;
		else if (cpuflags & _3DNOW)
			rs_render = rs_render_cms_3dnow;
		else
#endif
			rs_render = rs_render_cms;
	}
	else
	{
#if defined (__i386__) || defined (__x86_64__)
		if (cpuflags & _SSE)
			rs_render = rs_render_nocms_sse;
		else if (cpuflags & _3DNOW)
			rs_render = rs_render_nocms_3dnow;
		else
#endif
			rs_render = rs_render_nocms;
	}
#if defined( __i386__) || defined (__x86_64__)
	if (cpuflags & _CMOV)
		rs_render_histogram_table = rs_render_histogram_table_cmov;
	else
#endif
		rs_render_histogram_table = rs_render_histogram_table_c;
	return;
}

void
rs_render_previewtable(const gdouble contrast)
{
	register gint n;
	gdouble nd;
	register gint res;
	double gammavalue;
	const double postadd = 0.5 - (contrast/2.0);
	gammavalue = (1.0/GAMMA);

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		nd = pow(nd, gammavalue)*contrast+postadd;

		res = (gint) (nd*255.0);
		_CLAMP255(res);
		previewtable8[n] = res;

		nd = pow(nd, GAMMA);
		res = (gint) (nd*65535.0);
		_CLAMP65535(res);
		previewtable16[n] = res;
	}
}

#if defined (__i386__) || defined (__x86_64__)

void
rs_render_cms_sse(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	register glong r,g,b;
	gint destoffset;
	gint col;
	gfloat top[4] align(16) = {65535.0, 65535.0, 65535.0, 65535.0};
	gfloat mat[12] align(16) = {
		photo->mat.coeff[0][0],
		photo->mat.coeff[1][0]*0.5,
		photo->mat.coeff[2][0],
		photo->mat.coeff[1][0]*0.5,
		photo->mat.coeff[0][1],
		photo->mat.coeff[1][1]*0.5,
		photo->mat.coeff[2][1],
		photo->mat.coeff[1][1]*0.5,
		photo->mat.coeff[0][2],
		photo->mat.coeff[1][2]*0.5,
		photo->mat.coeff[2][2],
		photo->mat.coeff[1][2]*0.5 };
	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		:
		: "r" (mat), "r" (top), "r" (photo->pre_mul)
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
			buffer[destoffset++] = previewtable16[r];
			buffer[destoffset++] = previewtable16[g];
			buffer[destoffset++] = previewtable16[b];
			s += 4;
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+height * out_rowstride, width);
	}
	asm volatile("emms\n\t");
	g_free(buffer);
	return;
}

void
rs_render_cms_3dnow(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint destoffset;
	gint col;
	register glong r=0,g=0,b=0;
	gfloat mat[12] align(8);
	gfloat top[2] align(8);
	mat[0] = photo->mat.coeff[0][0];
	mat[1] = photo->mat.coeff[0][1]*0.5;
	mat[2] = photo->mat.coeff[0][2];
	mat[3] = photo->mat.coeff[0][1]*0.5;
	mat[4] = photo->mat.coeff[1][0];
	mat[5] = photo->mat.coeff[1][1]*0.5;
	mat[6] = photo->mat.coeff[1][2];
	mat[7] = photo->mat.coeff[1][1]*0.5;
	mat[8] = photo->mat.coeff[2][0];
	mat[9] = photo->mat.coeff[2][1]*0.5;
	mat[10] = photo->mat.coeff[2][2];
	mat[11] = photo->mat.coeff[2][1]*0.5;
	top[0] = 65535.0;
	top[1] = 65535.0;
	asm volatile (
		"femms\n\t"
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
		"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
		"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
		:
		: "r" (&photo->pre_mul), "r" (&top)
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
			buffer[destoffset++] = previewtable16[r];
			buffer[destoffset++] = previewtable16[g];
			buffer[destoffset++] = previewtable16[b];
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+height * out_rowstride, width);
	}
	asm volatile ("femms\n\t");
	g_free(buffer);
	return;
}

#endif /* __i386__ || __x86_64__ */

void
rs_render_cms(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_mul[4];
	for(x=0;x<4;x++)
		pre_mul[x] = (gint) (photo->pre_mul[x]*128.0);
	for(y=0 ; y<height ; y++)
	{
		destoffset = 0;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			rr = (in[srcoffset+R]*pre_mul[R])>>7;
			gg = (in[srcoffset+G]*pre_mul[G])>>7;
			bb = (in[srcoffset+B]*pre_mul[B])>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*photo->mati.coeff[0][0]
				+ gg*photo->mati.coeff[0][1]
				+ bb*photo->mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*photo->mati.coeff[1][0]
				+ gg*photo->mati.coeff[1][1]
				+ bb*photo->mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*photo->mati.coeff[2][0]
				+ gg*photo->mati.coeff[2][1]
				+ bb*photo->mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			buffer[destoffset++] = previewtable16[r];
			buffer[destoffset++] = previewtable16[g];
			buffer[destoffset++] = previewtable16[b];
			srcoffset+=in_channels;
		}
		cmsDoTransform((cmsHPROFILE) profile, buffer, out+y * out_rowstride, width);
	}
	g_free(buffer);
	return;
}

#if defined (__i386__) || defined (__x86_64__)

void
rs_render_nocms_sse(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile)
{
	register glong r,g,b;
	gint destoffset;
	gint col;
	gfloat top[4] align(16) = {65535.0, 65535.0, 65535.0, 65535.0};
	gfloat mat[12] align(16) = {
		photo->mat.coeff[0][0],
		photo->mat.coeff[1][0]*0.5,
		photo->mat.coeff[2][0],
		photo->mat.coeff[1][0]*0.5,
		photo->mat.coeff[0][1],
		photo->mat.coeff[1][1]*0.5,
		photo->mat.coeff[2][1],
		photo->mat.coeff[1][1]*0.5,
		photo->mat.coeff[0][2],
		photo->mat.coeff[1][2]*0.5,
		photo->mat.coeff[2][2],
		photo->mat.coeff[1][2]*0.5 };
	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		:
		: "r" (mat), "r" (top), "r" (photo->pre_mul)
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
			d[destoffset++] = previewtable8[r];
			d[destoffset++] = previewtable8[g];
			d[destoffset++] = previewtable8[b];
			s += 4;
		}
	}
	asm volatile("emms\n\t");
	return;
}

void
rs_render_nocms_3dnow(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile)
{
	gint destoffset;
	gint col;
	register glong r=0,g=0,b=0;
	gfloat mat[12] align(8);
	gfloat top[2] align(8);
	mat[0] = photo->mat.coeff[0][0];
	mat[1] = photo->mat.coeff[0][1]*0.5;
	mat[2] = photo->mat.coeff[0][2];
	mat[3] = photo->mat.coeff[0][1]*0.5;
	mat[4] = photo->mat.coeff[1][0];
	mat[5] = photo->mat.coeff[1][1]*0.5;
	mat[6] = photo->mat.coeff[1][2];
	mat[7] = photo->mat.coeff[1][1]*0.5;
	mat[8] = photo->mat.coeff[2][0];
	mat[9] = photo->mat.coeff[2][1]*0.5;
	mat[10] = photo->mat.coeff[2][2];
	mat[11] = photo->mat.coeff[2][1]*0.5;
	top[0] = 65535.0;
	top[1] = 65535.0;
	asm volatile (
		"femms\n\t"
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
		"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
		"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
		:
		: "r" (&photo->pre_mul), "r" (&top)
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
			d[destoffset++] = previewtable8[r];
			d[destoffset++] = previewtable8[g];
			d[destoffset++] = previewtable8[b];
		}
	}
	asm volatile ("femms\n\t");

	return;
}

#endif /* __i386__ || __x86_64__ */

void
rs_render_nocms(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile)
{
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_mul[4];
	for(x=0;x<4;x++)
		pre_mul[x] = (gint) (photo->pre_mul[x]*128.0);
	for(y=0 ; y<height ; y++)
	{
		destoffset = 0;
		srcoffset = y * in_rowstride;
		guchar *d = out + y * out_rowstride;
		for(x=0 ; x<width ; x++)
		{
			rr = (in[srcoffset+R]*pre_mul[R])>>7;
			gg = (in[srcoffset+G]*pre_mul[G])>>7;
			bb = (in[srcoffset+B]*pre_mul[B])>>7;
			_CLAMP65535_TRIPLET(rr,gg,bb);
			r = (rr*photo->mati.coeff[0][0]
				+ gg*photo->mati.coeff[0][1]
				+ bb*photo->mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*photo->mati.coeff[1][0]
				+ gg*photo->mati.coeff[1][1]
				+ bb*photo->mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*photo->mati.coeff[2][0]
				+ gg*photo->mati.coeff[2][1]
				+ bb*photo->mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			d[destoffset++] = previewtable8[r];
			d[destoffset++] = previewtable8[g];
			d[destoffset++] = previewtable8[b];
			srcoffset+=in_channels;
		}
	}
	return;
}

#if defined (__i386__) || defined (__x86_64__)

void
rs_render_histogram_table_cmov(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table)
{
	gint y,x;
	gint srcoffset;
	glong r,g,b,rr,gg,bb;
	gushort *in;
	gint pre_mul[4];

	if (unlikely(input==NULL)) return;

	for(x=0;x<4;x++)
		pre_mul[x] = (gint) (photo->pre_mul[x]*128.0);
	in	= input->pixels;
	for(y=0 ; y<input->h ; y++)
	{
		srcoffset = y * input->rowstride;
		for(x=0 ; x<input->w ; x++)
		{
			rr = (in[srcoffset+R]*pre_mul[R])>>7;
			gg = (in[srcoffset+G]*pre_mul[G])>>7;
			bb = (in[srcoffset+B]*pre_mul[B])>>7;
			_CLAMP65535_TRIPLET_CMOV(rr,gg,bb);
			r = (rr*photo->mati.coeff[0][0]
				+ gg*photo->mati.coeff[0][1]
				+ bb*photo->mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*photo->mati.coeff[1][0]
				+ gg*photo->mati.coeff[1][1]
				+ bb*photo->mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*photo->mati.coeff[2][0]
				+ gg*photo->mati.coeff[2][1]
				+ bb*photo->mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET_CMOV(r,g,b);
			table[previewtable8[r]]++;
			table[256+previewtable8[g]]++;
			table[512+previewtable8[b]]++;
			srcoffset+=input->pixelsize;
		}
	}
	return;
}

#endif

void
rs_render_histogram_table_c(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table)
{
	gint y,x;
	gint srcoffset;
	gint r,g,b,rr,gg,bb;
	gushort *in;
	gint pre_mul[4];

	if (unlikely(input==NULL)) return;

	for(x=0;x<4;x++)
		pre_mul[x] = (gint) (photo->pre_mul[x]*128.0);
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
			r = (rr*photo->mati.coeff[0][0]
				+ gg*photo->mati.coeff[0][1]
				+ bb*photo->mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (rr*photo->mati.coeff[1][0]
				+ gg*photo->mati.coeff[1][1]
				+ bb*photo->mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (rr*photo->mati.coeff[2][0]
				+ gg*photo->mati.coeff[2][1]
				+ bb*photo->mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			table[previewtable8[r]]++;
			table[256+previewtable8[g]]++;
			table[512+previewtable8[b]]++;
			srcoffset+=input->pixelsize;
		}
	}
	return;
}

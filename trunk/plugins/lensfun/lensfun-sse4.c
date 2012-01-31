/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>,
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

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include <lensfun.h>

#if defined(__SSE4_1__)

#include <smmintrin.h>

static gfloat twofiftytwo_ps[4] __attribute__ ((aligned (16))) = {256.0f, 256.0f, 256.0f, 0.0f};
static gint _zero12[4] __attribute__ ((aligned (16))) = {0,1,2,0};

gboolean is_sse4_compiled(void)
{
	return TRUE;
}

void
rs_image16_bilinear_nomeasure_sse4(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
	const gint m_w = (in->w-1);
	const gint m_h = (in->h-1);

	__m128 p0, p1;
	if ((uintptr_t)pos & 15)
	{
		p0 = _mm_loadu_ps(pos);		// y1x1 y0x0
		p1 = _mm_loadu_ps(pos+4);	// ---- y2x2
	} else 
	{
		p0 = _mm_load_ps(pos);		// y1x1 y0x0
		p1 = _mm_load_ps(pos+4);	// ---- y2x2
	}

	// to x2x2 x1x0 
	__m128 xf = _mm_shuffle_ps(p0, p1, _MM_SHUFFLE(0,0,2,0));
	// to y2y2 y1y0
	__m128 yf = _mm_shuffle_ps(p0, p1, _MM_SHUFFLE(1,1,3,1));

	__m128 fl256 = _mm_load_ps(twofiftytwo_ps);
	xf = _mm_mul_ps(xf, fl256);
	yf = _mm_mul_ps(yf, fl256);
	__m128i x = _mm_cvttps_epi32(xf);
	__m128i y = _mm_cvttps_epi32(yf);

	__m128i _m_w = _mm_slli_epi32(_mm_set1_epi32(m_w), 8);
	__m128i _m_h = _mm_slli_epi32(_mm_set1_epi32(m_h), 8);

	/* Clamping */
	x = _mm_min_epi32(x, _m_w);
	y = _mm_min_epi32(y, _m_h);

	__m128i zero = _mm_setzero_si128();
	x = _mm_max_epi32(x, zero);
	y = _mm_max_epi32(y, zero);

	__m128i one = _mm_set1_epi32(1);
	__m128i nx = _mm_add_epi32(one, _mm_srai_epi32(x, 8));
	__m128i ny = _mm_add_epi32(one, _mm_srai_epi32(y, 8));

	/* Check that 'next' pixels are in bounds */
	_m_w = _mm_srai_epi32(_m_w, 8);
	_m_h = _mm_srai_epi32(_m_h, 8);

	nx = _mm_min_epi32(nx, _m_w);
	ny = _mm_min_epi32(ny, _m_h);

	/* Pitch as pixels */
	__m128i pitch = _mm_set1_epi32(in->rowstride >> 2);

	/* Remove remainder */
	__m128i tx = _mm_srai_epi32(x, 8);
	__m128i ty = _mm_srai_epi32(y, 8);
	
	/* Multiply y by pitch */
	ty = _mm_mullo_epi32(ty, pitch);
	ny = _mm_mullo_epi32(ny, pitch);

	/* Add pitch and x offset */
	__m128i a_offset =  _mm_add_epi32(tx, ty);
	__m128i b_offset =  _mm_add_epi32(nx, ty);
	__m128i c_offset =  _mm_add_epi32(tx, ny);
	__m128i d_offset =  _mm_add_epi32(nx, ny);

	/* Multiply by pixelsize and add RGB offsets */
	__m128i zero12 = _mm_load_si128((__m128i*)_zero12);
	a_offset = _mm_add_epi32(zero12, _mm_slli_epi32(a_offset, 2));
	b_offset = _mm_add_epi32(zero12, _mm_slli_epi32(b_offset, 2));
	c_offset = _mm_add_epi32(zero12, _mm_slli_epi32(c_offset, 2));
	d_offset = _mm_add_epi32(zero12, _mm_slli_epi32(d_offset, 2));

#define GETW(a,b) _mm_extract_epi32(a,b)
	gushort* pixels[12];
	
	/* Loop unrolled, allows agressive instruction reordering */
	/* Red, then G & B */
	pixels[0] = in->pixels + GETW(a_offset,0);
	pixels[1] = in->pixels + GETW(b_offset,0);
	pixels[2] = in->pixels + GETW(c_offset,0);
	pixels[3] = in->pixels + GETW(d_offset,0);
		
	pixels[4] = in->pixels + GETW(a_offset,1);
	pixels[5] = in->pixels + GETW(b_offset,1);
	pixels[6] = in->pixels + GETW(c_offset,1);
	pixels[7] = in->pixels + GETW(d_offset,1);

	pixels[8] = in->pixels + GETW(a_offset,2);
	pixels[9] = in->pixels + GETW(b_offset,2);
	pixels[10] = in->pixels + GETW(c_offset,2);
	pixels[11] = in->pixels + GETW(d_offset,2);

	/* Calculate distances */
	__m128i twofiftyfive = _mm_set1_epi32(255);
	__m128i diffx = _mm_and_si128(x, twofiftyfive);	
	__m128i diffy = _mm_and_si128(y, twofiftyfive);	
	__m128i inv_diffx = _mm_andnot_si128(diffx, twofiftyfive);
	__m128i inv_diffy = _mm_andnot_si128(diffy, twofiftyfive);

	/* Calculate weights */
	__m128i aw = _mm_srai_epi32(_mm_mullo_epi16(inv_diffx, inv_diffy),1);
	__m128i bw = _mm_srai_epi32(_mm_mullo_epi16(diffx, inv_diffy),1);
	__m128i cw = _mm_srai_epi32(_mm_mullo_epi16(inv_diffx, diffy),1);
	__m128i dw = _mm_srai_epi32(_mm_mullo_epi16(diffx, diffy),1);

	gushort** p = pixels;
	/* Loop unrolled */
	out[0]  = (gushort) ((GETW(aw,0) * *p[0] + GETW(bw,0) * *p[1] + GETW(cw,0) * *p[2] + GETW(dw,0) * *p[3]  + 16384) >> 15 );
	p+=4;
	out[1]  = (gushort) ((GETW(aw,1) * *p[0] + GETW(bw,1) * *p[1] + GETW(cw,1) * *p[2] + GETW(dw,1) * *p[3]  + 16384) >> 15 );
	p+=4;
	out[2]  = (gushort) ((GETW(aw,2) * *p[0] + GETW(bw,2) * *p[1] + GETW(cw,2) * *p[2] + GETW(dw,2) * *p[3]  + 16384) >> 15 );
#undef GETW
}

#else // NO SSE4

gboolean is_sse4_compiled(void)
{
	return FALSE;
}


void
rs_image16_bilinear_nomeasure_sse4(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
}

#endif // defined (__SSE4_1__)

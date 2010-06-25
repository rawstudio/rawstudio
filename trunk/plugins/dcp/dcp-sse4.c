/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>,
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
 
#include "dcp.h"


#ifdef __SSE4_1__
#include <smmintrin.h>

#include <math.h> /* powf() */

#pragma GCC diagnostic ignored "-Wstrict-aliasing"
/* We ignore this pragma, because we are casting a pointer from float to int to pass a float using */
/* _mm_insert_epi32, since no-one was kind enough to include "insertps xmm, mem32, imm8" */
/* as a valid intrinsic. So we use the integer equivalent instead */

static gfloat _ones_ps[4] __attribute__ ((aligned (16))) = {1.0f, 1.0f, 1.0f, 1.0f};
static gfloat _two_ps[4] __attribute__ ((aligned (16))) = {2.0f, 2.0f, 2.0f, 2.0f};
static gfloat _six_ps[4] __attribute__ ((aligned (16))) = {6.0f-1e-15, 6.0f-1e-15, 6.0f-1e-15, 6.0f-1e-15};
static gfloat _very_small_ps[4] __attribute__ ((aligned (16))) = {1e-15, 1e-15, 1e-15, 1e-15};
static gfloat _mul_hue_ps[4] __attribute__ ((aligned (16))) = {6.0f / 360.0f, 6.0f / 360.0f, 6.0f / 360.0f, 6.0f / 360.0f};
static gint _ones_epi32[4] __attribute__ ((aligned (16))) = {1,1,1,1};
static gfloat _16_bit_ps[4] __attribute__ ((aligned (16))) = {65535.0, 65535.0, 65535.0, 65535.0};

extern void rgb_tone_sse2(__m128* _r, __m128* _g, __m128* _b, const gfloat * const tone_lut);

static inline __m128
sse_matrix3_mul(float* mul, __m128 a, __m128 b, __m128 c)
{

	__m128 v = _mm_load_ps(mul);
	__m128 acc = _mm_mul_ps(a, v);

	v = _mm_load_ps(mul+4);
	acc = _mm_add_ps(acc, _mm_mul_ps(b, v));

	v = _mm_load_ps(mul+8);
	acc = _mm_add_ps(acc, _mm_mul_ps(c, v));

	return acc;
}

static gfloat _rgb_div_ps[4] __attribute__ ((aligned (16))) = {1.0/65535.0, 1.0/65535.0, 1.0/65535.0, 1.0/65535.0};


static inline void
RGBtoHSV_SSE4(__m128 *c0, __m128 *c1, __m128 *c2)
{

	__m128 zero_ps = _mm_setzero_ps();
	__m128 small_ps = _mm_load_ps(_very_small_ps);
	__m128 ones_ps = _mm_load_ps(_ones_ps);
	
	// Any number > 1
	__m128 add_v = _mm_load_ps(_two_ps);

	__m128 r = *c0;
	__m128 g = *c1;
	__m128 b = *c2;
	
	/* Clamp */
	r = _mm_min_ps(_mm_max_ps(r, small_ps),ones_ps);
	g =  _mm_min_ps(_mm_max_ps(g, small_ps),ones_ps);
	b =  _mm_min_ps(_mm_max_ps(b, small_ps),ones_ps);

	__m128 h, v;
	v = _mm_max_ps(b,_mm_max_ps(r,g));

	__m128 m = _mm_min_ps(b,_mm_min_ps(r,g));
	__m128 gap = _mm_sub_ps(v,m);
	__m128 v_mask = _mm_cmpeq_ps(gap, zero_ps);
	v = _mm_add_ps(v, _mm_and_ps(add_v, v_mask));

	h = _mm_setzero_ps();

	/* Set gap to one where sat = 0, this will avoid divisions by zero, these values will not be used */
	ones_ps = _mm_and_ps(ones_ps, v_mask);
	gap = _mm_or_ps(gap, ones_ps);
	/*  gap_inv = 1.0 / gap */
	__m128 gap_inv = _mm_rcp_ps(gap);

	/* if r == v */
	/* h = (g - b) / gap; */
	__m128 mask = _mm_cmpeq_ps(r, v);
	__m128 val = _mm_mul_ps(gap_inv, _mm_sub_ps(g, b));

	/* fill h */
	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_blendv_ps(h, val, mask);

	/* if g == v */
	/* h = 2.0f + (b - r) / gap; */
	__m128 two_ps = _mm_load_ps(_two_ps);
	mask = _mm_cmpeq_ps(g, v);
	val = _mm_sub_ps(b, r);
	val = _mm_mul_ps(val, gap_inv);
	val = _mm_add_ps(val, two_ps);

	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_blendv_ps(h, val, mask);

	/* If (b == v) */
	/* h = 4.0f + (r - g) / gap; */
	__m128 four_ps = _mm_add_ps(two_ps, two_ps);
	mask = _mm_cmpeq_ps(b, v);
	val = _mm_add_ps(four_ps, _mm_mul_ps(gap_inv, _mm_sub_ps(r, g)));

	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_blendv_ps(h, val, mask);

	__m128 s;
	/* Fill s, if gap > 0 */
	v = _mm_sub_ps(v, add_v);
	val = _mm_mul_ps(gap,_mm_rcp_ps(v));
	s = _mm_andnot_ps(v_mask, val );

	/* Check if h < 0 */
	zero_ps = _mm_setzero_ps();
	__m128 six_ps = _mm_load_ps(_six_ps);
	mask = _mm_cmplt_ps(h, zero_ps);
	h = _mm_add_ps(h, _mm_and_ps(mask, six_ps));

	*c0 = h;
	*c1 = s;
	*c2 = v;
}


static inline void
HSVtoRGB_SSE4(__m128 *c0, __m128 *c1, __m128 *c2)
{
	__m128 h = *c0;
	__m128 s = *c1;
	__m128 v = *c2;
	__m128 r, g, b;
	
	/* Convert get the fraction of h
	* h_fraction = h - (float)(int)h */
	__m128 ones_ps = _mm_load_ps(_ones_ps);
	__m128 h_fraction = _mm_sub_ps(h, _mm_floor_ps(h));

	/* p = v * (1.0f - s)  */
	__m128 p = _mm_mul_ps(v,  _mm_sub_ps(ones_ps, s));
	/* q = (v * (1.0f - s * f)) */
	__m128 q = _mm_mul_ps(v, _mm_sub_ps(ones_ps, _mm_mul_ps(s, h_fraction)));
	/* t = (v * (1.0f - s * (1.0f - f))) */
	__m128 t = _mm_mul_ps(v, _mm_sub_ps(ones_ps, _mm_mul_ps(s, _mm_sub_ps(ones_ps, h_fraction))));

	/* h < 1  (case 0)*/
	/* case 0: *r = v; *g = t; *b = p; break; */
	__m128 h_threshold = _mm_add_ps(ones_ps, ones_ps);
	__m128 out_mask = _mm_cmplt_ps(h, ones_ps);
	r = _mm_and_ps(v, out_mask);
	g = _mm_and_ps(t, out_mask);
	b = _mm_and_ps(p, out_mask);

	/* h < 2 (case 1) */
	/* case 1: *r = q; *g = v; *b = p; break; */
	__m128 m = _mm_cmplt_ps(h, h_threshold);
	h_threshold = _mm_add_ps(h_threshold, ones_ps);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_blendv_ps(r, q, m);
	g = _mm_blendv_ps(g, v, m);
	b = _mm_blendv_ps(b, p, m);
	out_mask = _mm_or_ps(out_mask, m);

	/* h < 3 (case 2)*/
	/* case 2: *r = p; *g = v; *b = t; break; */
	m = _mm_cmplt_ps(h, h_threshold);
	h_threshold = _mm_add_ps(h_threshold, ones_ps);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_blendv_ps(r, p, m);
	g = _mm_blendv_ps(g, v, m);
	b = _mm_blendv_ps(b, t, m);
	out_mask = _mm_or_ps(out_mask, m);

	/* h < 4 (case 3)*/
	/* case 3: *r = p; *g = q; *b = v; break; */
	m = _mm_cmplt_ps(h, h_threshold);
	h_threshold = _mm_add_ps(h_threshold, ones_ps);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_blendv_ps(r, p, m);
	g = _mm_blendv_ps(g, q, m);
	b = _mm_blendv_ps(b, v, m);
	out_mask = _mm_or_ps(out_mask, m);

	/* h < 5 (case 4)*/
	/* case 4: *r = t; *g = p; *b = v; break; */
	m = _mm_cmplt_ps(h, h_threshold);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_or_ps(r, _mm_and_ps(t, m));
	g = _mm_or_ps(g, _mm_and_ps(p, m));
	b = _mm_or_ps(b, _mm_and_ps(v, m));
	out_mask = _mm_or_ps(out_mask, m);


	/* Remainder (case 5) */
	/* case 5: *r = v; *g = p; *b = q; break; */
	__m128 all_ones = _mm_cmpeq_ps(h,h);
	m = _mm_xor_ps(out_mask, all_ones);
	r = _mm_blendv_ps(r, v, m);
	g = _mm_blendv_ps(g, p, m);
	b = _mm_blendv_ps(b, q, m);
	
	*c0 = r;
	*c1 = g;
	*c2 = b;
}

#define DW(A) _mm_castps_si128(A)
#define PS(A) _mm_castsi128_ps(A)

static void
huesat_map_SSE4(RSHuesatMap *map, const PrecalcHSM* precalc, __m128 *_h, __m128 *_s, __m128 *_v)
{
	__m128 zero_ps = _mm_setzero_ps();
	__m128 ones_ps = _mm_load_ps(_ones_ps);
	
	__m128 h = *_h;
	__m128 s = *_s;
	__m128 v = *_v;
	
	/* Clamp - H must be pre-clamped*/
	s =  _mm_min_ps(_mm_max_ps(s, zero_ps),ones_ps);
	v =  _mm_min_ps(_mm_max_ps(v, zero_ps),ones_ps);

	const RS_VECTOR3 *tableBase = map->deltas;

	__m128 hueShift;
	__m128 satScale;
	__m128 valScale;

	if (map->val_divisions < 2)
	{
		__m128 hScaled = _mm_mul_ps(h, _mm_load_ps(precalc->hScale));
		__m128 sScaled = _mm_mul_ps(s,  _mm_load_ps(precalc->sScale));

		__m128i maxHueIndex0 = _mm_load_si128((__m128i*)precalc->maxHueIndex0);
		__m128i maxSatIndex0 = _mm_load_si128((__m128i*)precalc->maxSatIndex0);
		__m128i hIndex0 = _mm_cvttps_epi32( hScaled );
		__m128i sIndex0 = _mm_cvttps_epi32( sScaled );

		sIndex0 = _mm_min_epi16(sIndex0, maxSatIndex0);
		__m128i ones_epi32 = _mm_load_si128((__m128i*)_ones_epi32);
		__m128i hIndex1 = _mm_add_epi32(hIndex0, ones_epi32);

		/* if (hIndex0 >= maxHueIndex0) */
		__m128i hIndexMask = _mm_cmpgt_epi32( hIndex0, _mm_sub_epi32(maxHueIndex0, ones_epi32));
		hIndex0 = _mm_andnot_si128(hIndexMask, hIndex0);
		/* hIndex1 = 0; */
		hIndex1 = _mm_andnot_si128(hIndexMask, hIndex1);
		/* hIndex0 = maxHueIndex0 */
		hIndex0 = _mm_or_si128(hIndex0, _mm_and_si128(hIndexMask, maxHueIndex0));

		__m128 hFract1 = _mm_sub_ps( hScaled, _mm_cvtepi32_ps(hIndex0));
		__m128 sFract1 = _mm_sub_ps( sScaled, _mm_cvtepi32_ps(sIndex0));
		__m128 ones_ps = _mm_load_ps(_ones_ps);

		__m128 hFract0 = _mm_sub_ps(ones_ps, hFract1);
		__m128 sFract0 = _mm_sub_ps(ones_ps, sFract1);
		__m128i hueStep = _mm_load_si128((__m128i*)precalc->hueStep);
		__m128i table_offsets = _mm_add_epi32(sIndex0, _mm_mullo_epi16(hIndex0, hueStep));
		__m128i next_offsets = _mm_add_epi32(sIndex0, _mm_mullo_epi16(hIndex1, hueStep));

		const RS_VECTOR3 *entry00[4] = { tableBase + _mm_extract_epi32(table_offsets,0), tableBase + _mm_extract_epi32(table_offsets,1),
			tableBase + _mm_extract_epi32(table_offsets,2), tableBase + _mm_extract_epi32(table_offsets,3) };

		const RS_VECTOR3 *entry01[4] = { tableBase + _mm_extract_epi32(next_offsets,0), tableBase + _mm_extract_epi32(next_offsets,1),
			tableBase + _mm_extract_epi32(next_offsets,2), tableBase + _mm_extract_epi32(next_offsets,3)};

#define LOOK_SINGLE(A,B,C,D) A = _mm_insert_epi32( A, *(gint32*)&C[D]->B, D)
	
#define LOOKUP_FOUR(A, B, C) LOOK_SINGLE(A, B, C, 0);\
			LOOK_SINGLE(A, B, C, 1);\
			LOOK_SINGLE(A, B, C, 2);\
			LOOK_SINGLE(A, B, C, 3);

#define LOOK_SINGLE_ONE(A,B,C,D) A = _mm_insert_epi32( A, *(gint32*)&C[D][1].B, D)

#define LOOKUP_FOUR_ONE(A, B, C) LOOK_SINGLE_ONE(A, B, C, 0);\
			LOOK_SINGLE_ONE(A, B, C, 1);\
			LOOK_SINGLE_ONE(A, B, C, 2);\
			LOOK_SINGLE_ONE(A, B, C, 3);

		/* Initialize to something (will be overwritten) */
		__m128i h00 = _mm_setzero_si128();
		__m128i h01 = _mm_setzero_si128();
		
		LOOKUP_FOUR(h00, fHueShift, entry00);
		LOOKUP_FOUR(h01, fHueShift, entry01);
		__m128 hueShift0 = _mm_add_ps(_mm_mul_ps(PS(h00), hFract0), _mm_mul_ps(PS(h01), hFract1));
		hueShift0 = _mm_mul_ps(hueShift0, sFract0);
		
		LOOKUP_FOUR_ONE(h00, fHueShift, entry00);
		LOOKUP_FOUR_ONE(h01, fHueShift, entry01);
		__m128 hueShift1 = _mm_add_ps(_mm_mul_ps(PS(h00), hFract0), _mm_mul_ps(PS(h01), hFract1));
		hueShift = _mm_add_ps(hueShift0, _mm_mul_ps(hueShift1, sFract1));

		__m128i s00 = _mm_setzero_si128();
		__m128i s01 = _mm_setzero_si128();
		LOOKUP_FOUR(s00, fSatScale, entry00);
		LOOKUP_FOUR(s01, fSatScale, entry01);
		__m128 satScale0 = _mm_add_ps(_mm_mul_ps(PS(s00), hFract0), _mm_mul_ps(PS(s01), hFract1));
		satScale0 = _mm_mul_ps(satScale0, sFract0);
		LOOKUP_FOUR_ONE(s00, fSatScale, entry00);
		LOOKUP_FOUR_ONE(s01, fSatScale, entry01);
		__m128 satScale1 = _mm_add_ps(_mm_mul_ps(PS(s00), hFract0), _mm_mul_ps(PS(s01), hFract1));
		satScale = _mm_add_ps(satScale0, _mm_mul_ps(satScale1, sFract1));

		__m128i v00 = _mm_setzero_si128();
		__m128i v01 = _mm_setzero_si128();
		LOOKUP_FOUR(v00, fValScale, entry00);
		LOOKUP_FOUR(v01, fValScale, entry01);
		__m128 valScale0 = _mm_add_ps(_mm_mul_ps(PS(v00), hFract0), _mm_mul_ps(PS(v01), hFract1));
		valScale0 = _mm_mul_ps(valScale0, sFract0);
		
		LOOKUP_FOUR_ONE(v00, fValScale, entry00);
		LOOKUP_FOUR_ONE(v01, fValScale, entry01);
		__m128 valScale1 = _mm_add_ps(_mm_mul_ps(PS(v00), hFract0), _mm_mul_ps(PS(v01), hFract1));
		valScale = _mm_add_ps(valScale0, _mm_mul_ps(valScale1, sFract1));

	}
	else
	{
		__m128 hScaled = _mm_mul_ps(h, _mm_load_ps(precalc->hScale));
		__m128 sScaled = _mm_mul_ps(s,  _mm_load_ps(precalc->sScale));
		__m128 vScaled = _mm_mul_ps(v,  _mm_load_ps(precalc->vScale));

		__m128i maxHueIndex0 = _mm_load_si128((__m128i*)precalc->maxHueIndex0);
		__m128i maxSatIndex0 = _mm_load_si128((__m128i*)precalc->maxSatIndex0);
		__m128i maxValIndex0 = _mm_load_si128((__m128i*)precalc->maxValIndex0);
		
		__m128i hIndex0 = _mm_cvttps_epi32(hScaled);
		__m128i sIndex0 = _mm_cvttps_epi32(sScaled);
		__m128i vIndex0 = _mm_cvttps_epi32(vScaled);

		// Requires that maxSatIndex0 and sIndex0 can be contained within a 16 bit signed word.
		sIndex0 = _mm_min_epi16(sIndex0, maxSatIndex0);
		vIndex0 = _mm_min_epi16(vIndex0, maxValIndex0);
		__m128i ones_epi32 = _mm_load_si128((__m128i*)_ones_epi32);
		__m128i hIndex1 = _mm_add_epi32(hIndex0, ones_epi32);

		/* if (hIndex0 > (maxHueIndex0 - 1)) */
		__m128i hIndexMask = _mm_cmpgt_epi32( hIndex0, _mm_sub_epi32(maxHueIndex0, ones_epi32));
		/* Make room in hIndex0 */
		hIndex0 = _mm_andnot_si128(hIndexMask, hIndex0);
		/* hIndex1 = 0; */
		hIndex1 = _mm_andnot_si128(hIndexMask, hIndex1);
		/* hIndex0 = maxHueIndex0, where hIndex0 >= (maxHueIndex0) */
		hIndex0 = _mm_or_si128(hIndex0, _mm_and_si128(hIndexMask, maxHueIndex0));

		__m128 hFract1 = _mm_sub_ps( hScaled, _mm_cvtepi32_ps(hIndex0));
		__m128 sFract1 = _mm_sub_ps( sScaled, _mm_cvtepi32_ps(sIndex0));
		__m128 vFract1 = _mm_sub_ps( vScaled, _mm_cvtepi32_ps(vIndex0));
		__m128 ones_ps = _mm_load_ps(_ones_ps);

		__m128 hFract0 = _mm_sub_ps(ones_ps, hFract1);
		__m128 sFract0 = _mm_sub_ps(ones_ps, sFract1);
		__m128 vFract0 = _mm_sub_ps(ones_ps, vFract1);

		__m128i hueStep = _mm_load_si128((__m128i*)precalc->hueStep);
		__m128i valStep = _mm_load_si128((__m128i*)precalc->valStep);

		// This requires that hueStep and valStep can be contained in a 16 bit signed integer.
		__m128i table_offsets = _mm_add_epi32(sIndex0, _mm_mullo_epi16(vIndex0, valStep));
		__m128i next_offsets = _mm_mullo_epi16(hIndex1, hueStep);
		next_offsets = _mm_add_epi32(next_offsets, table_offsets);
		table_offsets = _mm_add_epi32(table_offsets, _mm_mullo_epi16(hIndex0, hueStep));

		gint _valStep = precalc->valStep[0];

		const RS_VECTOR3 *entry00[4] = { tableBase + _mm_extract_epi32(table_offsets,0), tableBase + _mm_extract_epi32(table_offsets,1),
			tableBase + _mm_extract_epi32(table_offsets,2), tableBase + _mm_extract_epi32(table_offsets,3) };
			
		const RS_VECTOR3 *entry10[4] = { entry00[0] + _valStep, entry00[1] + _valStep, entry00[2] + _valStep, entry00[3] + _valStep};

		const RS_VECTOR3 *entry01[4] = { tableBase + _mm_extract_epi32(next_offsets,0), tableBase + _mm_extract_epi32(next_offsets,1),
			tableBase + _mm_extract_epi32(next_offsets,2), tableBase + _mm_extract_epi32(next_offsets,3)};

		const RS_VECTOR3 *entry11[4] = { entry01[0] + _valStep, entry01[1] + _valStep, entry01[2] + _valStep, entry01[3] + _valStep};
		
		/* Initialize to something (will be overwritten) */
		__m128i temp_00 = next_offsets;
		__m128i temp_01 = next_offsets;
		__m128i temp_10 = next_offsets;
		__m128i temp_11 = next_offsets;

		/* Hue first element */
		LOOKUP_FOUR(temp_00, fHueShift, entry00);
		LOOKUP_FOUR(temp_01, fHueShift, entry01);
		LOOKUP_FOUR(temp_10, fHueShift, entry10);
		LOOKUP_FOUR(temp_11, fHueShift, entry11);

		__m128 hueShift0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(PS(temp_00), hFract0), _mm_mul_ps(PS(temp_01), hFract1)));
		__m128 hueShift1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(PS(temp_10), hFract0), _mm_mul_ps(PS(temp_11), hFract1)));
		hueShift = _mm_mul_ps(sFract0, _mm_add_ps(hueShift0, hueShift1));

		/* Hue second element */
		LOOKUP_FOUR_ONE(temp_00, fHueShift, entry00);
		LOOKUP_FOUR_ONE(temp_01, fHueShift, entry01);
		LOOKUP_FOUR_ONE(temp_10, fHueShift, entry10);
		LOOKUP_FOUR_ONE(temp_11, fHueShift, entry11);
		hueShift0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(PS(temp_00), hFract0), _mm_mul_ps(PS(temp_01), hFract1)));
		hueShift1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(PS(temp_10), hFract0), _mm_mul_ps(PS(temp_11), hFract1)));
		hueShift = _mm_add_ps(hueShift, _mm_mul_ps(sFract1, _mm_add_ps(hueShift0, hueShift1)));

		/* Sat first element */
		LOOKUP_FOUR(temp_00, fSatScale, entry00);
		LOOKUP_FOUR(temp_01, fSatScale, entry01);
		LOOKUP_FOUR(temp_10, fSatScale, entry10);
		LOOKUP_FOUR(temp_11, fSatScale, entry11);
		__m128 satScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(PS(temp_00), hFract0), _mm_mul_ps(PS(temp_01), hFract1)));
		__m128 satScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(PS(temp_10), hFract0), _mm_mul_ps(PS(temp_11), hFract1)));
		satScale = _mm_mul_ps(sFract0, _mm_add_ps(satScale0, satScale1));

		/* Sat second element */
		LOOKUP_FOUR_ONE(temp_00, fSatScale, entry00);
		LOOKUP_FOUR_ONE(temp_01, fSatScale, entry01);
		LOOKUP_FOUR_ONE(temp_10, fSatScale, entry10);
		LOOKUP_FOUR_ONE(temp_11, fSatScale, entry11);
		satScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(PS(temp_00), hFract0), _mm_mul_ps(PS(temp_01), hFract1)));
		satScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(PS(temp_10), hFract0), _mm_mul_ps(PS(temp_11), hFract1)));
		satScale = _mm_add_ps(satScale, _mm_mul_ps(sFract1, _mm_add_ps(satScale0, satScale1)));
		
		/* Val first element */
		LOOKUP_FOUR(temp_00, fValScale, entry00);
		LOOKUP_FOUR(temp_01, fValScale, entry01);
		LOOKUP_FOUR(temp_10, fValScale, entry10);
		LOOKUP_FOUR(temp_11, fValScale, entry11);
		__m128 valScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(PS(temp_00), hFract0), _mm_mul_ps(PS(temp_01), hFract1)));
		__m128 valScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(PS(temp_10), hFract0), _mm_mul_ps(PS(temp_11), hFract1)));
		valScale = _mm_mul_ps(sFract0, _mm_add_ps(valScale0, valScale1));

		/* Val second element */
		LOOKUP_FOUR_ONE(temp_00, fValScale, entry00);
		LOOKUP_FOUR_ONE(temp_01, fValScale, entry01);
		LOOKUP_FOUR_ONE(temp_10, fValScale, entry10);
		LOOKUP_FOUR_ONE(temp_11, fValScale, entry11);
		valScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(PS(temp_00), hFract0), _mm_mul_ps(PS(temp_01), hFract1)));
		valScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(PS(temp_10), hFract0), _mm_mul_ps(PS(temp_11), hFract1)));
		valScale = _mm_add_ps(valScale, _mm_mul_ps(sFract1, _mm_add_ps(valScale0, valScale1)));

	}

	__m128 mul_hue = _mm_load_ps(_mul_hue_ps);
	ones_ps = _mm_load_ps(_ones_ps);
	hueShift = _mm_mul_ps(hueShift, mul_hue);
	s = _mm_min_ps(ones_ps, _mm_mul_ps(s, satScale));
	v = _mm_min_ps(ones_ps, _mm_mul_ps(v, valScale));
	h = _mm_add_ps(h, hueShift);
	*_h = h;
	*_s = s;
	*_v = v;
}



#define SETFLOAT4(N, A, B, C, D) float N[4] __attribute__ ((aligned (16))); \
N[0] = D; N[1] = C; N[2] = B; N[3] = A;

#define SETFLOAT4_SAME(N, A) float N[4] __attribute__ ((aligned (16))); \
N[0] = A; N[1] = A; N[2] = A; N[3] = A;

static gfloat _twofiftysix_ps[4] __attribute__ ((aligned (16))) = {255.9999f,255.9999f,255.9999f,255.9999f};


gboolean
render_SSE4(ThreadInfo* t)
{
	RS_IMAGE16 *image = t->tmp;
	RSDcp *dcp = t->dcp;
	gint x, y;
	__m128 h, s, v;
	__m128i p1,p2;
	__m128 p1f, p2f, p3f, p4f;
	__m128 r, g, b, r2, g2, b2;
	__m128i zero;
	
	gboolean do_contrast = (dcp->contrast > 1.001f);
	gboolean do_highrec = (dcp->contrast < 0.999f);
	__m128 hue_add = _mm_set_ps(dcp->hue, dcp->hue, dcp->hue, dcp->hue);
	__m128 sat = _mm_set_ps(dcp->saturation, dcp->saturation, dcp->saturation, dcp->saturation);
	float exposure_simple = MAX(1.0, powf(2.0f, dcp->exposure));
	float __recover_radius = 0.5 * exposure_simple;
	SETFLOAT4_SAME(_inv_recover_radius, 1.0f / __recover_radius);
	SETFLOAT4_SAME(_recover_radius, 1.0 - __recover_radius);
	SETFLOAT4_SAME(_contr_base, 0.5f);
	SETFLOAT4_SAME(_inv_contrast, 1.0f - dcp->contrast);
	int xfer[4] __attribute__ ((aligned (16)));

	SETFLOAT4(_min_cam, 0.0f, dcp->camera_white.z, dcp->camera_white.y, dcp->camera_white.x);
	SETFLOAT4_SAME(_black_minus_radius, dcp->exposure_black - dcp->exposure_radius);
	SETFLOAT4_SAME(_black_plus_radius, dcp->exposure_black + dcp->exposure_radius);
	SETFLOAT4_SAME(_exposure_black, dcp->exposure_black);
	SETFLOAT4_SAME(_exposure_slope, dcp->exposure_slope);
	SETFLOAT4_SAME(_exposure_qscale, dcp->exposure_qscale);
	SETFLOAT4_SAME(_contrast, dcp->contrast);
	SETFLOAT4_SAME(_cm_r, dcp->channelmixer_red);
	SETFLOAT4_SAME(_cm_g, dcp->channelmixer_green);
	SETFLOAT4_SAME(_cm_b, dcp->channelmixer_blue);
	
	float cam_prof[4*4*3] __attribute__ ((aligned (16)));
	for (x = 0; x < 4; x++ ) {
		cam_prof[x] = dcp->camera_to_prophoto.coeff[0][0] * dcp->channelmixer_red;
		cam_prof[x+4] = dcp->camera_to_prophoto.coeff[0][1] * dcp->channelmixer_red;
		cam_prof[x+8] = dcp->camera_to_prophoto.coeff[0][2] * dcp->channelmixer_red;
		cam_prof[12+x] = dcp->camera_to_prophoto.coeff[1][0] * dcp->channelmixer_green;
		cam_prof[12+x+4] = dcp->camera_to_prophoto.coeff[1][1] * dcp->channelmixer_green;
		cam_prof[12+x+8] = dcp->camera_to_prophoto.coeff[1][2] * dcp->channelmixer_green;
		cam_prof[24+x] = dcp->camera_to_prophoto.coeff[2][0] * dcp->channelmixer_blue;
		cam_prof[24+x+4] = dcp->camera_to_prophoto.coeff[2][1] * dcp->channelmixer_blue;
		cam_prof[24+x+8] = dcp->camera_to_prophoto.coeff[2][2]* dcp->channelmixer_blue;
	}


	gint end_x = image->w - (image->w & 3);

	for(y = t->start_y ; y < t->end_y; y++)
	{
		for(x=0; x < end_x; x+=4)
		{
			__m128i* pixel = (__m128i*)GET_PIXEL(image, x, y);

			zero = _mm_setzero_si128();

			/* Convert to float */
			p1 = _mm_load_si128(pixel);
			p2 = _mm_load_si128(pixel + 1);

			/* Unpack to R G B x */
			p2f = _mm_cvtepi32_ps(_mm_unpackhi_epi16(p1, zero));
			p4f = _mm_cvtepi32_ps(_mm_unpackhi_epi16(p2, zero));
			p1f = _mm_cvtepi32_ps(_mm_unpacklo_epi16(p1, zero));
			p3f = _mm_cvtepi32_ps(_mm_unpacklo_epi16(p2, zero));

			/* Normalize to 0 to 1 range */
			__m128 rgb_div = _mm_load_ps(_rgb_div_ps);
			p1f = _mm_mul_ps(p1f, rgb_div);
			p2f = _mm_mul_ps(p2f, rgb_div);
			p3f = _mm_mul_ps(p3f, rgb_div);
			p4f = _mm_mul_ps(p4f, rgb_div);

			if (dcp->use_profile)
			{
				/* Restric to camera white */
				__m128 min_cam = _mm_load_ps(_min_cam);
				p1f = _mm_min_ps(p1f, min_cam);
				p2f = _mm_min_ps(p2f, min_cam);
				p3f = _mm_min_ps(p3f, min_cam);
				p4f = _mm_min_ps(p4f, min_cam);

				/* Convert to planar */
				__m128 g1g0r1r0 = _mm_unpacklo_ps(p1f, p2f);
				__m128 b1b0 = _mm_unpackhi_ps(p1f, p2f);
				__m128 g3g2r3r2 = _mm_unpacklo_ps(p3f, p4f);
				__m128 b3b2 = _mm_unpackhi_ps(p3f, p4f);
				r = _mm_movelh_ps(g1g0r1r0, g3g2r3r2);
				g = _mm_movehl_ps(g3g2r3r2, g1g0r1r0);
				b = _mm_movelh_ps(b1b0, b3b2);

				/* Convert to Prophoto */
				r2 = sse_matrix3_mul(cam_prof, r, g, b);
				g2 = sse_matrix3_mul(&cam_prof[12], r, g, b);
				b2 = sse_matrix3_mul(&cam_prof[24], r, g, b);
			} else
			{
				/* Convert to planar */
				__m128 g1g0r1r0 = _mm_unpacklo_ps(p1f, p2f);
				__m128 b1b0 = _mm_unpackhi_ps(p1f, p2f);
				__m128 g3g2r3r2 = _mm_unpacklo_ps(p3f, p4f);
				__m128 b3b2 = _mm_unpackhi_ps(p3f, p4f);
				r = _mm_movelh_ps(g1g0r1r0, g3g2r3r2);
				g = _mm_movehl_ps(g3g2r3r2, g1g0r1r0);
				b = _mm_movelh_ps(b1b0, b3b2);

				/* Multiply channel mixer */
				r2 = _mm_mul_ps(_mm_load_ps(_cm_r), r);
				g2 = _mm_mul_ps(_mm_load_ps(_cm_g), g);
				b2 = _mm_mul_ps(_mm_load_ps(_cm_b), b);
			}

			RGBtoHSV_SSE4(&r, &g, &b);
			h = r; s = g; v = b;

			if (dcp->huesatmap)
			{
				huesat_map_SSE4(dcp->huesatmap, dcp->huesatmap_precalc, &h, &s, &v);
			}

			/* Saturation */
			__m128 max_val = _mm_load_ps(_ones_ps);
			__m128 min_val = _mm_load_ps(_very_small_ps);
			s = _mm_max_ps(min_val, _mm_min_ps(max_val, _mm_mul_ps(s, sat)));

			/* Hue */
			__m128 six_ps = _mm_load_ps(_six_ps);
			__m128 zero_ps = _mm_setzero_ps();
			h = _mm_add_ps(h, hue_add);

			/* Check if hue > 6 or < 0*/
			__m128 h_mask_gt = _mm_cmpgt_ps(h, six_ps);
			__m128 h_mask_lt = _mm_cmplt_ps(h, zero_ps);
			__m128 six_masked_gt = _mm_and_ps(six_ps, h_mask_gt);
			__m128 six_masked_lt = _mm_and_ps(six_ps, h_mask_lt);
			h = _mm_sub_ps(h, six_masked_gt);
			h = _mm_add_ps(h, six_masked_lt);

			__m128 v_stored = v;
			HSVtoRGB_SSE4(&h, &s, &v);
			r = h; g = s; b = v;
			
			/* Exposure */
			__m128 black_minus_radius = _mm_load_ps(_black_minus_radius);
			__m128 y_r = _mm_sub_ps(r, black_minus_radius);
			__m128 y_g = _mm_sub_ps(g, black_minus_radius);
			__m128 y_b = _mm_sub_ps(b, black_minus_radius);

			__m128 exposure_qscale = _mm_load_ps(_exposure_qscale);
			y_r = _mm_mul_ps(exposure_qscale,_mm_mul_ps(y_r, y_r));
			y_g = _mm_mul_ps(exposure_qscale,_mm_mul_ps(y_g, y_g));
			y_b = _mm_mul_ps(exposure_qscale,_mm_mul_ps(y_b, y_b));

			__m128 exposure_slope = _mm_load_ps(_exposure_slope);
			__m128 exposure_black = _mm_load_ps(_exposure_black);
			__m128 y2_r = _mm_mul_ps(exposure_slope, _mm_sub_ps(r, exposure_black));
			__m128 y2_g = _mm_mul_ps(exposure_slope, _mm_sub_ps(g, exposure_black));
			__m128 y2_b = _mm_mul_ps(exposure_slope, _mm_sub_ps(b, exposure_black));

			__m128 black_plus_radius = _mm_load_ps(_black_plus_radius);
			__m128 r_mask = _mm_cmpgt_ps(r, black_plus_radius);
			__m128 g_mask = _mm_cmpgt_ps(g, black_plus_radius);
			__m128 b_mask = _mm_cmpgt_ps(b, black_plus_radius);
			
			y_r = _mm_blendv_ps(y_r, y2_r, r_mask);
			y_g = _mm_blendv_ps(y_g, y2_g, g_mask);
			y_b = _mm_blendv_ps(y_b, y2_b, b_mask);
			black_minus_radius = _mm_load_ps(_black_minus_radius);
			r_mask = _mm_cmple_ps(r, black_minus_radius);
			g_mask = _mm_cmple_ps(g, black_minus_radius);
			b_mask = _mm_cmple_ps(b, black_minus_radius);
			r = _mm_andnot_ps(r_mask, y_r);
			g = _mm_andnot_ps(g_mask, y_g);
			b = _mm_andnot_ps(b_mask, y_b);

			/* Contrast in gamma 2.0 */
			if (do_contrast)
			{
				__m128 contr_base = _mm_load_ps(_contr_base);
				__m128 contrast = _mm_load_ps(_contrast);
				min_val = _mm_load_ps(_very_small_ps);
				r = _mm_max_ps(r, min_val);
				g = _mm_max_ps(g, min_val);
				b = _mm_max_ps(b, min_val);
				r = _mm_add_ps(_mm_mul_ps(contrast, _mm_sub_ps(_mm_sqrt_ps(r), contr_base)), contr_base);
				g = _mm_add_ps(_mm_mul_ps(contrast, _mm_sub_ps(_mm_sqrt_ps(g), contr_base)), contr_base);
				b = _mm_add_ps(_mm_mul_ps(contrast, _mm_sub_ps(_mm_sqrt_ps(b), contr_base)), contr_base);
				r = _mm_max_ps(r, min_val);
				g = _mm_max_ps(g, min_val);
				b = _mm_max_ps(b, min_val);
				r = _mm_mul_ps(r,r);
				g = _mm_mul_ps(g,g);
				b = _mm_mul_ps(b,b);
			}
			else if (do_highrec)
			{
				max_val = _mm_load_ps(_ones_ps);
				__m128 inv_contrast = _mm_load_ps(_inv_contrast);
				__m128 recover_radius = _mm_load_ps(_recover_radius);
				__m128 inv_recover_radius = _mm_load_ps(_inv_recover_radius);

				/* Distance from 1.0 - radius */
				__m128 dist = _mm_sub_ps(v_stored, recover_radius);
				/* Scale so distance is normalized, clamp */
				__m128 dist_scaled = _mm_min_ps(max_val, _mm_mul_ps(dist, inv_recover_radius));

				__m128 mul_val = _mm_sub_ps(max_val, _mm_mul_ps(dist_scaled, inv_contrast));

				r = _mm_mul_ps(r, mul_val);
				g = _mm_mul_ps(g, mul_val);
				b = _mm_mul_ps(b, mul_val);
			}

			/* Convert to HSV */
			RGBtoHSV_SSE4(&r, &g, &b);
			h = r; s = g; v = b;

			if (!dcp->curve_is_flat)			
			{
				/* Convert v to lookup values and interpolate */
				__m128 v_mul = _mm_mul_ps(v, _mm_load_ps(_twofiftysix_ps));
				__m128i lookup = _mm_cvtps_epi32(v_mul);
				_mm_store_si128((__m128i*)&xfer[0], lookup);

				/* Calculate fractions */
				__m128 frac = _mm_sub_ps(v_mul, _mm_floor_ps(v_mul));
				__m128 inv_frac = _mm_sub_ps(_mm_load_ps(_ones_ps), frac);
				
				/* Load two adjacent curve values and interpolate between them */
				__m128 p0p1 = _mm_castsi128_ps(_mm_loadl_epi64((__m128i*)&dcp->curve_samples[xfer[0]]));
				__m128 p2p3 = _mm_castsi128_ps(_mm_loadl_epi64((__m128i*)&dcp->curve_samples[xfer[2]]));
				p0p1 = _mm_loadh_pi(p0p1, (__m64*)&dcp->curve_samples[xfer[1]]);
				p2p3 = _mm_loadh_pi(p2p3, (__m64*)&dcp->curve_samples[xfer[3]]);
				
				/* Pack all lower values in v0, high in v1 and interpolate */
				__m128 v0 = _mm_shuffle_ps(p0p1, p2p3, _MM_SHUFFLE(2,0,2,0));
				__m128 v1 = _mm_shuffle_ps(p0p1, p2p3, _MM_SHUFFLE(3,1,3,1));
				v = _mm_add_ps(_mm_mul_ps(inv_frac, v0), _mm_mul_ps(frac, v1));
			}

			/* Apply looktable */
			if (dcp->looktable) {
				huesat_map_SSE4(dcp->looktable, dcp->looktable_precalc, &h, &s, &v);
			}
			
			/* Ensure that hue is within range */	
			zero_ps = _mm_setzero_ps();
			h_mask_gt = _mm_cmpgt_ps(h, six_ps);
			h_mask_lt = _mm_cmplt_ps(h, zero_ps);
			six_masked_gt = _mm_and_ps(six_ps, h_mask_gt);
			six_masked_lt = _mm_and_ps(six_ps, h_mask_lt);
			h = _mm_sub_ps(h, six_masked_gt);
			h = _mm_add_ps(h, six_masked_lt);
			
			/* s always slightly > 0 when converting to RGB */
			s = _mm_max_ps(s, min_val);

			HSVtoRGB_SSE4(&h, &s, &v);
			r = h; g = s; b = v;

			/* Apply Tone Curve  in RGB space*/
			if (dcp->tone_curve_lut) 
			{
				rgb_tone_sse2( &r, &g, &b, dcp->tone_curve_lut);
			}

			/* Convert to 16 bit */
			__m128 rgb_mul = _mm_load_ps(_16_bit_ps);
			r = _mm_mul_ps(r, rgb_mul);
			g = _mm_mul_ps(g, rgb_mul);
			b = _mm_mul_ps(b, rgb_mul);
			
			__m128i r_i = _mm_cvtps_epi32(r);
			__m128i g_i = _mm_cvtps_epi32(g);
			__m128i b_i = _mm_cvtps_epi32(b);

			/* Convert to 16 bit unsigned values */
			r_i = _mm_packus_epi32(r_i, r_i);
			b_i = _mm_packus_epi32(b_i, b_i);
			g_i = _mm_packus_epi32(g_i, g_i);

			/* Interleave*/
			__m128i rg_i = _mm_unpacklo_epi16(r_i, g_i);
			__m128i bb_i = _mm_unpacklo_epi16(b_i, b_i);
			p1 = _mm_unpacklo_epi32(rg_i, bb_i);
			p2 = _mm_unpackhi_epi32(rg_i, bb_i);

			/* Store processed pixel */
			_mm_store_si128(pixel, p1);
			_mm_store_si128(pixel + 1, p2);
		}
	}
	return TRUE;
}
#undef DW
#undef PS
#undef SETFLOAT4
#undef SETFLOAT4_SAME

#else	// not defined __SSE4_1__

gboolean render_SSE4(ThreadInfo* t)
{
	return FALSE;
}

#endif

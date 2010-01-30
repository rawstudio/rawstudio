/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

/* Plugin tmpl version 5 */

#include <rawstudio.h>
#include <lcms.h>
#include "rs-cmm.h"
#include "colorspace_transform.h"

#if defined(__SSE2__)

#include <emmintrin.h>


/* SSE2 Polynomial pow function from Mesa3d (MIT License) */

#define EXP_POLY_DEGREE 2

#define POLY0(x, c0) _mm_load_ps(c0)
#define POLY1(x, c0, c1) _mm_add_ps(_mm_mul_ps(POLY0(x, c1), x), _mm_load_ps(c0))
#define POLY2(x, c0, c1, c2) _mm_add_ps(_mm_mul_ps(POLY1(x, c1, c2), x), _mm_load_ps(c0))
#define POLY3(x, c0, c1, c2, c3) _mm_add_ps(_mm_mul_ps(POLY2(x, c1, c2, c3), x), _mm_load_ps(c0))
#define POLY4(x, c0, c1, c2, c3, c4) _mm_add_ps(_mm_mul_ps(POLY3(x, c1, c2, c3, c4), x), _mm_load_ps(c0))
#define POLY5(x, c0, c1, c2, c3, c4, c5) _mm_add_ps(_mm_mul_ps(POLY4(x, c1, c2, c3, c4, c5), x), _mm_load_ps(c0))

static const gfloat exp_p5_0[4] __attribute__ ((aligned (16))) = {9.9999994e-1f, 9.9999994e-1f, 9.9999994e-1f, 9.9999994e-1f};
static const gfloat exp_p5_1[4] __attribute__ ((aligned (16))) = {6.9315308e-1f, 6.9315308e-1f, 6.9315308e-1f, 6.9315308e-1f};
static const gfloat exp_p5_2[4] __attribute__ ((aligned (16))) = {2.4015361e-1f,  2.4015361e-1f,  2.4015361e-1f,  2.4015361e-1f};
static const gfloat exp_p5_3[4] __attribute__ ((aligned (16))) = {5.5826318e-2f, 5.5826318e-2f, 5.5826318e-2f, 5.5826318e-2f};
static const gfloat exp_p5_4[4] __attribute__ ((aligned (16))) = {8.9893397e-3f, 8.9893397e-3f, 8.9893397e-3f, 8.9893397e-3f};
static const gfloat exp_p5_5[4] __attribute__ ((aligned (16))) = {1.8775767e-3f, 1.8775767e-3f, 1.8775767e-3f, 1.8775767e-3f};

static const gfloat exp_p4_0[4] __attribute__ ((aligned (16))) = {1.0000026f, 1.0000026f, 1.0000026f, 1.0000026f};
static const gfloat exp_p4_1[4] __attribute__ ((aligned (16))) = {6.9300383e-1f,  6.9300383e-1f,  6.9300383e-1f,  6.9300383e-1f};
static const gfloat exp_p4_2[4] __attribute__ ((aligned (16))) = {2.4144275e-1f, 2.4144275e-1f, 2.4144275e-1f, 2.4144275e-1f};
static const gfloat exp_p4_3[4] __attribute__ ((aligned (16))) = {5.2011464e-2f, 5.2011464e-2f, 5.2011464e-2f, 5.2011464e-2f};
static const gfloat exp_p4_4[4] __attribute__ ((aligned (16))) = {1.3534167e-2f, 1.3534167e-2f, 1.3534167e-2f, 1.3534167e-2f};

static const gfloat exp_p3_0[4] __attribute__ ((aligned (16))) = {9.9992520e-1f, 9.9992520e-1f, 9.9992520e-1f, 9.9992520e-1f};
static const gfloat exp_p3_1[4] __attribute__ ((aligned (16))) = {6.9583356e-1f, 6.9583356e-1f, 6.9583356e-1f, 6.9583356e-1f};
static const gfloat exp_p3_2[4] __attribute__ ((aligned (16))) = {2.2606716e-1f, 2.2606716e-1f, 2.2606716e-1f, 2.2606716e-1f};
static const gfloat exp_p3_3[4] __attribute__ ((aligned (16))) = {7.8024521e-2f, 7.8024521e-2f, 7.8024521e-2f, 7.8024521e-2f};

static const gfloat exp_p2_0[4] __attribute__ ((aligned (16))) = {1.0017247f, 1.0017247f, 1.0017247f, 1.0017247f};
static const gfloat exp_p2_1[4] __attribute__ ((aligned (16))) = {6.5763628e-1f, 6.5763628e-1f, 6.5763628e-1f, 6.5763628e-1f};
static const gfloat exp_p2_2[4] __attribute__ ((aligned (16))) = {3.3718944e-1f, 3.3718944e-1f, 3.3718944e-1f, 3.3718944e-1f};

static const gfloat _ones_ps[4] __attribute__ ((aligned (16))) = {1.0f, 1.0f, 1.0f, 1.0f};
static const gfloat _one29_ps[4] __attribute__ ((aligned (16))) = {129.00000f, 129.00000f, 129.00000f, 129.00000f};
static const gfloat _minusone27_ps[4] __attribute__ ((aligned (16))) = {-126.99999f, -126.99999f, -126.99999f, -126.99999f};
static const gfloat _half_ps[4] __attribute__ ((aligned (16))) = {0.5f, 0.5f, 0.5f, 0.5f};
static const guint _one27[4] __attribute__ ((aligned (16))) = {127,127,127,127};
	
static inline __m128 
exp2f4(__m128 x)
{
	__m128i ipart;
	__m128 fpart, expipart, expfpart;

	x = _mm_min_ps(x, _mm_load_ps(_one29_ps));
	x = _mm_max_ps(x, _mm_load_ps(_minusone27_ps));

	/* ipart = int(x - 0.5) */
	ipart = _mm_cvtps_epi32(_mm_sub_ps(x, _mm_load_ps(_half_ps)));

	/* fpart = x - ipart */
	fpart = _mm_sub_ps(x, _mm_cvtepi32_ps(ipart));

	/* expipart = (float) (1 << ipart) */
	expipart = _mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(ipart, _mm_load_si128((__m128i*)_one27)), 23));

	/* minimax polynomial fit of 2**x, in range [-0.5, 0.5[ */
#if EXP_POLY_DEGREE == 5
	expfpart = POLY5(fpart, exp_p5_0, exp_p5_1, exp_p5_2, exp_p5_3, exp_p5_4, exp_p5_5);
#elif EXP_POLY_DEGREE == 4
	expfpart = POLY4(fpart, exp_p4_0, exp_p4_1, exp_p4_2, exp_p4_3, exp_p4_4);
#elif EXP_POLY_DEGREE == 3
	expfpart = POLY3(fpart, exp_p3_0, exp_p3_1, exp_p3_2, exp_p3_3);
#elif EXP_POLY_DEGREE == 2
	expfpart = POLY2(fpart, exp_p2_0, exp_p2_1, exp_p2_2);
#else
#error
#endif

	return _mm_mul_ps(expipart, expfpart);
}


#define LOG_POLY_DEGREE 4

static const gfloat log_p5_0[4] __attribute__ ((aligned (16))) = {3.1157899f, 3.1157899f, 3.1157899f, 3.1157899f};
static const gfloat log_p5_1[4] __attribute__ ((aligned (16))) = {-3.3241990f, -3.3241990f, -3.3241990f, -3.3241990f};
static const gfloat log_p5_2[4] __attribute__ ((aligned (16))) = {2.5988452f, 2.5988452f, 2.5988452f, 2.5988452f};
static const gfloat log_p5_3[4] __attribute__ ((aligned (16))) = {-1.2315303f, -1.2315303f, -1.2315303f, -1.2315303f};
static const gfloat log_p5_4[4] __attribute__ ((aligned (16))) = {3.1821337e-1f, 3.1821337e-1f, 3.1821337e-1f, 3.1821337e-1f};
static const gfloat log_p5_5[4] __attribute__ ((aligned (16))) = {-3.4436006e-2f, -3.4436006e-2f, -3.4436006e-2f, -3.4436006e-2f};

static const gfloat log_p4_0[4] __attribute__ ((aligned (16))) = {2.8882704548164776201f, 2.8882704548164776201f, 2.8882704548164776201f, 2.8882704548164776201f};
static const gfloat log_p4_1[4] __attribute__ ((aligned (16))) = {-2.52074962577807006663f, -2.52074962577807006663f, -2.52074962577807006663f, -2.52074962577807006663f};
static const gfloat log_p4_2[4] __attribute__ ((aligned (16))) = {1.48116647521213171641f, 1.48116647521213171641f, 1.48116647521213171641f, 1.48116647521213171641f};
static const gfloat log_p4_3[4] __attribute__ ((aligned (16))) = {-0.465725644288844778798f, -0.465725644288844778798f,-0.465725644288844778798f, -0.465725644288844778798f};
static const gfloat log_p4_4[4] __attribute__ ((aligned (16))) = {0.0596515482674574969533f, 0.0596515482674574969533f, 0.0596515482674574969533f, 0.0596515482674574969533f};

static const gfloat log_p3_0[4] __attribute__ ((aligned (16))) = {2.61761038894603480148f, 2.61761038894603480148f, 2.61761038894603480148f, 2.61761038894603480148f};
static const gfloat log_p3_1[4] __attribute__ ((aligned (16))) = {-1.75647175389045657003f, -1.75647175389045657003f, -1.75647175389045657003f, -1.75647175389045657003f};
static const gfloat log_p3_2[4] __attribute__ ((aligned (16))) = {0.688243882994381274313f, 0.688243882994381274313f, 0.688243882994381274313f, 0.688243882994381274313f};
static const gfloat log_p3_3[4] __attribute__ ((aligned (16))) = {-0.107254423828329604454f, -0.107254423828329604454f, -0.107254423828329604454f, -0.107254423828329604454f};

static const gfloat log_p2_0[4] __attribute__ ((aligned (16))) = {2.28330284476918490682f, 2.28330284476918490682f, 2.28330284476918490682f, 2.28330284476918490682f};
static const gfloat log_p2_1[4] __attribute__ ((aligned (16))) = {-1.04913055217340124191f, -1.04913055217340124191f, -1.04913055217340124191f, -1.04913055217340124191f};
static const gfloat log_p2_2[4] __attribute__ ((aligned (16))) = {0.204446009836232697516f, 0.204446009836232697516f, 0.204446009836232697516f, 0.204446009836232697516f};

static const guint _exp_mask[4] __attribute__ ((aligned (16))) = {0x7F800000,0x7F800000,0x7F800000,0x7F800000};
static const guint _mantissa_mask[4] __attribute__ ((aligned (16))) = {0x007FFFFF,0x007FFFFF,0x007FFFFF,0x007FFFFF};

static inline __m128 
log2f4(__m128 x)
{
	__m128i exp = _mm_load_si128((__m128i*)_exp_mask);
	__m128i mant = _mm_load_si128((__m128i*)_mantissa_mask);
	__m128 one = _mm_load_ps(_ones_ps);
	__m128i i = _mm_castps_si128(x);
	__m128 e = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_srli_epi32(_mm_and_si128(i, exp), 23), _mm_load_si128((__m128i*)_one27)));
	__m128 m = _mm_or_ps(_mm_castsi128_ps(_mm_and_si128(i, mant)), one);
	__m128 p;

	/* Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[ */
#if LOG_POLY_DEGREE == 6
	p = POLY5( m, log_p5_0, log_p5_1, log_p5_2, log_p5_3, log_p5_4, log_p5_5);
#elif LOG_POLY_DEGREE == 5
	p = POLY4(m, log_p4_0, log_p4_1, log_p4_2, log_p4_3, log_p4_4);
#elif LOG_POLY_DEGREE == 4
	p = POLY3(m, log_p3_0, log_p3_1, log_p3_2, log_p3_3);
#elif LOG_POLY_DEGREE == 3
	p = POLY2(m, log_p2_0, log_p2_1, log_p2_2);
#else
#error
#endif

	/* This effectively increases the polynomial degree by one, but ensures that log2(1) == 0*/
	p = _mm_mul_ps(p, _mm_sub_ps(m, one));

	return _mm_add_ps(p, e);
}

static inline __m128
_mm_fastpow_ps(__m128 x, __m128 y)
{
	return exp2f4(_mm_mul_ps(log2f4(x), y));
}

/* END: SSE2 Polynomial pow function from Mesa3d (MIT License) */


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


static const gfloat _junction_ps[4] __attribute__ ((aligned (16))) = {0.0031308, 0.0031308, 0.0031308, 0.0031308};
static const gfloat _normalize[4] __attribute__ ((aligned (16))) = {1.0f/65535.0f, 1.0f/65535.0f, 1.0f/65535.0f, 1.0f/65535.0f};
static const gfloat _8bit[4] __attribute__ ((aligned (16))) = {255.5f, 255.5f, 255.5f, 255.5f};
static const gfloat _srb_mul_under[4] __attribute__ ((aligned (16))) = {12.92f, 12.92f, 12.92f, 12.92f};
static const gfloat _srb_mul_over[4] __attribute__ ((aligned (16))) = {1.055f, 1.055f, 1.055f, 1.055f};
static const gfloat _srb_sub_over[4] __attribute__ ((aligned (16))) = {0.055f, 0.055f, 0.055f, 0.055f};
static const gfloat _srb_pow_over[4] __attribute__ ((aligned (16))) = {1.0/2.4, 1.0/2.4, 1.0/2.4, 1.0/2.4};
static const guint _alpha_mask[4] __attribute__ ((aligned (16))) = {0xff000000,0xff000000,0xff000000,0xff000000};

void
transform8_srgb_sse2(ThreadInfo* t)
{
	RS_IMAGE16 *input = t->input;
	GdkPixbuf *output = t->output;
	RS_MATRIX3 *matrix = t->matrix;
	gint x,y;
	gint width;

	float mat_ps[4*4*3] __attribute__ ((aligned (16)));
	for (x = 0; x < 4; x++ ) {
		mat_ps[x] = matrix->coeff[0][0];
		mat_ps[x+4] = matrix->coeff[0][1];
		mat_ps[x+8] = matrix->coeff[0][2];
		mat_ps[12+x] = matrix->coeff[1][0];
		mat_ps[12+x+4] = matrix->coeff[1][1];
		mat_ps[12+x+8] = matrix->coeff[1][2];
		mat_ps[24+x] = matrix->coeff[2][0];
		mat_ps[24+x+4] = matrix->coeff[2][1];
		mat_ps[24+x+8] = matrix->coeff[2][2];
	}
	
	int start_x = t->start_x;
	/* Always have aligned input and output adress */
	if (start_x & 3)
		start_x = ((start_x) / 4) * 4;
	
	int complete_w = t->end_x - start_x;
	/* If width is not multiple of 4, check if we can extend it a bit */
	if (complete_w & 3)
	{
		if ((t->end_x+4) < input->w)
			complete_w = (((complete_w + 3) / 4) * 4);
	}
	
	for(y=t->start_y ; y<t->end_y ; y++)
	{
		gushort *i = GET_PIXEL(input, start_x, y);
		guchar *o = GET_PIXBUF_PIXEL(output, start_x, y);
		gboolean aligned_write = !((guintptr)(o)&0xf);

		width = complete_w >> 2;

		while(width--)
		{
			/* Load and convert to float */
			__m128i zero = _mm_setzero_si128();
			__m128i in = _mm_load_si128((__m128i*)i); // Load two pixels
			__m128i in2 = _mm_load_si128((__m128i*)i+1); // Load two pixels
			_mm_prefetch(i + 64, _MM_HINT_NTA);
			__m128i p1 =_mm_unpacklo_epi16(in, zero);
			__m128i p2 =_mm_unpackhi_epi16(in, zero);
			__m128i p3 =_mm_unpacklo_epi16(in2, zero);
			__m128i p4 =_mm_unpackhi_epi16(in2, zero);
			__m128 p1f  = _mm_cvtepi32_ps(p1);
			__m128 p2f  = _mm_cvtepi32_ps(p2);
			__m128 p3f  = _mm_cvtepi32_ps(p3);
			__m128 p4f  = _mm_cvtepi32_ps(p4);
			
			/* Convert to planar */
			__m128 g1g0r1r0 = _mm_unpacklo_ps(p1f, p2f);
			__m128 b1b0 = _mm_unpackhi_ps(p1f, p2f);
			__m128 g3g2r3r2 = _mm_unpacklo_ps(p3f, p4f);
			__m128 b3b2 = _mm_unpackhi_ps(p3f, p4f);
			__m128 r = _mm_movelh_ps(g1g0r1r0, g3g2r3r2);
			__m128 g = _mm_movehl_ps(g3g2r3r2, g1g0r1r0);
			__m128 b = _mm_movelh_ps(b1b0, b3b2);

			/* Apply matrix to convert to sRGB */
			__m128 r2 = sse_matrix3_mul(mat_ps, r, g, b);
			__m128 g2 = sse_matrix3_mul(&mat_ps[12], r, g, b);
			__m128 b2 = sse_matrix3_mul(&mat_ps[24], r, g, b);

			/* Normalize to 0->1 and clamp */
			__m128 normalize = _mm_load_ps(_normalize);
			__m128 max_val = _mm_load_ps(_ones_ps);
			__m128 min_val = _mm_setzero_ps();
			r = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, r2)));
			g = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, g2)));
			b = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, b2)));

			/* Apply Gamma */
			/* Calculate values to be used if larger than junction point */
			__m128 mul_over = _mm_load_ps(_srb_mul_over);
			__m128 sub_over = _mm_load_ps(_srb_sub_over);
			__m128 pow_over = _mm_load_ps(_srb_pow_over);
			__m128 r_gam = _mm_sub_ps(_mm_mul_ps( mul_over, _mm_fastpow_ps(r, pow_over)), sub_over);
			__m128 g_gam = _mm_sub_ps(_mm_mul_ps( mul_over, _mm_fastpow_ps(g, pow_over)), sub_over);
			__m128 b_gam = _mm_sub_ps(_mm_mul_ps( mul_over, _mm_fastpow_ps(b, pow_over)), sub_over);

			/* Create mask for values smaller than junction point */
			__m128 junction = _mm_load_ps(_junction_ps);
			__m128 mask_r = _mm_cmplt_ps(r, junction);
			__m128 mask_g = _mm_cmplt_ps(g, junction);
			__m128 mask_b = _mm_cmplt_ps(b, junction);

			/* Calculate value to be used if under junction */
			__m128 mul_under = _mm_load_ps(_srb_mul_under);
			__m128 r_mul = _mm_and_ps(mask_r, _mm_mul_ps(mul_under, r));
			__m128 g_mul = _mm_and_ps(mask_g, _mm_mul_ps(mul_under, g));
			__m128 b_mul = _mm_and_ps(mask_b, _mm_mul_ps(mul_under, b));

			/* Select the value to be used based on the junction mask and scale to 8 bit */
			__m128 upscale = _mm_load_ps(_8bit);
			r = _mm_mul_ps(upscale, _mm_or_ps(r_mul, _mm_andnot_ps(mask_r, r_gam)));
			g = _mm_mul_ps(upscale, _mm_or_ps(g_mul, _mm_andnot_ps(mask_g, g_gam)));
			b = _mm_mul_ps(upscale, _mm_or_ps(b_mul, _mm_andnot_ps(mask_b, b_gam)));
			
			/* Convert to 8 bit unsigned  and interleave*/
			__m128i r_i = _mm_cvtps_epi32(r);
			__m128i g_i = _mm_cvtps_epi32(g);
			__m128i b_i = _mm_cvtps_epi32(b);
			
			r_i = _mm_packs_epi32(r_i, r_i);
			g_i = _mm_packs_epi32(g_i, g_i);
			b_i = _mm_packs_epi32(b_i, b_i);

			/* Set alpha value to 255 and store */
			__m128i alpha_mask = _mm_load_si128((__m128i*)_alpha_mask);
			__m128i rg_i = _mm_unpacklo_epi16(r_i, g_i);
			__m128i bb_i = _mm_unpacklo_epi16(b_i, b_i);
			p1 = _mm_unpacklo_epi32(rg_i, bb_i);
			p2 = _mm_unpackhi_epi32(rg_i, bb_i);
	
			p1 = _mm_or_si128(alpha_mask, _mm_packus_epi16(p1, p2));

			if (aligned_write)
				_mm_store_si128((__m128i*)o, p1);
			else
				_mm_storeu_si128((__m128i*)o, p1);

			i += 16;
			o += 16;
		}

		/* Process remaining pixels */
		width = complete_w & 3;

		while(width--)
		{
			__m128i zero = _mm_setzero_si128();
			__m128i in = _mm_loadl_epi64((__m128i*)i); // Load one pixel
			__m128i p1 =_mm_unpacklo_epi16(in, zero);
			__m128 p1f  = _mm_cvtepi32_ps(p1);

			/* Splat r,g,b */
			__m128 r =  _mm_shuffle_ps(p1f, p1f, _MM_SHUFFLE(0,0,0,0));
			__m128 g =  _mm_shuffle_ps(p1f, p1f, _MM_SHUFFLE(1,1,1,1));
			__m128 b =  _mm_shuffle_ps(p1f, p1f, _MM_SHUFFLE(2,2,2,2));

			__m128 r2 = sse_matrix3_mul(mat_ps, r, g, b);
			__m128 g2 = sse_matrix3_mul(&mat_ps[12], r, g, b);
			__m128 b2 = sse_matrix3_mul(&mat_ps[24], r, g, b);

			r = _mm_unpacklo_ps(r2, g2);	// RR GG RR GG
			r = _mm_movelh_ps(r, b2);		// RR GG BB BB

			__m128 normalize = _mm_load_ps(_normalize);
			__m128 max_val = _mm_load_ps(_ones_ps);
			__m128 min_val = _mm_setzero_ps();
			r = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, r)));
			__m128 mul_over = _mm_load_ps(_srb_mul_over);
			__m128 sub_over = _mm_load_ps(_srb_sub_over);
			__m128 pow_over = _mm_load_ps(_srb_pow_over);
			__m128 r_gam = _mm_sub_ps(_mm_mul_ps( mul_over, _mm_fastpow_ps(r, pow_over)), sub_over);
			__m128 junction = _mm_load_ps(_junction_ps);
			__m128 mask_r = _mm_cmplt_ps(r, junction);
			__m128 mul_under = _mm_load_ps(_srb_mul_under);
			__m128 r_mul = _mm_and_ps(mask_r, _mm_mul_ps(mul_under, r));
			__m128 upscale = _mm_load_ps(_8bit);
			r = _mm_mul_ps(upscale, _mm_or_ps(r_mul, _mm_andnot_ps(mask_r, r_gam)));
			
			/* Convert to 8 bit unsigned */
			zero = _mm_setzero_si128();
			__m128i r_i = _mm_cvtps_epi32(r);
			/* To 16 bit signed */
			r_i = _mm_packs_epi32(r_i, zero);
			/* To 8 bit unsigned - set alpha channel*/
			__m128i alpha_mask = _mm_load_si128((__m128i*)_alpha_mask);
			r_i = _mm_or_si128(alpha_mask, _mm_packus_epi16(r_i, zero));
			*(int*)o = _mm_cvtsi128_si32(r_i);
			i+=4;
			o+=4;
		}
	}
}


void
transform8_otherrgb_sse2(ThreadInfo* t)
{
	RS_IMAGE16 *input = t->input;
	GdkPixbuf *output = t->output;
	RS_MATRIX3 *matrix = t->matrix;
	gint x,y;
	gint width;

	float mat_ps[4*4*3] __attribute__ ((aligned (16)));
	for (x = 0; x < 4; x++ ) {
		mat_ps[x] = matrix->coeff[0][0];
		mat_ps[x+4] = matrix->coeff[0][1];
		mat_ps[x+8] = matrix->coeff[0][2];
		mat_ps[12+x] = matrix->coeff[1][0];
		mat_ps[12+x+4] = matrix->coeff[1][1];
		mat_ps[12+x+8] = matrix->coeff[1][2];
		mat_ps[24+x] = matrix->coeff[2][0];
		mat_ps[24+x+4] = matrix->coeff[2][1];
		mat_ps[24+x+8] = matrix->coeff[2][2];
	}
	
	int start_x = t->start_x;
	/* Always have aligned input and output adress */
	if (start_x & 3)
		start_x = ((start_x) / 4) * 4;
	
	int complete_w = t->end_x - start_x;
	/* If width is not multiple of 4, check if we can extend it a bit */
	if (complete_w & 3)
	{
		if ((t->end_x+4) < input->w)
			complete_w = ((complete_w+3) / 4 * 4);
	}
	__m128 gamma = _mm_set1_ps(t->output_gamma);

	for(y=t->start_y ; y<t->end_y ; y++)
	{
		gushort *i = GET_PIXEL(input, start_x, y);
		guchar *o = GET_PIXBUF_PIXEL(output, start_x, y);
		gboolean aligned_write = !((guintptr)(o)&0xf);

		width = complete_w >> 2;

		while(width--)
		{
			/* Load and convert to float */
			__m128i zero = _mm_setzero_si128();
			__m128i in = _mm_load_si128((__m128i*)i); // Load two pixels
			__m128i in2 = _mm_load_si128((__m128i*)i+1); // Load two pixels
			_mm_prefetch(i + 64, _MM_HINT_NTA);
			__m128i p1 =_mm_unpacklo_epi16(in, zero);
			__m128i p2 =_mm_unpackhi_epi16(in, zero);
			__m128i p3 =_mm_unpacklo_epi16(in2, zero);
			__m128i p4 =_mm_unpackhi_epi16(in2, zero);
			__m128 p1f  = _mm_cvtepi32_ps(p1);
			__m128 p2f  = _mm_cvtepi32_ps(p2);
			__m128 p3f  = _mm_cvtepi32_ps(p3);
			__m128 p4f  = _mm_cvtepi32_ps(p4);
			
			/* Convert to planar */
			__m128 g1g0r1r0 = _mm_unpacklo_ps(p1f, p2f);
			__m128 b1b0 = _mm_unpackhi_ps(p1f, p2f);
			__m128 g3g2r3r2 = _mm_unpacklo_ps(p3f, p4f);
			__m128 b3b2 = _mm_unpackhi_ps(p3f, p4f);
			__m128 r = _mm_movelh_ps(g1g0r1r0, g3g2r3r2);
			__m128 g = _mm_movehl_ps(g3g2r3r2, g1g0r1r0);
			__m128 b = _mm_movelh_ps(b1b0, b3b2);

			/* Apply matrix to convert to sRGB */
			__m128 r2 = sse_matrix3_mul(mat_ps, r, g, b);
			__m128 g2 = sse_matrix3_mul(&mat_ps[12], r, g, b);
			__m128 b2 = sse_matrix3_mul(&mat_ps[24], r, g, b);

			/* Normalize to 0->1 and clamp */
			__m128 normalize = _mm_load_ps(_normalize);
			__m128 max_val = _mm_load_ps(_ones_ps);
			__m128 min_val = _mm_setzero_ps();
			r = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, r2)));
			g = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, g2)));
			b = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, b2)));

			/* Apply Gamma */
			__m128 upscale = _mm_load_ps(_8bit);
			r = _mm_mul_ps(upscale, _mm_fastpow_ps(r, gamma));
			g = _mm_mul_ps(upscale, _mm_fastpow_ps(g, gamma));
			b = _mm_mul_ps(upscale, _mm_fastpow_ps(b, gamma));

			/* Convert to 8 bit unsigned  and interleave*/
			__m128i r_i = _mm_cvtps_epi32(r);
			__m128i g_i = _mm_cvtps_epi32(g);
			__m128i b_i = _mm_cvtps_epi32(b);
			
			r_i = _mm_packs_epi32(r_i, r_i);
			g_i = _mm_packs_epi32(g_i, g_i);
			b_i = _mm_packs_epi32(b_i, b_i);

			/* Set alpha value to 255 and store */
			__m128i alpha_mask = _mm_load_si128((__m128i*)_alpha_mask);
			__m128i rg_i = _mm_unpacklo_epi16(r_i, g_i);
			__m128i bb_i = _mm_unpacklo_epi16(b_i, b_i);
			p1 = _mm_unpacklo_epi32(rg_i, bb_i);
			p2 = _mm_unpackhi_epi32(rg_i, bb_i);
	
			p1 = _mm_or_si128(alpha_mask, _mm_packus_epi16(p1, p2));

			if (aligned_write)
				_mm_store_si128((__m128i*)o, p1);
			else
				_mm_storeu_si128((__m128i*)o, p1);

			i += 16;
			o += 16;
		}
		/* Process remaining pixels */
		width = complete_w & 3;
		while(width--)
		{
			__m128i zero = _mm_setzero_si128();
			__m128i in = _mm_loadl_epi64((__m128i*)i); // Load two pixels
			__m128i p1 =_mm_unpacklo_epi16(in, zero);
			__m128 p1f  = _mm_cvtepi32_ps(p1);

			/* Splat r,g,b */
			__m128 r =  _mm_shuffle_ps(p1f, p1f, _MM_SHUFFLE(0,0,0,0));
			__m128 g =  _mm_shuffle_ps(p1f, p1f, _MM_SHUFFLE(1,1,1,1));
			__m128 b =  _mm_shuffle_ps(p1f, p1f, _MM_SHUFFLE(2,2,2,2));

			__m128 r2 = sse_matrix3_mul(mat_ps, r, g, b);
			__m128 g2 = sse_matrix3_mul(&mat_ps[12], r, g, b);
			__m128 b2 = sse_matrix3_mul(&mat_ps[24], r, g, b);

			r = _mm_unpacklo_ps(r2, g2);	// GG RR GG RR
			r = _mm_movelh_ps(r, b2);		// BB BB GG RR

			__m128 normalize = _mm_load_ps(_normalize);
			__m128 max_val = _mm_load_ps(_ones_ps);
			__m128 min_val = _mm_setzero_ps();
			r = _mm_min_ps(max_val, _mm_max_ps(min_val, _mm_mul_ps(normalize, r)));
			__m128 upscale = _mm_load_ps(_8bit);
			r = _mm_mul_ps(upscale, _mm_fastpow_ps(r, gamma));
			
			/* Convert to 8 bit unsigned */
			zero = _mm_setzero_si128();
			__m128i r_i = _mm_cvtps_epi32(r);
			/* To 16 bit signed */
			r_i = _mm_packs_epi32(r_i, zero);
			/* To 8 bit unsigned - set alpha channel*/
			__m128i alpha_mask = _mm_load_si128((__m128i*)_alpha_mask);
			r_i = _mm_or_si128(alpha_mask, _mm_packus_epi16(r_i, zero));
			*(int*)o = _mm_cvtsi128_si32(r_i);
			i+=4;
			o+=4;
		}
	}
}

gboolean cst_has_sse2() 
{
	return TRUE;
}

#else // !defined __SSE2__

/* Provide empty functions if not SSE2 compiled to avoid linker errors */

void transform8_sse2(ThreadInfo* t)
{
	/* We should never even get here */
	g_assert(FALSE);
}

void
transform8_otherrgb_sse2(ThreadInfo* t)
{
	/* We should never even get here */
	g_assert(FALSE);
}

gboolean cst_has_sse2() 
{
	return FALSE;
}

#endif
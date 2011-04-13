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
#include <math.h>


/* Special Vertical AVX resampler, that has massive parallism.
 * An important restriction is that "info->dest_offset_other", must result
 * in a 16 byte aligned memory pointer.
 */

typedef struct {
	RS_IMAGE16 *input;			/* Input Image to Resampler */
	RS_IMAGE16 *output;			/* Output Image from Resampler */
	guint old_size;				/* Old dimension in the direction of the resampler*/
	guint new_size;				/* New size in the direction of the resampler */
	guint dest_offset_other;	/* Where in the unchanged direction should we begin writing? */
	guint dest_end_other;		/* Where in the unchanged direction should we stop writing? */
	guint (*resample_support)(void);
	gfloat (*resample_func)(gfloat);
	GThread *threadid;
	gboolean use_compatible;	/* Use compatible resampler if pixelsize != 4 */
	gboolean use_fast;		/* Use nearest neighbour resampler, also compatible*/
} ResampleInfo;

extern void ResizeV(ResampleInfo *info);
extern void ResizeV_fast(ResampleInfo *info);
extern void ResizeV_SSE4(ResampleInfo *info);
static inline guint clampbits(gint x, guint n) { guint32 _y_temp; if( (_y_temp=x>>n) ) x = ~_y_temp >> (32-n); return x;}

static guint
lanczos_taps(void)
{
	return 3;
}

static gfloat
sinc(gfloat value)
{
	if (value != 0.0f)
	{
		value *= M_PI;
		return sinf(value) / value;
	}
	else
		return 1.0f;
}

static gfloat
lanczos_weight(gfloat value)
{
	value = fabsf(value);
	if (value < lanczos_taps())
	{
		return (sinc(value) * sinc(value / lanczos_taps()));
	}
	else
		return 0.0f;
}

const static gint FPScale = 16384; /* fixed point scaler */
const static gint FPScaleShift = 14; /* fixed point scaler */


#if defined (__x86_64__) && defined(__AVX__)
#include <smmintrin.h>

void
ResizeV_AVX(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other * input->pixelsize;
	const guint end_x = info->dest_end_other * input->pixelsize;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);
	gfloat filter_step = MIN(1.0f / pos_step, 1.0f);
	gfloat filter_support = (gfloat) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gfloat pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gfloat total = 0.0;

		/* Ensure that we have a valid position */
		gfloat ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gfloat total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gfloat total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = ((gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5)) & 0xffff;
			
			total2 = total3;
		}
		pos += pos_step;
	}

	guint y,x;
	gint *wg = weights;

	/* 24 pixels = 48 bytes/loop */
	gint end_x_sse = (end_x/24)*24;
	
	/* 0.5 pixel value is lost to rounding times fir_filter_size, compensate */
	gint add_round_sub = fir_filter_size * (FPScale >> 1);
	
	__m128i add_32 = _mm_set_epi32(add_round_sub, add_round_sub, add_round_sub, add_round_sub);

	for (y = 0; y < new_size ; y++)
	{
		gushort *in = GET_PIXEL(input, start_x / input->pixelsize, offsets[y]);
		gushort *out = GET_PIXEL(output, 0, y);
		__m128i zero;
		zero = _mm_setzero_si128();
		for (x = start_x; x <= (end_x_sse-24); x+=24)
		{
			/* Accumulators, set to 0 */
			__m128i acc1, acc2,  acc3, acc1_h, acc2_h, acc3_h;
			acc1 = acc2 = acc3 = acc1_h = acc2_h = acc3_h = zero;

			for (i = 0; i < fir_filter_size; i++) {
				/* Load weight */
				__m128i w = _mm_set_epi32(wg[i],wg[i],wg[i],wg[i]);
				
				/* Load source and prefetch next line */
				int pos = i * input->rowstride;
				__m128i src1i, src2i, src3i;
				__m128i* in_sse =  (__m128i*)&in[pos];
				src1i = _mm_load_si128(in_sse);
				src2i = _mm_load_si128(in_sse+1);
				src3i = _mm_load_si128(in_sse+2);
				_mm_prefetch(&in[pos + 32], _MM_HINT_NTA);
				
				/* Unpack to dwords */
				__m128i src1i_h, src2i_h, src3i_h;
				src1i_h = _mm_unpackhi_epi16(src1i, zero);
				src2i_h = _mm_unpackhi_epi16(src2i, zero);
				src3i_h = _mm_unpackhi_epi16(src3i, zero);
				src1i = _mm_unpacklo_epi16(src1i, zero);
				src2i = _mm_unpacklo_epi16(src2i, zero);
				src3i = _mm_unpacklo_epi16(src3i, zero);
				
				/* Multiply my weight */
				src1i_h = _mm_mullo_epi32(src1i_h, w);
				src2i_h = _mm_mullo_epi32(src2i_h, w);
				src3i_h = _mm_mullo_epi32(src3i_h, w);
				src1i = _mm_mullo_epi32(src1i, w);
				src2i = _mm_mullo_epi32(src2i, w);
				src3i = _mm_mullo_epi32(src3i, w);

				/* Accumulate */
				acc1_h = _mm_add_epi32(acc1_h, src1i_h);
				acc2_h = _mm_add_epi32(acc2_h, src2i_h);
				acc3_h = _mm_add_epi32(acc3_h, src3i_h);
				acc1 = _mm_add_epi32(acc1, src1i);
				acc2 = _mm_add_epi32(acc2, src2i);
				acc3 = _mm_add_epi32(acc3, src3i);
			}
			
			/* Add rounder and subtract 32768 */
			acc1_h = _mm_add_epi32(acc1_h, add_32);
			acc2_h = _mm_add_epi32(acc2_h, add_32);
			acc3_h = _mm_add_epi32(acc3_h, add_32);
			acc1 = _mm_add_epi32(acc1, add_32);
			acc2 = _mm_add_epi32(acc2, add_32);
			acc3 = _mm_add_epi32(acc3, add_32);
			
			/* Shift down */
			acc1_h = _mm_srai_epi32(acc1_h, FPScaleShift);
			acc2_h = _mm_srai_epi32(acc2_h, FPScaleShift);
			acc3_h = _mm_srai_epi32(acc3_h, FPScaleShift);
			acc1 = _mm_srai_epi32(acc1, FPScaleShift);
			acc2 = _mm_srai_epi32(acc2, FPScaleShift);
			acc3 = _mm_srai_epi32(acc3, FPScaleShift);
			
			/* Pack to signed shorts */
			acc1 = _mm_packus_epi32(acc1, acc1_h);
			acc2 = _mm_packus_epi32(acc2, acc2_h);
			acc3 = _mm_packus_epi32(acc3, acc3_h);

			/* Store result */
			__m128i* sse_dst = (__m128i*)&out[x];
			_mm_stream_si128(sse_dst, acc1);
			_mm_stream_si128(sse_dst + 1, acc2);
			_mm_stream_si128(sse_dst + 2, acc3);
			in += 24;
		}

		/* Process remaining pixels */
		for (; x < end_x; x++)
		{
			gint acc1 = 0;
			for (i = 0; i < fir_filter_size; i++)
			{
				acc1 += in[i * input->rowstride] * *(gshort*)&wg[i];
			}
			out[x] = clampbits((acc1 + (FPScale / 2)) >> FPScaleShift, 16);
			in++;
		}
		wg += fir_filter_size;
	}
	_mm_sfence();
	g_free(weights);
	g_free(offsets);
}

#elif defined (__AVX__)
#include <smmintrin.h>

void
ResizeV_AVX(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other * input->pixelsize;
	const guint end_x = info->dest_end_other * input->pixelsize;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);
	gfloat filter_step = MIN(1.0f / pos_step, 1.0f);
	gfloat filter_support = (gfloat) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gfloat pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gfloat total = 0.0;

		/* Ensure that we have a valid position */
		gfloat ok_pos = MAX(0.0f,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gfloat total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gfloat total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = ((gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5)) & 0xffff;
			
			total2 = total3;
		}
		pos += pos_step;
	}

	guint y,x;
	gint *wg = weights;

	/* 8 pixels = 16 bytes/loop */
	gint end_x_sse = (end_x/8)*8;
	
	/* Rounder after accumulation */
	gint add_round_sub = (FPScale >> 1);
	/* 0.5 pixel value is lost to rounding times fir_filter_size, compensate */
	add_round_sub += fir_filter_size * (FPScale >> 1);

	for (y = 0; y < new_size ; y++)
	{
		gushort *in = GET_PIXEL(input, start_x / input->pixelsize, offsets[y]);
		gushort *out = GET_PIXEL(output, 0, y);
		__m128i zero;
		zero = _mm_setzero_si128();
		for (x = start_x; x <= (end_x_sse-8); x+=8)
		{
			/* Accumulators, set to 0 */
			__m128i acc1, acc1_h;
			acc1 = acc1_h = zero;

			for (i = 0; i < fir_filter_size; i++) {
				/* Load weight */
				__m128i w = _mm_set_epi32(wg[i],wg[i],wg[i],wg[i]);
				/* Load source */
				__m128i src1i;
				__m128i* in_sse =  (__m128i*)&in[i * input->rowstride];
				src1i = _mm_load_si128(in_sse);
				_mm_prefetch(in_sse+4, _MM_HINT_NTA);

				/* Unpack to dwords */
				__m128i src1i_h;
				src1i_h = _mm_unpackhi_epi16(src1i, zero);
				src1i = _mm_unpacklo_epi16(src1i, zero);
				
				/* Multiply my weight */
				src1i_h = _mm_mullo_epi32(src1i_h, w);
				src1i = _mm_mullo_epi32(src1i, w);

				/* Accumulate */
				acc1_h = _mm_add_epi32(acc1_h, src1i_h);
				acc1 = _mm_add_epi32(acc1, src1i);
			}
			__m128i add_32 = _mm_set_epi32(add_round_sub, add_round_sub, add_round_sub, add_round_sub);
			
			/* Add rounder and subtract 32768 */
			acc1_h = _mm_add_epi32(acc1_h, add_32);
			acc1 = _mm_add_epi32(acc1, add_32);
			
			/* Shift down */
			acc1_h = _mm_srai_epi32(acc1_h, FPScaleShift);
			acc1 = _mm_srai_epi32(acc1, FPScaleShift);
			
			/* Pack to signed shorts */
			acc1 = _mm_packus_epi32(acc1, acc1_h);

			/* Store result */
			__m128i* sse_dst = (__m128i*)&out[x];
			_mm_store_si128(sse_dst, acc1);
			in += 8;
		}
		
		/* Process remaining pixels */
		for (; x < end_x; x++)
		{
			gint acc1 = 0;
			for (i = 0; i < fir_filter_size; i++)
			{
				acc1 += in[i * input->rowstride] * *(gshort*)&wg[i];
			}
			out[x] = clampbits((acc1 + (FPScale / 2)) >> FPScaleShift, 16);
			in++;
		}
		wg += fir_filter_size;
	}
	g_free(weights);
	g_free(offsets);
}

#else // not defined (__AVX__)

void
ResizeV_AVX(ResampleInfo *info)
{
	ResizeV_SSE4(info);
}

#endif // not defined (__x86_64__) and not defined (__AVX__)



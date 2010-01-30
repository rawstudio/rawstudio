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

#include <rawstudio.h>
#include <string.h> /* memset() */
#include <math.h> /* pow() */

/* FIXME: Stupid hack */
extern void rs_cms_do_transform(gpointer transform, gpointer input, gpointer output, guint size);

static void make_tables(RSColorTransform *rct);
static gboolean select_render(RSColorTransform *rct);

#define LUM_PRECISION 15
#define LUM_FIXED(a) ((guint)((a)*(1<<LUM_PRECISION)))
#define RLUMF LUM_FIXED(0.212671f)
#define GLUMF LUM_FIXED(0.715160f)
#define BLUMF LUM_FIXED(0.072169f)
#define HALFF LUM_FIXED(0.5f)

/* Function pointers - initialized by arch binders */
COLOR_TRANSFORM(*transform_nocms8);
COLOR_TRANSFORM(*transform_cms8);

/* Color transformers */
COLOR_TRANSFORM(transform_null);
COLOR_TRANSFORM(transform_nocms_float);

struct _RSColorTransform {
	GObject parent;
	COLOR_TRANSFORM(*transform_func);
	gdouble gamma;
	gdouble contrast;
	gfloat pre_mul[4] align(16);
	guint bits_per_color;
	guint pixelsize;
	RS_MATRIX4 color_matrix;
	RS_MATRIX4 adobe_matrix;
	guchar *table8;
	gushort *table16;
	gint nknots;
	gfloat *curve_samples;
	void *cms_transform;
};

G_DEFINE_TYPE (RSColorTransform, rs_color_transform, G_TYPE_OBJECT);

static void
rs_color_transform_finalize(GObject *object)
{
	RSColorTransform *rct = RS_COLOR_TRANSFORM(object);

	g_free(rct->curve_samples);
	g_free(rct->table8);
	g_free(rct->table16);

	if (G_OBJECT_CLASS (rs_color_transform_parent_class)->finalize)
		G_OBJECT_CLASS (rs_color_transform_parent_class)->finalize (object);
}

static void
rs_color_transform_class_init(RSColorTransformClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = rs_color_transform_finalize;
}

static void
rs_color_transform_init(RSColorTransform *rct)
{
	gint i;
	rct->transform_func = transform_null;
	/* Initialize with sane values */
	rct->gamma = GAMMA;
	rct->contrast = 1.0;
	for (i=0;i<4;i++)
		rct->pre_mul[i] = 1.0;
	rct->bits_per_color = 8;
	matrix4_identity(&rct->color_matrix);
	matrix4_identity(&rct->adobe_matrix);
	rct->nknots = 0;
	rct->curve_samples = g_new(gfloat, 65536);
	for(i=0;i<65536;i++)
		rct->curve_samples[i] = ((gfloat)i)/65536.0;
	rct->cms_transform = NULL;

	rct->table8 = g_new(guchar, 65536);
	rct->table16 = g_new(gushort, 65536);
	/* Prepare tables */
	make_tables(rct);

	/* Select renderer */
	select_render(rct);
}

RSColorTransform *
rs_color_transform_new()
{
	return g_object_new (RS_TYPE_COLOR_TRANSFORM, NULL);
}

/* Just a simple wrapper */
COLOR_TRANSFORM(rs_color_transform_transform)
{
	rct->transform_func(rct, width, height, in, in_rowstride, out, out_rowstride);
}

/**
 * Set the gamma used for 8 bit output
 * @param rct A RS_COLOR_TRANSFORM
 * @param gamma The desired gamma
 */
gboolean
rs_color_transform_set_gamma(RSColorTransform *rct, gdouble gamma)
{
	g_assert(rct != NULL);

	if (gamma>0.0)
	{
		rct->gamma = gamma;
		make_tables(rct);
		return TRUE;
	}
	else
		return FALSE;

}

/**
 * Sets the contrast, this is applied individually to each R, G and B channel.
 * @param rct A RS_COLOR_TRANSFORM
 * @param contrast The desired contrast, range 0-3
 */
gboolean
rs_color_transform_set_contrast(RSColorTransform *rct, gdouble contrast)
{
	g_assert(rct != NULL);

	if (contrast>0.0)
	{
		rct->contrast = contrast;
		make_tables(rct);
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * Set RGB-multiplers. These will be applied before any color adjustments
 * @param rct A RS_COLOR_TRANSFORM
 * @param premul A gfloat *, {1.0, 1.0, 1.0} will result in no change
 */
gboolean
rs_color_transform_set_premul(RSColorTransform *rct, gfloat *premul)
{
	g_assert(rct != NULL);
	g_assert(premul != NULL);

	if ((premul[R]>0.0) && (premul[G]>0.0) && (premul[B]>0.0) && (premul[G2]>0.0))
	{
		rct->pre_mul[R] = premul[R];
		rct->pre_mul[G] = premul[G];
		rct->pre_mul[B] = premul[B];
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * Sets the color transformation matrix
 * @param rct A RS_COLOR_TRANSFORM
 * @param matrix A pointer to a color matrix
 */
gboolean
rs_color_transform_set_matrix(RSColorTransform *rct, RS_MATRIX4 *matrix)
{
	g_assert(rct != NULL);
	g_assert(matrix != NULL);

	rct->color_matrix = *matrix;
	return TRUE;
}

void
rs_color_transform_set_from_settings(RSColorTransform *rct, RSSettings *settings, const RSSettingsMask mask)
{
	gboolean update_tables = FALSE;

	g_assert(rct != NULL);
	g_assert(RS_IS_SETTINGS(settings));

	if (mask & (MASK_EXPOSURE|MASK_SATURATION|MASK_HUE))
	{
		gfloat exposure;
		gfloat saturation;
		gfloat hue;
		g_object_get(settings, "exposure", &exposure, "saturation", &saturation, "hue", &hue, NULL);

		/* FIXME: this is broken, we should cache the results */
		if (rct->cms_transform)
			matrix4_identity(&rct->color_matrix);
		else
			rct->color_matrix = rct->adobe_matrix;

		matrix4_color_exposure(&rct->color_matrix, exposure);
		matrix4_color_saturate(&rct->color_matrix, saturation);
		matrix4_color_hue(&rct->color_matrix, hue);
	}

	if ((mask & MASK_WB) || (mask & MASK_CHANNELMIXER))
	{
		const gfloat warmth;
		const gfloat tint;
		g_object_get(settings, "warmth", &warmth, "tint", &tint, NULL);


		const gfloat channelmixer_red;
		const gfloat channelmixer_green;
		const gfloat channelmixer_blue;
		g_object_get(settings, 
			     "channelmixer_red", &channelmixer_red,
			     "channelmixer_green", &channelmixer_green,
			     "channelmixer_blue", &channelmixer_blue,
			     NULL);

		rct->pre_mul[R] = (1.0+warmth)*(2.0-tint)*(channelmixer_red*3.0/100.0);
		rct->pre_mul[G] = 1.0*(channelmixer_green*3.0/100.0);
		rct->pre_mul[B] = (1.0-warmth)*(2.0-tint)*(channelmixer_blue*3.0/100.0);
		rct->pre_mul[G2] = 1.0*(channelmixer_green*3.0/100.0);
	}

	if (mask & MASK_CONTRAST)
	{
		gfloat contrast;
		g_object_get(settings, "contrast", &contrast, NULL);

		if (rct->contrast != contrast)
		{
			update_tables = TRUE;
			rct->contrast = contrast;
		}
	}

	if (mask & MASK_CURVE)
	{
		const gint nknots = rs_settings_get_curve_nknots(settings);

		if (nknots > 1)
		{
			gfloat *knots = rs_settings_get_curve_knots(settings);
			if (knots)
			{
				rct->nknots = nknots;
				RSSpline *spline = rs_spline_new(knots, rct->nknots, NATURAL);
				rs_spline_sample(spline, rct->curve_samples, 65536);
				g_object_unref(spline);
				g_free(knots);
				update_tables = TRUE;
			}
		}
		else
		{
			gint i;
			for(i=0;i<65536;i++)
				rct->curve_samples[i] = ((gfloat)i)/65536.0;
		}
	}

	if (update_tables)
		make_tables(rct);
}

/**
 * Sets the output format
 * @param rct A RS_COLOR_TRANSFORM
 * @param bits_per_color The number of bits per color (8 or 16)
 * @return TRUE on success
 */
gboolean
rs_color_transform_set_output_format(RSColorTransform *rct, guint bits_per_color)
{
	gboolean changes = FALSE;
	gboolean ret = FALSE;

	g_assert(rct != NULL);

	if (rct->bits_per_color != bits_per_color)
	{
		changes = TRUE;
		rct->bits_per_color = bits_per_color;
	}

	if (changes)
		ret = select_render(rct);
	else
		ret = TRUE;

	return ret;
}

void
rs_color_transform_set_cms_transform(RSColorTransform *rct, void *transform)
{
	g_assert(rct != NULL);

	rct->cms_transform = transform;
	select_render(rct);
}

void
rs_color_transform_set_adobe_matrix(RSColorTransform *rct, RS_MATRIX4 *matrix)
{
	g_assert(rct != NULL);
	g_assert(matrix != NULL);

	rct->adobe_matrix = *matrix;
}

static void
make_tables(RSColorTransform *rct)
{
	static const gdouble rec65535 = (1.0f / 65536.0f);
	register gint n;
	gdouble nd;
	register gint res;
	const gdouble contrast = rct->contrast + 0.01f; /* magic */
	const gdouble postadd = 0.5f - (contrast/2.0f);
	const gdouble gammavalue = (1.0f/rct->gamma);

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) * rec65535;
		nd = pow(nd, gammavalue);

		if (likely(rct->curve_samples))
			nd = (gdouble) rct->curve_samples[((gint) (nd*65535.0f))];

		nd = nd*contrast+postadd;

		/* 8 bit output */
		if ((rct->bits_per_color == 8) && (rct->cms_transform == NULL))
		{
			res = (gint) (nd*255.0f);
			_CLAMP255(res);
			rct->table8[n] = res;
		}

		/* 16 bit output */
		else if ((rct->bits_per_color == 16) || (rct->cms_transform != NULL))
		{
			nd = pow(nd, rct->gamma);
			res = (gint) (nd*65535.0f);
			_CLAMP65535(res);
			rct->table16[n] = res;
		}
	}

	return;
}

static gboolean
select_render(RSColorTransform *rct)
{
	gboolean ret = FALSE;
	g_assert(rct != NULL);

	/* Start with null renderer, replace if possible */
	rct->transform_func = transform_null;

	if ((rct->bits_per_color == 8) && (rct->cms_transform != NULL))
	{
		rct->transform_func = transform_cms8;
		ret = TRUE;
	}
	else if ((rct->bits_per_color == 8) && (rct->cms_transform == NULL))
	{
		rct->transform_func = transform_nocms8;
		ret = TRUE;
	}
	else if ((rct->bits_per_color == 8) || (rct->bits_per_color == 16))
	{
		rct->transform_func = transform_nocms_float;
		ret = TRUE;
	}
	/* Make sure the appropriate tables are ready for the new renderer */
	make_tables(rct);

	return TRUE;
}

/* Null renderer - doesn't do anything */
COLOR_TRANSFORM(transform_null)
{
}

/** Reference implementation of renderer
*/
COLOR_TRANSFORM(transform_nocms_float)
{
	gint srcoffset;
	register gint x,y;
	register gint r,g,b;
	gfloat r1, r2, g1, g2, b1, b2;

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	for(y=0 ; y<height ; y++)
	{
		guchar *d8 = out + y * out_rowstride;
		gushort *d16 = out + y * out_rowstride;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			/* pre multipliers */
			r1 = in[srcoffset+R] * rct->pre_mul[R];
			g1 = in[srcoffset+G] * rct->pre_mul[G];
			b1 = in[srcoffset+B] * rct->pre_mul[B];

			/* clamp top */
			if (r1>65535.0) r1 = 65535.0;
			if (g1>65535.0) g1 = 65535.0;
			if (b1>65535.0) b1 = 65535.0;

			/* apply color matrix */
			r2 = (gint) (r1*rct->color_matrix.coeff[0][0]
				+ g1*rct->color_matrix.coeff[0][1]
				+ b1*rct->color_matrix.coeff[0][2]);
			g2 = (gint) (r1*rct->color_matrix.coeff[1][0]
				+ g1*rct->color_matrix.coeff[1][1]
				+ b1*rct->color_matrix.coeff[1][2]);
			b2 = (gint) (r1*rct->color_matrix.coeff[2][0]
				+ g1*rct->color_matrix.coeff[2][1]
				+ b1*rct->color_matrix.coeff[2][2]);

			/* we need integers for lookup */
			r = r2;
			g = g2;
			b = b2;

			/* clamp to unsigned short */
			_CLAMP65535_TRIPLET(r,g,b);

			/* look up all colors in gammatable */
			if (unlikely(rct->bits_per_color == 16))
			{
				*d16++ = rct->table16[r];
				*d16++ = rct->table16[g];
				*d16++ = rct->table16[b];
			}
			else
			{
				*d8++ = rct->table8[r];
				*d8++ = rct->table8[g];
				*d8++ = rct->table8[b];
			}

			/* input is always aligned to 64 bits */
			srcoffset += 4;
		}
	}

	return;
}

#if defined (__i386__) || defined (__x86_64__)
static const gfloat top[4] align(16) = {65535.f, 65535.f, 65535.f, 65535.f};

COLOR_TRANSFORM(transform_nocms8_sse)
{
	register glong r,g,b;
	gint destoffset;
	gint col;
	RS_DECLARE_ALIGNED(gfloat, mat, 4, 3, 16);

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	mat[0] = rct->color_matrix.coeff[0][0];
	mat[1] = rct->color_matrix.coeff[1][0];
	mat[2] = rct->color_matrix.coeff[2][0];
	mat[3] = 0.f;

	mat[4] = rct->color_matrix.coeff[0][1];
	mat[5] = rct->color_matrix.coeff[1][1];
	mat[6] = rct->color_matrix.coeff[2][1];
	mat[7] = 0.f;

	mat[8]  = rct->color_matrix.coeff[0][2];
	mat[9]  = rct->color_matrix.coeff[1][2];
	mat[10] = rct->color_matrix.coeff[2][2];
	mat[11] = 0.f;

	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		:
		: "r" (&mat[0]), "r" (&top[0]), "r" (rct->pre_mul)
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

				"mulps %%xmm2, %%xmm0\n\t" /* (R | G | B | _) * premul */
				"maxps %%xmm7, %%xmm0\n\t" /* MAX (0.0, in) */
				"minps %%xmm6, %%xmm0\n\t" /* MIN (65535.0, in) */

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0x0, %%xmm0, %%xmm1\n\t" /* R | R | R | R */
				"mulps %%xmm3, %%xmm1\n\t"
				"addps %%xmm1, %%xmm7\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0x55, %%xmm1, %%xmm1\n\t" /* G | G | G | G */
				"mulps %%xmm4, %%xmm1\n\t"
				"addps %%xmm1, %%xmm7\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0xAA, %%xmm1, %%xmm1\n\t" /* B | B | B | B */
				"mulps %%xmm5, %%xmm1\n\t"
				"addps %%xmm7, %%xmm1\n\t"

				"xorps %%xmm7, %%xmm7\n\t" /* 0 | 0 | 0 | 0 */
				"minps %%xmm6, %%xmm1\n\t" /* MIN (65535.0, in) */
				"maxps %%xmm7, %%xmm1\n\t" /* MAX (0.0, in) */

				/* xmm1: R | G | B | _ */
//				"shufps $0xFF, %%xmm1, %%xmm1\n\t"
				"cvtss2si %%xmm1, %0\n\t"
				"shufps $0xF9, %%xmm1, %%xmm1\n\t" /* xmm1: G | B | _ | _ */
				"cvtss2si %%xmm1, %1\n\t"
				"shufps $0xF9, %%xmm1, %%xmm1\n\t" /* xmm1: B | _ | _ | _ */
				"cvtss2si %%xmm1, %2\n\t"
				: "=r" (r), "=r" (g), "=r" (b)
				: "r" (s)
				: "memory"
			);
			d[destoffset++] = rct->table8[r];
			d[destoffset++] = rct->table8[g];
			d[destoffset++] = rct->table8[b];
			s += 4;
		}
	}
	asm volatile("emms\n\t");
	return;
}

COLOR_TRANSFORM(transform_nocms8_3dnow)
{
	gint destoffset;
	gint col;
	register glong r=0,g=0,b=0;
	RS_DECLARE_ALIGNED(gfloat, mat, 4, 3, 8);

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	mat[0] = rct->color_matrix.coeff[0][0];
	mat[1] = rct->color_matrix.coeff[0][1];
	mat[2] = rct->color_matrix.coeff[0][2];
	mat[3] = 0.f;

	mat[4] = rct->color_matrix.coeff[1][0];
	mat[5] = rct->color_matrix.coeff[1][1];
	mat[6] = rct->color_matrix.coeff[1][2];
	mat[7] = 0.f;

	mat[8]  = rct->color_matrix.coeff[2][0];
	mat[9]  = rct->color_matrix.coeff[2][1];
	mat[10] = rct->color_matrix.coeff[2][2];
	mat[11] = 0.f;

	asm volatile (
		"femms\n\t"
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
		"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
		"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
		:
		: "r" (rct->pre_mul), "r" (&top[0])
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
				: "r" (&mat[0])
			);
			d[destoffset++] = rct->table8[r];
			d[destoffset++] = rct->table8[g];
			d[destoffset++] = rct->table8[b];
		}
	}
	asm volatile ("femms\n\t");

	return;
}

COLOR_TRANSFORM(transform_cms8_sse)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	register glong r,g,b;
	gint destoffset;
	gint col;
	RS_DECLARE_ALIGNED(gfloat, mat, 4, 3, 16);

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	mat[0] = rct->color_matrix.coeff[0][0];
	mat[1] = rct->color_matrix.coeff[1][0];
	mat[2] = rct->color_matrix.coeff[2][0];
	mat[3] = 0.f;

	mat[4] = rct->color_matrix.coeff[0][1];
	mat[5] = rct->color_matrix.coeff[1][1];
	mat[6] = rct->color_matrix.coeff[2][1];
	mat[7] = 0.f;

	mat[8]  = rct->color_matrix.coeff[0][2];
	mat[9]  = rct->color_matrix.coeff[1][2];
	mat[10] = rct->color_matrix.coeff[2][2];
	mat[11] = 0.f;

	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		:
		: "r" (&mat[0]), "r" (&top[0]), "r" (rct->pre_mul)
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
			buffer[destoffset++] = rct->table16[r];
			buffer[destoffset++] = rct->table16[g];
			buffer[destoffset++] = rct->table16[b];
			s += 4;
		}
		rs_cms_do_transform(rct->cms_transform, buffer, out+height * out_rowstride, width);
	}
	asm volatile("emms\n\t");
	g_free(buffer);
	return;
}

COLOR_TRANSFORM(transform_cms8_3dnow)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint destoffset;
	gint col;
	register glong r=0,g=0,b=0;
	RS_DECLARE_ALIGNED(gfloat, mat, 4, 3, 8);

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	mat[0] = rct->color_matrix.coeff[0][0];
	mat[1] = rct->color_matrix.coeff[0][1];
	mat[2] = rct->color_matrix.coeff[0][2];
	mat[3] = 0.f;

	mat[4] = rct->color_matrix.coeff[1][0];
	mat[5] = rct->color_matrix.coeff[1][1];
	mat[6] = rct->color_matrix.coeff[1][2];
	mat[7] = 0.f;

	mat[8]  = rct->color_matrix.coeff[2][0];
	mat[9]  = rct->color_matrix.coeff[2][1];
	mat[10] = rct->color_matrix.coeff[2][2];
	mat[11] = 0.f;

	asm volatile (
		"femms\n\t"
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"movq (%0), %%mm2\n\t" /* pre_mul R | pre_mul G */
		"movq 8(%0), %%mm3\n\t" /* pre_mul B | pre_mul G2 */
		"movq (%1), %%mm6\n\t" /* 65535.0 | 65535.0 */
		:
		: "r" (rct->pre_mul), "r" (&top[0])
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
				: "r" (&mat[0])
			);
			buffer[destoffset++] = rct->table16[r];
			buffer[destoffset++] = rct->table16[g];
			buffer[destoffset++] = rct->table16[b];
		}
		rs_cms_do_transform(rct->cms_transform, buffer, out+height * out_rowstride, width);
	}
	asm volatile ("femms\n\t");
	g_free(buffer);
	return;
}
#endif /* __i386__ || __x86_64__ */

COLOR_TRANSFORM(transform_cms_c)
{
	gushort *buffer = g_malloc(width*3*sizeof(gushort));
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	matrix4_to_matrix4int(&rct->color_matrix, &mati);
	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (rct->pre_mul[x]*128.0);
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
				+ bb*mati.coeff[0][2] + MATRIX_RESOLUTION_ROUNDER)>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2] + MATRIX_RESOLUTION_ROUNDER)>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2] + MATRIX_RESOLUTION_ROUNDER)>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			buffer[destoffset++] = rct->table16[r];
			buffer[destoffset++] = rct->table16[g];
			buffer[destoffset++] = rct->table16[b];
			srcoffset+=4;
		}
		rs_cms_do_transform(rct->cms_transform, buffer, out+y * out_rowstride, width);
	}
	g_free(buffer);
	return;
}

COLOR_TRANSFORM(transform_nocms_c)
{
	gint srcoffset, destoffset;
	register gint x,y;
	register gint r,g,b;
	gint rr,gg,bb;
	gint pre_muli[4];
	RS_MATRIX4Int mati;

	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
		return;

	matrix4_to_matrix4int(&rct->color_matrix, &mati);
	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (rct->pre_mul[x]*128.0);
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
				+ bb*mati.coeff[0][2] + MATRIX_RESOLUTION_ROUNDER)>>MATRIX_RESOLUTION;
			g = (rr*mati.coeff[1][0]
				+ gg*mati.coeff[1][1]
				+ bb*mati.coeff[1][2] + MATRIX_RESOLUTION_ROUNDER)>>MATRIX_RESOLUTION;
			b = (rr*mati.coeff[2][0]
				+ gg*mati.coeff[2][1]
				+ bb*mati.coeff[2][2] + MATRIX_RESOLUTION_ROUNDER)>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			d[destoffset++] = rct->table8[r];
			d[destoffset++] = rct->table8[g];
			d[destoffset++] = rct->table8[b];
			srcoffset+=4;
		}
	}
	return;
}

void
rs_color_transform_make_histogram(RSColorTransform *rct, RS_IMAGE16 *input, guint histogram[4][256])
{
	gint y,x;
	gint srcoffset;
	gint r,g,b,rr,gg,bb;
	gushort *in;
	gint pre_muli[4];
	RS_MATRIX4Int mati;
	gushort *buffer16 = NULL;
	guchar *buffer8 = NULL;
	guint luma;

	/* Check input sanity */
	if (unlikely(rct==NULL)) return;
	if (unlikely(input==NULL)) return;
	if (unlikely(histogram==NULL)) return;

	/* Allocate buffers for CMS if needed */
	if (rct->cms_transform != NULL)
	{
		buffer16 = g_new(gushort, input->w*3);
		buffer8 = g_new(guchar, input->w*3);
	}

	/* Reset table */
	memset(histogram, 0x00, sizeof(guint)*4*256);

	matrix4_to_matrix4int(&rct->color_matrix, &mati);

	for(x=0;x<4;x++)
		pre_muli[x] = (gint) (rct->pre_mul[x]*128.0);
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

			if (rct->cms_transform != NULL)
			{
				buffer16[x*3+R] = r;
				buffer16[x*3+G] = g;
				buffer16[x*3+B] = b;
			}
			else
			{
				histogram[0][rct->table8[r]]++;
				histogram[1][rct->table8[g]]++;
				histogram[2][rct->table8[b]]++;
				luma = (RLUMF*(guint)r
					+ GLUMF*(guint)g
					+ BLUMF*(guint)b
					+ HALFF)>>LUM_PRECISION;
				luma = rct->table8[luma];
				_CLAMP255(luma);
				histogram[3][luma]++;
			}
			srcoffset+=input->pixelsize;
		}
		if (rct->cms_transform != NULL)
		{
			rs_cms_do_transform(rct->cms_transform, buffer16, buffer8, input->w);
			for(x=0 ; x<input->w ; x++)
			{
				histogram[R][buffer8[x*3+R]]++;
				histogram[G][buffer8[x*3+G]]++;
				histogram[B][buffer8[x*3+B]]++;
				luma = (RLUMF*(guint)buffer8[x*3+R]
					+ GLUMF*(guint)buffer8[x*3+G]
					+ BLUMF*(guint)buffer8[x*3+B]
					+ HALFF)>>LUM_PRECISION;
				_CLAMP255(luma);
				histogram[3][luma]++;
			}
		}
	}
	return;
}

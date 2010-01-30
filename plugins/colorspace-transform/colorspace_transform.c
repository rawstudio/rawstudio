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


struct _RSColorspaceTransform {
	RSFilter parent;

	RSCmm *cmm;
};

struct _RSColorspaceTransformClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_colorspace_transform, RSColorspaceTransform)

enum {
	PROP_0,
	PROP_CHANGEME
};

static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static RSFilterResponse *get_image8(RSFilter *filter, const RSFilterRequest *request);
static gboolean convert_colorspace16(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, RS_IMAGE16 *output_image, RSColorSpace *input_space, RSColorSpace *output_space);
static void convert_colorspace8(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, GdkPixbuf *output_image, RSColorSpace *input_space, RSColorSpace *output_space, GdkRectangle *roi);

static RSFilterClass *rs_colorspace_transform_parent_class = NULL;

/* SSE2 optimized functions */
extern void transform8_srgb_sse2(ThreadInfo* t);
extern void transform8_otherrgb_sse2(ThreadInfo* t);
extern gboolean cst_has_sse2();

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_colorspace_transform_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_colorspace_transform_class_init(RSColorspaceTransformClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);

	rs_colorspace_transform_parent_class = g_type_class_peek_parent (klass);

	filter_class->name = "ColorspaceTransform filter";
	filter_class->get_image = get_image;
	filter_class->get_image8 = get_image8;
}

static void
rs_colorspace_transform_init(RSColorspaceTransform *colorspace_transform)
{
	/* FIXME: unref this at some point */
	colorspace_transform->cmm = rs_cmm_new();
	rs_cmm_set_num_threads(colorspace_transform->cmm, rs_get_number_of_processor_cores());
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSColorspaceTransform *colorspace_transform = RS_COLORSPACE_TRANSFORM(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	RSColorSpace *input_space = rs_filter_param_get_object_with_type(RS_FILTER_PARAM(previous_response), "colorspace", RS_TYPE_COLOR_SPACE);
	RSColorSpace *output_space = rs_filter_param_get_object_with_type(RS_FILTER_PARAM(request), "colorspace", RS_TYPE_COLOR_SPACE);


	if (input_space && output_space && (input_space != output_space))
	{
		gfloat premul[4] = { 1.0, 1.0, 1.0, 1.0 };
		rs_filter_param_get_float4(RS_FILTER_PARAM(request), "premul", premul);
		rs_cmm_set_premul(colorspace_transform->cmm, premul);

		output = rs_image16_copy(input, FALSE);

		if (convert_colorspace16(colorspace_transform, input, output, input_space, output_space))
		{
			/* Image was converted */
			response = rs_filter_response_clone(previous_response);
			g_object_unref(previous_response);
			rs_filter_response_set_image(response, output);
			g_object_unref(output);
			g_object_unref(input);
			return response;
		} else
		{
			/* No conversion was needed */
			g_object_unref(input);
			g_object_unref(output);
			return previous_response;
		}
	}
	else
	{
		return previous_response;
	}

}

static RSFilterResponse *
get_image8(RSFilter *filter, const RSFilterRequest *request)
{
	RSColorspaceTransform *colorspace_transform = RS_COLORSPACE_TRANSFORM(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	GdkPixbuf *output = NULL;
	GdkRectangle *roi;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	roi = rs_filter_request_get_roi(request);
	RSColorSpace *input_space = rs_filter_param_get_object_with_type(RS_FILTER_PARAM(previous_response), "colorspace", RS_TYPE_COLOR_SPACE);
	RSColorSpace *output_space = rs_filter_param_get_object_with_type(RS_FILTER_PARAM(request), "colorspace", RS_TYPE_COLOR_SPACE);

#if 0
	printf("\033[33m8 input_space: %s\033[0m\n", (input_space) ? G_OBJECT_TYPE_NAME(input_space) : "none");
	printf("\033[33m8 output_space: %s\n\033[0m", (output_space) ? G_OBJECT_TYPE_NAME(output_space) : "none");
#endif

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, input->w, input->h);

	/* Process output */
	convert_colorspace8(colorspace_transform, input, output, input_space, output_space, roi);

	rs_filter_response_set_image8(response, output);
	g_object_unref(output);
	g_object_unref(input);
	return response;
}

static void
transform8_c(ThreadInfo* t)
{
	gint row;
	gint r,g,b;
	gint width;
	RS_MATRIX3Int mati;
	RS_IMAGE16 *input = t->input;
	GdkPixbuf *output = (GdkPixbuf *)t->output;
	guchar *table8 = t->table8;
	
	g_assert(RS_IS_IMAGE16(input));
	g_assert(GDK_IS_PIXBUF(output));
	gint o_channels = gdk_pixbuf_get_n_channels(output);

	matrix3_to_matrix3int(t->matrix, &mati);

	for(row=t->start_y ; row<t->end_y ; row++)
	{
		gushort *i = GET_PIXEL(input, t->start_x, row);
		guchar *o = GET_PIXBUF_PIXEL(output, t->start_x, row);

		width = t->end_x - t->start_x;

		while(width-- > 0)
		{
			r =
				( i[R] * mati.coeff[0][0]
				+ i[G] * mati.coeff[0][1]
				+ i[B] * mati.coeff[0][2]
				+ MATRIX_RESOLUTION_ROUNDER ) >> MATRIX_RESOLUTION;
			g =
				( i[R] * mati.coeff[1][0]
				+ i[G] * mati.coeff[1][1]
				+ i[B] * mati.coeff[1][2]
				+ MATRIX_RESOLUTION_ROUNDER ) >> MATRIX_RESOLUTION;
			b =
				( i[R] * mati.coeff[2][0]
				+ i[G] * mati.coeff[2][1]
				+ i[B] * mati.coeff[2][2]
				+ MATRIX_RESOLUTION_ROUNDER ) >> MATRIX_RESOLUTION;

			r = CLAMP(r, 0, 65535);
			g = CLAMP(g, 0, 65535);
			b = CLAMP(b, 0, 65535);

			o[R] = table8[r];
			o[G] = table8[g];
			o[B] = table8[b];
			o[3] = 255;

			i += input->pixelsize;
			o += o_channels;
		}
	}
}


static void
transform16_c(gushort* __restrict input, gushort* __restrict output, gint num_pixels, const gint pixelsize, RS_MATRIX3 *matrix)
{
	gint r,g,b;
	RS_MATRIX3Int mati;

	matrix3_to_matrix3int(matrix, &mati);

	while(num_pixels--)
	{
		r =
			( input[R] * mati.coeff[0][0]
			+ input[G] * mati.coeff[0][1]
			+ input[B] * mati.coeff[0][2]
			+ MATRIX_RESOLUTION_ROUNDER ) >> MATRIX_RESOLUTION;
		g =
			( input[R] * mati.coeff[1][0]
			+ input[G] * mati.coeff[1][1]
			+ input[B] * mati.coeff[1][2]
			+ MATRIX_RESOLUTION_ROUNDER ) >> MATRIX_RESOLUTION;
		b =
			( input[R] * mati.coeff[2][0]
			+ input[G] * mati.coeff[2][1]
			+ input[B] * mati.coeff[2][2]
			+ MATRIX_RESOLUTION_ROUNDER ) >> MATRIX_RESOLUTION;

		r = CLAMP(r, 0, 65535);
		g = CLAMP(g, 0, 65535);
		b = CLAMP(b, 0, 65535);

		output[R] = r;
		output[G] = g;
		output[B] = b;

		input += pixelsize;
		output += pixelsize;
	}
}

static gboolean
convert_colorspace16(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, RS_IMAGE16 *output_image, RSColorSpace *input_space, RSColorSpace *output_space)
{
	g_assert(RS_IS_IMAGE16(input_image));
	g_assert(RS_IS_IMAGE16(output_image));
	g_assert(RS_IS_COLOR_SPACE(input_space));
	g_assert(RS_IS_COLOR_SPACE(output_space));

	/* If input/output-image doesn't differ, return no transformation needed */
	if (input_space == output_space)
		return FALSE;

	/* A few sanity checks */
	g_assert(input_image->w == output_image->w);
	g_assert(input_image->h == output_image->h);

	/* If a CMS is needed, do the transformation using LCMS */
	if (RS_COLOR_SPACE_REQUIRES_CMS(input_space) || RS_COLOR_SPACE_REQUIRES_CMS(output_space))
	{
		const RSIccProfile *i, *o;

		i = rs_color_space_get_icc_profile(input_space);
		o = rs_color_space_get_icc_profile(output_space);

		rs_cmm_set_input_profile(colorspace_transform->cmm, i);
		rs_cmm_set_output_profile(colorspace_transform->cmm, o);

		rs_cmm_transform16(colorspace_transform->cmm, input_image, output_image);
	}

	/* If we get here, we can transform using simple vector math */
	else
	{
		const RS_MATRIX3 a = rs_color_space_get_matrix_from_pcs(input_space);
		const RS_MATRIX3 b = rs_color_space_get_matrix_to_pcs(output_space);
		RS_MATRIX3 mat;
		matrix3_multiply(&b, &a, &mat);

		transform16_c(
			GET_PIXEL(input_image, 0, 0),
			GET_PIXEL(output_image, 0, 0),
			input_image->h * input_image->pitch,
			input_image->pixelsize,
			&mat);
	}
	return TRUE;
}

gpointer
start_single_cs8_transform_thread(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	RS_IMAGE16 *input_image = t->input; 
	GdkPixbuf *output = (GdkPixbuf*) t->output;
	RSColorSpace *input_space = t->input_space;
	RSColorSpace *output_space = t->output_space;

	g_assert(RS_IS_IMAGE16(input_image));
	g_assert(GDK_IS_PIXBUF(output));
	g_assert(RS_IS_COLOR_SPACE(input_space));
	g_assert(RS_IS_COLOR_SPACE(output_space));

	gboolean sse2_available = (!!(rs_detect_cpu_features() & RS_CPU_FLAG_SSE2)) && cst_has_sse2();

	if (sse2_available && rs_color_space_new_singleton("RSSrgb") == output_space)
	{
		transform8_srgb_sse2(t);
		return (NULL);
	}
	if (sse2_available && rs_color_space_new_singleton("RSAdobeRGB") == output_space)
	{
		t->output_gamma = 1.0 / 2.19921875;
		transform8_otherrgb_sse2(t);
		return (NULL);
	}
	if (sse2_available && rs_color_space_new_singleton("RSProphoto") == output_space)
	{
		t->output_gamma = 1.0 / 1.8;
		transform8_otherrgb_sse2(t);
		return (NULL);
	}
	
	/* Fall back to C-functions */
	/* Calculate our gamma table */
	const RS1dFunction *input_gamma = rs_color_space_get_gamma_function(input_space);
	const RS1dFunction *output_gamma = rs_color_space_get_gamma_function(output_space);
	guchar table8[65536];
	gint i;
	for(i=0;i<65536;i++)
	{
		gdouble nd = ((gdouble) i) * (1.0/65535.0);

		nd = rs_1d_function_evaluate_inverse(input_gamma, nd);
		nd = rs_1d_function_evaluate(output_gamma, nd);

		/* 8 bit output */
		gint res = (gint) (nd*255.0 + 0.5f);
		_CLAMP255(res);
		table8[i] = res;
	}
	t->table8 = table8;
	transform8_c(t);
	return (NULL);
}

static void
convert_colorspace8(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, GdkPixbuf *output_image, RSColorSpace *input_space, RSColorSpace *output_space, GdkRectangle *_roi)
{
	g_assert(RS_IS_IMAGE16(input_image));
	g_assert(GDK_IS_PIXBUF(output_image));
	g_assert(RS_IS_COLOR_SPACE(input_space));
	g_assert(RS_IS_COLOR_SPACE(output_space));

	/* A few sanity checks */
	g_assert(input_image->w == gdk_pixbuf_get_width(output_image));
	g_assert(input_image->h == gdk_pixbuf_get_height(output_image));

	GdkRectangle *roi = _roi;
	if (!roi) 
	{
		roi = g_new(GdkRectangle, 1);
		roi->x = 0;
		roi->y = 0;
		roi->width = input_image->w;
		roi->height = input_image->h;
	}

	/* If a CMS is needed, do the transformation using LCMS */
	if (RS_COLOR_SPACE_REQUIRES_CMS(input_space) || RS_COLOR_SPACE_REQUIRES_CMS(output_space))
	{
		const RSIccProfile *i, *o;

		i = rs_color_space_get_icc_profile(input_space);
		o = rs_color_space_get_icc_profile(output_space);

		rs_cmm_set_input_profile(colorspace_transform->cmm, i);
		rs_cmm_set_output_profile(colorspace_transform->cmm, o);

		rs_cmm_transform8(colorspace_transform->cmm, input_image, output_image);
	}

	/* If we get here, we can transform using simple vector math and a lookup table */
	else
	{
		const RS_MATRIX3 a = rs_color_space_get_matrix_from_pcs(input_space);
		const RS_MATRIX3 b = rs_color_space_get_matrix_to_pcs(output_space);
		
		RS_MATRIX3 mat;
		matrix3_multiply(&b, &a, &mat);

		gint i;
		guint y_offset, y_per_thread, threaded_h;
		const guint threads = rs_get_number_of_processor_cores();
		ThreadInfo *t = g_new(ThreadInfo, threads);

		threaded_h = roi->height;
		y_per_thread = (threaded_h + threads-1)/threads;
		y_offset = roi->y;

		for (i = 0; i < threads; i++)
		{
			t[i].input = input_image;
			t[i].output = output_image;
			t[i].start_y = y_offset;
			t[i].start_x = roi->x;
			t[i].end_x = roi->x + roi->width;
			t[i].cst = colorspace_transform;
			t[i].input_space = input_space;
			t[i].output_space = output_space;
			y_offset += y_per_thread;
			y_offset = MIN(input_image->h, y_offset);
			t[i].end_y = y_offset;
			t[i].matrix = &mat;
			t[i].table8 = NULL;

			t[i].threadid = g_thread_create(start_single_cs8_transform_thread, &t[i], TRUE, NULL);
		}

		/* Wait for threads to finish */
		for(i = 0; i < threads; i++)
			g_thread_join(t[i].threadid);

		g_free(t);
	}
	/* If we created the ROI here, free it */
	if (!_roi) 
		g_free(roi);
}

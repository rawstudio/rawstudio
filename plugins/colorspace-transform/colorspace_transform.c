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

#define RS_TYPE_COLORSPACE_TRANSFORM (rs_colorspace_transform_type)
#define RS_COLORSPACE_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_COLORSPACE_TRANSFORM, RSColorspaceTransform))
#define RS_COLORSPACE_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_COLORSPACE_TRANSFORM, RSColorspaceTransformClass))
#define RS_IS_COLORSPACE_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_COLORSPACE_TRANSFORM))

typedef struct _RSColorspaceTransform RSColorspaceTransform;
typedef struct _RSColorspaceTransformClass RSColorspaceTransformClass;

struct _RSColorspaceTransform {
	RSFilter parent;

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
static void convert_colorspace16(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, RS_IMAGE16 *output_image, RSColorSpace *input_space, RSColorSpace *output_space);
static void convert_colorspace8(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, GdkPixbuf *output_image, RSColorSpace *input_space, RSColorSpace *output_space);

static RSFilterClass *rs_colorspace_transform_parent_class = NULL;

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

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = rs_image16_copy(input, FALSE);

	convert_colorspace16(colorspace_transform, input, output, input_space, output_space);

	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	/* Process output */

	g_object_unref(input);
	return response;
}

static RSFilterResponse *
get_image8(RSFilter *filter, const RSFilterRequest *request)
{
	RSColorspaceTransform *colorspace_transform = RS_COLORSPACE_TRANSFORM(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	GdkPixbuf *output = NULL;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	RSColorSpace *input_space = rs_filter_param_get_object_with_type(RS_FILTER_PARAM(previous_response), "colorspace", RS_TYPE_COLOR_SPACE);
	RSColorSpace *output_space = rs_filter_param_get_object_with_type(RS_FILTER_PARAM(request), "colorspace", RS_TYPE_COLOR_SPACE);

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, input->w, input->h);

	convert_colorspace8(colorspace_transform, input, output, input_space, output_space);

	rs_filter_response_set_image8(response, output);
	g_object_unref(output);

	/* Process output */

	g_object_unref(input);
	return response;
}

//static cmsHTRANSFORM
//get_lcms_transform16(RSColorspaceTransform *colorspace_transform, RSIccProfile *input_profile, RSIccProfile *output_profile)
//{
//}

static void
transform8_c(RS_IMAGE16 *input, GdkPixbuf *output, RS_MATRIX3 *matrix, guchar *table8)
{
	gint row;
	gint r,g,b;
	gint width;
	RS_MATRIX3Int mati;

	gint o_channels = gdk_pixbuf_get_n_channels(output);

	matrix3_to_matrix3int(matrix, &mati);

	for(row=0 ; row<input->h ; row++)
	{
		gushort *i = GET_PIXEL(input, 0, row);
		guchar *o = GET_PIXBUF_PIXEL(output, 0, row);

		width = input->w;

		while(width--)
		{
			r =
				( i[R] * mati.coeff[0][0]
				+ i[G] * mati.coeff[0][1]
				+ i[B] * mati.coeff[0][2]
				+ 128 ) >> 8;
			g =
				( i[R] * mati.coeff[1][0]
				+ i[G] * mati.coeff[1][1]
				+ i[B] * mati.coeff[1][2]
				+ 128 ) >> 8;
			b =
				( i[R] * mati.coeff[2][0]
				+ i[G] * mati.coeff[2][1]
				+ i[B] * mati.coeff[2][2]
				+ 128 ) >> 8;

			r = CLAMP(r, 0, 65535);
			g = CLAMP(g, 0, 65535);
			b = CLAMP(b, 0, 65535);

			o[R] = table8[r];
			o[G] = table8[g];
			o[B] = table8[b];
			i += input->pixelsize;
			o += o_channels;
		}
	}
}

static void
transform16_c(gushort *input, gushort *output, gint num_pixels, const gint pixelsize, RS_MATRIX3 *matrix)
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
			+ 128 ) >> 8;
		g =
			( input[R] * mati.coeff[1][0]
			+ input[G] * mati.coeff[1][1]
			+ input[B] * mati.coeff[1][2]
			+ 128 ) >> 8;
		b =
			( input[R] * mati.coeff[2][0]
			+ input[G] * mati.coeff[2][1]
			+ input[B] * mati.coeff[2][2]
			+ 128 ) >> 8;

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

static void
convert_colorspace16(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, RS_IMAGE16 *output_image, RSColorSpace *input_space, RSColorSpace *output_space)
{
	g_assert(RS_IS_IMAGE16(input_image));
	g_assert(RS_IS_IMAGE16(output_image) || (output_image == NULL));
	g_assert(RS_IS_COLOR_SPACE(input_space));
	g_assert(RS_IS_COLOR_SPACE(output_space));

	/* Do the transformation inplace if needed */
	if (output_image == NULL)
		output_image = input_image;

	/* If both input/output images and colorspace are the same, return immediately */
	if ((input_image == output_image) && (input_space == output_space))
		return;

	/* A few sanity checks */
	if (input_image->w != output_image->w)
		return;
	if (input_image->h != output_image->h)
		return;

	/* If input/output-image differ, but colorspace is the same, do a simple copy */
	else if (input_space == output_space)
	{
		/* FIXME: Do some sanity checking! */
		memcpy(output_image->pixels, input_image->pixels, input_image->rowstride*input_image->h*2);
	}

	/* If a CMS is needed, do the transformation using LCMS */
	else if (RS_COLOR_SPACE_REQUIRES_CMS(input_space) || RS_COLOR_SPACE_REQUIRES_CMS(output_space))
	{
		g_warning("FIXME: (stub) LCMS support is not implemented yet");
		memcpy(output_image->pixels, input_image->pixels, input_image->rowstride*input_image->h*2);
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
}

static void
convert_colorspace8(RSColorspaceTransform *colorspace_transform, RS_IMAGE16 *input_image, GdkPixbuf *output_image, RSColorSpace *input_space, RSColorSpace *output_space)
{
	g_assert(RS_IS_IMAGE16(input_image));
	g_assert(GDK_IS_PIXBUF(output_image));
	g_assert(RS_IS_COLOR_SPACE(input_space));
	g_assert(RS_IS_COLOR_SPACE(output_space));

	/* A few sanity checks */
	if (input_image->w != gdk_pixbuf_get_width(output_image))
		return;
	if (input_image->h != gdk_pixbuf_get_height(output_image))
		return;

	/* If a CMS is needed, do the transformation using LCMS */
	if (RS_COLOR_SPACE_REQUIRES_CMS(input_space) || RS_COLOR_SPACE_REQUIRES_CMS(output_space))
	{
		g_warning("FIXME: (stub) LCMS support is not implemented yet");
	}

	/* If we get here, we can transform using simple vector math and a lookup table */
//	else
	{
		const RS_MATRIX3 a = rs_color_space_get_matrix_from_pcs(input_space);
		const RS_MATRIX3 b = rs_color_space_get_matrix_to_pcs(output_space);
		RS_MATRIX3 mat;
		matrix3_multiply(&b, &a, &mat);

		/* Calculate our gamma table */
		/* FIXME: Move this to someplace where we can cache it! */
		guchar table8[65536];
		gint i;

		const RS1dFunction *input_gamma = rs_color_space_get_gamma_function(input_space);
		const RS1dFunction *output_gamma = rs_color_space_get_gamma_function(output_space);

		for(i=0;i<65536;i++)
		{
			gdouble nd = ((gdouble) i) * (1.0/65535.0);

			nd = rs_1d_function_evaluate_inverse(input_gamma, nd);
			nd = rs_1d_function_evaluate(output_gamma, nd);

			/* 8 bit output */
			gint res = (gint) (nd*255.0);
			_CLAMP255(res);
			table8[i] = res;
		}

		transform8_c(
			input_image,
			output_image,
			&mat,
			table8);
	}
}

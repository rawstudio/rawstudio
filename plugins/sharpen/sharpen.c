/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

/* Plugin tmpl version 4 */

#include <rawstudio.h>

#define RS_TYPE_SHARPEN (rs_sharpen_type)
#define RS_SHARPEN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_SHARPEN, RSSharpen))
#define RS_SHARPEN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_SHARPEN, RSSharpenClass))
#define RS_IS_SHARPEN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_SHARPEN))

typedef struct _RSSharpen RSSharpen;
typedef struct _RSSharpenClass RSSharpenClass;

struct _RSSharpen {
	RSFilter parent;

	gfloat amount;
};

struct _RSSharpenClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_sharpen, RSSharpen)

enum {
	PROP_0,
	PROP_AMOUNT
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);
static RS_IMAGE16 *rs_image16_convolve(RS_IMAGE16 *input, RS_IMAGE16 *output, RS_MATRIX3 *matrix, gfloat scaler, gboolean *abort);
static RS_IMAGE16 *rs_image16_sharpen(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble amount, gboolean *abort);

static RSFilterClass *rs_sharpen_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_sharpen_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_sharpen_class_init(RSSharpenClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_sharpen_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_AMOUNT, g_param_spec_float(
			"amount", "amount", "Sharpen amount",
			0.0, 10.0, 0.0,
			G_PARAM_READWRITE)
	);

	filter_class->name = "Sharpen filter";
	filter_class->get_image = get_image;
}

static void
rs_sharpen_init(RSSharpen *sharpen)
{
	sharpen->amount = 0.0;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSSharpen *sharpen = RS_SHARPEN(object);

	switch (property_id)
	{
		case PROP_AMOUNT:
			g_value_set_float(value, sharpen->amount);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	gfloat new_amount;
	RSSharpen *sharpen = RS_SHARPEN(object);

	switch (property_id)
	{
		case PROP_AMOUNT:
			new_amount = g_value_get_float(value);
			if (ABS(new_amount - sharpen->amount) > 0.001)
			{
				sharpen->amount = new_amount;
				rs_filter_changed(RS_FILTER(sharpen));
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSSharpen *sharpen = RS_SHARPEN(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	input = rs_filter_get_image(filter->previous);

	/* FIXME: Move all this crap to this filter - or maybe a convolution-filter */
	if (sharpen->amount > 0.01)
		output = rs_image16_sharpen(input, NULL, sharpen->amount, NULL);
	else
		output = g_object_ref(input);

	g_object_unref(input);

	return output;
}

struct struct_program {
	gint divisor;
	gint scale[9];
};

static void
convolve_line(RS_IMAGE16 *input, RS_IMAGE16 *output, guint line, struct struct_program *program)
{
	gint size;
	gint col;
	gint accu;
	gushort *line0, *line1, *line2, *dest;

	g_assert(line >= 0);
	g_assert(line < input->h);

	line0 = GET_PIXEL(input, 0, line-1);
	line1 = GET_PIXEL(input, 0, line);
	line2 = GET_PIXEL(input, 0, line+1);
	dest = GET_PIXEL(output, 0, line);

	/* special case for first line */
	if (line == 0)
		line0 = line1;

	/* special case for last line */
	else if (line == (input->h-1))
		line2 = line1;

	/* special case for first pixel */
	for (col = 0; col < input->pixelsize; col++)
	{
		accu
			= program->scale[0] * *line0
			+ program->scale[1] * *line0
			+ program->scale[2] * *(line0+input->pixelsize)
			+ program->scale[3] * *line1
			+ program->scale[4] * *line1
			+ program->scale[5] * *(line1+input->pixelsize)
			+ program->scale[6] * *line2
			+ program->scale[7] * *line2
			+ program->scale[8] * *(line2+input->pixelsize);
		accu /= program->divisor;
		_CLAMP65535(accu);
		*dest = accu;
		line0++; line1++; line2++; dest++;
	}

	size = (input->w-1)*input->pixelsize;

	for(col = input->pixelsize; col < size; col++)
	{
		accu
			= program->scale[0] * *(line0-input->pixelsize)
			+ program->scale[1] * *line0
			+ program->scale[2] * *(line0+input->pixelsize)
			+ program->scale[3] * *(line1-input->pixelsize)
			+ program->scale[4] * *line1
			+ program->scale[5] * *(line1+input->pixelsize)
			+ program->scale[6] * *(line2-input->pixelsize)
			+ program->scale[7] * *line2
			+ program->scale[8] * *(line2+input->pixelsize);
		accu /= program->divisor;
		_CLAMP65535(accu);
		*dest = accu;
		line0++; line1++; line2++; dest++;
	}

	/* special case for last pixel */
	for (col = size; col < input->w*input->pixelsize; col++)
	{
		accu
			= program->scale[0] * *(line0-input->pixelsize)
			+ program->scale[1] * *line0
			+ program->scale[2] * *line0
			+ program->scale[3] * *(line1-input->pixelsize)
			+ program->scale[4] * *line1
			+ program->scale[5] * *line1
			+ program->scale[6] * *(line2-input->pixelsize)
			+ program->scale[7] * *line2
			+ program->scale[8] * *line2;
		accu /= program->divisor;
		_CLAMP65535(accu);
		*dest = accu;
		line0++; line1++; line2++; dest++;
	}
}

/**
 * Concolve a RS_IMAGE16 using a 3x3 kernel
 * @param input The input image
 * @param output The output image
 * @param matrix A 3x3 convolution kernel
 * @param scaler The result will be scaled like this: convolve/scaler
 * @return output image for convenience
 */
static RS_IMAGE16 *
rs_image16_convolve(RS_IMAGE16 *input, RS_IMAGE16 *output, RS_MATRIX3 *matrix, gfloat scaler, gboolean *abort)
{
	gint row;
	struct struct_program *program;

	g_assert(RS_IS_IMAGE16(input));
	g_assert(RS_IS_IMAGE16(output));
	g_assert(((output->w == input->w) && (output->h == input->h)));

	rs_image16_ref(input);
	rs_image16_ref(output);

	/* Make the integer based convolve program */
	program = (struct struct_program *) g_new(struct struct_program, 1);
	program->scale[0] = (gint) (matrix->coeff[0][0]*256.0);
	program->scale[1] = (gint) (matrix->coeff[0][1]*256.0);
	program->scale[2] = (gint) (matrix->coeff[0][2]*256.0);
	program->scale[3] = (gint) (matrix->coeff[1][0]*256.0);
	program->scale[4] = (gint) (matrix->coeff[1][1]*256.0);
	program->scale[5] = (gint) (matrix->coeff[1][2]*256.0);
	program->scale[6] = (gint) (matrix->coeff[2][0]*256.0);
	program->scale[7] = (gint) (matrix->coeff[2][1]*256.0);
	program->scale[8] = (gint) (matrix->coeff[2][2]*256.0);
	program->divisor = (gint) (scaler * 256.0);

	for(row = 0; row < input->h; row++)
	{
		convolve_line(input, output, row, program);
		if (abort && *abort) goto abort;
	}

abort:
	g_free(program);
	rs_image16_unref(input);
	rs_image16_unref(output);

	return output;
}

static RS_IMAGE16 *
rs_image16_sharpen(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble amount, gboolean *abort)
{
	amount = amount/5;
	
	gdouble corner = (amount/1.4)*-1.0;
	gdouble regular = (amount)*-1.0;
	gdouble center = ((corner*4+regular*4)*-1.0)+1.0;
	
	RS_MATRIX3 sharpen = {	{ { corner, regular, corner },
							{ regular, center, regular },
							{ corner, regular, corner } } };
	if (!out)
		out = rs_image16_new(in->w, in->h, in->channels, in->pixelsize);

	rs_image16_convolve(in, out, &sharpen, 1, abort);

	return out;
}

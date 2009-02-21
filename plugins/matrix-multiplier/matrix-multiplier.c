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

/* Plugin tmpl version 2 */

#include <rawstudio.h>

#define RS_TYPE_MATRIX_MULTIPLIER (rs_matrix_multiplier_type)
#define RS_MATRIX_MULTIPLIER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_MATRIX_MULTIPLIER, RSMatrixMultiplier))
#define RS_MATRIX_MULTIPLIER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_MATRIX_MULTIPLIER, RSMatrixMultiplierClass))
#define RS_IS_MATRIX_MULTIPLIER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_MATRIX_MULTIPLIER))

typedef struct _RSMatrixMultiplier RSMatrixMultiplier;
typedef struct _RSMatrixMultiplierClass RSMatrixMultiplierClass;

struct _RSMatrixMultiplier {
	RSFilter parent;

	RS_MATRIX4Int matrix;
};

struct _RSMatrixMultiplierClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_matrix_multiplier, RSMatrixMultiplier)

enum {
	PROP_0,
	PROP_MATRIX
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);

static RSFilterClass *rs_matrix_multiplier_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	/* Let the GType system register our type */
	rs_matrix_multiplier_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_matrix_multiplier_class_init (RSMatrixMultiplierClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_matrix_multiplier_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_MATRIX, g_param_spec_pointer (
			"matrix",
			"matrix",
			"Matrix",
			G_PARAM_READWRITE)
	);

	filter_class->name = "Multiply pixel data by a RS_MATRIX4";
	filter_class->get_image = get_image;
}

static void
rs_matrix_multiplier_init (RSMatrixMultiplier *filter)
{
	RS_MATRIX4 identity;
	matrix4_identity(&identity);
	matrix4_to_matrix4int(&identity, &filter->matrix);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
		case PROP_MATRIX:
			g_value_get_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSMatrixMultiplier *matrix_multiplier = RS_MATRIX_MULTIPLIER(object);
	switch (property_id)
	{
		case PROP_MATRIX:
			matrix4_to_matrix4int(g_value_get_pointer(value), &matrix_multiplier->matrix);
			rs_filter_changed(RS_FILTER(matrix_multiplier));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSMatrixMultiplier *matrix_multiplier = RS_MATRIX_MULTIPLIER(filter);
	gint row, col, width, height, r, g, b;
	RS_IMAGE16 *input = rs_filter_get_image(filter->previous);
	RS_IMAGE16 *output = rs_image16_copy(input, FALSE);

	width = input->w;
	height = input->h;

	for(row=0; row<height; row++)
	{
		gushort *in = GET_PIXEL(input, 0, row);
		gushort *out = GET_PIXEL(output, 0, row);
		for(col=0; col<width; col++)
		{
			r = (in[R]*matrix_multiplier->matrix.coeff[0][0]
				+ in[G]*matrix_multiplier->matrix.coeff[0][1]
				+ in[B]*matrix_multiplier->matrix.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (in[R]*matrix_multiplier->matrix.coeff[1][0]
				+ in[G]*matrix_multiplier->matrix.coeff[1][1]
				+ in[B]*matrix_multiplier->matrix.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (in[R]*matrix_multiplier->matrix.coeff[2][0]
				+ in[G]*matrix_multiplier->matrix.coeff[2][1]
				+ in[B]*matrix_multiplier->matrix.coeff[2][2])>>MATRIX_RESOLUTION;

			_CLAMP65535_TRIPLET(r,g,b);

			out[R] = r;
			out[G] = g;
			out[B] = b;
			in += 4;
			out += 4;
		}
	}

	g_object_unref(input);

	return output;
}

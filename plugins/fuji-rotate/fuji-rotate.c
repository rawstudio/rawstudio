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

/* Plugin tmpl version 5 */

#include <rawstudio.h>
#include <math.h>

#define RS_TYPE_FUJI_ROTATE (rs_fuji_rotate_type)
#define RS_FUJI_ROTATE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FUJI_ROTATE, RSFujiRotate))
#define RS_FUJI_ROTATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FUJI_ROTATE, RSFujiRotateClass))
#define RS_IS_FUJI_ROTATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FUJI_ROTATE))

typedef struct _RSFujiRotate RSFujiRotate;
typedef struct _RSFujiRotateClass RSFujiRotateClass;

struct _RSFujiRotate {
	RSFilter parent;

	gchar *changeme;
	gint fuji_width;
};

struct _RSFujiRotateClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_fuji_rotate, RSFujiRotate)

enum {
	PROP_0,
	PROP_CHANGEME
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static RSFilterResponse *get_size(RSFilter *filter, const RSFilterRequest *request);

static RSFilterClass *rs_fuji_rotate_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_fuji_rotate_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_fuji_rotate_class_init(RSFujiRotateClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_fuji_rotate_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_CHANGEME, g_param_spec_string (
			"changeme",
			"Changeme nick",
			"Changeme blurb",
			NULL,
			G_PARAM_READWRITE)
	);

	filter_class->name = "FujiRotate filter";
	filter_class->get_image = get_image;
	filter_class->get_size = get_size;
}

static void
rs_fuji_rotate_init(RSFujiRotate *fuji_rotate)
{
	fuji_rotate->changeme = NULL;
	fuji_rotate->fuji_width = 0;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
//	RSFujiRotate *fuji_rotate = RS_FUJI_ROTATE(object);

	switch (property_id)
	{
		case PROP_CHANGEME:
			g_value_get_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSFujiRotate *fuji_rotate = RS_FUJI_ROTATE(object);

	switch (property_id)
	{
		case PROP_CHANGEME:
			g_free(fuji_rotate->changeme);
			fuji_rotate->changeme = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

RS_IMAGE16 *
do_rotate(RS_IMAGE16 *input, gint fuji_width)
{
	const gint colors = 3;
	gint height = input->h;
	gint width = input->w;
	gint i, row, col;
	gfloat r, c, fr, fc;
	gint ur, uc;
	gushort wide, high;

	if (!fuji_width)
		return g_object_ref(input);

	fuji_width = (fuji_width - 1);
	const gdouble step = sqrt(0.5);
	wide = fuji_width / step;
	high = (height - fuji_width) / step;

	RS_IMAGE16 *output = rs_image16_new(wide, high, 3, 4);

	for (row=0; row < high; row++)
	{
		for (col=0; col < wide; col++)
		{
			ur = r = fuji_width + (row-col)*step;
			uc = c = (row+col)*step;

			if (ur > height-2 || uc > width-2)
				continue;

			fr = r - ur;
			fc = c - uc;

			gushort *out = GET_PIXEL(output, col, row);
			gushort *top = GET_PIXEL(input, uc, ur);
			gushort *bottom = GET_PIXEL(input, uc, ur+1);
			for (i=0; i < colors; i++)
			{
				out[i] =
					  (top[i]    * (1-fc) + top[input->pixelsize+i]    * fc) * (1-fr)
					+ (bottom[i] * (1-fc) + bottom[input->pixelsize+i] * fc) * fr;
			}
		}
	}

	return output;
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSFujiRotate *fuji_rotate = RS_FUJI_ROTATE(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	previous_response = rs_filter_get_image(filter->previous, request);

	if (!rs_filter_param_get_integer(RS_FILTER_PARAM(previous_response), "fuji-width", &fuji_rotate->fuji_width) || (fuji_rotate->fuji_width == 0))
		return previous_response;

	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = do_rotate(input, fuji_rotate->fuji_width);
	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	g_object_unref(input);
	return response;
}

static RSFilterResponse *
get_size(RSFilter *filter, const RSFilterRequest *request)
{
	gint fuji_width = 0;
	RSFilterResponse *previous_response = rs_filter_get_size(filter->previous, request);

	/* If we have no specific fuji-width, just return previous response */
	if (!rs_filter_param_get_integer(RS_FILTER_PARAM(previous_response), "fuji-width", &fuji_width) || (fuji_width == 0))
		return previous_response;

	RSFilterResponse *response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	rs_filter_response_set_width(response, fuji_width / sqrt(0.5));
	rs_filter_response_set_height(response, (rs_filter_response_get_height(previous_response) - fuji_width) / sqrt(0.5));

	return response;
}

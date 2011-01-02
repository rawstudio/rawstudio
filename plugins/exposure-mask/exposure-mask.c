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

#define RS_TYPE_EXPOSURE_MASK (rs_exposure_mask_type)
#define RS_EXPOSURE_MASK(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_EXPOSURE_MASK, RSExposureMask))
#define RS_EXPOSURE_MASK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_EXPOSURE_MASK, RSExposureMaskClass))
#define RS_IS_EXPOSURE_MASK(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_EXPOSURE_MASK))

typedef struct _RSExposureMask RSExposureMask;
typedef struct _RSExposureMaskClass RSExposureMaskClass;

struct _RSExposureMask {
	RSFilter parent;

	gboolean exposure_mask;
};

struct _RSExposureMaskClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_exposure_mask, RSExposureMask)

enum {
	PROP_0,
	PROP_EXPOSURE_MASK
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image8(RSFilter *filter, const RSFilterRequest *request);

static RSFilterClass *rs_exposure_mask_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_exposure_mask_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_exposure_mask_class_init(RSExposureMaskClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_exposure_mask_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_EXPOSURE_MASK, g_param_spec_boolean (
			"exposure-mask", "exposure-mask", "Show exposure mask",
			FALSE, G_PARAM_READWRITE));

	filter_class->name = "ExposureMask filter";
	filter_class->get_image8 = get_image8;
}

static void
rs_exposure_mask_init(RSExposureMask *exposure_mask)
{
	exposure_mask->exposure_mask = FALSE;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSExposureMask *exposure_mask = RS_EXPOSURE_MASK(object);

	switch (property_id)
	{
		case PROP_EXPOSURE_MASK:
			g_value_set_boolean(value, exposure_mask->exposure_mask);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSExposureMask *exposure_mask = RS_EXPOSURE_MASK(object);

	switch (property_id)
	{
		case PROP_EXPOSURE_MASK:
			exposure_mask->exposure_mask = g_value_get_boolean(value);
			rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RSFilterResponse *
get_image8(RSFilter *filter, const RSFilterRequest *request)
{
	RSExposureMask *exposure_mask = RS_EXPOSURE_MASK(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	GdkPixbuf *input;
	GdkPixbuf *output;
	gint width, col;
	gint height, row;
	guchar *in_pixel;
	guchar *out_pixel;
	gint channels;

	previous_response = rs_filter_get_image8(filter->previous, request);
	input = rs_filter_response_get_image8(previous_response);
	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	/* FIXME: Support ROI */
	if (exposure_mask->exposure_mask)
	{
		output = gdk_pixbuf_copy(input);
		width = gdk_pixbuf_get_width(input);
		height = gdk_pixbuf_get_height(input);
		channels = gdk_pixbuf_get_n_channels(input);

		g_assert(channels == gdk_pixbuf_get_n_channels(output));
		for(row=0;row<height;row++)
		{
			in_pixel = GET_PIXBUF_PIXEL(input, 0, row);
			out_pixel = GET_PIXBUF_PIXEL(output, 0, row);
			for(col=0;col<width;col++)
			{
				/* Catch pixels overexposed and color them red */
				if ((in_pixel[R]==0xFF) || (in_pixel[G]==0xFF) || (in_pixel[B]==0xFF))
				{
					out_pixel[R] = 0xFF;
					out_pixel[G] = 0x00;
					out_pixel[B] = 0x00;
				}
				/* Color underexposed pixels blue */
				else if ((in_pixel[R]<2) && (in_pixel[G]<2) && (in_pixel[B]<2))
				{
					out_pixel[R] = 0x00;
					out_pixel[G] = 0x00;
					out_pixel[B] = 0xFF;
				}
				else
				{
					/* Luminance weights doesn't matter much here */
					gint tmp = (in_pixel[R]*3 + in_pixel[G]*6 + in_pixel[B]) / 10;
					_CLAMP255(tmp);
					out_pixel[R] = tmp;
					out_pixel[G] = tmp;
					out_pixel[B] = tmp;
				}
				out_pixel += channels;
				in_pixel += channels;
			}
		}

		g_object_unref(input);
	}
	else
		output = input;

	if (output)
	{
		rs_filter_response_set_image8(response, output);
		g_object_unref(output);
	}

	return response;
}

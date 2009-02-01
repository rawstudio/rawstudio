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

/* Plugin tmpl version 3 */

#include <rawstudio.h>
#include <string.h>

#define RS_TYPE_CROP (rs_crop_type)
#define RS_CROP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_CROP, RSCrop))
#define RS_CROP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_CROP, RSCropClass))
#define RS_IS_CROP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_CROP))

typedef struct _RSCrop RSCrop;
typedef struct _RSCropClass RSCropClass;

struct _RSCrop {
	RSFilter parent;

	RS_RECT rectangle;
};

struct _RSCropClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_crop, RSCrop)

enum {
	PROP_0,
	PROP_RECTANGLE,
	PROP_X1,
	PROP_X2,
	PROP_Y1,
	PROP_Y2
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);

static RSFilterClass *rs_crop_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	/* Let the GType system register our type */
	rs_crop_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_crop_class_init (RSCropClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_crop_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_RECTANGLE, g_param_spec_pointer (
			"rectangle",
			"rectangle",
			"RS_RECT to crop",
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_X1, g_param_spec_int("x1", "x1", "x1", 0, 2147483647, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_Y1, g_param_spec_int("y1", "y1", "y1", 0, 2147483647, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_X2, g_param_spec_int("x2", "x2", "x2", 0, 2147483647, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_Y2, g_param_spec_int("y2", "y2", "y2", 0, 2147483647, 0, G_PARAM_READWRITE)
	);

	filter_class->name = "Crop filter";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
}

static void
rs_crop_init (RSCrop *crop)
{
	crop->rectangle.x1 = 0;
	crop->rectangle.x2 = 65535;
	crop->rectangle.y1 = 0;
	crop->rectangle.y2 = 65535;
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSCrop *crop = RS_CROP(object);

	switch (property_id)
	{
		case PROP_RECTANGLE:
			g_value_set_pointer(value, &crop->rectangle);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSCrop *crop = RS_CROP(object);
	RSFilter *filter = RS_FILTER(crop);
	gint parent_width = rs_filter_get_width(filter->previous);
	gint parent_height = rs_filter_get_height(filter->previous);
	RS_RECT *rect;
	switch (property_id)
	{
		case PROP_RECTANGLE:
			rect = g_value_get_pointer(value);
			if (rect)
			{
				crop->rectangle.x1 = MAX(rect->x1, 0);
				crop->rectangle.y1 = MAX(rect->y1, 0);
				crop->rectangle.x2 = MIN(rect->x2, parent_width);
				crop->rectangle.y2 = MIN(rect->y2, parent_height);
			}
			else
			{
				crop->rectangle.x1 = MAX(0, 0);
				crop->rectangle.y1 = MAX(0, 0);
				crop->rectangle.x2 = MIN(65535, parent_width);
				crop->rectangle.y2 = MIN(65535, parent_height);
			}
			rs_filter_changed(filter);
			break;
		case PROP_X1:
			crop->rectangle.x1 = MAX(g_value_get_int(value), 0);
			rs_filter_changed(filter);
			break;
		case PROP_Y1:
			crop->rectangle.y1 = MAX(g_value_get_int(value), 0);
			rs_filter_changed(filter);
			break;
		case PROP_X2:
			crop->rectangle.x2 = MIN(g_value_get_int(value), parent_width);
			rs_filter_changed(filter);
			break;
		case PROP_Y2:
			crop->rectangle.y2 = MIN(g_value_get_int(value), parent_height);
			rs_filter_changed(filter);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	g_assert(RS_IS_FILTER(filter));
	RSCrop *crop = RS_CROP(filter);
	RS_IMAGE16 *output;
	RS_IMAGE16 *input;
	gint parent_width = rs_filter_get_width(filter->previous);
	gint parent_height = rs_filter_get_height(filter->previous);

	gint x1 = CLAMP(crop->rectangle.x1, 0, parent_width-1);
	gint x2 = CLAMP(crop->rectangle.x2, 0, parent_width-1);
	gint y1 = CLAMP(crop->rectangle.y1, 0, parent_height-1);
	gint y2 = CLAMP(crop->rectangle.y2, 0, parent_height-1);
	gint row;
	gint width = x2 - x1 + 1;
	gint height = y2 - y1 + 1;

	input = rs_filter_get_image(filter->previous);

	if (!RS_IS_IMAGE16(input))
		return input;

	/* Special case for full crop */
	if ((width == parent_width) && (height==parent_height))
		return input;

	output = rs_image16_new(width, height, 3, 4);

	/* Copy a row at a time */
	for(row=0; row<output->h; row++)
		memcpy(GET_PIXEL(output, 0, row), GET_PIXEL(input, x1, row+y1), output->rowstride*sizeof(gushort));

	g_object_unref(input);

	return output;
}

static gint
get_width(RSFilter *filter)
{
	RSCrop *crop = RS_CROP(filter);
	gint parent_width = rs_filter_get_width(filter->previous);
	gint width = crop->rectangle.x2 - crop->rectangle.x1 + 1;

	return CLAMP(width, 0, parent_width);
}

static gint
get_height(RSFilter *filter)
{
	RSCrop *crop = RS_CROP(filter);
	gint parent_height = rs_filter_get_height(filter->previous);
	gint height = crop->rectangle.y2 - crop->rectangle.y1 + 1;

	return CLAMP(height, 0, parent_height);
}

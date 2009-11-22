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

	RS_RECT target;
	RS_RECT effective;
	gint width;
	gint height;
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
	PROP_Y2,
	PROP_EFFECTIVE_X1,
	PROP_EFFECTIVE_X2,
	PROP_EFFECTIVE_Y1,
	PROP_EFFECTIVE_Y2,
	PROP_WIDTH,
	PROP_HEIGHT
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void calc(RSCrop *crop);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
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

	g_object_class_install_property(object_class,
		PROP_EFFECTIVE_X1, g_param_spec_int("effective-x1", "effective-x1", "Effective x1", 0, 2147483647, 0, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_EFFECTIVE_Y1, g_param_spec_int("effective-y1", "effective-y1", "Effective y1", 0, 2147483647, 0, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_EFFECTIVE_X2, g_param_spec_int("effective-x2", "effective-x2", "Effective x2", 0, 2147483647, 0, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_EFFECTIVE_Y2, g_param_spec_int("effective-y2", "effective-y2", "Effective y2", 0, 2147483647, 0, G_PARAM_READABLE)
	);

	g_object_class_install_property(object_class,
		PROP_WIDTH, g_param_spec_int("width", "width", "Width", 0, 2147483647, 0, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_HEIGHT, g_param_spec_int("height", "height", "Height", 0, 2147483647, 0, G_PARAM_READABLE)
	);

	filter_class->name = "Crop filter";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
}

static void
rs_crop_init (RSCrop *crop)
{
	crop->target.x1 = 0;
	crop->target.x2 = 65535;
	crop->target.y1 = 0;
	crop->target.y2 = 65535;
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSCrop *crop = RS_CROP(object);

	calc(crop);

	switch (property_id)
	{
		case PROP_RECTANGLE:
			g_value_set_pointer(value, &crop->target);
			break;
		case PROP_X1:
			g_value_set_int(value, crop->target.x1);
			break;
		case PROP_X2:
			g_value_set_int(value, crop->target.x2);
			break;
		case PROP_Y1:
			g_value_set_int(value, crop->target.y1);
			break;
		case PROP_Y2:
			g_value_set_int(value, crop->target.y2);
			break;
		case PROP_EFFECTIVE_X1:
			g_value_set_int(value, crop->effective.x1);
			break;
		case PROP_EFFECTIVE_X2:
			g_value_set_int(value, crop->effective.x2);
			break;
		case PROP_EFFECTIVE_Y1:
			g_value_set_int(value, crop->effective.y1);
			break;
		case PROP_EFFECTIVE_Y2:
			g_value_set_int(value, crop->effective.y2);
			break;
		case PROP_WIDTH:
			g_value_set_int(value, crop->width);
			break;
		case PROP_HEIGHT:
			g_value_set_int(value, crop->height);
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
	RS_RECT *rect;

	switch (property_id)
	{
		case PROP_RECTANGLE:
			rect = g_value_get_pointer(value);
			if (rect)
				crop->target = *rect;
			else
			{
				crop->target.x1 = 0;
				crop->target.x2 = 65535;
				crop->target.y1 = 0;
				crop->target.y2 = 65535;
			}
			rs_filter_changed(filter, RS_FILTER_CHANGED_DIMENSION);
			break;
		case PROP_X1:
			crop->target.x1 = g_value_get_int(value);
			rs_filter_changed(filter, RS_FILTER_CHANGED_DIMENSION);
			break;
		case PROP_Y1:
			crop->target.y1 = g_value_get_int(value);
			rs_filter_changed(filter, RS_FILTER_CHANGED_DIMENSION);
			break;
		case PROP_X2:
			crop->target.x2 = g_value_get_int(value);
			rs_filter_changed(filter, RS_FILTER_CHANGED_DIMENSION);
			break;
		case PROP_Y2:
			crop->target.y2 = g_value_get_int(value);
			rs_filter_changed(filter, RS_FILTER_CHANGED_DIMENSION);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
calc(RSCrop *crop)
{
	RSFilter *filter = RS_FILTER(crop);

	gint parent_width = rs_filter_get_width(filter->previous);
	gint parent_height = rs_filter_get_height(filter->previous);

	crop->effective.x1 = CLAMP(crop->target.x1, 0, parent_width-1);
	crop->effective.x2 = CLAMP(crop->target.x2, 0, parent_width-1);
	crop->effective.y1 = CLAMP(crop->target.y1, 0, parent_height-1);
	crop->effective.y2 = CLAMP(crop->target.y2, 0, parent_height-1);

	crop->width = crop->effective.x2 - crop->effective.x1 + 1;
	crop->height = crop->effective.y2 - crop->effective.y1 + 1;
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	g_assert(RS_IS_FILTER(filter));
	RSCrop *crop = RS_CROP(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *output;
	RS_IMAGE16 *input;
	gint parent_width = rs_filter_get_width(filter->previous);
	gint parent_height = rs_filter_get_height(filter->previous);
	gint row;

	calc(crop);

	/* Special case for full crop */
	if ((crop->width == parent_width) && (crop->height==parent_height))
		return rs_filter_get_image(filter->previous, request);
	
	/* Add ROI for cropped region */
	if (!rs_filter_request_get_roi(request))
	{
		GdkRectangle* roi = g_new(GdkRectangle, 1);
		roi->x = crop->effective.x1;
		roi->y = crop->effective.y1;
		roi->width = crop->width;
		roi->height = crop->height;
		RSFilterRequest *new_request = rs_filter_request_clone(request);
		rs_filter_request_set_roi(new_request, roi);
		previous_response = rs_filter_get_image(filter->previous, new_request);
		g_free(roi);
		g_object_unref(new_request);
	} 
	else 
	{
		/* Add crop to ROI */
		GdkRectangle* org_roi = rs_filter_request_get_roi(request);
		GdkRectangle* roi = g_new(GdkRectangle, 1);
		roi->x = org_roi->x + crop->effective.x1;
		roi->y = org_roi->y + crop->effective.y1;
		roi->width = MIN(org_roi->width, crop->width - org_roi->x);
		roi->height = MIN(org_roi->height, crop->height - org_roi->y);
		RSFilterRequest *new_request = rs_filter_request_clone(request);
		rs_filter_request_set_roi(new_request, roi);
		previous_response = rs_filter_get_image(filter->previous, new_request);
		g_free(roi);
		g_object_unref(new_request);
	}
	
	input = rs_filter_response_get_image(previous_response);

	if (!RS_IS_IMAGE16(input))
		return previous_response;

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	output = rs_image16_new(crop->width, crop->height, 3, 4);
	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	/* Copy a row at a time */
	for(row=0; row<output->h; row++)
		memcpy(GET_PIXEL(output, 0, row), GET_PIXEL(input, crop->effective.x1, row+crop->effective.y1), output->rowstride*sizeof(gushort));

	g_object_unref(input);

	return response;
}

static gint
get_width(RSFilter *filter)
{
	RSCrop *crop = RS_CROP(filter);

	calc(crop);

	return crop->width;
}

static gint
get_height(RSFilter *filter)
{
	RSCrop *crop = RS_CROP(filter);
	calc(crop);

	return crop->height;
}

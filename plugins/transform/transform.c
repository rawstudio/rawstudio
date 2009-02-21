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

/* Plugin tmpl version 4 */

#include <rawstudio.h>

#define RS_TYPE_TRANSFORM (rs_transform_type)
#define RS_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TRANSFORM, RSTransform))
#define RS_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TRANSFORM, RSTransformClass))
#define RS_IS_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TRANSFORM))

typedef struct _RSTransform RSTransform;
typedef struct _RSTransformClass RSTransformClass;

struct _RSTransform {
	RSFilter parent;

	RS_MATRIX3 forward_affine;
	gint new_width;
	gint new_height;
	RS_MATRIX3 affine;
	RS_MATRIX3 inverse_affine;
	gboolean keep_aspect;
	gint width;
	gint height;
	RS_RECT *crop;
	gfloat scale;
	gfloat actual_scale;
	gfloat angle;
	gint orientation;
};

struct _RSTransformClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_transform, RSTransform)

enum {
	PROP_0,
	PROP_AFFINE,
	PROP_INVERSE_AFFINE,
	PROP_KEEP_ASPECT,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_CROP,
	PROP_SCALE,
	PROP_ACTUAL_SCALE,
	PROP_ANGLE,
	PROP_ORIENTATION
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);
static void previous_changed(RSFilter *filter, RSFilter *parent);
static void recompute(RSTransform *transform);

static RSFilterClass *rs_transform_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	/* Let the GType system register our type */
	rs_transform_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_transform_class_init (RSTransformClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_transform_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_AFFINE, g_param_spec_pointer (
			"affine", "affine", "Affine transformation matrix",
			G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_INVERSE_AFFINE, g_param_spec_pointer (
			"inverseaffine", "inverseaffine", "Inverse affine transformation matrix",
			G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_KEEP_ASPECT, g_param_spec_boolean (
			"aspect", "aspect", "Retain aspect",
			TRUE,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_WIDTH, g_param_spec_int (
			"width", "width", "Output width",
			-1, 65535, -1,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_HEIGHT, g_param_spec_int (
			"height", "height", "Output height",
			-1, 65535, -1,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_CROP, g_param_spec_pointer (
			"crop",
			"crop",
			"RS_RECT to crop",
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_SCALE, g_param_spec_float (
			"scale", "scale", "Scale",
			-2.0, 100.0, 1.0,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_ACTUAL_SCALE, g_param_spec_float (
			"actual_scale", "actual_scale", "The computed scale factor",
			-2.0, 100.0, 1.0,
			G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_ANGLE, g_param_spec_float (
			"angle", "angle", "Rotation angle in degrees",
			-G_MAXFLOAT, G_MAXFLOAT, 1.0,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_ORIENTATION, g_param_spec_uint (
			"orientation", "orientation", "Orientation",
			0, 65536, 0,
			G_PARAM_READWRITE)
	);

	filter_class->name = "Transform filter providing scale and rotation";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
	filter_class->previous_changed = previous_changed;
}

static void
rs_transform_init (RSTransform *transform)
{
	transform->new_width = -1;
	transform->new_height = -1;
	matrix3_identity(&transform->affine);
	matrix3_identity(&transform->inverse_affine);
	transform->keep_aspect = TRUE;
	transform->width = -1;
	transform->height = -1;
	transform->crop = NULL;
	transform->scale = 1.0;
	transform->actual_scale = 1.0;
	transform->angle = 0.0;
	ORIENTATION_RESET(transform->orientation);
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSTransform *transform = RS_TRANSFORM(object);
	switch (property_id)
	{
		case PROP_AFFINE:
			g_value_set_pointer(value, &transform->affine);
			break;
		case PROP_INVERSE_AFFINE:
			g_value_set_pointer(value, &transform->inverse_affine);
			break;
		case PROP_KEEP_ASPECT:
			g_value_set_boolean(value, transform->keep_aspect);
			break;
		case PROP_WIDTH:
			g_value_set_int(value, transform->width);
			break;
		case PROP_HEIGHT:
			g_value_set_int(value, transform->height);
			break;
		case PROP_CROP:
			g_value_set_pointer(value, transform->crop);
			break;
		case PROP_SCALE:
			g_value_set_float(value, transform->scale);
			break;
		case PROP_ACTUAL_SCALE:
			g_value_set_float(value, transform->actual_scale);
			break;
		case PROP_ANGLE:
			g_value_set_float(value, transform->angle);
			break;
		case PROP_ORIENTATION:
			g_value_set_uint(value, transform->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSTransform *transform = RS_TRANSFORM(object);
	RS_RECT *new_crop;
	switch (property_id)
	{
		case PROP_KEEP_ASPECT:
			if (transform->keep_aspect != g_value_get_boolean(value))
			{
				transform->keep_aspect = g_value_get_boolean(value);
				recompute(transform);
				rs_filter_changed(RS_FILTER(transform));
			}
			break;
		case PROP_WIDTH:
			if (transform->width != g_value_get_int(value))
			{
				transform->width = g_value_get_int(value);
				recompute(transform);
				rs_filter_changed(RS_FILTER(transform));
			}
			break;
		case PROP_HEIGHT:
			if (transform->height != g_value_get_int(value))
			{
				transform->height = g_value_get_int(value);
				recompute(transform);
				rs_filter_changed(RS_FILTER(transform));
			}
			break;
		case PROP_CROP:
			new_crop = (RS_RECT *) g_value_get_pointer(value);
			g_free(transform->crop);
			transform->crop = NULL;
			if (new_crop)
			{
				transform->crop = g_new(RS_RECT, 1);
				*transform->crop = *new_crop;
			}
			recompute(transform);
			rs_filter_changed(RS_FILTER(transform));
			break;
		case PROP_SCALE:
			if (transform->scale != g_value_get_float(value))
			{
				
				transform->scale = g_value_get_float(value);
				recompute(transform);
				rs_filter_changed(RS_FILTER(transform));
			}
			break;
		case PROP_ANGLE:
			if (transform->angle != g_value_get_float(value))
			{
				transform->angle = g_value_get_float(value);
				recompute(transform);
				rs_filter_changed(RS_FILTER(transform));
			}
			break;
		case PROP_ORIENTATION:
			if (transform->orientation != g_value_get_uint(value))
			{
				transform->orientation = g_value_get_uint(value);
				recompute(transform);
				rs_filter_changed(RS_FILTER(transform));
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void inline
rs_image16_bilinear(RS_IMAGE16 *in, gushort *out, gint x, gint y)
{
	gint fx = x>>8;
	gint fy = y>>8;
	gint nextx;
	gint nexty;
	
	gushort *a, *b, *c, *d;
	gint diffx, diffy, inv_diffx, inv_diffy;
	gint aw, bw, cw, dw;

	if (unlikely((fx>(in->w-1))||(fy>(in->h-1))))
		return;
	else if (unlikely(fx<0))
	{
		if (likely(fx<-1))
			return;
		else
		{
			fx = 0;
			x = 0;
		}
	}

	if (unlikely(fy<0))
	{
		if (likely(fy<-1))
			return;
		else
		{
			fy = 0;
			y = 0;
		}
	}
	
	if (fx < in->w-1)
		nextx = in->pixelsize;
	else
		nextx = 0;
	
	if (fy < in->h-1)
		nexty = in->rowstride;
	else
		nexty = 0;
	
	fx *= in->pixelsize;
	fy *= in->rowstride;
	
	/* find four cornerpixels */
	a = &in->pixels[fy + fx];
	b = a + nextx;
	c = &in->pixels[fy+nexty + fx];
	d = c + nextx;

	/* calculate weightings */
	diffx = x&0xff; /* x distance from a */
	diffy = y&0xff; /* y distance fromy a */
	inv_diffx = 256 - diffx; /* inverse x distance from a */
	inv_diffy = 256 - diffy; /* inverse y distance from a */
	
	aw = (inv_diffx * inv_diffy) >> 1;  /* Weight is now 0.15 fp */
	bw = (diffx * inv_diffy) >> 1;
	cw = (inv_diffx * diffy) >> 1;
	dw = (diffx * diffy) >> 1;

	out[R]  = (gushort) ((a[R]*aw  + b[R]*bw  + c[R]*cw  + d[R]*dw + 16384) >> 15 );
	out[G]  = (gushort) ((a[G]*aw  + b[G]*bw  + c[G]*cw  + d[G]*dw + 16384) >> 15 );
	out[B]  = (gushort) ((a[B]*aw  + b[B]*bw  + c[B]*cw  + d[B]*dw + 16384) >> 15 );
	out[G2] = 0;
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSTransform *transform = RS_TRANSFORM(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	gdouble x, y;
	gint row, col;
	gint destoffset;
	gint fixed_x, fixed_y;

	input = rs_filter_get_image(filter->previous);

	if (!RS_IS_IMAGE16(input))
		return input;

	recompute(transform);

	output = rs_image16_new(transform->new_width, transform->new_height, 3, 4);

	for(row=0;row<output->h;row++)
	{
		gdouble foox = ((gdouble)row) * transform->forward_affine.coeff[1][0] + transform->forward_affine.coeff[2][0];
		gdouble fooy = ((gdouble)row) * transform->forward_affine.coeff[1][1] + transform->forward_affine.coeff[2][1];
		destoffset = row * output->rowstride;
		for(col=0;col<output->w;col++,destoffset += output->pixelsize)
		{
			x = ((gdouble)col)*transform->forward_affine.coeff[0][0] + foox;
			y = ((gdouble)col)*transform->forward_affine.coeff[0][1] + fooy;
			fixed_x = (int)(256.0f * x);
			fixed_y = (int)(256.0f * y);
			rs_image16_bilinear(input, &output->pixels[destoffset], fixed_x, fixed_y);
		}
	}

	g_object_unref(input);

	return output;
}

static gint
get_width(RSFilter *filter)
{
	RSTransform *transform = RS_TRANSFORM(filter);

	return transform->new_width;
}

static gint
get_height(RSFilter *filter)
{
	RSTransform *transform = RS_TRANSFORM(filter);

	return transform->new_height;
}

static void
previous_changed(RSFilter *filter, RSFilter *parent)
{
	RSTransform *transform = RS_TRANSFORM(filter);

	recompute(transform);

	rs_filter_changed(filter);
}

static void
recompute(RSTransform *transform)
{
	RSFilter *previous = RS_FILTER(transform)->previous;
	gdouble xscale=1.0, yscale=1.0;
	gdouble minx, miny;
	gdouble maxx, maxy;
	gdouble w, h;

	/* Start out clean */
	matrix3_identity(&transform->forward_affine);

	/* rotate straighten-angle + orientation-angle */
	matrix3_affine_rotate(&transform->forward_affine, transform->angle+(transform->orientation&3)*90.0);

	/* flip if needed */
	if (transform->orientation&4)
		matrix3_affine_scale(&transform->forward_affine, 1.0, -1.0);

	/* translate into positive x,y*/
	matrix3_affine_get_minmax(&transform->forward_affine, &minx, &miny, &maxx, &maxy, 0.0, 0.0, (gdouble) (rs_filter_get_width(previous)-1), (gdouble) (rs_filter_get_height(previous)-1));
	matrix3_affine_translate(&transform->forward_affine, -minx, -miny);

	/* get width and height used for calculating scale */
	w = maxx - minx + 1.0;
	h = maxy - miny + 1.0;

	/* apply crop if needed */
	if (transform->crop)
	{
		/* calculate cropped width and height */
		w = (gdouble) ABS(transform->crop->x2 - transform->crop->x1);
		h = (gdouble) ABS(transform->crop->y2 - transform->crop->y1);
		/* translate non-cropped area into negative x,y*/
		matrix3_affine_translate(&transform->forward_affine, ((gdouble) -transform->crop->x1), ((gdouble) -transform->crop->y1));
	}

	/* calculate scale */
	if (transform->scale > 0.0)
		xscale = yscale = transform->scale;
	else
	{
		if (transform->width > 0)
		{
			xscale = ((gdouble)transform->width)/w;
			if (transform->height<1)
				yscale = xscale;
		}
		if (transform->height > 0)
		{
			yscale = ((gdouble)transform->height)/h;
			if (transform->width<1)
				xscale = yscale;
		}
		if ((transform->width>0) && (transform->height>0) && transform->keep_aspect)
		{
			if ((h*xscale)>((gdouble) transform->height))
				xscale = yscale;
			if ((w*yscale)>((gdouble) transform->width))
				yscale = xscale;
		}
	}

	/* scale */
	matrix3_affine_scale(&transform->forward_affine, xscale, yscale);

	transform->actual_scale = (xscale+yscale)/2.0;

	/* apply scaling to our previously calculated width and height */
	w *= xscale;
	h *= yscale;

	/* calculate inverse affine (without rotation and orientation) */
	matrix3_identity(&transform->inverse_affine);
	if (transform->crop)
		matrix3_affine_translate(&transform->inverse_affine, ((gdouble) -transform->crop->x1), ((gdouble) -transform->crop->y1));
	matrix3_affine_scale(&transform->inverse_affine, xscale, yscale);
	matrix3_affine_invert(&transform->inverse_affine);

	matrix3_identity(&transform->affine);
	if (transform->crop)
		matrix3_affine_translate(&transform->affine, ((gdouble) -transform->crop->x1), ((gdouble) -transform->crop->y1));
	matrix3_affine_scale(&transform->affine, xscale, yscale);

	transform->new_width = (gint) w;
	transform->new_height = (gint) h;

	/* we use the inverse matrix */
	matrix3_affine_invert(&transform->forward_affine);
}

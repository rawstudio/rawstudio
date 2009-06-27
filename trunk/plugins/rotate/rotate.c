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
#include <math.h>

#define RS_TYPE_ROTATE (rs_rotate_type)
#define RS_ROTATE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_ROTATE, RSRotate))
#define RS_ROTATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_ROTATE, RSRotateClass))
#define RS_IS_ROTATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_ROTATE))

typedef struct _RSRotate RSRotate;
typedef struct _RSRotateClass RSRotateClass;

struct _RSRotate {
	RSFilter parent;

	RS_MATRIX3 affine;
	gboolean dirty;
	gfloat angle;
	gint orientation;
	gint new_width;
	gint new_height;
	gfloat sine;
	gfloat cosine;
	gint translate_x;
	gint translate_y;
};

struct _RSRotateClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_rotate, RSRotate)

enum {
	PROP_0,
	PROP_ANGLE,
	PROP_ORIENTATION
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask);
static RS_IMAGE16 *get_image(RSFilter *filter, RS_FILTER_PARAM *param);
static RS_IMAGE16 *turn_right_angle(RS_IMAGE16 *in, const int direction);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);
static void inline bilinear(RS_IMAGE16 *in, gushort *out, gint x, gint y);
static void recalculate(RSRotate *rotate);

static RSFilterClass *rs_rotate_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_rotate_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_rotate_class_init(RSRotateClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_rotate_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_ANGLE, g_param_spec_float(
			"angle", "Angle", "Rotation angle in degrees",
			-G_MAXFLOAT, G_MAXFLOAT, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_ORIENTATION, g_param_spec_uint (
			"orientation", "orientation", "Orientation",
			0, 65536, 0, G_PARAM_READWRITE)
	);

	filter_class->name = "Bilinear rotate filter";
	filter_class->previous_changed = previous_changed;
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
}

static void
rs_rotate_init(RSRotate *rotate)
{
	rotate->angle = 0.0;
	rotate->dirty = TRUE;
	ORIENTATION_RESET(rotate->orientation);
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSRotate *rotate = RS_ROTATE(object);

	switch (property_id)
	{
		case PROP_ANGLE:
			g_value_set_float(value, rotate->angle);
			break;
		case PROP_ORIENTATION:
			g_value_set_uint(value, rotate->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSRotate *rotate = RS_ROTATE(object);

	switch (property_id)
	{
		case PROP_ANGLE:
			if (rotate->angle != g_value_get_float(value))
			{
				rotate->angle = g_value_get_float(value);

				/* We only support positive */
				while(rotate->angle < 0.0)
					rotate->angle += 360.0;

				rotate->dirty = TRUE;
				rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_DIMENSION);
			}
			break;
		case PROP_ORIENTATION:
			if (rotate->orientation != g_value_get_uint(value))
			{
				rotate->orientation = g_value_get_uint(value);

				rotate->dirty = TRUE;
				rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_DIMENSION);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask)
{
	RSRotate *rotate = RS_ROTATE(filter);

	if (mask & RS_FILTER_CHANGED_DIMENSION)
		rotate->dirty = TRUE;

	rs_filter_changed(filter, mask);
}

static RS_IMAGE16 *
get_image(RSFilter *filter, RS_FILTER_PARAM *param)
{
	RSRotate *rotate = RS_ROTATE(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	gint x, y;
	gint row, col;
	gint destoffset;

	input = rs_filter_get_image(filter->previous, param);

	if (!RS_IS_IMAGE16(input))
		return input;

	if ((rotate->angle < 0.001) && (rotate->orientation==0))
		return input;

	if ((rotate->angle < 0.001) && (rotate->orientation < 4)) 
	{
		output = turn_right_angle(input, rotate->orientation);
		g_object_unref(input);
		return output;
	}

	recalculate(rotate);

	output = rs_image16_new(rotate->new_width, rotate->new_height, 3, 4);

	gint crapx = (gint) (rotate->affine.coeff[0][0]*65536.0);
	gint crapy = (gint) (rotate->affine.coeff[0][1]*65536.0);
	for(row=0;row<output->h;row++)
	{
		gint foox = (gint) ((((gdouble)row) * rotate->affine.coeff[1][0] + rotate->affine.coeff[2][0])*65536.0);
		gint fooy = (gint) ((((gdouble)row) * rotate->affine.coeff[1][1] + rotate->affine.coeff[2][1])*65536.0);
		destoffset = row * output->rowstride;
		for(col=0;col<output->w;col++,destoffset += output->pixelsize)
		{
			x = col * crapx + foox + 32768;
			y = col * crapy + fooy + 32768;
			bilinear(input, &output->pixels[destoffset], x>>8, y>>8);
		}
	}

	g_object_unref(input);

	return output;
}

static gint
get_width(RSFilter *filter)
{
	RSRotate *rotate = RS_ROTATE(filter);

	if (rotate->dirty)
		recalculate(rotate);

	return rotate->new_width;
}

static gint
get_height(RSFilter *filter)
{
	RSRotate *rotate = RS_ROTATE(filter);

	if (rotate->dirty)
		recalculate(rotate);

	return rotate->new_height;
}

static void inline
bilinear(RS_IMAGE16 *in, gushort *out, gint x, gint y)
{
	const static gushort black[4] = {0, 0, 0, 0};

	const gint fx = x>>8;
	const gint fy = y>>8;

	const gushort *a, *b, *c, *d; /* pointers to four "corner" pixels */

	/* Calculate distances */
	const gint diffx = x & 0xff; /* x distance from a */
	const gint diffy = y & 0xff; /* y distance fromy a */
	const gint inv_diffx = 256 - diffx; /* inverse x distance from a */
	const gint inv_diffy = 256 - diffy; /* inverse y distance from a */
	
	/* Calculate weightings */
	const gint aw = (inv_diffx * inv_diffy) >> 1;  /* Weight is now 0.15 fp */
	const gint bw = (diffx * inv_diffy) >> 1;
	const gint cw = (inv_diffx * diffy) >> 1;
	const gint dw = (diffx * diffy) >> 1;

	/* find four cornerpixels */
	a = GET_PIXEL(in, fx, fy);
	b = GET_PIXEL(in, fx+1, fy);
	c = GET_PIXEL(in, fx, fy+1);
	d = GET_PIXEL(in, fx+1, fy+1);

	/* Try to interpolate borders against black */
	if (unlikely(fx < 0))
	{
		a = black;
		c = black;
		if (fx < -1)
			return;
	}
	if (unlikely(fy < 0))
	{
		a = black;
		b = black;
		if (fy < -1)
			return;
	}

	if (unlikely(fx >= (in->w-1)))
	{
		c = black;
		d = black;
		if (fx >= in->w)
			return;
	}
	if (unlikely(fy >= (in->h-1)))
	{
		c = black;
		d = black;
		if (fy >= in->h)
			return;
	}

	out[R]  = (gushort) ((a[R]*aw  + b[R]*bw  + c[R]*cw  + d[R]*dw + 16384) >> 15 );
	out[G]  = (gushort) ((a[G]*aw  + b[G]*bw  + c[G]*cw  + d[G]*dw + 16384) >> 15 );
	out[B]  = (gushort) ((a[B]*aw  + b[B]*bw  + c[B]*cw  + d[B]*dw + 16384) >> 15 );
}

static void
recalculate(RSRotate *rotate)
{
	RSFilter *previous = RS_FILTER(rotate)->previous;
	gint previous_width = rs_filter_get_width(previous);
	gint previous_height = rs_filter_get_height(previous);

	/* Bail out, if parent returns negative dimensions */
	if ((previous_width < 0) || (previous_height < 0))
	{
		rotate->new_width = -1;
		rotate->new_height = -1;
		return;
	}
	gdouble minx, miny;
	gdouble maxx, maxy;

	/* Start clean */
	matrix3_identity(&rotate->affine);

	/* Rotate + orientation-angle */
	matrix3_affine_rotate(&rotate->affine, rotate->angle+(rotate->orientation&3)*90.0);

	/* Flip if needed */
	if (rotate->orientation&4)
		matrix3_affine_scale(&rotate->affine, 1.0, -1.0);

	/* Translate into positive x,y */
	matrix3_affine_get_minmax(&rotate->affine, &minx, &miny, &maxx, &maxy, 0.0, 0.0, (gdouble) (previous_width-1), (gdouble) (previous_height-1));
	minx -= 0.5; /* This SHOULD be the correct rounding :) */
	miny -= 0.5;
	matrix3_affine_translate(&rotate->affine, -minx, -miny);

	/* Get width and height used for calculating scale */
	rotate->new_width = (gint) (maxx - minx + 1.0);
	rotate->new_height = (gint) (maxy - miny + 1.0);

	/* We use the inverse matrix for our transform */
	matrix3_affine_invert(&rotate->affine);

	rotate->dirty = FALSE;
}

static RS_IMAGE16 * turn_right_angle(RS_IMAGE16 *in, const int direction)
{
	RS_IMAGE16 *out = NULL;
	int dstp_offset, x, y, p;

	if (direction == 1) /* Rotate Left */
	{ 
		out = rs_image16_new(in->h, in->w, 3, in->pixelsize);
		p = out->pitch * out->pixelsize;
		gushort *dstp = GET_PIXEL(out, 0, 0);
		for (y = 0; y < in->h; y++) 
		{
			gushort *srcp = GET_PIXEL(in, 0, y);
			dstp_offset = (in->h - 1 - y) * in->pixelsize;
			for (x = 0; x < in->w * in->pixelsize; x += in->pixelsize) 
			{
				dstp[dstp_offset] = srcp[x];
				dstp[dstp_offset + 1] = srcp[x + 1];
				dstp[dstp_offset + 2] = srcp[x + 2];
				dstp_offset += p;
			}
		}
	}

	if (direction == 3) /* Rotate Right */
	{ 
		out = rs_image16_new(in->h, in->w, 3, in->pixelsize);
		p = out->pitch * out->pixelsize;
		gushort *dstp = GET_PIXEL(out, 0, in->w - 1);
		for (y = 0; y < in->h; y++) 
		{
			gushort *srcp = GET_PIXEL(in, 0, y);
			dstp_offset = y * in->pixelsize;
			for (x = 0; x < in->w * in->pixelsize; x += in->pixelsize) 
			{
				dstp[dstp_offset] = srcp[x];
				dstp[dstp_offset + 1] = srcp[x + 1];
				dstp[dstp_offset + 2] = srcp[x + 2];
				dstp_offset -= p;
			}
		}
	}

	if (direction == 2) /* Rotate 180 */
	{
		out = rs_image16_new(in->w, in->h, 3, in->pixelsize);
		for (y = 0; y < in->h; y++) 
		{
			gushort *srcp = GET_PIXEL(in, 0, y);
			gushort *dstp = GET_PIXEL(out, in->w - 1, in->h - 1 - y);
			dstp_offset = 0;
			for (x = 0; x < in->w * in->pixelsize; x += in->pixelsize) 
			{
				dstp[dstp_offset] = srcp[x];
				dstp[dstp_offset + 1] = srcp[x + 1];
				dstp[dstp_offset + 2] = srcp[x + 2];
				dstp_offset -= in->pixelsize;
			}
		}
	}

	return out;
}

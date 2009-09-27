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
#include <math.h> /* pow() */

#define RS_TYPE_DCP (rs_dcp_type)
#define RS_DCP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DCP, RSDcp))
#define RS_DCP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DCP, RSDcpClass))
#define RS_IS_DCP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DCP))

typedef struct _RSDcp RSDcp;
typedef struct _RSDcpClass RSDcpClass;

struct _RSDcp {
	RSFilter parent;

	gfloat exposure;
	gfloat saturation;
	gfloat hue;

	RS_xy_COORD white_xy;

	gint nknots;
	gfloat *curve_samples;

	gfloat temp1;
	gfloat temp2;

	RSSpline *baseline_exposure;
	gfloat *baseline_exposure_lut;

	gboolean has_color_matrix1;
	gboolean has_color_matrix2;
	RS_MATRIX3 color_matrix1;
	RS_MATRIX3 color_matrix2;

	gboolean has_reduction_matrix1;
	gboolean has_reduction_matrix2;
	RS_MATRIX3 reduction_matrix1;
	RS_MATRIX3 reduction_matrix2;
	RS_MATRIX3 reduction_matrix;

	gboolean has_forward_matrix1;
	gboolean has_forward_matrix2;
	RS_MATRIX3 forward_matrix1;
	RS_MATRIX3 forward_matrix2;
	RS_MATRIX3 forward_matrix;

	RSHuesatMap *looktable;

	RSHuesatMap *huesatmap;
	RSHuesatMap *huesatmap1;
	RSHuesatMap *huesatmap2;

	RS_MATRIX3 camera_to_pcs;

	RS_VECTOR3 camera_white;
	RS_MATRIX3 camera_to_prophoto;

	RS_MATRIX3 prophoto_to_srgb;
};

struct _RSDcpClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_dcp, RSDcp)

enum {
	PROP_0,
	PROP_SETTINGS,
	PROP_PROFILE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterParam *param);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSDcp *dcp);
static RS_xy_COORD neutral_to_xy(RSDcp *dcp, const RS_VECTOR3 *neutral);
static RS_MATRIX3 find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix, RS_MATRIX3 *reduction_matrix, RS_MATRIX3 *camera_calibration);
static void set_white_xy(RSDcp *dcp, const RS_xy_COORD *xy);
static void precalc(RSDcp *dcp);
static void render(RSDcp *dcp, RS_IMAGE16 *image);
static void read_profile(RSDcp *dcp, const gchar *filename);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_dcp_get_type(G_TYPE_MODULE(plugin));
}

static void
finalize(GObject *object)
{
	RSDcp *dcp = RS_DCP(object);

	g_free(dcp->curve_samples);
}

static void
rs_dcp_class_init(RSDcpClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->finalize = finalize;

	g_object_class_install_property(object_class,
		PROP_SETTINGS, g_param_spec_object(
			"settings", "Settings", "Settings to render from",
			RS_TYPE_SETTINGS, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_PROFILE, g_param_spec_string(
			"profile", "profile", "DCP Profile",
			NULL, G_PARAM_READWRITE)
	);

	filter_class->name = "Adobe DNG camera profile filter";
	filter_class->get_image = get_image;
}

static void
settings_changed(RSSettings *settings, RSSettingsMask mask, RSDcp *dcp)
{
	gboolean changed = FALSE;

	if (mask & MASK_EXPOSURE)
	{
		g_object_get(settings, "exposure", &dcp->exposure, NULL);
		changed = TRUE;
	}

	if (mask & MASK_SATURATION)
	{
		g_object_get(settings, "saturation", &dcp->saturation, NULL);
		changed = TRUE;
	}

	if (mask & MASK_HUE)
	{
		g_object_get(settings, "hue", &dcp->hue, NULL);
		dcp->hue /= 60.0;
		changed = TRUE;
	}

	if ((mask & MASK_WB) || (mask & MASK_CHANNELMIXER))
	{
		const gfloat warmth;
		gfloat tint;
		const gfloat channelmixer_red;
		const gfloat channelmixer_green;
		const gfloat channelmixer_blue;

		g_object_get(settings,
			"warmth", &warmth,
			"tint", &tint,
			"channelmixer_red", &channelmixer_red,
			"channelmixer_green", &channelmixer_green,
			"channelmixer_blue", &channelmixer_blue,
			NULL);

		RS_xy_COORD whitepoint;
		RS_VECTOR3 pre_mul;
		/* This is messy, but we're essentially converting from warmth/tint to cameraneutral */
        pre_mul.x = (1.0+warmth)*(2.0-tint)*(channelmixer_red/100.0);
        pre_mul.y = 1.0*(channelmixer_green/100.0);
        pre_mul.z = (1.0-warmth)*(2.0-tint)*(channelmixer_blue/100.0);
		RS_VECTOR3 neutral;
		neutral.x = 1.0 / CLAMP(pre_mul.x, 0.001, 100.00);
		neutral.y = 1.0 / CLAMP(pre_mul.y, 0.001, 100.00);
		neutral.z = 1.0 / CLAMP(pre_mul.z, 0.001, 100.00);
		gfloat max = vector3_max(&neutral);
		neutral.x = neutral.x / max;
		neutral.y = neutral.y / max;
		neutral.z = neutral.z / max;
		whitepoint = neutral_to_xy(dcp, &neutral);

		set_white_xy(dcp, &whitepoint);
		precalc(dcp);
		changed = TRUE;
	}

	if (mask & MASK_CURVE)
	{
		const gint nknots = rs_settings_get_curve_nknots(settings);

		if (nknots > 1)
		{
			gfloat *knots = rs_settings_get_curve_knots(settings);
			if (knots)
			{
				dcp->nknots = nknots;
				RSSpline *spline = rs_spline_new(knots, dcp->nknots, NATURAL);
				rs_spline_sample(spline, dcp->curve_samples, 65536);
				g_object_unref(spline);
				g_free(knots);
			}
		}
		else
		{
			gint i;
			for(i=0;i<65536;i++)
				dcp->curve_samples[i] = ((gfloat)i)/65536.0;
		}
		changed = TRUE;
	}

	if (changed)
		rs_filter_changed(RS_FILTER(dcp), RS_FILTER_CHANGED_PIXELDATA);
}

static void
rs_dcp_init(RSDcp *dcp)
{
	gint i;

	dcp->curve_samples = g_new(gfloat, 65536);

	for(i=0;i<65536;i++)
		dcp->curve_samples[i] = ((gfloat)i)/65536.0;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
//	RSDcp *dcp = RS_DCP(object);

	switch (property_id)
	{
		case PROP_SETTINGS:
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSDcp *dcp = RS_DCP(object);
//	RSFilter *filter = RS_FILTER(dcp);
	RSSettings *settings;
	const gchar *profile_filename;

	switch (property_id)
	{
		case PROP_SETTINGS:
			settings = g_value_get_object(value);
			g_signal_connect(settings, "settings-changed", G_CALLBACK(settings_changed), dcp);
			settings_changed(settings, MASK_ALL, dcp);
			break;
		case PROP_PROFILE:
			profile_filename = g_value_get_string(value);
			read_profile(dcp, profile_filename);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterParam *param)
{
	RSDcp *dcp = RS_DCP(filter);
	GdkRectangle *roi;
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	RS_IMAGE16 *tmp;

	previous_response = rs_filter_get_image(filter->previous, param);

	if (!RS_IS_FILTER(filter->previous))
		return previous_response;

	input = rs_filter_response_get_image(previous_response);
	if (!input) return previous_response;
	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	output = rs_image16_copy(input, TRUE);
	g_object_unref(input);

	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	if ((roi = rs_filter_param_get_roi(param)))
		tmp = rs_image16_new_subframe(output, roi);
	else
		tmp = g_object_ref(output);

	render(dcp, tmp);

	g_object_unref(tmp);

	return response;
}

/* dng_color_spec::NeutralToXY */
static RS_xy_COORD
neutral_to_xy(RSDcp *dcp, const RS_VECTOR3 *neutral)
{
	const guint max_passes = 30;
	guint pass;
	RS_xy_COORD last;

	last = XYZ_to_xy(&XYZ_WP_D50);

	for(pass = 0; pass < max_passes; pass++)
	{
		RS_MATRIX3 xyz_to_camera = find_xyz_to_camera(dcp, &last, NULL, NULL, NULL);
		RS_MATRIX3 camera_to_xyz = matrix3_invert(&xyz_to_camera);

		RS_XYZ_VECTOR tmp = vector3_multiply_matrix(neutral, &camera_to_xyz);
		RS_xy_COORD next = XYZ_to_xy(&tmp);

		if (ABS(next.x - last.x) + ABS(next.y - last.y) < 0.0000001)
		{
			last = next;
			break;
		}

		// If we reach the limit without converging, we are most likely
		// in a two value oscillation.  So take the average of the last
		// two estimates and give up.
		if (pass == max_passes - 1)
		{
			next.x = (last.x + next.x) * 0.5;
			next.y = (last.y + next.y) * 0.5;
		}
		last = next;
	}

	return last;
}

inline void
RGBtoHSV(gfloat r, gfloat g, gfloat b, gfloat *h, gfloat *s, gfloat *v)
{
	*v = MAX(r, MAX (g, b));

	gfloat gap = *v - MIN (r, MIN (g, b));

	if (gap > 0.0f)
	{
		if (r == *v)
		{
			*h = (g - b) / gap;

			if (*h < 0.0f)
				*h += 6.0f;
		}
		else if (g == *v)
			*h = 2.0f + (b - r) / gap;
		else
			*h = 4.0f + (r - g) / gap;

		*s = gap / *v;
	}
	else
	{
		*h = 0.0f;
		*s = 0.0f;
	}
}

inline void
HSVtoRGB(gfloat h, gfloat s, gfloat v, gfloat *r, gfloat *g, gfloat *b)
{
	if (s > 0.0f)
	{

		if (h < 0.0f)
			h += 6.0f;

		if (h >= 6.0f)
			h -= 6.0f;

		gint i = (gint) h;
		gfloat f = h - (gint) i;

		gfloat p = v * (1.0f - s);

#define q   (v * (1.0f - s * f))
#define t   (v * (1.0f - s * (1.0f - f)))

		switch (i)
		{
			case 0: *r = v; *g = t; *b = p; break;
			case 1: *r = q; *g = v; *b = p; break;
			case 2: *r = p; *g = v; *b = t; break;
			case 3: *r = p; *g = q; *b = v; break;
			case 4: *r = t; *g = p; *b = v; break;
			case 5: *r = v; *g = p; *b = q; break;
		}

#undef q
#undef t

	}
	else
	{
		*r = v;
		*g = v;
		*b = v;
	}
}

#define _F(x) (x / 65535.0)
#define _S(x) CLAMP(((gint) (x * 65535.0)), 0, 65535)

static void
huesat_map(RSHuesatMap *map, gfloat *h, gfloat *s, gfloat *v)
{
	g_assert(RS_IS_HUESAT_MAP(map));

	gfloat hScale = (map->hue_divisions < 2) ? 0.0f : (map->hue_divisions * (1.0f / 6.0f));
	gfloat sScale = (gfloat) (map->sat_divisions - 1);
    gfloat vScale = (gfloat) (map->val_divisions - 1);

	gint maxHueIndex0 = map->hue_divisions - 1;
    gint maxSatIndex0 = map->sat_divisions - 2;
    gint maxValIndex0 = map->val_divisions - 2;

    const RS_VECTOR3 *tableBase = map->deltas;

    gint hueStep = map->sat_divisions;
    gint valStep = map->hue_divisions * hueStep;

	gfloat hueShift;
	gfloat satScale;
	gfloat valScale;

	if (map->val_divisions < 2)
	{
		gfloat hScaled = *h * hScale;
		gfloat sScaled = *s * sScale;

		gint hIndex0 = (gint) hScaled;
		gint sIndex0 = (gint) sScaled;

		sIndex0 = MIN(sIndex0, maxSatIndex0);

		gint hIndex1 = hIndex0 + 1;

		if (hIndex0 >= maxHueIndex0)
		{
			hIndex0 = maxHueIndex0;
			hIndex1 = 0;
		}

		gfloat hFract1 = hScaled - (gfloat) hIndex0;
		gfloat sFract1 = sScaled - (gfloat) sIndex0;

		gfloat hFract0 = 1.0f - hFract1;
		gfloat sFract0 = 1.0f - sFract1;

		const RS_VECTOR3 *entry00 = tableBase + hIndex0 * hueStep + sIndex0;

		const RS_VECTOR3 *entry01 = entry00 + (hIndex1 - hIndex0) * hueStep;
		gfloat hueShift0 = hFract0 * entry00->fHueShift +
		hFract1 * entry01->fHueShift;

		gfloat satScale0 = hFract0 * entry00->fSatScale +
		hFract1 * entry01->fSatScale;

		gfloat valScale0 = hFract0 * entry00->fValScale +
		hFract1 * entry01->fValScale;

		entry00++;
		entry01++;

		gfloat hueShift1 = hFract0 * entry00->fHueShift +
		hFract1 * entry01->fHueShift;

		gfloat satScale1 = hFract0 * entry00->fSatScale +
		hFract1 * entry01->fSatScale;

		gfloat valScale1 = hFract0 * entry00->fValScale +
		hFract1 * entry01->fValScale;

		hueShift = sFract0 * hueShift0 + sFract1 * hueShift1;
		satScale = sFract0 * satScale0 + sFract1 * satScale1;
		valScale = sFract0 * valScale0 + sFract1 * valScale1;
	}
	else
	{
		gfloat hScaled = *h * hScale;
		gfloat sScaled = *s * sScale;
		gfloat vScaled = *v * vScale;

		gint hIndex0 = (gint) hScaled;
		gint sIndex0 = (gint) sScaled;
		gint vIndex0 = (gint) vScaled;

		sIndex0 = MIN(sIndex0, maxSatIndex0);
		vIndex0 = MIN(vIndex0, maxValIndex0);

		gint hIndex1 = hIndex0 + 1;

		if (hIndex0 >= maxHueIndex0)
		{
			hIndex0 = maxHueIndex0;
			hIndex1 = 0;
		}

		gfloat hFract1 = hScaled - (gfloat) hIndex0;
		gfloat sFract1 = sScaled - (gfloat) sIndex0;
		gfloat vFract1 = vScaled - (gfloat) vIndex0;

		gfloat hFract0 = 1.0f - hFract1;
		gfloat sFract0 = 1.0f - sFract1;
		gfloat vFract0 = 1.0f - vFract1;

		const RS_VECTOR3 *entry00 = tableBase + vIndex0 * valStep + hIndex0 * hueStep + sIndex0;
		const RS_VECTOR3 *entry01 = entry00 + (hIndex1 - hIndex0) * hueStep;

		const RS_VECTOR3 *entry10 = entry00 + valStep;
		const RS_VECTOR3 *entry11 = entry01 + valStep;

		gfloat hueShift0 = vFract0 * (hFract0 * entry00->fHueShift +
			hFract1 * entry01->fHueShift) +
			vFract1 * (hFract0 * entry10->fHueShift +
			hFract1 * entry11->fHueShift);

		gfloat satScale0 = vFract0 * (hFract0 * entry00->fSatScale +
			hFract1 * entry01->fSatScale) +
			vFract1 * (hFract0 * entry10->fSatScale +
			hFract1 * entry11->fSatScale);

		gfloat valScale0 = vFract0 * (hFract0 * entry00->fValScale +
			hFract1 * entry01->fValScale) +
			vFract1 * (hFract0 * entry10->fValScale +
			hFract1 * entry11->fValScale);

		entry00++;
		entry01++;
		entry10++;
		entry11++;

		gfloat hueShift1 = vFract0 * (hFract0 * entry00->fHueShift +
			hFract1 * entry01->fHueShift) +
			vFract1 * (hFract0 * entry10->fHueShift +
			hFract1 * entry11->fHueShift);

		gfloat satScale1 = vFract0 * (hFract0 * entry00->fSatScale +
			hFract1 * entry01->fSatScale) +
			vFract1 * (hFract0 * entry10->fSatScale +
			hFract1 * entry11->fSatScale);

		gfloat valScale1 = vFract0 * (hFract0 * entry00->fValScale +
			hFract1 * entry01->fValScale) +
			vFract1 * (hFract0 * entry10->fValScale +
			hFract1 * entry11->fValScale);

		hueShift = sFract0 * hueShift0 + sFract1 * hueShift1;
		satScale = sFract0 * satScale0 + sFract1 * satScale1;
		valScale = sFract0 * valScale0 + sFract1 * valScale1;
	}

	hueShift *= (6.0f / 360.0f);

	*h += hueShift;
	*s = MIN(*s * satScale, 1.0);
	*v = MIN(*v * valScale, 1.0);
}

/* RefBaselineRGBTone() */
void
rgb_tone(gfloat *_r, gfloat *_g, gfloat *_b, const gfloat * const tone_lut)
{
	gfloat r = *_r;
	gfloat g = *_g;
	gfloat b = *_b;
		gfloat rr;
		gfloat gg;
		gfloat bb;
		
		#define RGBTone(r, g, b, rr, gg, bb)\
			{\
			\
/*			DNG_ASSERT (r >= g && g >= b && r > b, "Logic Error RGBTone");*/\
			\
			rr = tone_lut[_S(r)];\
			bb = tone_lut[_S(b)];\
			\
			gg = bb + ((rr - bb) * (g - b) / (r - b));\
			\
			}
		
		if (r >= g)
			{
			
			if (g > b)
				{
				
				// Case 1: r >= g > b
				
				RGBTone (r, g, b, rr, gg, bb);
				
				}
					
			else if (b > r)
				{
				
				// Case 2: b > r >= g
				
				RGBTone (b, r, g, bb, rr, gg);
								
				}
				
			else if (b > g)
				{
				
				// Case 3: r >= b > g
				
				RGBTone (r, b, g, rr, bb, gg);
				
				}
				
			else
				{
				
				// Case 4: r >= g == b
				
//				DNG_ASSERT (r >= g && g == b, "Logic Error 2");
				
				rr = tone_lut[_S(r)];
				gg = tone_lut[_S(b)];
//				rr = table.Interpolate (r);
//				gg = table.Interpolate (g);
				bb = gg;
				
				}
				
			}
			
		else
			{
			
			if (r >= b)
				{
				
				// Case 5: g > r >= b
				
				RGBTone (g, r, b, gg, rr, bb);
				
				}
				
			else if (b > g)
				{
				
				// Case 6: b > g > r
				
				RGBTone (b, g, r, bb, gg, rr);
				
				}
				
			else
				{
				
				// Case 7: g >= b > r
				
				RGBTone (g, b, r, gg, bb, rr);
				
				}
			
			}
			
		#undef RGBTone
		
		*_r = rr;
		*_g = gg;
		*_b = bb;
		
}

static void
render(RSDcp *dcp, RS_IMAGE16 *image)
{
	gint x, y;
	gfloat h, s, v;
	gfloat r, g, b;
	RS_VECTOR3 pix;

	const gfloat exposure_comp = pow(2.0, dcp->exposure);

	for(y = 0 ; y < image->h; y++)
	{
		for(x=0; x < image->w; x++)
		{
			gushort *pixel = GET_PIXEL(image, x, y);

			/* Convert to float */
			r = _F(pixel[R]);
			g = _F(pixel[G]);
			b = _F(pixel[B]);

			r = MIN(dcp->camera_white.x, r);
			g = MIN(dcp->camera_white.y, g);
			b = MIN(dcp->camera_white.z, b);

			pix.R = r;
			pix.G = g;
			pix.B = b;
			pix = vector3_multiply_matrix(&pix, &dcp->camera_to_prophoto);
			r = pix.R;
			g = pix.G;
			b = pix.B;

			r = CLAMP(pix.R, 0.0, 1.0);
			g = CLAMP(pix.G, 0.0, 1.0);
			b = CLAMP(pix.B, 0.0, 1.0);

			/* Does it matter if we're above 1.0 at this point? */

			/* To HSV */
			RGBtoHSV(r, g, b, &h, &s, &v);

			v = MIN(v * exposure_comp, 1.0);

			if (dcp->huesatmap)
				huesat_map(dcp->huesatmap, &h, &s, &v);

			/* Saturation */
			s *= dcp->saturation;
			s = MIN(s, 1.0);

			/* Hue */
			h += dcp->hue;

			/* Curve */
			v = dcp->curve_samples[_S(v)];

			if (dcp->looktable)
				huesat_map(dcp->looktable, &h, &s, &v);

			/* Back to RGB */
			HSVtoRGB(h, s, v, &r, &g, &b);

			pix.R = r;
			pix.G = g;
			pix.B = b;

			pix = vector3_multiply_matrix(&pix, &dcp->prophoto_to_srgb);

			r = pix.R;
			g = pix.G;
			b = pix.B;

			/* Save as gushort */
			pixel[R] = _S(r);
			pixel[G] = _S(g);
			pixel[B] = _S(b);
		}
	}
}

#undef _F
#undef _S

static gfloat
temp_from_exif_illuminant(guint illuminant)
{
	enum {
		lsUnknown                   =  0,

		lsDaylight                  =  1,
		lsFluorescent               =  2,
		lsTungsten                  =  3,
		lsFlash                     =  4,
		lsFineWeather               =  9,
		lsCloudyWeather             = 10,
		lsShade                     = 11,
		lsDaylightFluorescent       = 12,       // D 5700 - 7100K
		lsDayWhiteFluorescent       = 13,       // N 4600 - 5400K
		lsCoolWhiteFluorescent      = 14,       // W 3900 - 4500K
		lsWhiteFluorescent          = 15,       // WW 3200 - 3700K
		lsStandardLightA            = 17,
		lsStandardLightB            = 18,
		lsStandardLightC            = 19,
		lsD55                       = 20,
		lsD65                       = 21,
		lsD75                       = 22,
		lsD50                       = 23,
		lsISOStudioTungsten         = 24,

		lsOther                     = 255
	};
	switch (illuminant)
	{
		case lsStandardLightA:
		case lsTungsten:
			return 2850.0;

		case lsISOStudioTungsten:
			return 3200.0;

		case lsD50:
			return 5000.0;

		case lsD55:
		case lsDaylight:
		case lsFineWeather:
		case lsFlash:
		case lsStandardLightB:
			return 5500.0;
		case lsD65:
		case lsStandardLightC:
		case lsCloudyWeather:
			return 6500.0;

		case lsD75:
		case lsShade:
			return 7500.0;

		case lsDaylightFluorescent:
			return (5700.0 + 7100.0) * 0.5;

		case lsDayWhiteFluorescent:
			return (4600.0 + 5400.0) * 0.5;

		case lsCoolWhiteFluorescent:
		case lsFluorescent:
			return (3900.0 + 4500.0) * 0.5;

		case lsWhiteFluorescent:
			return (3200.0 + 3700.0) * 0.5;

		default:
			return 0.0;
	}
}

/* dng_color_spec::FindXYZtoCamera */
static RS_MATRIX3
find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix, RS_MATRIX3 *reduction_matrix, RS_MATRIX3 *camera_calibration)
{
	gfloat temp = 5000.0;

	rs_color_whitepoint_to_temp(white_xy, &temp, NULL);

	gfloat alpha = 0.0;

	if (temp <=  dcp->temp1)
		alpha = 1.0;
	else if (temp >=  dcp->temp2)
		alpha = 0.0;
	else if ((dcp->temp2 > 0.0) && (dcp->temp1 > 0.0) && (temp > 0.0))
	{
		gdouble invT = 1.0 / temp;
		alpha = (invT - (1.0 / dcp->temp2)) / ((1.0 / dcp->temp1) - (1.0 / dcp->temp2));
	}

	RS_MATRIX3 color_matrix;

	matrix3_interpolate(&dcp->color_matrix1, &dcp->color_matrix2, alpha, &color_matrix);

	if (forward_matrix)
	{
		if (dcp->has_forward_matrix1 && dcp->has_forward_matrix2)
			matrix3_interpolate(&dcp->forward_matrix1, &dcp->forward_matrix2, 1.0-alpha, forward_matrix);
		else if (dcp->has_forward_matrix1)
			*forward_matrix = dcp->forward_matrix1;
		else if (dcp->has_forward_matrix2)
			*forward_matrix = dcp->forward_matrix2;
	}

	if (reduction_matrix)
	{
		if (dcp->has_reduction_matrix1 && dcp->has_reduction_matrix2)
			matrix3_interpolate(&dcp->reduction_matrix1, &dcp->reduction_matrix2, alpha, reduction_matrix);
		else if (dcp->has_reduction_matrix1)
			*reduction_matrix = dcp->reduction_matrix1;
		else if (dcp->has_reduction_matrix2)
			*reduction_matrix = dcp->reduction_matrix2;
	}

	/* We don't have camera_calibration anyway! */

	return color_matrix;
}

/* Verified to behave like dng_camera_profile::NormalizeForwardMatrix */
static void
normalize_forward_matrix(RS_MATRIX3 *matrix)
{
	RS_MATRIX3 tmp;
	RS_VECTOR3 camera_one = {{1.0}, {1.0}, {1.0} };

	RS_MATRIX3 pcs_to_xyz_dia = vector3_as_diagonal(&XYZ_WP_D50);
	RS_VECTOR3 xyz = vector3_multiply_matrix(&camera_one, matrix);
	RS_MATRIX3 xyz_as_dia = vector3_as_diagonal(&xyz);
	RS_MATRIX3 xyz_as_dia_inv = matrix3_invert(&xyz_as_dia);

	matrix3_multiply(&pcs_to_xyz_dia, &xyz_as_dia_inv, &tmp);
	matrix3_multiply(&tmp, matrix, matrix);
}

/* dng_color_spec::SetWhiteXY */
static void
set_white_xy(RSDcp *dcp, const RS_xy_COORD *xy)
{
	RS_MATRIX3 color_matrix;
	RS_MATRIX3 forward_matrix;
	RS_MATRIX3 reduction_matrix;

	dcp->white_xy = *xy;

	color_matrix = find_xyz_to_camera(dcp, xy, &forward_matrix, &reduction_matrix, NULL);

	RS_XYZ_VECTOR white = xy_to_XYZ(xy);

	dcp->camera_white = vector3_multiply_matrix(&white, &color_matrix);

	gfloat white_scale = 1.0 / vector3_max(&dcp->camera_white);

	dcp->camera_white.x = CLAMP(0.001, white_scale * dcp->camera_white.x, 1.0);
	dcp->camera_white.y = CLAMP(0.001, white_scale * dcp->camera_white.y, 1.0);
	dcp->camera_white.z = CLAMP(0.001, white_scale * dcp->camera_white.z, 1.0);

	if (dcp->has_forward_matrix1 || dcp->has_forward_matrix2)
	{
		/* verified by DNG SDK */
		RS_MATRIX3 refCameraWhite_diagonal = vector3_as_diagonal(&dcp->camera_white);

		RS_MATRIX3 refCameraWhite_diagonal_inv = matrix3_invert(&refCameraWhite_diagonal); /* D */
		matrix3_multiply(&forward_matrix, &refCameraWhite_diagonal_inv, &dcp->camera_to_pcs);
	}
	else
	{
		/* FIXME: test this */
		RS_xy_COORD PCStoXY = {0.3457, 0.3585};
		RS_MATRIX3 map = rs_calculate_map_white_matrix(&PCStoXY, xy); /* or &white?! */
		RS_MATRIX3 pcs_to_camera;
		matrix3_multiply(&color_matrix, &map, &pcs_to_camera);
		RS_VECTOR3 tmp = vector3_multiply_matrix(&XYZ_WP_D50, &pcs_to_camera);
		gfloat scale = vector3_max(&tmp);
		matrix3_scale(&pcs_to_camera, 1.0 / scale, &pcs_to_camera);
		dcp->camera_to_pcs = matrix3_invert(&pcs_to_camera);
	}

}

static void
precalc(RSDcp *dcp)
{
	const static RS_MATRIX3 xyz_to_prophoto = {{
		{  1.3459433, -0.2556075, -0.0511118 },
		{ -0.5445989,  1.5081673,  0.0205351 },
		{  0.0000000,  0.0000000,  1.2118128 }
	}};
	const RS_MATRIX3 prophoto_to_xyz = matrix3_invert(&xyz_to_prophoto);
	/* This HAS ben adopted for D50 -> D65 white point */
	const static RS_MATRIX3 xyz_to_rgb = {{
		{ 3.1338582120812,   - 1.6168645994761,  - 0.4906125135547 },
		{ - 0.978769586326,   1.9161399511888,    0.0334523812116  },
		{ 0.0719452014624,   - 0.2289912335361,   1.4052430533683  }
	}};

	/* Build Prophoto to sRGB */
	matrix3_multiply(&xyz_to_rgb, &prophoto_to_xyz, &dcp->prophoto_to_srgb);

	/* Camera to ProPhoto */
	matrix3_multiply(&xyz_to_prophoto, &dcp->camera_to_pcs, &dcp->camera_to_prophoto); /* verified by SDK */
}

static RS_MATRIX3
read_matrix(RSTiff *tiff, guint offset)
{
	RS_MATRIX3 matrix;

	matrix.coeff[0][0] = rs_tiff_get_rational(tiff, offset);
	matrix.coeff[0][1] = rs_tiff_get_rational(tiff, offset+8);
	matrix.coeff[0][2] = rs_tiff_get_rational(tiff, offset+16);
	matrix.coeff[1][0] = rs_tiff_get_rational(tiff, offset+24);
	matrix.coeff[1][1] = rs_tiff_get_rational(tiff, offset+32);
	matrix.coeff[1][2] = rs_tiff_get_rational(tiff, offset+40);
	matrix.coeff[2][0] = rs_tiff_get_rational(tiff, offset+48);
	matrix.coeff[2][1] = rs_tiff_get_rational(tiff, offset+56);
	matrix.coeff[2][2] = rs_tiff_get_rational(tiff, offset+64);

	return matrix;
}

static void
read_profile(RSDcp *dcp, const gchar *filename)
{
	RSTiff *tiff = rs_tiff_new_from_file(filename);
	RSTiffIfdEntry *entry;

	/* FIXME: Reset this properly */
	dcp->has_color_matrix1 = FALSE;
	dcp->has_color_matrix2 = FALSE;
	dcp->has_reduction_matrix1 = FALSE;
	dcp->has_reduction_matrix2 = FALSE;
	dcp->has_forward_matrix1 = FALSE;
	dcp->has_forward_matrix2 = FALSE;

	/* ColorMatrix1 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc621);
	if (entry)
	{
		dcp->color_matrix1 = read_matrix(tiff, entry->value_offset);
		dcp->has_color_matrix1 = TRUE;
	}
	else
		matrix3_identity(&dcp->color_matrix1);

	/* ColorMatrix2 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc622);
	if (entry)
	{
		dcp->color_matrix2 = read_matrix(tiff, entry->value_offset);
		dcp->has_color_matrix2 = TRUE;
	}
	else
		matrix3_identity(&dcp->color_matrix2);

 	/* ReductionMatrix1 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc725);
	if (entry)
	{
		dcp->reduction_matrix1 = read_matrix(tiff, entry->value_offset);
		dcp->has_reduction_matrix1 = TRUE;
	}
	else
		matrix3_identity(&dcp->reduction_matrix1);

 	/* ReductionMatrix2 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc726);
	if (entry)
	{
		dcp->reduction_matrix2 = read_matrix(tiff, entry->value_offset);
		dcp->has_reduction_matrix2 = TRUE;
	}
	else
		matrix3_identity(&dcp->reduction_matrix2);

	/* CalibrationIlluminant1 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc65a);
	if (entry)
		dcp->temp1 = temp_from_exif_illuminant(entry->value_offset);
	else
		dcp->temp1 = 5000;

	/* CalibrationIlluminant2 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc65b);
	if (entry)
		dcp->temp2 = temp_from_exif_illuminant(entry->value_offset);
	else
		dcp->temp2 = 5000;

	/* ProfileToneCurve */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc6fc);
	if (entry)
	{
		gint i;
		gint num_knots = entry->count / 2;
		gfloat *knots = g_new0(gfloat, entry->count);

		for(i = 0; i < entry->count; i++)
			knots[i] = rs_tiff_get_float(tiff, (entry->value_offset+(i*4)));

		dcp->baseline_exposure = rs_spline_new(knots, num_knots, NATURAL);
		dcp->baseline_exposure_lut = rs_spline_sample(dcp->baseline_exposure, NULL, 65536);
	}
	/* ForwardMatrix1 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc714);
	if (entry)
	{
		dcp->forward_matrix1 = read_matrix(tiff, entry->value_offset);
		dcp->has_forward_matrix1 = TRUE;
		normalize_forward_matrix(&dcp->forward_matrix1);
	}
	else
		matrix3_identity(&dcp->forward_matrix1);

	/* ForwardMatrix2 */
	entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc715);
	if (entry)
	{
		dcp->forward_matrix2 = read_matrix(tiff, entry->value_offset);
		dcp->has_forward_matrix2 = TRUE;
	}
	else
		matrix3_identity(&dcp->forward_matrix2);

	dcp->looktable = rs_huesat_map_new_from_dcp(tiff, 0, 0xc725, 0xc726);
	dcp->huesatmap1 = rs_huesat_map_new_from_dcp(tiff, 0, 0xc6f9, 0xc6fa);
	dcp->huesatmap2 = rs_huesat_map_new_from_dcp(tiff, 0, 0xc6f9, 0xc6fb);
	dcp->huesatmap = dcp->huesatmap1;
	g_object_unref(tiff);

	precalc(dcp);
}

/*
+ 0xc621 ColorMatrix1 (9 * SRATIONAL)
+ 0xc622 ColorMatrix2 (9 * SRATIONAL)
+ 0xc725 ReductionMatrix1 (9 * SRATIONAL)
+ 0xc726 ReductionMatrix2 (9 * SRATIONAL)
+ 0xc65a CalibrationIlluminant1 (1 * SHORT)
+ 0xc65b CalibrationIlluminant2 (1 * SHORT)
• 0xc6f4 ProfileCalibrationSignature (ASCII or BYTE)
• 0xc6f8 ProfileName (ASCII or BYTE)
• 0xc6f9 ProfileHueSatMapDims (3 * LONG)
• 0xc6fa ProfileHueSatMapData1 (FLOAT)
• 0xc6fb ProfileHueSatMapData2 (FLOAT)
• 0xc6fc ProfileToneCurve (FLOAT)
• 0xc6fd ProfileEmbedPolicy (LONG)
• 0xc6fe ProfileCopyright (ASCII or BYTE)
+ 0xc714 ForwardMatrix1 (SRATIONAL)
+ 0xc715 ForwardMatrix2 (SRATIONAL)
• 0xc725 ProfileLookTableDims (3 * LONG)
• 0xc726 ProfileLookTableData
*/

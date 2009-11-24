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

#include "config.h"
#include <rawstudio.h>
#include <math.h> /* pow() */
#if defined (__SSE2__)
#include <emmintrin.h>
#endif /* __SSE2__ */
#define RS_TYPE_DCP (rs_dcp_type)
#define RS_DCP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DCP, RSDcp))
#define RS_DCP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DCP, RSDcpClass))
#define RS_IS_DCP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DCP))
#define RS_DCP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_DCP, RSDcpClass))

typedef struct _RSDcp RSDcp;
typedef struct _RSDcpClass RSDcpClass;

#if defined (__SSE2__)

typedef struct {
	//Precalc:
	gfloat hScale[4] __attribute__ ((aligned (16)));
	gfloat sScale[4] __attribute__ ((aligned (16)));
	gfloat vScale[4] __attribute__ ((aligned (16)));
	gint maxHueIndex0[4] __attribute__ ((aligned (16)));
	gint maxSatIndex0[4] __attribute__ ((aligned (16)));
	gint maxValIndex0[4] __attribute__ ((aligned (16)));
	gint hueStep[4] __attribute__ ((aligned (16)));
	gint valStep[4] __attribute__ ((aligned (16)));
} PrecalcHSM;

#endif  // defined (__SSE2__)

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

	RSSpline *tone_curve;
	gfloat *tone_curve_lut;

	gboolean has_color_matrix1;
	gboolean has_color_matrix2;
	RS_MATRIX3 color_matrix1;
	RS_MATRIX3 color_matrix2;

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
	
#if defined (__SSE2__)
	PrecalcHSM huesatmap_precalc;
	PrecalcHSM looktable_precalc;
#endif  // defined (__SSE2__)
};

struct _RSDcpClass {
	RSFilterClass parent_class;

	RSIccProfile *prophoto_profile;
};

typedef struct {
	RSDcp *dcp;
	GThread *threadid;
	gint start_x;
	gint start_y;
	gint end_y;
	RS_IMAGE16 *tmp;

} ThreadInfo;

RS_DEFINE_FILTER(rs_dcp, RSDcp)

enum {
	PROP_0,
	PROP_SETTINGS,
	PROP_PROFILE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSDcp *dcp);
static RS_xy_COORD neutral_to_xy(RSDcp *dcp, const RS_VECTOR3 *neutral);
static RS_MATRIX3 find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix);
static void set_white_xy(RSDcp *dcp, const RS_xy_COORD *xy);
static void precalc(RSDcp *dcp);
static void render(ThreadInfo* t);
#if defined (__SSE2__)
static void render_SSE2(ThreadInfo* t);
static void calc_hsm_constants(const RSHuesatMap *map, PrecalcHSM* table); 
#endif
static void read_profile(RSDcp *dcp, RSDcpFile *dcp_file);
static RSIccProfile *get_icc_profile(RSFilter *filter);

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

	klass->prophoto_profile = rs_icc_profile_new_from_file(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/prophoto.icc");

	g_object_class_install_property(object_class,
		PROP_SETTINGS, g_param_spec_object(
			"settings", "Settings", "Settings to render from",
			RS_TYPE_SETTINGS, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_PROFILE, g_param_spec_object(
			"profile", "profile", "DCP Profile",
			RS_TYPE_DCP_FILE, G_PARAM_READWRITE)
	);

	filter_class->name = "Adobe DNG camera profile filter";
	filter_class->get_image = get_image;
	filter_class->get_icc_profile = get_icc_profile;
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
	RSFilter *filter = RS_FILTER(dcp);
	RSSettings *settings;

	switch (property_id)
	{
		case PROP_SETTINGS:
			settings = g_value_get_object(value);
			g_signal_connect(settings, "settings-changed", G_CALLBACK(settings_changed), dcp);
			settings_changed(settings, MASK_ALL, dcp);
			break;
		case PROP_PROFILE:
			read_profile(dcp, g_value_get_object(value));
			rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

gpointer
start_single_dcp_thread(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	RS_IMAGE16 *tmp = t->tmp;

#if defined (__SSE2__)
	if (rs_detect_cpu_features() & RS_CPU_FLAG_SSE2)
	{
		render_SSE2(t);
		if (tmp->w & 3)
		{
			t->start_x = tmp->w - (tmp->w & 3);
			render(t);
		}

	}
	else
#endif
		render(t);

	g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSDcp *dcp = RS_DCP(filter);
	GdkRectangle *roi;
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	RS_IMAGE16 *tmp;

	previous_response = rs_filter_get_image(filter->previous, request);

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

	if ((roi = rs_filter_request_get_roi(request)))
		tmp = rs_image16_new_subframe(output, roi);
	else
		tmp = g_object_ref(output);

	guint i, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	ThreadInfo *t = g_new(ThreadInfo, threads);

	threaded_h = tmp->h;
	y_per_thread = (threaded_h + threads-1)/threads;
	y_offset = 0;

	for (i = 0; i < threads; i++)
	{
		t[i].tmp = tmp;
		t[i].start_y = y_offset;
		t[i].start_x = 0;
		t[i].dcp = dcp;
		y_offset += y_per_thread;
		y_offset = MIN(tmp->h, y_offset);
		t[i].end_y = y_offset;

		t[i].threadid = g_thread_create(start_single_dcp_thread, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);

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
		RS_MATRIX3 xyz_to_camera = find_xyz_to_camera(dcp, &last, NULL);
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

#if defined (__SSE2__)

static gfloat _zero_ps[4] __attribute__ ((aligned (16))) = {0.0f, 0.0f, 0.0f, 0.0f};
static gfloat _ones_ps[4] __attribute__ ((aligned (16))) = {1.0f, 1.0f, 1.0f, 1.0f};
static gfloat _two_ps[4] __attribute__ ((aligned (16))) = {2.0f, 2.0f, 2.0f, 2.0f};
static gfloat _six_ps[4] __attribute__ ((aligned (16))) = {6.0f-1e-15, 6.0f-1e-15, 6.0f-1e-15, 6.0f-1e-15};

static inline void
RGBtoHSV_SSE(__m128 *c0, __m128 *c1, __m128 *c2)
{

	__m128 zero_ps = _mm_load_ps(_zero_ps);
	__m128 ones_ps = _mm_load_ps(_ones_ps);
	// Any number > 1
	__m128 add_v = _mm_load_ps(_two_ps);

	__m128 r = *c0;
	__m128 g = *c1;
	__m128 b = *c2;

	__m128 h, v;
	v = _mm_max_ps(b,_mm_max_ps(r,g));

	__m128 m = _mm_min_ps(b,_mm_min_ps(r,g));
	__m128 gap = _mm_sub_ps(v,m);
	__m128 v_mask = _mm_cmpeq_ps(gap, zero_ps);
	v = _mm_add_ps(v, _mm_and_ps(add_v, v_mask));

	h = _mm_xor_ps(r,r);

	/* Set gap to one where sat = 0, this will avoid divisions by zero, these values will not be used */
	ones_ps = _mm_and_ps(ones_ps, v_mask);
	gap = _mm_or_ps(gap, ones_ps);
	/*  gap_inv = 1.0 / gap */
	__m128 gap_inv = _mm_rcp_ps(gap);

	/* if r == v */
	/* h = (g - b) / gap; */
	__m128 mask = _mm_cmpeq_ps(r, v);
	__m128 val = _mm_mul_ps(gap_inv, _mm_sub_ps(g, b));

	/* fill h */
	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_or_ps(h, _mm_and_ps(val, mask));

	/* if g == v */
	/* h = 2.0f + (b - r) / gap; */
	__m128 two_ps = _mm_load_ps(_two_ps);
	mask = _mm_cmpeq_ps(g, v);
	val = _mm_sub_ps(b, r);
	val = _mm_mul_ps(val, gap_inv);
	val = _mm_add_ps(val, two_ps);

	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_or_ps(h, _mm_and_ps(val, mask));

	/* If (b == v) */
	/* h = 4.0f + (r - g) / gap; */
	__m128 four_ps = _mm_add_ps(two_ps, two_ps);
	mask = _mm_cmpeq_ps(b, v);
	val = _mm_add_ps(four_ps, _mm_mul_ps(gap_inv, _mm_sub_ps(r, g)));

	v = _mm_add_ps(v, _mm_and_ps(add_v, mask));
	h = _mm_or_ps(h, _mm_and_ps(val, mask));

	__m128 s;
	/* Fill s, if gap > 0 */
	v = _mm_sub_ps(v, add_v);
	val = _mm_mul_ps(gap,_mm_rcp_ps(v));
	s = _mm_andnot_ps(v_mask, val );

	/* Check if h < 0 */
	__m128 six_ps = _mm_load_ps(_six_ps);
	mask = _mm_cmplt_ps(h, zero_ps);
	h = _mm_add_ps(h, _mm_and_ps(mask, six_ps));

	*c0 = h;
	*c1 = s;
	*c2 = v;
}


static inline void
HSVtoRGB_SSE(__m128 *c0, __m128 *c1, __m128 *c2)
{
	__m128 h = *c0;
	__m128 s = *c1;
	__m128 v = *c2;
	__m128 r, g, b;
	
	/* Convert get the fraction of h
	* h_fraction = h - (float)(int)h */
	__m128 ones_ps = _mm_load_ps(_ones_ps);
	__m128 h_fraction = _mm_sub_ps(h,_mm_cvtepi32_ps(_mm_cvttps_epi32(h)));

	/* p = v * (1.0f - s)  */
	__m128 p = _mm_mul_ps(v,  _mm_sub_ps(ones_ps, s));
	/* q = (v * (1.0f - s * f)) */
	__m128 q = _mm_mul_ps(v, _mm_sub_ps(ones_ps, _mm_mul_ps(s, h_fraction)));
	/* t = (v * (1.0f - s * (1.0f - f))) */
	__m128 t = _mm_mul_ps(v, _mm_sub_ps(ones_ps, _mm_mul_ps(s, _mm_sub_ps(ones_ps, h_fraction))));

	/* h < 1  (case 0)*/
	/* case 0: *r = v; *g = t; *b = p; break; */
	__m128 h_threshold = _mm_add_ps(ones_ps, ones_ps);
	__m128 out_mask = _mm_cmplt_ps(h, ones_ps);
	r = _mm_and_ps(v, out_mask);
	g = _mm_and_ps(t, out_mask);
	b = _mm_and_ps(p, out_mask);

	/* h < 2 (case 1) */
	/* case 1: *r = q; *g = v; *b = p; break; */
	__m128 m = _mm_cmplt_ps(h, h_threshold);
	h_threshold = _mm_add_ps(h_threshold, ones_ps);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_or_ps(r, _mm_and_ps(q, m));
	g = _mm_or_ps(g, _mm_and_ps(v, m));
	b = _mm_or_ps(b, _mm_and_ps(p, m));
	out_mask = _mm_or_ps(out_mask, m);

	/* h < 3 (case 2)*/
	/* case 2: *r = p; *g = v; *b = t; break; */
	m = _mm_cmplt_ps(h, h_threshold);
	h_threshold = _mm_add_ps(h_threshold, ones_ps);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_or_ps(r, _mm_and_ps(p, m));
	g = _mm_or_ps(g, _mm_and_ps(v, m));
	b = _mm_or_ps(b, _mm_and_ps(t, m));
	out_mask = _mm_or_ps(out_mask, m);

	/* h < 4 (case 3)*/
	/* case 3: *r = p; *g = q; *b = v; break; */
	m = _mm_cmplt_ps(h, h_threshold);
	h_threshold = _mm_add_ps(h_threshold, ones_ps);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_or_ps(r, _mm_and_ps(p, m));
	g = _mm_or_ps(g, _mm_and_ps(q, m));
	b = _mm_or_ps(b, _mm_and_ps(v, m));
	out_mask = _mm_or_ps(out_mask, m);

	/* h < 5 (case 4)*/
	/* case 4: *r = t; *g = p; *b = v; break; */
	m = _mm_cmplt_ps(h, h_threshold);
	m = _mm_andnot_ps(out_mask, m);
	r = _mm_or_ps(r, _mm_and_ps(t, m));
	g = _mm_or_ps(g, _mm_and_ps(p, m));
	b = _mm_or_ps(b, _mm_and_ps(v, m));
	out_mask = _mm_or_ps(out_mask, m);


	/* Remainder (case 5) */
	/* case 5: *r = v; *g = p; *b = q; break; */
	__m128 all_ones = _mm_cmpeq_ps(h,h);
	m = _mm_xor_ps(out_mask, all_ones);
	r = _mm_or_ps(r, _mm_and_ps(v, m));
	g = _mm_or_ps(g, _mm_and_ps(p, m));
	b = _mm_or_ps(b, _mm_and_ps(q, m));
	
	*c0 = r;
	*c1 = g;
	*c2 = b;
}

#endif

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

#if defined (__SSE2__)

/* SSE2 implementation, matches the reference implementation pretty closely */


static void 
calc_hsm_constants(const RSHuesatMap *map, PrecalcHSM* table) 
{
	g_assert(RS_IS_HUESAT_MAP(map));
	int i;
	for (i = 0; i < 4; i++) 
	{
		table->hScale[i] = (map->hue_divisions < 2) ? 0.0f : (map->hue_divisions * (1.0f / 6.0f));
		table->sScale[i] = (gfloat) (map->sat_divisions - 1);
		table->vScale[i] =  (gfloat) (map->val_divisions - 1);
		table->maxHueIndex0[i] = map->hue_divisions - 1;
		table->maxSatIndex0[i] = map->sat_divisions - 2;
		table->maxValIndex0[i] = map->val_divisions - 2;
		table->hueStep[i] =  map->sat_divisions;
		table->valStep[i] = map->hue_divisions * map->sat_divisions;
	}
}

static gfloat _mul_hue_ps[4] __attribute__ ((aligned (16))) = {6.0f / 360.0f, 6.0f / 360.0f, 6.0f / 360.0f, 6.0f / 360.0f};
static gint _ones_epi32[4] __attribute__ ((aligned (16))) = {1,1,1,1};

static void
huesat_map_SSE2(RSHuesatMap *map, const PrecalcHSM* precalc, __m128 *_h, __m128 *_s, __m128 *_v)
{
	g_assert(RS_IS_HUESAT_MAP(map));

	__m128 h = *_h;
	__m128 s = *_s;
	__m128 v = *_v;
	gint i;
	gint xfer_0[4] __attribute__ ((aligned (16)));
	gint xfer_1[4] __attribute__ ((aligned (16)));

	const RS_VECTOR3 *tableBase = map->deltas;

	__m128 hueShift;
	__m128 satScale;
	__m128 valScale;

	if (map->val_divisions < 2)
	{
		__m128 hScaled = _mm_mul_ps(h, _mm_load_ps(precalc->hScale));
		__m128 sScaled = _mm_mul_ps(s,  _mm_load_ps(precalc->sScale));

		__m128i maxHueIndex0 = _mm_load_si128((__m128i*)precalc->maxHueIndex0);
		__m128i maxSatIndex0 = _mm_load_si128((__m128i*)precalc->maxSatIndex0);
		__m128i hIndex0 = _mm_cvttps_epi32( hScaled );
		__m128i sIndex0 = _mm_cvttps_epi32( sScaled );

		sIndex0 = _mm_min_epi16(sIndex0, maxSatIndex0);
		__m128i ones_epi32 = _mm_load_si128((__m128i*)_ones_epi32);
		__m128i hIndex1 = _mm_add_epi32(hIndex0, ones_epi32);

		/* if (hIndex0 >= maxHueIndex0) */
		__m128i hIndexMask = _mm_cmpgt_epi32( hIndex0, _mm_sub_epi32(maxHueIndex0, ones_epi32));
		hIndex0 = _mm_andnot_si128(hIndexMask, hIndex0);
		/* hIndex1 = 0; */
		hIndex1 = _mm_andnot_si128(hIndexMask, hIndex1);
		/* hIndex0 = maxHueIndex0 */
		hIndex0 = _mm_or_si128(hIndex0, _mm_and_si128(hIndexMask, maxHueIndex0));

		__m128 hFract1 = _mm_sub_ps( hScaled, _mm_cvtepi32_ps(hIndex0));
		__m128 sFract1 = _mm_sub_ps( sScaled, _mm_cvtepi32_ps(sIndex0));
		__m128 ones_ps = _mm_load_ps(_ones_ps);

		__m128 hFract0 = _mm_sub_ps(ones_ps, hFract1);
		__m128 sFract0 = _mm_sub_ps(ones_ps, sFract1);
		__m128i hueStep = _mm_load_si128((__m128i*)precalc->hueStep);
		__m128i table_offsets = _mm_add_epi32(sIndex0, _mm_mullo_epi16(hIndex0, hueStep));
		__m128i next_offsets = _mm_add_epi32(sIndex0, _mm_mullo_epi16(hIndex1, hueStep));

		_mm_store_si128((__m128i*)xfer_0, table_offsets);
		_mm_store_si128((__m128i*)xfer_1, next_offsets);

		const RS_VECTOR3 *entry00[4] = { tableBase + xfer_0[0], tableBase + xfer_0[1], tableBase + xfer_0[2], tableBase + xfer_0[3]};
		const RS_VECTOR3 *entry01[4] = { tableBase + xfer_1[0], tableBase + xfer_1[1], tableBase + xfer_1[2], tableBase + xfer_1[3]};

		__m128 hs0 = _mm_set_ps(entry00[3]->fHueShift, entry00[2]->fHueShift, entry00[1]->fHueShift, entry00[0]->fHueShift);
		__m128 hs1 = _mm_set_ps(entry01[3]->fHueShift, entry01[2]->fHueShift, entry01[1]->fHueShift, entry01[0]->fHueShift);
		__m128 hueShift0 = _mm_add_ps(_mm_mul_ps(hs0, hFract0), _mm_mul_ps(hs1, hFract1));
		hueShift0 = _mm_mul_ps(hueShift0, sFract0);

		__m128 ss0 = _mm_set_ps(entry00[3]->fSatScale, entry00[2]->fSatScale, entry00[1]->fSatScale, entry00[0]->fSatScale);
		__m128 ss1 = _mm_set_ps(entry01[3]->fSatScale, entry01[2]->fSatScale, entry01[1]->fSatScale, entry01[0]->fSatScale);
		__m128 satScale0 = _mm_add_ps(_mm_mul_ps(ss0, hFract0), _mm_mul_ps(ss1, hFract1));
		satScale0 = _mm_mul_ps(satScale0, sFract0);

		__m128 vs0 = _mm_set_ps(entry00[3]->fValScale, entry00[2]->fValScale, entry00[1]->fValScale, entry00[0]->fValScale);
		__m128 vs1 = _mm_set_ps(entry01[3]->fValScale, entry01[2]->fValScale, entry01[1]->fValScale, entry01[0]->fValScale);
		__m128 valScale0 = _mm_add_ps(_mm_mul_ps(vs0, hFract0), _mm_mul_ps(vs1, hFract1));
		valScale0 = _mm_mul_ps(valScale0, sFract0);

		for (i = 0; i < 4; i++) {
			entry00[i]++;
			entry01[i]++;
		}

		hs0 = _mm_set_ps(entry00[3]->fHueShift, entry00[2]->fHueShift, entry00[1]->fHueShift, entry00[0]->fHueShift);
		hs1 = _mm_set_ps(entry01[3]->fHueShift, entry01[2]->fHueShift, entry01[1]->fHueShift, entry01[0]->fHueShift);
		__m128 hueShift1 = _mm_add_ps(_mm_mul_ps(hs0, hFract0), _mm_mul_ps(hs1, hFract1));
		hueShift = _mm_add_ps(hueShift0, _mm_mul_ps(hueShift1, sFract1));

		ss0 = _mm_set_ps(entry00[3]->fSatScale, entry00[2]->fSatScale, entry00[1]->fSatScale, entry00[0]->fSatScale);
		ss1 = _mm_set_ps(entry01[3]->fSatScale, entry01[2]->fSatScale, entry01[1]->fSatScale, entry01[0]->fSatScale);
		__m128 satScale1 = _mm_add_ps(_mm_mul_ps(ss0, hFract0), _mm_mul_ps(ss1, hFract1));
		satScale = _mm_add_ps(satScale0, _mm_mul_ps(satScale1, sFract1));

		vs0 = _mm_set_ps(entry00[3]->fValScale, entry00[2]->fValScale, entry00[1]->fValScale, entry00[0]->fValScale);
		vs1 = _mm_set_ps(entry01[3]->fValScale, entry01[2]->fValScale, entry01[1]->fValScale, entry01[0]->fValScale);
		__m128 valScale1 = _mm_add_ps(_mm_mul_ps(vs0, hFract0), _mm_mul_ps(vs1, hFract1));
		valScale = _mm_add_ps(valScale0, _mm_mul_ps(valScale1, sFract1));

	}
	else
	{
		__m128 hScaled = _mm_mul_ps(h, _mm_load_ps(precalc->hScale));
		__m128 sScaled = _mm_mul_ps(s,  _mm_load_ps(precalc->sScale));
		__m128 vScaled = _mm_mul_ps(v,  _mm_load_ps(precalc->vScale));

		__m128i maxHueIndex0 = _mm_load_si128((__m128i*)precalc->maxHueIndex0);
		__m128i maxSatIndex0 = _mm_load_si128((__m128i*)precalc->maxSatIndex0);
		__m128i maxValIndex0 = _mm_load_si128((__m128i*)precalc->maxValIndex0);
		
		__m128i hIndex0 = _mm_cvttps_epi32(hScaled);
		__m128i sIndex0 = _mm_cvttps_epi32(sScaled);
		__m128i vIndex0 = _mm_cvttps_epi32(vScaled);

		// Requires that maxSatIndex0 and sIndex0 can be contained within a 16 bit signed word.
		sIndex0 = _mm_min_epi16(sIndex0, maxSatIndex0);
		vIndex0 = _mm_min_epi16(vIndex0, maxValIndex0);
		__m128i ones_epi32 = _mm_load_si128((__m128i*)_ones_epi32);
		__m128i hIndex1 = _mm_add_epi32(hIndex0, ones_epi32);

		/* if (hIndex0 > (maxHueIndex0 - 1)) */
		__m128i hIndexMask = _mm_cmpgt_epi32( hIndex0, _mm_sub_epi32(maxHueIndex0, ones_epi32));
		/* Make room in hIndex0 */
		hIndex0 = _mm_andnot_si128(hIndexMask, hIndex0);
		/* hIndex1 = 0; */
		hIndex1 = _mm_andnot_si128(hIndexMask, hIndex1);
		/* hIndex0 = maxHueIndex0, where hIndex0 >= (maxHueIndex0) */
		hIndex0 = _mm_or_si128(hIndex0, _mm_and_si128(hIndexMask, maxHueIndex0));

		__m128 hFract1 = _mm_sub_ps( hScaled, _mm_cvtepi32_ps(hIndex0));
		__m128 sFract1 = _mm_sub_ps( sScaled, _mm_cvtepi32_ps(sIndex0));
		__m128 vFract1 = _mm_sub_ps( vScaled, _mm_cvtepi32_ps(vIndex0));
		__m128 ones_ps = _mm_load_ps(_ones_ps);

		__m128 hFract0 = _mm_sub_ps(ones_ps, hFract1);
		__m128 sFract0 = _mm_sub_ps(ones_ps, sFract1);
		__m128 vFract0 = _mm_sub_ps(ones_ps, vFract1);

		__m128i hueStep = _mm_load_si128((__m128i*)precalc->hueStep);
		__m128i valStep = _mm_load_si128((__m128i*)precalc->valStep);

		// This requires that hueStep and valStep can be contained in a 16 bit signed integer.
		__m128i table_offsets = _mm_add_epi32(sIndex0, _mm_mullo_epi16(vIndex0, valStep));
		__m128i next_offsets = _mm_mullo_epi16(hIndex1, hueStep);
		next_offsets = _mm_add_epi32(next_offsets, table_offsets);
		table_offsets = _mm_add_epi32(table_offsets, _mm_mullo_epi16(hIndex0, hueStep));

		// TODO: This will result in a store->load forward size mismatch penalty, if possible, avoid.
		_mm_store_si128((__m128i*)xfer_0, table_offsets);
		_mm_store_si128((__m128i*)xfer_1, next_offsets);
		gint _valStep = precalc->valStep[0];
		
		const RS_VECTOR3 *entry00[4] = { tableBase + xfer_0[0], tableBase + xfer_0[1], tableBase + xfer_0[2], tableBase + xfer_0[3]};
		const RS_VECTOR3 *entry01[4] = { tableBase + xfer_1[0], tableBase + xfer_1[1], tableBase + xfer_1[2], tableBase + xfer_1[3]};
		const RS_VECTOR3 *entry10[4] = { entry00[0] + _valStep, entry00[1] + _valStep, entry00[2] + _valStep, entry00[3] + _valStep};
		const RS_VECTOR3 *entry11[4] = { entry01[0] + _valStep, entry01[1] + _valStep, entry01[2] + _valStep, entry01[3] + _valStep};

		__m128 hs00 = _mm_set_ps(entry00[3]->fHueShift, entry00[2]->fHueShift, entry00[1]->fHueShift, entry00[0]->fHueShift);
		__m128 hs01 = _mm_set_ps(entry01[3]->fHueShift, entry01[2]->fHueShift, entry01[1]->fHueShift, entry01[0]->fHueShift);
		__m128 hs10 = _mm_set_ps(entry10[3]->fHueShift, entry10[2]->fHueShift, entry10[1]->fHueShift, entry10[0]->fHueShift);
		__m128 hs11 = _mm_set_ps(entry11[3]->fHueShift, entry11[2]->fHueShift, entry11[1]->fHueShift, entry11[0]->fHueShift);
		__m128 hueShift0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(hs00, hFract0), _mm_mul_ps(hs01, hFract1)));
		__m128 hueShift1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(hs10, hFract0), _mm_mul_ps(hs11, hFract1)));
		hueShift = _mm_mul_ps(sFract0, _mm_add_ps(hueShift0, hueShift1));

		__m128 ss00 = _mm_set_ps(entry00[3]->fSatScale, entry00[2]->fSatScale, entry00[1]->fSatScale, entry00[0]->fSatScale);
		__m128 ss01 = _mm_set_ps(entry01[3]->fSatScale, entry01[2]->fSatScale, entry01[1]->fSatScale, entry01[0]->fSatScale);
		__m128 ss10 = _mm_set_ps(entry10[3]->fSatScale, entry10[2]->fSatScale, entry10[1]->fSatScale, entry10[0]->fSatScale);
		__m128 ss11 = _mm_set_ps(entry11[3]->fSatScale, entry11[2]->fSatScale, entry11[1]->fSatScale, entry11[0]->fSatScale);
		__m128 satScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(ss00, hFract0), _mm_mul_ps(ss01, hFract1)));
		__m128 satScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(ss10, hFract0), _mm_mul_ps(ss11, hFract1)));
		satScale = _mm_mul_ps(sFract0, _mm_add_ps(satScale0, satScale1));

		__m128 vs00 = _mm_set_ps(entry00[3]->fValScale, entry00[2]->fValScale, entry00[1]->fValScale, entry00[0]->fValScale);
		__m128 vs01 = _mm_set_ps(entry01[3]->fValScale, entry01[2]->fValScale, entry01[1]->fValScale, entry01[0]->fValScale);
		__m128 vs10 = _mm_set_ps(entry10[3]->fValScale, entry10[2]->fValScale, entry10[1]->fValScale, entry10[0]->fValScale);
		__m128 vs11 = _mm_set_ps(entry11[3]->fValScale, entry11[2]->fValScale, entry11[1]->fValScale, entry11[0]->fValScale);
		__m128 valScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(vs00, hFract0), _mm_mul_ps(vs01, hFract1)));
		__m128 valScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(vs10, hFract0), _mm_mul_ps(vs11, hFract1)));
		valScale = _mm_mul_ps(sFract0, _mm_add_ps(valScale0, valScale1));

		for (i = 0; i < 4; i++) {
			entry00[i]++;
			entry01[i]++;
			entry10[i]++;
			entry11[i]++;
		}

		hs00 = _mm_set_ps(entry00[3]->fHueShift, entry00[2]->fHueShift, entry00[1]->fHueShift, entry00[0]->fHueShift);
		hs01 = _mm_set_ps(entry01[3]->fHueShift, entry01[2]->fHueShift, entry01[1]->fHueShift, entry01[0]->fHueShift);
		hs10 = _mm_set_ps(entry10[3]->fHueShift, entry10[2]->fHueShift, entry10[1]->fHueShift, entry10[0]->fHueShift);
		hs11 = _mm_set_ps(entry11[3]->fHueShift, entry11[2]->fHueShift, entry11[1]->fHueShift, entry11[0]->fHueShift);
		hueShift0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(hs00, hFract0), _mm_mul_ps(hs01, hFract1)));
		hueShift1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(hs10, hFract0), _mm_mul_ps(hs11, hFract1)));
		hueShift = _mm_add_ps(hueShift, _mm_mul_ps(sFract1, _mm_add_ps(hueShift0, hueShift1)));

		ss00 = _mm_set_ps(entry00[3]->fSatScale, entry00[2]->fSatScale, entry00[1]->fSatScale, entry00[0]->fSatScale);
		ss01 = _mm_set_ps(entry01[3]->fSatScale, entry01[2]->fSatScale, entry01[1]->fSatScale, entry01[0]->fSatScale);
		ss10 = _mm_set_ps(entry10[3]->fSatScale, entry10[2]->fSatScale, entry10[1]->fSatScale, entry10[0]->fSatScale);
		ss11 = _mm_set_ps(entry11[3]->fSatScale, entry11[2]->fSatScale, entry11[1]->fSatScale, entry11[0]->fSatScale);
		satScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(ss00, hFract0), _mm_mul_ps(ss01, hFract1)));
		satScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(ss10, hFract0), _mm_mul_ps(ss11, hFract1)));
		satScale = _mm_add_ps(satScale, _mm_mul_ps(sFract1, _mm_add_ps(satScale0, satScale1)));

		vs00 = _mm_set_ps(entry00[3]->fValScale, entry00[2]->fValScale, entry00[1]->fValScale, entry00[0]->fValScale);
		vs01 = _mm_set_ps(entry01[3]->fValScale, entry01[2]->fValScale, entry01[1]->fValScale, entry01[0]->fValScale);
		vs10 = _mm_set_ps(entry10[3]->fValScale, entry10[2]->fValScale, entry10[1]->fValScale, entry10[0]->fValScale);
		vs11 = _mm_set_ps(entry11[3]->fValScale, entry11[2]->fValScale, entry11[1]->fValScale, entry11[0]->fValScale);
		valScale0 = _mm_mul_ps(vFract0, _mm_add_ps(_mm_mul_ps(vs00, hFract0), _mm_mul_ps(vs01, hFract1)));
		valScale1 = _mm_mul_ps(vFract1, _mm_add_ps(_mm_mul_ps(vs10, hFract0), _mm_mul_ps(vs11, hFract1)));
		valScale = _mm_add_ps(valScale, _mm_mul_ps(sFract1, _mm_add_ps(valScale0, valScale1)));
	}

	__m128 mul_hue = _mm_load_ps(_mul_hue_ps);
	__m128 ones_ps = _mm_load_ps(_ones_ps);
	hueShift = _mm_mul_ps(hueShift, mul_hue);
	s = _mm_min_ps(ones_ps, _mm_mul_ps(s, satScale));
	v = _mm_min_ps(ones_ps, _mm_mul_ps(v, valScale));
	h = _mm_add_ps(h, hueShift);
	*_h = h;
	*_s = s;
	*_v = v;
}
#define DW(A) _mm_castps_si128(A)
#define PS(A) _mm_castsi128_ps(A)

static gfloat _very_small_ps[4] __attribute__ ((aligned (16))) = {1e-15, 1e-15, 1e-15, 1e-15};
static gfloat _16_bit_ps[4] __attribute__ ((aligned (16))) = {65535.0, 65535.0, 65535.0, 65535.0};

void inline
rgb_tone_sse2(__m128* _r, __m128* _g, __m128* _b, const gfloat * const tone_lut)
{
	int xfer[8] __attribute__ ((aligned (16)));

	__m128 r = *_r;
	__m128 g = *_g;
	__m128 b = *_b;
	
	__m128 lg = _mm_max_ps(b, _mm_max_ps(r, g));
	__m128 sm = _mm_min_ps(b, _mm_min_ps(r, g));
	__m128i lookup_max = _mm_cvtps_epi32(_mm_mul_ps(lg,
										 _mm_load_ps(_16_bit_ps)));
	__m128i lookup_min = _mm_cvtps_epi32(_mm_mul_ps(sm,
										 _mm_load_ps(_16_bit_ps)));

	_mm_store_si128((__m128i*)&xfer[0], lookup_max);
	_mm_store_si128((__m128i*)&xfer[4], lookup_min);
	
    /* Lookup */
	__m128 LG = _mm_set_ps(tone_lut[xfer[3]], tone_lut[xfer[2]], tone_lut[xfer[1]], tone_lut[xfer[0]]);
	__m128 SM = _mm_set_ps(tone_lut[xfer[7]], tone_lut[xfer[6]], tone_lut[xfer[5]], tone_lut[xfer[4]]);

	__m128i ones = _mm_cmpeq_epi32(DW(r), DW(r));
	__m128i is_r_lg = _mm_cmpeq_epi32(DW(r), DW(lg));
	__m128i is_g_lg = _mm_cmpeq_epi32(DW(g), DW(lg));
	__m128i is_b_lg = _mm_cmpeq_epi32(DW(b), DW(lg));
	
	__m128i is_r_sm = _mm_andnot_si128(is_r_lg, _mm_cmpeq_epi32(DW(r), DW(sm)));
	__m128i is_g_sm = _mm_andnot_si128(is_g_lg, _mm_cmpeq_epi32(DW(g), DW(sm)));
	__m128i is_b_sm = _mm_andnot_si128(is_b_lg, _mm_cmpeq_epi32(DW(b), DW(sm)));
	
	__m128i is_r_md = _mm_xor_si128(ones, _mm_or_si128(is_r_lg, is_r_sm));
	__m128i is_g_md = _mm_xor_si128(ones, _mm_or_si128(is_g_lg, is_g_sm));
	__m128i is_b_md = _mm_xor_si128(ones, _mm_or_si128(is_b_lg, is_b_sm));

	__m128 md = PS(_mm_or_si128(_mm_or_si128(
					_mm_and_si128(DW(r), is_r_md), 
					_mm_and_si128(DW(g), is_g_md)),
					_mm_and_si128(DW(b), is_b_md)));
	
	__m128 p = _mm_rcp_ps(_mm_sub_ps(lg, sm));
	__m128 q = _mm_sub_ps(md, sm);
	__m128 o = _mm_sub_ps(LG, SM);
	__m128 MD = _mm_add_ps(SM, _mm_mul_ps(o, _mm_mul_ps(p, q)));

	is_r_lg = _mm_cmpeq_epi32(DW(r), DW(lg));
	is_g_lg = _mm_cmpeq_epi32(DW(g), DW(lg));
	is_b_lg = _mm_cmpeq_epi32(DW(b), DW(lg));

	r = PS(_mm_or_si128( _mm_or_si128(
			_mm_and_si128(DW(LG), is_r_lg),
			_mm_and_si128(DW(SM), is_r_sm)), 
			_mm_and_si128(DW(MD), is_r_md)));
	
	g = PS(_mm_or_si128( _mm_or_si128(
			_mm_and_si128(DW(LG), is_g_lg),
			_mm_and_si128(DW(SM), is_g_sm)), 
			_mm_and_si128(DW(MD), is_g_md)));
	
	b = PS(_mm_or_si128( _mm_or_si128(
			_mm_and_si128(DW(LG), is_b_lg),
			_mm_and_si128(DW(SM), is_b_sm)), 
			_mm_and_si128(DW(MD), is_b_md)));
	*_r = r;
	*_g = g;
	*_b = b;
}

#undef DW
#undef PS

#endif // defined __SSE2__

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

	#define RGBTone(lg, md, sm, LG, MD, SM)\
	{\
		LG = tone_lut[_S(lg)];\
		SM = tone_lut[_S(sm)];\
		\
		MD = SM + ((LG - SM) * (md - sm) / (lg - sm));\
		\
	}
	/* Tone curve is:
		1. Lookup smallest and largest of R,G,B
		2. Middle value is calculated as (CAPS is curve corrected)
		   MD = SM +  ((LG - SM) * (md - sm) / (lg - sm))
		3. Store.  
	*/
	if (r >= g)
	{

		if (g > b)
		{
			// Case 1: r >= g > b; hue = 0-1
			RGBTone (r, g, b, rr, gg, bb);
		}
		else if (b > r)
		{
			// Case 2: b > r >= g; hue = 4-5
			RGBTone (b, r, g, bb, rr, gg);
		}
		else if (b > g)
		{
			// Case 3: r >= b > g; hue = 5-6
			RGBTone (r, b, g, rr, bb, gg);
		}
		else
		{
			// Case 4: r >= g == b; s = 0;
			rr = tone_lut[_S(r)];
			gg = tone_lut[_S(b)];
			bb = gg;
		}
	}
	else	// g > r
	{
		if (r >= b)
		{
			// Case 5: g > r >= b; hue = 1-2
			RGBTone (g, r, b, gg, rr, bb);
		}
		else if (b > g)
		{
			// Case 6: b > g > r; hue = 3-4
			RGBTone (b, g, r, bb, gg, rr);
		}
		else
		{
			// Case 7: g >= b > r; hue = 2-3
			RGBTone (g, b, r, gg, bb, rr);
		}
	}

	#undef RGBTone
	*_r = rr;
	*_g = gg;
	*_b = bb;

}

#if defined (__SSE2__)

static inline __m128
sse_matrix3_mul(float* mul, __m128 a, __m128 b, __m128 c)
{

	__m128 v = _mm_load_ps(mul);
	__m128 acc = _mm_mul_ps(a, v);

	v = _mm_load_ps(mul+4);
	acc = _mm_add_ps(acc, _mm_mul_ps(b, v));

	v = _mm_load_ps(mul+8);
	acc = _mm_add_ps(acc, _mm_mul_ps(c, v));

	return acc;
}

static gfloat _rgb_div_ps[4] __attribute__ ((aligned (16))) = {1.0/65535.0, 1.0/65535.0, 1.0/65535.0, 1.0/65535.0};
static gint _15_bit_epi32[4] __attribute__ ((aligned (16))) = { 32768, 32768, 32768, 32768};
static guint _16_bit_sign[4] __attribute__ ((aligned (16))) = {0x80008000,0x80008000,0x80008000,0x80008000};

static void
render_SSE2(ThreadInfo* t)
{
	RS_IMAGE16 *image = t->tmp;
	RSDcp *dcp = t->dcp;
	gint x, y;
	__m128 h, s, v;
	__m128i p1,p2;
	__m128 p1f, p2f, p3f, p4f;
	__m128 r, g, b, r2, g2, b2;
	__m128i zero = _mm_load_si128((__m128i*)_15_bit_epi32);

	int xfer[4] __attribute__ ((aligned (16)));

	const gfloat exposure_comp = pow(2.0, dcp->exposure);
	__m128 exp = _mm_set_ps(exposure_comp, exposure_comp, exposure_comp, exposure_comp);
	__m128 hue_add = _mm_set_ps(dcp->hue, dcp->hue, dcp->hue, dcp->hue);
	__m128 sat = _mm_set_ps(dcp->saturation, dcp->saturation, dcp->saturation, dcp->saturation);
	
	float cam_prof[4*4*3] __attribute__ ((aligned (16)));
	for (x = 0; x < 4; x++ ) {
		cam_prof[x] = dcp->camera_to_prophoto.coeff[0][0];
		cam_prof[x+4] = dcp->camera_to_prophoto.coeff[0][1];
		cam_prof[x+8] = dcp->camera_to_prophoto.coeff[0][2];
		cam_prof[12+x] = dcp->camera_to_prophoto.coeff[1][0];
		cam_prof[12+x+4] = dcp->camera_to_prophoto.coeff[1][1];
		cam_prof[12+x+8] = dcp->camera_to_prophoto.coeff[1][2];
		cam_prof[24+x] = dcp->camera_to_prophoto.coeff[2][0];
		cam_prof[24+x+4] = dcp->camera_to_prophoto.coeff[2][1];
		cam_prof[24+x+8] = dcp->camera_to_prophoto.coeff[2][2];
	}
	
	gint end_x = image->w - (image->w & 3);

	for(y = t->start_y ; y < t->end_y; y++)
	{
		for(x=0; x < end_x; x+=4)
		{
			__m128i* pixel = (__m128i*)GET_PIXEL(image, x, y);

			zero = _mm_xor_si128(zero,zero);

			/* Convert to float */
			p1 = _mm_load_si128(pixel);
			p2 = _mm_load_si128(pixel + 1);

			/* Unpack to R G B x */
			p2f = _mm_cvtepi32_ps(_mm_unpackhi_epi16(p1, zero));
			p4f = _mm_cvtepi32_ps(_mm_unpackhi_epi16(p2, zero));
			p1f = _mm_cvtepi32_ps(_mm_unpacklo_epi16(p1, zero));
			p3f = _mm_cvtepi32_ps(_mm_unpacklo_epi16(p2, zero));

			/* Normalize to 0 to 1 range */
			__m128 rgb_div = _mm_load_ps(_rgb_div_ps);
			p1f = _mm_mul_ps(p1f, rgb_div);
			p2f = _mm_mul_ps(p2f, rgb_div);
			p3f = _mm_mul_ps(p3f, rgb_div);
			p4f = _mm_mul_ps(p4f, rgb_div);

			/* Restric to camera white */
			__m128 min_cam = _mm_set_ps(0.0f, dcp->camera_white.z, dcp->camera_white.y, dcp->camera_white.x);
			p1f = _mm_min_ps(p1f, min_cam);
			p2f = _mm_min_ps(p2f, min_cam);
			p3f = _mm_min_ps(p3f, min_cam);
			p4f = _mm_min_ps(p4f, min_cam);

			/* Convert to planar */
			__m128 g1g0r1r0 = _mm_unpacklo_ps(p1f, p2f);
			__m128 b1b0 = _mm_unpackhi_ps(p1f, p2f);
			__m128 g3g2r3r2 = _mm_unpacklo_ps(p3f, p4f);
			__m128 b3b2 = _mm_unpackhi_ps(p3f, p4f);
			r = _mm_movelh_ps(g1g0r1r0, g3g2r3r2);
			g = _mm_movehl_ps(g3g2r3r2, g1g0r1r0);
			b = _mm_movelh_ps(b1b0, b3b2);

			/* Convert to Prophoto */
			r2 = sse_matrix3_mul(cam_prof, r, g, b);
			g2 = sse_matrix3_mul(&cam_prof[12], r, g, b);
			b2 = sse_matrix3_mul(&cam_prof[24], r, g, b);

			/* Set min/max before HSV conversion */
			__m128 min_val = _mm_load_ps(_very_small_ps);
			__m128 max_val = _mm_load_ps(_ones_ps);
			r = _mm_max_ps(_mm_min_ps(r2, max_val), min_val);
			g = _mm_max_ps(_mm_min_ps(g2, max_val), min_val);
			b = _mm_max_ps(_mm_min_ps(b2, max_val), min_val);

			RGBtoHSV_SSE(&r, &g, &b);
			h = r; s = g; v = b;

			if (dcp->huesatmap)
			{
				huesat_map_SSE2(dcp->huesatmap, &dcp->huesatmap_precalc, &h, &s, &v);
			}

			/* Saturation */
			s = _mm_max_ps(min_val, _mm_min_ps(max_val, _mm_mul_ps(s, sat)));

			/* Hue */
			__m128 six_ps = _mm_load_ps(_six_ps);
			__m128 zero_ps = _mm_load_ps(_zero_ps);
			h = _mm_add_ps(h, hue_add);

			/* Check if hue > 6 or < 0*/
			__m128 h_mask_gt = _mm_cmpgt_ps(h, six_ps);
			__m128 h_mask_lt = _mm_cmplt_ps(h, zero_ps);
			__m128 six_masked_gt = _mm_and_ps(six_ps, h_mask_gt);
			__m128 six_masked_lt = _mm_and_ps(six_ps, h_mask_lt);
			h = _mm_sub_ps(h, six_masked_gt);
			h = _mm_add_ps(h, six_masked_lt);

			HSVtoRGB_SSE(&h, &s, &v);
			r = h; g = s; b = v;
			
			/* Exposure */
			r = _mm_min_ps(max_val, _mm_mul_ps(r, exp));
			g = _mm_min_ps(max_val, _mm_mul_ps(g, exp));
			b = _mm_min_ps(max_val, _mm_mul_ps(b, exp));

			RGBtoHSV_SSE(&r, &g, &b);
			h = r; s = g; v = b;


			/* Convert v to lookup values */

			/* TODO: Use 8 bit fraction as interpolation, for interpolating
			 * a more precise lookup using linear interpolation. Maybe use less than
			 * 16 bits for lookup for speed, 10 bits with interpolation should be enough */
			__m128 v_mul = _mm_load_ps(_16_bit_ps);
			v = _mm_mul_ps(v, v_mul);
			__m128i lookup = _mm_cvtps_epi32(v);
			gfloat* v_p = (gfloat*)&v;
			_mm_store_si128((__m128i*)&xfer[0], lookup);

			v_p[0] = dcp->curve_samples[xfer[0]];
			v_p[1] = dcp->curve_samples[xfer[1]];
			v_p[2] = dcp->curve_samples[xfer[2]];
			v_p[3] = dcp->curve_samples[xfer[3]];

			/* Apply looktable */
			if (dcp->looktable) {
				huesat_map_SSE2(dcp->looktable, &dcp->looktable_precalc, &h, &s, &v);
			}
			
			/* Ensure that hue is within range */	
			h_mask_gt = _mm_cmpgt_ps(h, six_ps);
			h_mask_lt = _mm_cmplt_ps(h, zero_ps);
			six_masked_gt = _mm_and_ps(six_ps, h_mask_gt);
			six_masked_lt = _mm_and_ps(six_ps, h_mask_lt);
			h = _mm_sub_ps(h, six_masked_gt);
			h = _mm_add_ps(h, six_masked_lt);
			
			/* s always slightly > 0 when converting to RGB */
			s = _mm_max_ps(s, min_val);

			HSVtoRGB_SSE(&h, &s, &v);
			r = h; g = s; b = v;

			/* Apply Tone Curve  in RGB space*/
			if (dcp->tone_curve_lut) 
			{
				rgb_tone_sse2( &r, &g, &b, dcp->tone_curve_lut);
			}

			/* Convert to 16 bit */
			__m128 rgb_mul = _mm_load_ps(_16_bit_ps);
			r = _mm_mul_ps(r, rgb_mul);
			g = _mm_mul_ps(g, rgb_mul);
			b = _mm_mul_ps(b, rgb_mul);
			
			__m128i r_i = _mm_cvtps_epi32(r);
			__m128i g_i = _mm_cvtps_epi32(g);
			__m128i b_i = _mm_cvtps_epi32(b);
			__m128i sub_32 = _mm_load_si128((__m128i*)_15_bit_epi32);
			__m128i signxor = _mm_load_si128((__m128i*)_16_bit_sign);

			/* Subtract 32768 to avoid saturation */
			r_i = _mm_sub_epi32(r_i, sub_32);
			g_i = _mm_sub_epi32(g_i, sub_32);
			b_i = _mm_sub_epi32(b_i, sub_32);

			/* 32 bit signed -> 16 bit signed conversion, all in lower 64 bit */
			r_i = _mm_packs_epi32(r_i, r_i);
			g_i = _mm_packs_epi32(g_i, g_i);
			b_i = _mm_packs_epi32(b_i, b_i);

			/* Interleave*/
			__m128i rg_i = _mm_unpacklo_epi16(r_i, g_i);
			__m128i bb_i = _mm_unpacklo_epi16(b_i, b_i);
			p1 = _mm_unpacklo_epi32(rg_i, bb_i);
			p2 = _mm_unpackhi_epi32(rg_i, bb_i);

			/* Convert sign back */
			p1 = _mm_xor_si128(p1, signxor);
			p2 = _mm_xor_si128(p2, signxor);

			/* Store processed pixel */
			_mm_store_si128(pixel, p1);
			_mm_store_si128(pixel + 1, p2);
		}
	}
}
#endif

static void
render(ThreadInfo* t)
{
	RS_IMAGE16 *image = t->tmp;
	RSDcp *dcp = t->dcp;

	gint x, y;
	gfloat h, s, v;
	gfloat r, g, b;
	RS_VECTOR3 pix;

	const gfloat exposure_comp = pow(2.0, dcp->exposure);

	for(y = t->start_y ; y < t->end_y; y++)
	{
		for(x=t->start_x; x < image->w; x++)
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

			r = CLAMP(pix.R, 0.0, 1.0);
			g = CLAMP(pix.G, 0.0, 1.0);
			b = CLAMP(pix.B, 0.0, 1.0);

			/* To HSV */
			RGBtoHSV(r, g, b, &h, &s, &v);

			if (dcp->huesatmap)
				huesat_map(dcp->huesatmap, &h, &s, &v);

			/* Saturation */
			s *= dcp->saturation;
			s = MIN(s, 1.0);

			/* Hue */
			h += dcp->hue;

			/* Back to RGB */
			HSVtoRGB(h, s, v, &r, &g, &b);
			
			/* Exposure Compensation */
			r = MIN(r * exposure_comp, 1.0);
			g = MIN(g * exposure_comp, 1.0);
			b = MIN(b * exposure_comp, 1.0);
			
			/* To HSV */
			RGBtoHSV(r, g, b, &h, &s, &v);

			/* Curve */
			v = dcp->curve_samples[_S(v)];

			if (dcp->looktable)
				huesat_map(dcp->looktable, &h, &s, &v);

			/* Back to RGB */
			HSVtoRGB(h, s, v, &r, &g, &b);

			/* Apply tone curve */
			if (dcp->tone_curve_lut) 
				rgb_tone(&r, &g, &b, dcp->tone_curve_lut);

			/* Save as gushort */
			pixel[R] = _S(r);
			pixel[G] = _S(g);
			pixel[B] = _S(b);
		}
	}
}

#undef _F
#undef _S

/* dng_color_spec::FindXYZtoCamera */
static RS_MATRIX3
find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix)
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

	dcp->white_xy = *xy;

	color_matrix = find_xyz_to_camera(dcp, xy, &forward_matrix);

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

	/* Camera to ProPhoto */
	matrix3_multiply(&xyz_to_prophoto, &dcp->camera_to_pcs, &dcp->camera_to_prophoto); /* verified by SDK */
#if defined (__SSE2__)
		if (dcp->huesatmap)
			calc_hsm_constants(dcp->huesatmap, &dcp->huesatmap_precalc); 
		if (dcp->looktable)
			calc_hsm_constants(dcp->looktable, &dcp->looktable_precalc); 
#endif
	
}

static void
read_profile(RSDcp *dcp, RSDcpFile *dcp_file)
{
	/* ColorMatrix */
	dcp->has_color_matrix1 = rs_dcp_file_get_color_matrix1(dcp_file, &dcp->color_matrix1);
	dcp->has_color_matrix2 = rs_dcp_file_get_color_matrix2(dcp_file, &dcp->color_matrix2);

	/* CalibrationIlluminant */
	dcp->temp1 = rs_dcp_file_get_illuminant1(dcp_file);
	dcp->temp2 = rs_dcp_file_get_illuminant2(dcp_file);

	/* ProfileToneCurve */
	dcp->tone_curve = rs_dcp_file_get_tonecurve(dcp_file);
	if (dcp->tone_curve)
		dcp->tone_curve_lut = rs_spline_sample(dcp->tone_curve, NULL, 65536);
	/* FIXME: Free these at some point! */

	/* ForwardMatrix */
	dcp->has_forward_matrix1 = rs_dcp_file_get_forward_matrix1(dcp_file, &dcp->forward_matrix1);
	dcp->has_forward_matrix2 = rs_dcp_file_get_forward_matrix2(dcp_file, &dcp->forward_matrix2);
	if (dcp->has_forward_matrix1)
		normalize_forward_matrix(&dcp->forward_matrix1);
	if (dcp->has_forward_matrix2)
		normalize_forward_matrix(&dcp->forward_matrix2);

	dcp->looktable = rs_dcp_file_get_looktable(dcp_file);

	dcp->huesatmap1 = rs_dcp_file_get_huesatmap1(dcp_file);
	dcp->huesatmap2 = rs_dcp_file_get_huesatmap2(dcp_file);
	dcp->huesatmap = dcp->huesatmap2; /* FIXME: Interpolate this! */
}

static RSIccProfile *
get_icc_profile(RSFilter *filter)
{
	/* We discard all earlier profiles before returning our own ProPhoto profile */
	return g_object_ref(RS_DCP_GET_CLASS(filter)->prophoto_profile);
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

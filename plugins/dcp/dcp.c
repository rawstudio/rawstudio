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

#include "config.h"
#include <math.h> /* pow() */
#include "dcp.h"
#include "adobe-camera-raw-tone.h"
#include <string.h> /* memcpy */
#include <stdlib.h>  /* posix_memalign() */

RS_DEFINE_FILTER(rs_dcp, RSDcp)

enum {
	PROP_0,
	PROP_SETTINGS,
	PROP_PROFILE,
	PROP_USE_PROFILE,
	PROP_READ_OUT_CURVE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSDcp *dcp);
static void settings_weak_notify(gpointer data, GObject *where_the_object_was);
static RS_xy_COORD neutral_to_xy(RSDcp *dcp, const RS_VECTOR3 *neutral);
static RS_MATRIX3 find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix);
static void set_white_xy(RSDcp *dcp, const RS_xy_COORD *xy);
static void precalc(RSDcp *dcp);
static void pre_cache_tables(RSDcp *dcp);
static void render(ThreadInfo* t);
static void read_profile(RSDcp *dcp, RSDcpFile *dcp_file);
static void free_dcp_profile(RSDcp *dcp);
static void set_prophoto_wb(RSDcp *dcp, gfloat warmth, gfloat tint);
static void calculate_huesat_maps(RSDcp *dcp, gfloat temp);
static GRecMutex dcp_mutex;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_dcp_get_type(G_TYPE_MODULE(plugin));
}

static void
finalize(GObject *object)
{
	RSDcp *dcp = RS_DCP(object);

	if (dcp->curve_samples)
		free(dcp->curve_samples);
	g_free(dcp->_huesatmap_precalc_unaligned);
	g_free(dcp->_looktable_precalc_unaligned);

	free_dcp_profile(dcp);	
	
	if (dcp->settings_signal_id && dcp->settings)
	{
		g_signal_handler_disconnect(dcp->settings, dcp->settings_signal_id);
		g_object_weak_unref(G_OBJECT(dcp->settings), settings_weak_notify, dcp);
	}
	dcp->settings_signal_id = 0;
	dcp->settings = NULL;
	dcp->read_out_curve = NULL;
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
		PROP_PROFILE, g_param_spec_object(
			"profile", "profile", "DCP Profile",
			RS_TYPE_DCP_FILE, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_USE_PROFILE, g_param_spec_boolean(
			"use-profile", "use-profile", "Use DCP profile",
			FALSE, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_READ_OUT_CURVE, g_param_spec_object(
			"read-out-curve", "read-out-curve", "Read out curve data and send to this widget",
			RS_CURVE_TYPE_WIDGET, G_PARAM_READWRITE)
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
	
	if (mask & MASK_CONTRAST)
	{
		g_object_get(settings, "contrast", &dcp->contrast, NULL);
		changed = TRUE;
	}

	if (mask & MASK_HUE)
	{
		g_object_get(settings, "hue", &dcp->hue, NULL);
		dcp->hue /= 60.0;
		changed = TRUE;
	}

	if (mask & MASK_CHANNELMIXER)
	{
		const gfloat channelmixer_red;
		const gfloat channelmixer_green;
		const gfloat channelmixer_blue;
		g_object_get(settings,
			"channelmixer_red", &channelmixer_red,
			"channelmixer_green", &channelmixer_green,
			"channelmixer_blue", &channelmixer_blue,
			NULL);
		dcp->channelmixer_red = channelmixer_red / 100.0f;
		dcp->channelmixer_green = channelmixer_green / 100.0f;
		dcp->channelmixer_blue = channelmixer_blue / 100.0f;
		changed = TRUE;
	}

	if (mask & MASK_WB)
	{
		dcp->warmth = -1.0;
		dcp->tint = -1.0;
		gfloat premul_warmth = -1.0;
		gfloat pre_mul_tint = -1.0;
		gboolean recalc = FALSE;

		g_object_get(settings,
			"dcp-temp", &dcp->warmth,
			"dcp-tint", &dcp->tint,
			"warmth", &premul_warmth,
			"tint", &pre_mul_tint,
			"recalc-temp", &recalc,
			NULL);

		RS_xy_COORD whitepoint;
		RS_VECTOR3 neutral;
		/* This is messy, but we're essentially converting from warmth/tint to cameraneutral */
		dcp->pre_mul.x = (1.0+premul_warmth)*(2.0-pre_mul_tint);
		dcp->pre_mul.y = 1.0;
		dcp->pre_mul.z = (1.0-premul_warmth)*(2.0-pre_mul_tint);

		if (recalc)
		{
			neutral.x = 1.0 / CLAMP(dcp->pre_mul.x, 0.001, 100.00);
			neutral.y = 1.0 / CLAMP(dcp->pre_mul.y, 0.001, 100.00);
			neutral.z = 1.0 / CLAMP(dcp->pre_mul.z, 0.001, 100.00);
			gfloat max = vector3_max(&neutral);
			neutral.x = neutral.x / max;
			neutral.y = neutral.y / max;
			neutral.z = neutral.z / max;
			whitepoint = neutral_to_xy(dcp, &neutral);

			if (dcp->use_profile)
			{
				rs_color_whitepoint_to_temp(&whitepoint, &dcp->warmth, &dcp->tint);
			} else {
				dcp->warmth = 5000;
				dcp->tint = 0;
			}
			dcp->warmth = CLAMP(dcp->warmth, 2000, 12000);
			dcp->tint = CLAMP(dcp->tint, -150, 150);
			g_object_set(settings,
				"dcp-temp", dcp->warmth,
				"dcp-tint", dcp->tint,
				"recalc-temp", FALSE,
				NULL);
			g_signal_emit_by_name(settings, "wb-recalculated");
		}
		if (dcp->use_profile)
		{
			whitepoint = rs_color_temp_to_whitepoint(dcp->warmth, dcp->tint);
			set_white_xy(dcp, &whitepoint);
			precalc(dcp);
		}
		else
		{
			set_prophoto_wb(dcp, dcp->warmth, dcp->tint);
		}
		changed = TRUE;
	}

	if (mask & MASK_CURVE)
	{
		const gint nknots = rs_settings_get_curve_nknots(settings);
		gint i;

		if (nknots > 1)
		{
			gfloat *knots = rs_settings_get_curve_knots(settings);
			if (knots)
			{
				dcp->nknots = nknots;
				dcp->curve_is_flat = FALSE;
				if (nknots == 2)
					if (ABS(knots[0]) < 0.0001 && ABS(knots[1]) < 0.0001)
						if (ABS(1.0 - knots[2]) < 0.0001 && ABS(1.0 - knots[3]) < 0.0001)
							dcp->curve_is_flat = TRUE;

				if (!dcp->curve_is_flat)
				{
					gfloat sampled[65537];
					RSSpline *spline = rs_spline_new(knots, dcp->nknots, NATURAL);
					rs_spline_sample(spline, sampled, sizeof(sampled) / sizeof(gfloat));
					g_object_unref(spline);
					/* Create extra entry */
					sampled[65536] = sampled[65535];
					for (i = 0; i < 256; i++)
					{
						gfloat value = (gfloat)i * (1.0 / 255.0f);
						/* Gamma correct value */
						value = powf(value, 1.0f / 2.0f);
						
						/* Lookup curve corrected value */
						gfloat lookup = (int)(value * 65535.0f);
						gfloat v0 = sampled[(int)lookup];
						gfloat v1 = sampled[(int)lookup+1];
						lookup -= (gfloat)(gint)lookup;
						value = v0 * (1.0f-lookup) + v1 * lookup;

						/* Convert from gamma 2.0 back to linear */
						value = powf(value, 2.0f);

						/* Store in table */
						if (i>0)
							dcp->curve_samples[i*2-1] = value;
						dcp->curve_samples[i*2] = value;
					}
					dcp->curve_samples[256*2-1] = dcp->curve_samples[256*2] = dcp->curve_samples[256*2+1] = dcp->curve_samples[255*2];
				}
			}
			if (knots)
				g_free(knots);
		}
		else
			dcp->curve_is_flat = TRUE;

		for(i=0;i<257*2;i++)
			dcp->curve_samples[i] = MIN(1.0f, MAX(0.0f, dcp->curve_samples[i]));

		changed = TRUE;
	}

	if (changed)
	{
		rs_filter_changed(RS_FILTER(dcp), RS_FILTER_CHANGED_PIXELDATA);
	}
}

/* This will free all ressources that are related to a DCP profile */
static void 
free_dcp_profile(RSDcp *dcp)
{
	if (dcp->tone_curve)
		g_object_unref(dcp->tone_curve);
	if (dcp->looktable)
		g_object_unref(dcp->looktable);
	if (dcp->huesatmap_interpolated)
		g_object_unref(dcp->huesatmap_interpolated);
	if (dcp->huesatmap1)
		g_object_unref(dcp->huesatmap1);
	if (dcp->huesatmap2)
		g_object_unref(dcp->huesatmap2);
	if (dcp->tone_curve_lut)
		free(dcp->tone_curve_lut);
	dcp->huesatmap1 = NULL;
	dcp->huesatmap2 = NULL;
	dcp->huesatmap_interpolated = NULL;
	dcp->huesatmap = NULL;
	dcp->tone_curve = NULL;
	dcp->looktable = NULL;
	dcp->tone_curve_lut = NULL;
	dcp->use_profile = FALSE;
	if (dcp->huesatmap_precalc->lookups)
	{
		free(dcp->huesatmap_precalc->lookups);
		dcp->huesatmap_precalc->lookups = NULL;
	}
	if (dcp->looktable_precalc->lookups)
	{
		free(dcp->looktable_precalc->lookups);
		dcp->looktable_precalc->lookups = NULL;
	}
	dcp->temp1 = dcp->temp2 = 0;
	dcp->has_color_matrix1 = dcp->has_color_matrix2 = dcp->has_forward_matrix1 = dcp->has_forward_matrix2 = FALSE;
	
}

#define ALIGNTO16(PTR) ((guintptr)PTR + ((16 - ((guintptr)PTR % 16)) % 16))

static void
rs_dcp_init(RSDcp *dcp)
{
	RSDcpClass *klass = RS_DCP_GET_CLASS(dcp);
	g_assert(0 == posix_memalign((void**)&dcp->curve_samples, 16, sizeof(gfloat)*2*257));
	dcp->huesatmap_interpolated = NULL;
	dcp->use_profile = FALSE;
	dcp->curve_is_flat = TRUE;
	dcp->read_out_curve = NULL;
	/* Standard D65, this default should really not be used */
	dcp->white_xy.x = 0.31271f;
	dcp->white_xy.y = 0.32902f;

	/* We cannot initialize this in class init, the RSProphoto plugin may not
	 * be loaded yet at that time :( */
	if (!klass->prophoto)
		klass->prophoto = rs_color_space_new_singleton("RSProphoto");

	/* Allocate aligned precalc tables */
	dcp->_huesatmap_precalc_unaligned = g_malloc(sizeof(PrecalcHSM)+16);
	dcp->_looktable_precalc_unaligned = g_malloc(sizeof(PrecalcHSM)+16);
	dcp->huesatmap_precalc = (PrecalcHSM*)ALIGNTO16(dcp->_huesatmap_precalc_unaligned);
	dcp->looktable_precalc = (PrecalcHSM*)ALIGNTO16(dcp->_looktable_precalc_unaligned);
	memset(dcp->huesatmap_precalc, 0, sizeof(PrecalcHSM));
	memset(dcp->looktable_precalc, 0, sizeof(PrecalcHSM));
}

#undef ALIGNTO16

static void
init_exposure(RSDcp *dcp)
{
	/* Adobe applies negative exposure to the tone curve instead */
	
	/* Todo: Maybe enable shadow (black point) adjustment. */
	gfloat shadow = 5.0;
	gfloat minBlack = shadow * 0.001f;
	gfloat white  = 1.0 / pow (2.0, dcp->exposure);
	dcp->exposure_black = 0;
	
	dcp->exposure_slope = 1.0 / (white - dcp->exposure_black);
	const gfloat kMaxCurveX = 0.5;	
	const gfloat kMaxCurveY = 1.0 / 16.0;
	
	dcp->exposure_radius = MIN (kMaxCurveX * minBlack,
						  kMaxCurveY / dcp->exposure_slope);
	
	if (dcp->exposure_radius > 0.0)
		dcp->exposure_qscale = dcp->exposure_slope / (4.0 * dcp->exposure_radius);
	else
		dcp->exposure_qscale = 0.0;
}


static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSDcp *dcp = RS_DCP(object);

	switch (property_id)
	{
		case PROP_SETTINGS:
			break;
		case PROP_USE_PROFILE:
			g_value_set_boolean(value, dcp->use_profile);
			break;
		case PROP_READ_OUT_CURVE:
			g_value_set_object(value, dcp->read_out_curve);
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
	gpointer temp;
	gboolean changed = FALSE;

	switch (property_id)
	{
		case PROP_SETTINGS:
			if (dcp->settings && dcp->settings_signal_id)
			{
				if (dcp->settings == g_value_get_object(value))
				{
					settings_changed(dcp->settings, MASK_ALL, dcp);
					break;
				}
				g_signal_handler_disconnect(dcp->settings, dcp->settings_signal_id);
				g_object_weak_unref(G_OBJECT(dcp->settings), settings_weak_notify, dcp);
			}
			dcp->settings = g_value_get_object(value);
			dcp->settings_signal_id = g_signal_connect(dcp->settings, "settings-changed", G_CALLBACK(settings_changed), dcp);
			settings_changed(dcp->settings, MASK_ALL, dcp);
			g_object_weak_ref(G_OBJECT(dcp->settings), settings_weak_notify, dcp);
			break;
		case PROP_PROFILE:
			g_rec_mutex_lock(&dcp_mutex);
			read_profile(dcp, g_value_get_object(value));
			changed = TRUE;
			g_rec_mutex_unlock(&dcp_mutex);
			break;
		case PROP_READ_OUT_CURVE:
			temp = g_value_get_object(value);
			if (temp != dcp->read_out_curve)
				changed = TRUE;
			dcp->read_out_curve = temp;
			break;
		case PROP_USE_PROFILE:
			g_rec_mutex_lock(&dcp_mutex);
			dcp->use_profile = g_value_get_boolean(value);
			if (!dcp->use_profile)
				free_dcp_profile(dcp);
			else
				precalc(dcp);
			g_rec_mutex_unlock(&dcp_mutex);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	if (changed)
		rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
}

static void
settings_weak_notify(gpointer data, GObject *where_the_object_was)
{
	RSDcp *dcp = RS_DCP(data);
	dcp->settings = NULL;
}


gpointer
start_single_dcp_thread(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	RS_IMAGE16 *tmp = t->tmp;

	pre_cache_tables(t->dcp);
	if (tmp->pixelsize == 4  && (rs_detect_cpu_features() & RS_CPU_FLAG_SSE2) && !t->dcp->read_out_curve)
	{
		if ((rs_detect_cpu_features() & RS_CPU_FLAG_AVX) && render_AVX(t))
		{
			/* AVX routine renders 4 pixels in parallel, but any remaining must be */
			/* calculated using C routines */
			if (tmp->w & 3)
			{
				t->start_x = tmp->w - (tmp->w & 3);
				render(t);
			}
		} 
		else if ((rs_detect_cpu_features() & RS_CPU_FLAG_SSE4_1) && render_SSE4(t))
		{
			/* SSE4 routine renders 4 pixels in parallel, but any remaining must be */
			/* calculated using C routines */
			if (tmp->w & 3)
			{
				t->start_x = tmp->w - (tmp->w & 3);
				render(t);
			}
		} 
		else if (render_SSE2(t))
		{
			/* SSE2 routine renders 4 pixels in parallel, but any remaining must be */
			/* calculated using C routines */
			if (tmp->w & 3)
			{
				t->start_x = tmp->w - (tmp->w & 3);
				render(t);
			}
		} else {
			/* Not SSE2 compiled, render using plain C */
			render(t);
		}
	}
	else
		render(t);

	if (!t->single_thread)
		g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static inline void 
bit_blt(char* dstp, int dst_pitch, const char* srcp, int src_pitch, int row_size, int height) 
{
	if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)) 
	{
		memcpy(dstp, srcp, row_size*height);
		return;
	}

	int y;
	for (y = height; y > 0; --y)
	{
		memcpy(dstp, srcp, row_size);
		dstp += dst_pitch;
		srcp += src_pitch;
	}
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSDcp *dcp = RS_DCP(filter);
	RSDcpClass *klass = RS_DCP_GET_CLASS(dcp);
	GdkRectangle *roi;
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	RS_IMAGE16 *tmp;

	gint j;

	RSFilterRequest *request_clone = rs_filter_request_clone(request);

	if (!dcp->use_profile)
	{
		gfloat premul[4] = {dcp->pre_mul.x, dcp->pre_mul.y, dcp->pre_mul.z, 1.0};
		rs_filter_param_set_float4(RS_FILTER_PARAM(request_clone), "premul", premul);
	}

	rs_filter_param_set_object(RS_FILTER_PARAM(request_clone), "colorspace", klass->prophoto);
	previous_response = rs_filter_get_image(filter->previous, request_clone);
	g_object_unref(request_clone);

	if (!RS_IS_FILTER(filter->previous))
		return previous_response;

	input = rs_filter_response_get_image(previous_response);
	if (!input) return previous_response;
	response = rs_filter_response_clone(previous_response);

	/* We always deliver in ProPhoto */
	rs_filter_param_set_object(RS_FILTER_PARAM(response), "colorspace", klass->prophoto);
	g_object_unref(previous_response);

	if ((roi = rs_filter_request_get_roi(request)))
	{
		/* Align so we start at even pixel counts */
		roi->width += (roi->x&1);
		roi->x -= (roi->x&1);
		roi->width = MIN(input->w - roi->x, roi->width);
		output = rs_image16_copy(input, FALSE);
		tmp = rs_image16_new_subframe(output, roi);
		bit_blt((char*)GET_PIXEL(tmp,0,0), tmp->rowstride * 2, 
			(const char*)GET_PIXEL(input,roi->x,roi->y), input->rowstride * 2, tmp->w * tmp->pixelsize * 2, tmp->h);
	}
	else
	{
		output = rs_image16_copy(input, TRUE);
		tmp = g_object_ref(output);
	}
	g_object_unref(input);
	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	g_rec_mutex_lock(&dcp_mutex);
	init_exposure(dcp);

	guint i, y_offset, y_per_thread, threaded_h;
	guint threads = rs_get_number_of_processor_cores();
	if (tmp->h * tmp->w < 200*200)
		threads = 1;

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
		for(j = 0; j < 256; j++)
			t[i].curve_input_values[j] = 0;
		t[i].single_thread = (threads == 1);
		if (threads == 1)
			start_single_dcp_thread(&t[0]);
		else	
			t[i].threadid = g_thread_new("RSDcp worker", start_single_dcp_thread, &t[i]);
	}

	/* Wait for threads to finish */
	for(i = 0; threads > 1 && i < threads; i++)
		g_thread_join(t[i].threadid);

	/* Settings can change now */
	g_rec_mutex_unlock(&dcp_mutex);

	/* If we must deliver histogram data, do it now */
	if (dcp->read_out_curve)
	{
		gint *values = g_malloc0(256*sizeof(gint));
		for(i = 0; i < threads; i++)
			for(j = 0; j < 256; j++)
				values[j] += t[i].curve_input_values[j];
		rs_curve_set_histogram_data(RS_CURVE_WIDGET(dcp->read_out_curve), values);
		g_free(values);
	}
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

static inline void
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

static inline gfloat
exposure_ramp (RSDcp *dcp, gfloat x)
{
	if (x <= dcp->exposure_black - dcp->exposure_radius)
		return 0.0;
		
	if (x >= dcp->exposure_black + dcp->exposure_radius)
		return (x - dcp->exposure_black) * dcp->exposure_slope;
		
	gfloat y = x - (dcp->exposure_black - dcp->exposure_radius);
	
	return dcp->exposure_qscale * y * y;
}


static inline void
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
	g_return_if_fail(RS_IS_HUESAT_MAP(map));

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
		*v = MIN(*v * valScale, 1.0);
	}
	else
	{
		if (map->v_encoding == 1)
			*v = powf(*v , 1.0f/2.2f);

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

		valScale = sFract0 * valScale0 + sFract1 * valScale1;
		hueShift = sFract0 * hueShift0 + sFract1 * hueShift1;
		satScale = sFract0 * satScale0 + sFract1 * satScale1;

		if (map->v_encoding == 1)
			*v = powf(MIN(*v * valScale, 1.0), 2.2f);
		else
			*v = MIN(*v * valScale, 1.0);
	}

	hueShift *= (6.0f / 360.0f);

	*h += hueShift;
	*s = MIN(*s * satScale, 1.0);
}

static inline gfloat 
lookup_tone(gfloat value, const gfloat * const tone_lut)
{
	gfloat lookup = CLAMP(value * 1024.0f, 0.0f, 1023.9999f);
	gfloat v0 = tone_lut[(gint)lookup*2];
	gfloat v1 = tone_lut[(gint)lookup*2 + 1];
	lookup -= floorf(lookup);
	return v0 * (1.0f - lookup) + v1 * lookup;	
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

	#define RGBTone(lg, md, sm, LG, MD, SM)\
	{\
		LG = lookup_tone(lg, tone_lut);\
		SM = lookup_tone(sm, tone_lut);\
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
			rr = lookup_tone(r, tone_lut);
			gg = lookup_tone(b, tone_lut);
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

static void 
pre_cache_tables(RSDcp *dcp)
{
	int i;
	gfloat unused = 0;
	const int cache_line_bytes = 64;

	/* Preloads cache with lookup data */
	if (!dcp->curve_is_flat)
	{
		for (i = 0; i < 514; i+=(cache_line_bytes/sizeof(gfloat)))
			unused = dcp->curve_samples[i];
	}

	if (dcp->tone_curve_lut) 
	{
		for (i = 0; i < 2050; i+=(cache_line_bytes/sizeof(gfloat)))
			unused = dcp->tone_curve_lut[i];
	}

	if (dcp->huesatmap_precalc->lookups || dcp->looktable_precalc->lookups)
	{
		if (dcp->huesatmap_precalc->lookups)
		{
			int num = dcp->huesatmap_precalc->valStep[0] * dcp->huesatmap->val_divisions * sizeof(gfloat);
			gfloat *data = dcp->huesatmap_precalc->lookups;
			for (i = 0; i < num; i+=(cache_line_bytes/sizeof(gfloat)))
				unused = data[i];
		}

		if (dcp->looktable_precalc->lookups)
		{
			int num = dcp->looktable_precalc->valStep[0] * dcp->looktable->val_divisions * sizeof(gfloat);
			gfloat *data = dcp->looktable_precalc->lookups;
			for (i = 0; i < num; i+=(cache_line_bytes/sizeof(gfloat)))
				unused = data[i];
		}
	}
	else
	{
		if (dcp->huesatmap)
		{
			int num = dcp->huesatmap->hue_divisions * dcp->huesatmap->sat_divisions * dcp->huesatmap->val_divisions;
			num = num * sizeof(RS_VECTOR3) / sizeof(gfloat);
			gfloat *data = (gfloat*)dcp->huesatmap->deltas;
			for (i = 0; i < num; i+=(cache_line_bytes/sizeof(gfloat)))
				unused = data[i];
		}

		if (dcp->looktable)
		{
			int num = dcp->looktable->hue_divisions * dcp->looktable->sat_divisions * dcp->looktable->val_divisions;
			num = num * sizeof(RS_VECTOR3) / sizeof(gfloat);
			gfloat *data = (gfloat*)dcp->looktable->deltas;
			for (i = 0; i < num; i+=(cache_line_bytes/sizeof(gfloat)))
				unused = data[i];
		}
	}

	/* This is needed so the optimizer doesn't believe the value is unused */
	dcp->junk_value = unused;
}

static void
render(ThreadInfo* t)
{
	RS_IMAGE16 *image = t->tmp;
	RSDcp *dcp = t->dcp;

	gint x, y;
	gfloat h, s, v;
	gfloat r, g, b;
	RS_VECTOR3 pix;
	gboolean do_contrast = (dcp->contrast > 1.001f);
	gboolean do_highrec = (dcp->contrast < 0.999f);
	float contr_base = 0.5;
	float exposure_simple = MAX(1.0, powf(2.0f, dcp->exposure));
	float recover_radius = 0.5 * exposure_simple;
	float inv_recover_radius = 1.0f / recover_radius;
	recover_radius = 1.0 - recover_radius;

	RS_VECTOR3 clip;

	clip.R = dcp->camera_white.R;
	clip.G = dcp->camera_white.G;
	clip.B = dcp->camera_white.B;

	for(y = t->start_y ; y < t->end_y; y++)
	{
		for(x=t->start_x; x < image->w; x++)
		{
			gushort *pixel = GET_PIXEL(image, x, y);

			/* Convert to float */
			r = _F(pixel[R]);
			g = _F(pixel[G]);
			b = _F(pixel[B]);

			if (dcp->use_profile)
			{
				r = MIN(clip.R, r);
				g = MIN(clip.G, g);
				b = MIN(clip.B, b);
			}

			pix.R = r;
			pix.G = g;
			pix.B = b;
			pix = vector3_multiply_matrix(&pix, &dcp->camera_to_prophoto);
				
			r = pix.R;
			g = pix.G;
			b = pix.B;

			r = CLAMP(r * dcp->channelmixer_red, 0.0, 1.0);
			g = CLAMP(g * dcp->channelmixer_green, 0.0, 1.0);
			b = CLAMP(b * dcp->channelmixer_blue, 0.0, 1.0);

			/* To HSV */
			RGBtoHSV(r, g, b, &h, &s, &v);

			if (dcp->huesatmap)
				huesat_map(dcp->huesatmap, &h, &s, &v);

			/* Saturation */
			if (dcp->saturation > 1.0)
			{
				/* Apply curved saturation, when we add saturation */
				float sat_val = dcp->saturation - 1.0f;
				
				s = (sat_val * (s * 2.0f - (s * s))) + ((1.0f - sat_val) * s);
				s = MIN(s, 1.0);
			}
			else
			{
				s *= dcp->saturation;
				s = MIN(s, 1.0);
			}

			/* Hue */
			h += dcp->hue;

			/* Back to RGB */
			HSVtoRGB(h, s, v, &r, &g, &b);
			
			/* Exposure Compensation */
			r = exposure_ramp(dcp, r);
			g = exposure_ramp(dcp, g);
			b = exposure_ramp(dcp, b);
			
			/* Contrast in gamma 2.0 */
			if (do_contrast)
			{
				r = MAX((sqrtf(r) - contr_base) * dcp->contrast + contr_base, 0.0f);
				r *= r;
				g = MAX((sqrtf(g) - contr_base) * dcp->contrast + contr_base, 0.0f);
				g *= g;
				b = MAX((sqrtf(b) - contr_base) * dcp->contrast + contr_base, 0.0f);
				b *= b;
			}
			else if (do_highrec)
			{
				/* Distance from 1.0 - radius */
				float dist = v - recover_radius;
				/* Scale so distance is normalized, clamp */
				float dist_scaled = MIN(1.0, dist *  inv_recover_radius);

				float mul_val = 1.0 - dist_scaled * (1.0 - dcp->contrast);
				r = r * mul_val;
				g = g * mul_val;
				b = b * mul_val;
			}
			/* To HSV */
			r = MIN(r, 1.0f);
			g = MIN(g, 1.0f);
			b = MIN(b, 1.0f);
			
			RGBtoHSV(r, g, b, &h, &s, &v);

			/* Curve */
			if (dcp->read_out_curve)
			{
				gfloat t1 = v,t2,t3;
				if (dcp->tone_curve_lut) 
				{
					t2 = t3 = v;
					rgb_tone(&t1, &t2, &t3, dcp->tone_curve_lut);
				}
				int input = (int)(CLAMP(sqrtf(t1) * 256.0f, 0.0f, 255.9999f));
				t->curve_input_values[input]++;
			}
			if (!dcp->curve_is_flat)
			{
				gfloat lookup = CLAMP(v * 256.0f, 0.0f, 255.9999f);
				gfloat v0 = dcp->curve_samples[(gint)lookup*2];
				gfloat v1 = dcp->curve_samples[(gint)lookup*2 + 1];
				lookup -= floorf(lookup);
				v = v0 * (1.0f - lookup) + v1 * lookup;
			}

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

const static RS_MATRIX3 xyz_to_prophoto = {{
	{  1.3459433, -0.2556075, -0.0511118 },
	{ -0.5445989,  1.5081673,  0.0205351 },
	{  0.0000000,  0.0000000,  1.2118128 }
}};
const static RS_MATRIX3 prophoto_to_xyz = {{
	{ 0.7976749,  0.1351917,  0.0313534},
	{ 0.2880402,  0.7118741,  0.0000857},
	{ 0.0000000,  0.0000000,  0.8252100}
}};

/* dng_color_spec::FindXYZtoCamera */
static RS_MATRIX3
find_xyz_to_camera(RSDcp *dcp, const RS_xy_COORD *white_xy, RS_MATRIX3 *forward_matrix)
{
	if (!dcp->use_profile)
	{
		return xyz_to_prophoto;
	}
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

	/* Interpolate if more than one color matrix */
	RS_MATRIX3 color_matrix;
	if(dcp->has_color_matrix1 && dcp->has_color_matrix2) 
		matrix3_interpolate(&dcp->color_matrix1, &dcp->color_matrix2, alpha, &color_matrix);
	else if (dcp->has_color_matrix1)
		color_matrix = dcp->color_matrix1;
	else if (dcp->has_color_matrix2)
		color_matrix = dcp->color_matrix2;

	if (forward_matrix)
	{
		if (dcp->has_forward_matrix1 && dcp->has_forward_matrix2)
			matrix3_interpolate(&dcp->forward_matrix1, &dcp->forward_matrix2, 1.0-alpha, forward_matrix);
		else if (dcp->has_forward_matrix1)
			*forward_matrix = dcp->forward_matrix1;
		else if (dcp->has_forward_matrix2)
			*forward_matrix = dcp->forward_matrix2;
	}
	calculate_huesat_maps(dcp, temp);
	return color_matrix;
}

static void
calculate_huesat_maps(RSDcp *dcp, gfloat temp)
{
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

	dcp->huesatmap = 0;
	if (dcp->huesatmap1 != NULL &&  dcp->huesatmap2 != NULL) 
	{
		gint hd = dcp->huesatmap1->hue_divisions;
		gint sd = dcp->huesatmap1->sat_divisions;
		gint vd = dcp->huesatmap1->val_divisions;

		if (hd == dcp->huesatmap2->hue_divisions && sd == dcp->huesatmap2->sat_divisions && vd == dcp->huesatmap2->val_divisions)
		{
			if (temp > dcp->temp1 && temp < dcp->temp2)
			{
				if (dcp->huesatmap_interpolated)
					g_object_unref(dcp->huesatmap_interpolated);

				dcp->huesatmap_interpolated = rs_huesat_map_new(hd, sd, vd);
				float t1_weight = alpha;
				float t2_weight = 1.0f - alpha;

				int vals = hd * sd * vd;
				RS_VECTOR3 *t_out = dcp->huesatmap_interpolated->deltas;
				RS_VECTOR3 *t1 = dcp->huesatmap1->deltas;
				RS_VECTOR3 *t2 = dcp->huesatmap2->deltas;
				gint i;
				for (i = 0; i < vals; i++)
				{
					t_out[i].x = t1[i].x * t1_weight + t2[i].x * t2_weight;
					t_out[i].y = t1[i].y * t1_weight + t2[i].y * t2_weight;
					t_out[i].z = t1[i].z * t1_weight + t2[i].z * t2_weight;
				}
//				printf("T1:%f, T2:%f, Cam:%f, t1w:%f, t2w:%f, vals:%d\n",dcp->temp1, dcp->temp2, temp, t1_weight, t2_weight, vals );
			} 
			else if (temp <= dcp->temp1)
				dcp->huesatmap = dcp->huesatmap1;
			else
				dcp->huesatmap = dcp->huesatmap2;
		}
	}
	/* If we don't have two huesatmaps, it will still be 0. */
	/* If that is the case, set it to the one that is present */
	if (dcp->huesatmap == 0) 
	{
		if (dcp->huesatmap1 != 0)
			dcp->huesatmap = dcp->huesatmap1;
		else
			dcp->huesatmap = dcp->huesatmap2;
	}
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

static void
set_prophoto_wb(RSDcp *dcp, gfloat warmth, gfloat tint) 
{
	RS_xy_COORD prophoto_white;
	prophoto_white.x = 0.3457;
	prophoto_white.y = 0.3585;
	RS_xy_COORD dest_xy = rs_color_temp_to_whitepoint(warmth, tint);
	RS_MATRIX3 map = rs_calculate_map_white_matrix(&dest_xy, &prophoto_white);

	RS_MATRIX3 pcs_to_camera;
	matrix3_multiply(&prophoto_to_xyz, &map, &pcs_to_camera);
	matrix3_multiply(&pcs_to_camera, &xyz_to_prophoto, &dcp->camera_to_prophoto);
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
	g_rec_mutex_lock(&dcp_mutex);
	if (dcp->use_profile)
		matrix3_multiply(&xyz_to_prophoto, &dcp->camera_to_pcs, &dcp->camera_to_prophoto); /* verified by SDK */
	if (dcp->huesatmap && (rs_detect_cpu_features() & RS_CPU_FLAG_SSE2))
		calc_hsm_constants(dcp->huesatmap, dcp->huesatmap_precalc); 
	if (dcp->looktable && (rs_detect_cpu_features() & RS_CPU_FLAG_SSE2))
		calc_hsm_constants(dcp->looktable, dcp->looktable_precalc); 
	g_rec_mutex_unlock(&dcp_mutex);
}

static void
read_profile(RSDcp *dcp, RSDcpFile *dcp_file)
{
	gint i;
	free_dcp_profile(dcp);
	
	/* ColorMatrix */
	dcp->has_color_matrix1 = rs_dcp_file_get_color_matrix1(dcp_file, &dcp->color_matrix1);
	dcp->has_color_matrix2 = rs_dcp_file_get_color_matrix2(dcp_file, &dcp->color_matrix2);

	/* CalibrationIlluminant */
	dcp->temp1 = rs_dcp_file_get_illuminant1(dcp_file);
	dcp->temp2 = rs_dcp_file_get_illuminant2(dcp_file);
	/* FIXME: If temp1 > temp2, swap them and data*/	

	/* ProfileToneCurve */
	dcp->tone_curve = rs_dcp_file_get_tonecurve(dcp_file);
	if (!dcp->tone_curve)
	{
		gint num_knots = adobe_default_table_size;
		gfloat *knots = g_new0(gfloat, adobe_default_table_size * 2);

		for(i = 0; i < adobe_default_table_size; i++)
		{
			knots[i*2] = (gfloat)i / (gfloat)adobe_default_table_size;
			knots[i*2+1] = adobe_default_table[i];
		}
		dcp->tone_curve = rs_spline_new(knots, num_knots, NATURAL);
		g_free(knots);
	}
	g_assert(0 == posix_memalign((void**)&dcp->tone_curve_lut, 16, sizeof(gfloat)*2*1025));
	gfloat *tc = rs_spline_sample(dcp->tone_curve, NULL, 1024);
	for (i=0; i< 1024; i++)
	{
		if (i>0)
			dcp->tone_curve_lut[i*2-1] = tc[i];
		dcp->tone_curve_lut[i*2] = tc[i];
	}
	dcp->tone_curve_lut[1024*2-1] = dcp->tone_curve_lut[1024*2] = dcp->tone_curve_lut[1024*2+1] = tc[1023];
	g_free(tc);

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
	dcp->huesatmap = 0;
	dcp->use_profile = TRUE;
	set_white_xy(dcp, &dcp->white_xy);
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

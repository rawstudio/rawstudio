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
#include <gettext.h>
#include <math.h> /* pow() */
#include "denoiseinterface.h"

#define RS_TYPE_DENOISE (rs_denoise_type)
#define RS_DENOISE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DENOISE, RSDenoise))
#define RS_DENOISE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DENOISE, RSDenoiseClass))
#define RS_IS_DENOISE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DENOISE))

typedef struct _RSDenoise RSDenoise;
typedef struct _RSDenoiseClass RSDenoiseClass;

struct _RSDenoise {
	RSFilter parent;

	FFTDenoiseInfo info;
	gint sharpen;
	gint denoise_luma;
	gint denoise_chroma;
	gfloat warmth, tint;
	gfloat exposure;
};

struct _RSDenoiseClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_denoise, RSDenoise)

enum {
	PROP_0,
	PROP_SHARPEN,
	PROP_DENOISE_LUMA,
	PROP_DENOISE_CHROMA,
	PROP_SETTINGS
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter, RS_FILTER_PARAM *param);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSDenoise *denoise);

static RSFilterClass *rs_denoise_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_denoise_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_denoise_class_init(RSDenoiseClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_denoise_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_SHARPEN, g_param_spec_int (
			"sharpen",
			_("Sharpen Amount"),
			_("How much image will be sharpened"),
			0, 100, 0,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_DENOISE_LUMA, g_param_spec_int (
			"denoise_luma",
			_("Denoise"),
			"FIXME",
			0, 100, 0,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_DENOISE_CHROMA, g_param_spec_int (
			"denoise_chroma",
			_("Color denoise"),
			"FIXME",
			0, 100, 0,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_SETTINGS, g_param_spec_object(
			"settings", "Settings", "Settings to render from",
			RS_TYPE_SETTINGS, G_PARAM_READWRITE)
	);

	filter_class->name = "FFT denoise filter";
	filter_class->get_image = get_image;
}

static void
settings_changed(RSSettings *settings, RSSettingsMask mask, RSDenoise *denoise)
{
	gboolean changed = FALSE;

	if (mask & MASK_WB)
	{
		const gfloat warmth;
		const gfloat tint;

		g_object_get(settings, 
			"warmth", &warmth,
			"tint", &tint, 
			NULL );
		if (ABS(warmth-denoise->warmth) > 0.01 || ABS(tint-denoise->tint) > 0.01) {
			changed = TRUE;
			denoise->warmth = warmth;
			denoise->tint = tint;
		}
	}

	if (mask & MASK_EXPOSURE)
	{
		const gfloat exposure;

		g_object_get(settings, 
			"exposure", &exposure,
			NULL );
		if (ABS(exposure-denoise->exposure) > 0.01) {
			changed = TRUE;
			denoise->exposure = exposure;
		}
	}

	if (changed)
		rs_filter_changed(RS_FILTER(denoise), RS_FILTER_CHANGED_PIXELDATA);
}

static void
rs_denoise_init(RSDenoise *denoise)
{
	denoise->info.processMode = PROCESS_YUV;
	initDenoiser(&denoise->info);
	denoise->sharpen = 0;
	denoise->denoise_luma = 0;
	denoise->denoise_chroma = 0;
	denoise->warmth = 0.23f;        // Default values
	denoise->tint = 0.07f;
	denoise->exposure = 0.0f;       // Exposure compensation
	/* FIXME: Remember to destroy */
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSDenoise *denoise = RS_DENOISE(object);

	switch (property_id)
	{
		case PROP_SHARPEN:
			g_value_set_int(value, denoise->sharpen);
			break;
		case PROP_DENOISE_LUMA:
			g_value_set_int(value, denoise->denoise_luma);
			break;
		case PROP_DENOISE_CHROMA:
			g_value_set_int(value, denoise->denoise_chroma);
			break;
		case PROP_SETTINGS:
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSDenoise *denoise = RS_DENOISE(object);
	RSFilter *filter = RS_FILTER(denoise);
	RSSettings *settings;

	switch (property_id)
	{
		case PROP_SHARPEN:
			if ((denoise->sharpen-g_value_get_int(value)) != 0)
			{
				denoise->sharpen = g_value_get_int(value);
				rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
			}
			break;
		case PROP_DENOISE_LUMA:
			if ((denoise->denoise_luma-g_value_get_int(value)) != 0)
			{
				denoise->denoise_luma = g_value_get_int(value);
				rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
			}
			break;
		case PROP_DENOISE_CHROMA:
			if ((denoise->denoise_chroma-g_value_get_int(value)) != 0)
			{
				denoise->denoise_chroma = g_value_get_int(value);
				rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
			}
			break;
		case PROP_SETTINGS:
			settings = g_value_get_object(value);
			g_signal_connect(settings, "settings-changed", G_CALLBACK(settings_changed), denoise);
			settings_changed(settings, MASK_ALL, denoise);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter, RS_FILTER_PARAM *param)
{
	RSDenoise *denoise = RS_DENOISE(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	RS_IMAGE16 *tmp;
	input = rs_filter_get_image(filter->previous, param);
	if (!RS_IS_FILTER(filter->previous))
		return input;

	if ((denoise->sharpen + denoise->denoise_luma + denoise->denoise_chroma) == 0)
		return input;

	output = rs_image16_copy(input, TRUE);
	g_object_unref(input);

	if (param && param->roi)
		tmp = rs_image16_new_subframe(output, param->roi);
	else
		tmp = g_object_ref(output);

	gfloat exp_comp = MIN (1.0, 1.0 / pow(2.0, denoise->exposure));

	denoise->info.image = tmp;
	denoise->info.sigmaLuma = exp_comp * ((float) denoise->denoise_luma) / 2.5;
	denoise->info.sigmaChroma = exp_comp * ((float) denoise->denoise_chroma) / 2.5;
	denoise->info.sharpenLuma = exp_comp * ((float) denoise->sharpen) / 20.0;
	denoise->info.sharpenCutoffLuma = 0.3f;
	denoise->info.beta = 1.0;
	denoise->info.sharpenChroma = 0.0f;
	denoise->info.sharpenMinSigmaLuma = denoise->info.sigmaLuma + exp_comp * 2.0;

	denoise->info.redCorrection = (1.0+denoise->warmth)*(2.0-denoise->tint);
	denoise->info.blueCorrection = (1.0-denoise->warmth)*(2.0-denoise->tint);

	denoiseImage(&denoise->info);
	g_object_unref(tmp);

	return output;
}

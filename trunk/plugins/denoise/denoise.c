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
};

struct _RSDenoiseClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_denoise, RSDenoise)

enum {
	PROP_0,
	PROP_SHARPEN,
	PROP_DENOISE_LUMA,
	PROP_DENOISE_CHROMA
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);

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

	filter_class->name = "FFT denoise filter";
	filter_class->get_image = get_image;
}

static void
rs_denoise_init(RSDenoise *denoise)
{
	denoise->info.processMode = PROCESS_YUV;
	initDenoiser(&denoise->info);
	denoise->sharpen = 0;
	denoise->denoise_luma = 0;
	denoise->denoise_chroma = 0;
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
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSDenoise *denoise = RS_DENOISE(object);
	RSFilter *filter = RS_FILTER(denoise);

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
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSDenoise *denoise = RS_DENOISE(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output;
	input = rs_filter_get_image(filter->previous);
//	if (!RS_IS_FILTER(input))
//		return input;

	if ((denoise->sharpen + denoise->denoise_luma + denoise->denoise_chroma) == 0)
		return input;

	output = rs_image16_copy(input, TRUE);
	g_object_unref(input);

	denoise->info.image = output;
	denoise->info.sigmaLuma = ((float) denoise->denoise_luma) / 5.0;
	denoise->info.sigmaChroma = ((float) denoise->denoise_chroma) / 5.0;
	denoise->info.sharpenLuma = ((float) denoise->sharpen) / 40.0;
	denoise->info.beta = 1.0;

	denoise->info.sharpenMinSigmaLuma = denoise->info.sigmaLuma + 2.0;

	GTimer *gt = g_timer_new();
	denoiseImage(&denoise->info);
	gfloat time = g_timer_elapsed(gt, NULL);
	gfloat mpps = (output->w*output->h) / (time*1000000.0);
	printf("Denoising took:%.03fsec, %.03fMpix/sec\n", time, mpps );
	g_timer_destroy(gt);

	return output;
}

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

#include "rs-settings.h"
#include "rs-utils.h"
#include <config.h>
#include "gettext.h"
#include <string.h> /* memcmp() */

G_DEFINE_TYPE (RSSettings, rs_settings, G_TYPE_OBJECT)

enum {
	SETTINGS_CHANGED,
	WB_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void rs_settings_update_settings(RSSettings *settings, const RSSettingsMask changed_mask);

static void
rs_settings_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (rs_settings_parent_class)->finalize)
		G_OBJECT_CLASS (rs_settings_parent_class)->finalize (object);
}

enum {
	PROP_0,
	PROP_EXPOSURE,
	PROP_SATURATION,
	PROP_HUE,
	PROP_CONTRAST,
	PROP_WARMTH,
	PROP_TINT,
	PROP_DCP_TEMP,
	PROP_DCP_TINT,
	PROP_WB_ASCII,
	PROP_SHARPEN,
	PROP_DENOISE_LUMA,
	PROP_DENOISE_CHROMA,
	PROP_TCA_KR,
	PROP_TCA_KB,
	PROP_VIGNETTING,
	PROP_CHANNELMIXER_RED,
	PROP_CHANNELMIXER_GREEN,
	PROP_CHANNELMIXER_BLUE,
	PROP_RECALC_TEMP
};

static void
rs_settings_class_init (RSSettingsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = rs_settings_finalize;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_EXPOSURE, g_param_spec_float(
	/* @TRANSLATORS: "Expos" is short version of "Exposure". You cannot use more than 5 characters for this! */
			"exposure", _("Expos"), _("Exposure Compensation"),
			-3.0, 3.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_SATURATION, g_param_spec_float(
			/* @TRANSLATORS: "Satur" is short version of "Saturation". You cannot use more than 5 characters for this! */
			"saturation", _("Satur"), _("Saturation"),
			0.0, 2.0, 1.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_HUE, g_param_spec_float(
			/* @TRANSLATORS: You cannot use more than 5 characters for "Hue" */
			"hue", _("Hue"), _("Hue Shift"),
			-180.0, 180.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_CONTRAST, g_param_spec_float(
			/* @TRANSLATORS: "Contr" is short version of "Contrast". You cannot use more than 5 characters for this! */
			"contrast", _("Contr"), _("Contrast"),
			0.5, 2.5, 1.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_WARMTH, g_param_spec_float(
			/* @TRANSLATORS: "Temp" is short version of "Temperature". You cannot use more than 5 characters for this! */
			"warmth", _("Temp"), _("Temperature"),
			-1.0, 1.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_TINT, g_param_spec_float(
			/* @TRANSLATORS: You cannot use more than 5 characters for "Tint" */
			"tint", _("Tint"), _("Tint Shift"),
			-2.0, 2.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_DCP_TEMP, g_param_spec_float(
			/* @TRANSLATORS: "Temp" is short version of "Temperature". You cannot use more than 5 characters for this! */
			"dcp-temp", _("Temp"), _("Temperature"),
			2000.0, 12000.0, 5000.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_DCP_TINT, g_param_spec_float(
			/* @TRANSLATORS: You cannot use more than 5 characters for "Tint" */
			"dcp-tint", _("Tint"), _("Tint Shift"),
			-150.0, 150.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_WB_ASCII, g_param_spec_string(
			"wb_ascii", _("WBAscii"), _("WBAscii"),
			NULL, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_SHARPEN, g_param_spec_float(
			/* @TRANSLATORS: "Sharp" is short version of "Sharpen". You cannot use more than 5 characters for this! */
			"sharpen", _("Sharp"), _("Sharpen Amount"),
			0.0, 100.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_DENOISE_LUMA, g_param_spec_float(
			/* @TRANSLATORS: "Denoi" is short version of "Denoise". You cannot use more than 5 characters for this! */
			"denoise_luma", _("Denoi"), _("Light Denoising"),
			0.0, 100.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_DENOISE_CHROMA, g_param_spec_float(
			/* @TRANSLATORS: "ColDn" is short version of "Colour Denoise". You cannot use more than 5 characters for this! */
			"denoise_chroma", _("ColDn"), _("Colour Denoising"),
			0.0, 100.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_TCA_KR, g_param_spec_float(
			/* @TRANSLATORS: "CA R" is short version of "Chromatic Aberration Red". You cannot use more than 5 characters for this! */
			"tca_kr", _("CA R"), _("Red Chromatic Aberration Correction"),
			-0.5, 0.5, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_TCA_KB, g_param_spec_float(
			/* @TRANSLATORS: "CA B" is short version of "Chromatic Aberration Blue". You cannot use more than 5 characters for this! */
			"tca_kb", _("CA B"), _("Blue Chromatic Aberration Correction"),
			-0.5, 0.5, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_VIGNETTING, g_param_spec_float(
			/* @TRANSLATORS: "Vign" is short version of "Vignetting". You cannot use more than 5 characters for this! */
			"vignetting", _("Vign"), _("Vignetting Correction"),
			-1.0, 1.0, 0.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_CHANNELMIXER_RED, g_param_spec_float(
			/* @TRANSLATORS: You cannot use more than 5 characters for "Red" */
			"channelmixer_red", _("Red"), _("Red Amount Adjustment"),
			0.0, 300.0, 100.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_CHANNELMIXER_GREEN, g_param_spec_float(
			/* @TRANSLATORS: You cannot use more than 5 characters for "Green" */
			"channelmixer_green", _("Green"), _("Green Amount Adjustment"),
			0.0, 300.0, 100.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_CHANNELMIXER_BLUE, g_param_spec_float(
			/* @TRANSLATORS: You cannot use more than 5 characters for "Blue" */
			"channelmixer_blue", _("Blue"), _("Blue Amount Adjustment"),
			0.0, 300.0, 100.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_RECALC_TEMP, g_param_spec_boolean(
			"recalc-temp", "recalc-temp", "Recalculate Temperature",
			FALSE, G_PARAM_READWRITE)
	);

	signals[SETTINGS_CHANGED] = g_signal_new ("settings-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
	signals[WB_CHANGED] = g_signal_new ("wb-recalculated",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
rs_settings_init (RSSettings *self)
{
	self->commit = 0;
	self->commit_todo = 0;
	self->curve_knots = NULL;
	self->wb_ascii = NULL;
	rs_settings_reset(self, MASK_ALL);
}

RSSettings *
rs_settings_new (void)
{
	return g_object_new (RS_TYPE_SETTINGS, NULL);
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSSettings *settings = RS_SETTINGS(object);

#define CASE(upper, lower) \
	case PROP_##upper: \
		g_value_set_float(value, settings->lower); \
		break
	switch (property_id)
	{
		CASE(EXPOSURE, exposure);
		CASE(SATURATION, saturation);
		CASE(HUE, hue);
		CASE(CONTRAST, contrast);
		CASE(WARMTH, warmth);
		CASE(TINT, tint);
		CASE(DCP_TEMP, dcp_temp);
		CASE(DCP_TINT, dcp_tint);
	case PROP_WB_ASCII:
		g_value_set_string(value, settings->wb_ascii);
		break;
		CASE(SHARPEN, sharpen);
		CASE(DENOISE_LUMA, denoise_luma);
		CASE(DENOISE_CHROMA, denoise_chroma);
		CASE(TCA_KR, tca_kr);
		CASE(TCA_KB, tca_kb);
		CASE(VIGNETTING, vignetting);
		CASE(CHANNELMIXER_RED, channelmixer_red);
		CASE(CHANNELMIXER_GREEN, channelmixer_green);
		CASE(CHANNELMIXER_BLUE, channelmixer_blue);
	case PROP_RECALC_TEMP:
		g_value_set_boolean(value, settings->recalc_temp);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
#undef CASE
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSSettings *settings = RS_SETTINGS(object);
	RSSettingsMask changed_mask = 0;

#define CASE(upper, lower) \
	case PROP_##upper: \
		if (settings->lower != g_value_get_float(value)) \
		{ \
			settings->lower = g_value_get_float(value); \
			changed_mask |= MASK_##upper; \
		} \
		break
	switch (property_id)
	{
		CASE(EXPOSURE, exposure);
		CASE(SATURATION, saturation);
		CASE(HUE, hue);
		CASE(CONTRAST, contrast);
	case PROP_WARMTH:
		if (settings->warmth != g_value_get_float(value))
		{
			settings->warmth = g_value_get_float(value);
			changed_mask |= MASK_WARMTH;
			g_object_set(settings, "wb_ascii", NULL, NULL);
		}
		break;
	case PROP_TINT:
		if (settings->tint != g_value_get_float(value))
		{
			settings->tint = g_value_get_float(value);
			changed_mask |= MASK_TINT;
			g_object_set(settings, "wb_ascii", NULL, NULL);
		}
		break;
	case PROP_DCP_TEMP:
		if (settings->dcp_temp != g_value_get_float(value))
		{
			settings->dcp_temp = g_value_get_float(value);
			changed_mask |= MASK_WARMTH;
			g_object_set(settings, "wb_ascii", NULL, NULL);
		}
		break;
	case PROP_DCP_TINT:
		if (settings->dcp_tint != g_value_get_float(value))
		{
			settings->dcp_tint = g_value_get_float(value);
			changed_mask |= MASK_TINT;
			g_object_set(settings, "wb_ascii", NULL, NULL);
		}
		break;
	case PROP_WB_ASCII:
		if (settings->wb_ascii)
			g_free(settings->wb_ascii);
		settings->wb_ascii = g_strdup(g_value_get_string(value));
		changed_mask |= MASK_WB;
		break;
		CASE(SHARPEN, sharpen);
		CASE(DENOISE_LUMA, denoise_luma);
		CASE(DENOISE_CHROMA, denoise_chroma);
		CASE(TCA_KR, tca_kr);
		CASE(TCA_KB, tca_kb);
		CASE(VIGNETTING, vignetting);
		CASE(CHANNELMIXER_RED, channelmixer_red);
		CASE(CHANNELMIXER_GREEN, channelmixer_green);
		CASE(CHANNELMIXER_BLUE, channelmixer_blue);
		case PROP_RECALC_TEMP:
			settings->recalc_temp = g_value_get_boolean(value);
			if (settings->recalc_temp)
				changed_mask |= MASK_WB;
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
#undef CASE

	if (changed_mask > 0)
	{
		if (settings->commit > 0)
			settings->commit_todo |= changed_mask;
		else
			rs_settings_update_settings(settings, changed_mask);
	}
}

static gfloat timespent[16];
static gint timed_count = 0;
static gint next_timing = 0;

/**
 * Sends updates of an RSSettings, and times the operation
 * @param settings An RSSettings
 * @param mask A mask for indicating the changed values 
 */
static void
rs_settings_update_settings(RSSettings *settings, const RSSettingsMask changed_mask)
{
	GTimer *gt = g_timer_new();
	g_signal_emit(settings, signals[SETTINGS_CHANGED], 0, changed_mask);
	gfloat time = g_timer_elapsed(gt, NULL);

	if (time > 0.001)
	{
		timespent[next_timing] = time;
		next_timing = (next_timing + 1) & 15;
		if (timed_count < 16)
			timed_count++;
		g_debug("Time: %d", (int)(time*1000.0));
	}
	g_timer_destroy(gt);
}

static int
compare_floats(gconstpointer a, gconstpointer b)
{
	if (*(gfloat*)a < *(gfloat*)b)
		return -1;
	if (*(gfloat*)a > *(gfloat*)b)
		return 1;
	return 0;
}

/**
 * Returns the 50% median time used for updating the last 16 settings.
 * Returns -1 if there hasn't been 16 updates yet.
 */
gint
rs_get_median_update_time()
{
	int i;

	if (timed_count < 16)
		return -1;

	GList *sorted = NULL;
	for (i = 0; i < 16; i++)
	{
		sorted = g_list_insert_sorted(sorted, &timespent[(i + next_timing) & 15], compare_floats);
	}
	gfloat median = *(gfloat*)g_list_nth_data(sorted, 7);
	g_list_free(sorted);
	return (int)(median * 1000.0);
}

/**
 * Reset a RSSettings
 * @param settings A RSSettings
 * @param mask A mask for only resetting some values 
 */
void
rs_settings_reset(RSSettings *settings, const RSSettingsMask mask)
{
	g_assert(RS_IS_SETTINGS(settings));
	GObject *object = G_OBJECT(settings);

	rs_settings_commit_start(settings);

	if (mask & MASK_EXPOSURE)
		rs_object_class_property_reset(object, "exposure");

	if (mask & MASK_SATURATION)
		rs_object_class_property_reset(object, "saturation");

	if (mask & MASK_HUE)
		rs_object_class_property_reset(object, "hue");

	if (mask & MASK_CONTRAST)
		rs_object_class_property_reset(object, "contrast");

	if (mask & MASK_WARMTH)
		rs_object_class_property_reset(object, "warmth");

	if (mask & MASK_TINT)
		rs_object_class_property_reset(object, "tint");

	if (mask & MASK_WARMTH)
		rs_object_class_property_reset(object, "dcp-temp");

	if (mask & MASK_TINT)
		rs_object_class_property_reset(object, "dcp-tint");

	if (mask & MASK_SHARPEN)
		rs_object_class_property_reset(object, "sharpen");

	if (mask & MASK_DENOISE_LUMA)
		rs_object_class_property_reset(object, "denoise_luma");

	if (mask & MASK_DENOISE_CHROMA)
		rs_object_class_property_reset(object, "denoise_chroma");

	if (mask & MASK_TCA_KR)
		rs_object_class_property_reset(object, "tca_kr");

	if (mask & MASK_TCA_KB)
		rs_object_class_property_reset(object, "tca_kb");

	if (mask & MASK_VIGNETTING)
		rs_object_class_property_reset(object, "vignetting");

	if (mask & MASK_CHANNELMIXER_RED)
		rs_object_class_property_reset(object, "channelmixer_red");

	if (mask & MASK_CHANNELMIXER_GREEN)
		rs_object_class_property_reset(object, "channelmixer_green");

	if (mask & MASK_CHANNELMIXER_BLUE)
		rs_object_class_property_reset(object, "channelmixer_blue");

	if (mask & MASK_CURVE)
	{
		if (settings->curve_knots)
			g_free(settings->curve_knots);
		settings->curve_knots = g_new(gfloat, 4);
		settings->curve_knots[0] = 0.0;
		settings->curve_knots[1] = 0.0;
		settings->curve_knots[2] = 1.0;
		settings->curve_knots[3] = 1.0;
		settings->curve_nknots = 2;
		settings->commit_todo |= MASK_CURVE;
	}
	rs_settings_commit_stop(settings);
}

/**
 * Stop signal emission from a RSSettings and queue up signals
 * @param settings A RSSettings
 */
void
rs_settings_commit_start(RSSettings *settings)
{
	g_assert(RS_IS_SETTINGS(settings));
	g_assert(settings->commit >= 0);

	/* If we have no current commit running, reset todo */
	if (settings->commit == 0)
		settings->commit_todo = 0;

	/* Increment commit */
	settings->commit++;
}

/**
 * Restart signal emission and process signal queue if any
 * @param settings A RSSettings
 * @return The mask of changes since rs_settings_commit_start()
 */
RSSettingsMask
rs_settings_commit_stop(RSSettings *settings)
{
	g_assert(RS_IS_SETTINGS(settings));
	g_assert(settings->commit >= 0);

	/* If this is the last nested commit, do the todo */
	if ((settings->commit == 1) && (settings->commit_todo != 0))
	{
		rs_settings_update_settings(settings, settings->commit_todo);
	}

	/* Make sure we never go below 0 */
	settings->commit = MAX(settings->commit-1, 0);

	return settings->commit_todo;
}

/**
 * Copy settings from one RSSettins to another
 * @param source The source RSSettings
 * @param mask A RSSettingsMask to do selective copying
 * @param target The target RSSettings
 */
RSSettingsMask
rs_settings_copy(RSSettings *source, RSSettingsMask mask, RSSettings *target)
{
	RSSettingsMask changed_mask = 0;

	g_assert(RS_IS_SETTINGS(source));
	g_assert(RS_IS_SETTINGS(target));

	/* Convenience macro */
#define SETTINGS_COPY(upper, lower) \
do { \
	if ((mask & MASK_##upper) && (target->lower != source->lower)) \
	{ \
		changed_mask |= MASK_ ##upper; \
		target->lower = source->lower; \
	} \
} while(0)

	if ((mask & MASK_WB) && (g_strcmp0(target->wb_ascii, source->wb_ascii) != 0))
	{
		if (target->wb_ascii)
			g_free(target->wb_ascii);

		changed_mask |= MASK_WB; \
		target->wb_ascii = g_strdup(source->wb_ascii);
	}
	SETTINGS_COPY(EXPOSURE, exposure);
	SETTINGS_COPY(SATURATION, saturation);
	SETTINGS_COPY(HUE, hue);
	SETTINGS_COPY(CONTRAST, contrast);
	SETTINGS_COPY(WARMTH, warmth);
	SETTINGS_COPY(TINT, tint);
	SETTINGS_COPY(DCP_TEMP, dcp_temp);
	SETTINGS_COPY(DCP_TINT, dcp_tint);
	SETTINGS_COPY(SHARPEN, sharpen);
	SETTINGS_COPY(DENOISE_LUMA, denoise_luma);
	SETTINGS_COPY(DENOISE_CHROMA, denoise_chroma);
	SETTINGS_COPY(TCA_KR, tca_kr);
	SETTINGS_COPY(TCA_KB, tca_kb);
	SETTINGS_COPY(VIGNETTING, vignetting);
	SETTINGS_COPY(CHANNELMIXER_RED, channelmixer_red);
	SETTINGS_COPY(CHANNELMIXER_GREEN, channelmixer_green);
	SETTINGS_COPY(CHANNELMIXER_BLUE, channelmixer_blue);
#undef SETTINGS_COPY

	if (mask & MASK_WB)
		target->recalc_temp = source->recalc_temp;

	if (mask & MASK_CURVE)
	{
		/* Check if we actually have changed */
		if (target->curve_nknots != source->curve_nknots)
			changed_mask |= MASK_CURVE;
		else
		{
			if (memcmp(source->curve_knots, target->curve_knots, sizeof(gfloat)*2*source->curve_nknots)!=0)
				changed_mask |= MASK_CURVE;
		}

		/* Copy the knots if needed */
		if (changed_mask & MASK_CURVE)
		{
			g_free(target->curve_knots);
			target->curve_knots = g_memdup(source->curve_knots, sizeof(gfloat)*2*source->curve_nknots);
			target->curve_nknots = source->curve_nknots;
		}
	}

	/* Emit seignal if needed */
	if (changed_mask > 0)
		rs_settings_update_settings(target, changed_mask);

	return changed_mask;
}

/**
 * Set curve knots
 * @param settings A RSSettings
 * @param knots Knots for curve
 * @param nknots Number of knots
 */
void
rs_settings_set_curve_knots(RSSettings *settings, const gfloat *knots, const gint nknots)
{
	g_assert(RS_IS_SETTINGS(settings));
	g_assert(nknots > 0);
	g_assert(knots != NULL);

	g_free(settings->curve_knots);

	settings->curve_knots = g_memdup(knots, sizeof(gfloat)*2*nknots);
	settings->curve_nknots = nknots;

	rs_settings_update_settings(settings, MASK_CURVE);
}

/**
 * Set the warmth and tint values of a RSSettings
 * @param settings A RSSettings
 * @param exposure New value
 */
void
rs_settings_set_wb(RSSettings *settings, const gfloat warmth, const gfloat tint, const gchar *ascii)
{
	g_assert(RS_IS_SETTINGS(settings));

	rs_settings_commit_start(settings);
	g_object_set(settings, "warmth", warmth, "tint", tint, "wb_ascii", ascii, "recalc-temp", TRUE, NULL);
	rs_settings_commit_stop(settings);
}

/**
 * Get the knots from the curve
 * @param settings A RSSettings
 * @return All knots as a newly allocated array
 */
gfloat *
rs_settings_get_curve_knots(RSSettings *settings)
{
	g_assert(RS_IS_SETTINGS(settings));

	return g_memdup(settings->curve_knots, sizeof(gfloat)*2*settings->curve_nknots);
}

/**
 * Get number of knots in curve in a RSSettings
 * @param settings A RSSettings
 * @return Number of knots
 */
gint
rs_settings_get_curve_nknots(RSSettings *settings)
{
	g_assert(RS_IS_SETTINGS(settings));

	return settings->curve_nknots;
}

/**
 * Link two RSSettings together, if source gets updated, it will propagate to target
 * @param source A RSSettings
 * @param target A RSSettings
 */
void
rs_settings_link(RSSettings *source, RSSettings *target)
{
	g_assert(RS_IS_SETTINGS(source));
	g_assert(RS_IS_SETTINGS(target));

	/* Add a weak reference to target, we would really like to know if it disappears */
	g_object_weak_ref(G_OBJECT(target), (GWeakNotify) rs_settings_unlink, source);

	/* Use glib signals to propagate changes */
	g_signal_connect(source, "settings-changed", G_CALLBACK(rs_settings_copy), target);
}

/**
 * Unlink two RSSettings - this will be done automaticly if target from a
 * previous rs_settings_link() is finalized
 * @param source A RSSettings
 * @param target A RSSettings - can be destroyed, doesn't matter, we just need the pointer
 */
void
rs_settings_unlink(RSSettings *source, RSSettings *target)
{
	gulong signal_id;

	g_assert(RS_IS_SETTINGS(source));

	/* If we can find a signal linking these two pointers, disconnect it */
	signal_id = g_signal_handler_find(source, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, target);
	if (signal_id > 0)
		g_signal_handler_disconnect(source, signal_id);
}

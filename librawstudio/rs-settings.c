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

#include "rs-settings.h"
#include <string.h> /* memcmp() */

G_DEFINE_TYPE (RSSettings, rs_settings, G_TYPE_OBJECT)

enum {
	SETTINGS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
rs_settings_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (rs_settings_parent_class)->finalize)
		G_OBJECT_CLASS (rs_settings_parent_class)->finalize (object);
}

static void
rs_settings_class_init (RSSettingsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = rs_settings_finalize;

	signals[SETTINGS_CHANGED] = g_signal_new ("settings-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
rs_settings_init (RSSettings *self)
{
	self->commit = 0;
	self->commit_todo = 0;
	self->curve_knots = NULL;
	rs_settings_reset(self, MASK_ALL);
}

RSSettings *
rs_settings_new (void)
{
	return g_object_new (RS_TYPE_SETTINGS, NULL);
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

	if (mask & MASK_EXPOSURE)
		settings->exposure = 0;

	if (mask & MASK_SATURATION)
		settings->saturation = 1.0;

	if (mask & MASK_HUE)
		settings->hue = 0.0;

	if (mask & MASK_CONTRAST)
		settings->contrast = 1.0;

	if (mask & MASK_WARMTH)
		settings->warmth = 0.0;

	if (mask & MASK_TINT)
		settings->tint = 0.0; 

	if (mask & MASK_SHARPEN)
		settings->sharpen = 0.0;

	if (mask && MASK_CURVE)
	{
		if (settings->curve_knots)
			g_free(settings->curve_knots);
		settings->curve_knots = g_new(gfloat, 4);
		settings->curve_knots[0] = 0.0;
		settings->curve_knots[1] = 0.0;
		settings->curve_knots[2] = 1.0;
		settings->curve_knots[3] = 1.0;
		settings->curve_nknots = 2;
	}

	if (mask > 0)
		g_signal_emit(settings, signals[SETTINGS_CHANGED], 0, mask);
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
		g_signal_emit(settings, signals[SETTINGS_CHANGED], 0, settings->commit_todo);
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

	SETTINGS_COPY(EXPOSURE, exposure);
	SETTINGS_COPY(SATURATION, saturation);
	SETTINGS_COPY(HUE, hue);
	SETTINGS_COPY(CONTRAST, contrast);
	SETTINGS_COPY(WARMTH, warmth);
	SETTINGS_COPY(TINT, tint);
	SETTINGS_COPY(SHARPEN, sharpen);
#undef SETTINGS_COPY

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
		g_signal_emit(target, signals[SETTINGS_CHANGED], 0, changed_mask);

	return changed_mask;
}

/* Macro to create functions for changing single parameters, programmers are lazy */
#define RS_SETTINGS_SET(upper, lower) \
gfloat \
rs_settings_set_##lower(RSSettings *settings, const gfloat lower) \
{ \
	gfloat previous; \
	g_assert(RS_IS_SETTINGS(settings)); \
\
	previous = settings->lower; \
\
	if (settings->lower != lower) \
	{ \
		settings->lower = lower; \
\
		if (settings->commit > 0) \
		{ \
			settings->commit_todo |= MASK_##upper; \
		} \
		else \
			g_signal_emit(settings, signals[SETTINGS_CHANGED], 0, MASK_##upper); \
	} \
	return previous; \
}

RS_SETTINGS_SET(EXPOSURE, exposure)
RS_SETTINGS_SET(SATURATION, saturation)
RS_SETTINGS_SET(HUE, hue)
RS_SETTINGS_SET(CONTRAST, contrast)
RS_SETTINGS_SET(WARMTH, warmth)
RS_SETTINGS_SET(TINT, tint)
RS_SETTINGS_SET(SHARPEN, sharpen)

#undef RS_SETTINGS_SET

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

	g_signal_emit(settings, signals[SETTINGS_CHANGED], 0, MASK_CURVE);
}

/**
 * Set the warmth and tint values of a RSSettings
 * @param settings A RSSettings
 * @param exposure New value
 */
void
rs_settings_set_wb(RSSettings *settings, const gfloat warmth, const gfloat tint)
{
	g_assert(RS_IS_SETTINGS(settings));

	rs_settings_commit_start(settings);
	rs_settings_set_warmth(settings, warmth);
	rs_settings_set_tint(settings, tint);
	rs_settings_commit_stop(settings);
}

/* Programmers write programs to write programs */
#define RS_SETTINGS_GET(lower) \
gfloat \
rs_settings_get_##lower(RSSettings *settings) \
{ \
	g_assert(RS_IS_SETTINGS(settings)); \
	return settings->lower; \
}

RS_SETTINGS_GET(exposure);
RS_SETTINGS_GET(saturation);
RS_SETTINGS_GET(hue);
RS_SETTINGS_GET(contrast);
RS_SETTINGS_GET(warmth);
RS_SETTINGS_GET(tint);
RS_SETTINGS_GET(sharpen);

#undef RS_SETTINGS_GET

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

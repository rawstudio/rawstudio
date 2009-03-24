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

#include <rawstudio.h>
#include "rs-filter.h"

#if 0 /* Change to 1 to enable debugging info */
#define filter_debug g_debug
#else
#define filter_debug(...)
#endif

G_DEFINE_TYPE (RSFilter, rs_filter, G_TYPE_OBJECT)

enum {
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
rs_filter_class_init(RSFilterClass *klass)
{
	filter_debug("rs_filter_class_init(%p)", klass);

	signals[CHANGED_SIGNAL] = g_signal_new ("changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	klass->get_image = NULL;
	klass->get_width = NULL;
	klass->get_height = NULL;
	klass->previous_changed = NULL;
}

static void
rs_filter_init(RSFilter *self)
{
	filter_debug("rs_filter_init(%p)", self);
	self->previous = NULL;
	self->next_filters = NULL;
}

/**
 * Return a new instance of a RSFilter
 * @param name The name of the filter
 * @param previous The previous filter or NULL
 * @return The newly instantiated RSFilter or NULL
 */
RSFilter *
rs_filter_new(const gchar *name, RSFilter *previous)
{
	filter_debug("rs_filter_new(%s, %s [%p])", name, RS_FILTER_NAME(previous), previous);
	g_assert(name != NULL);
	g_assert((previous == NULL) || RS_IS_FILTER(previous));

	GType type = g_type_from_name(name);
	RSFilter *filter = NULL;

	if (g_type_is_a (type, RS_TYPE_FILTER))
		filter = g_object_new(type, NULL);

	if (!RS_IS_FILTER(filter))
		g_warning("Could not instantiate filter of type \"%s\"", name);

	if (previous)
		rs_filter_set_previous(filter, previous);

	return filter;
}

/**
 * Set the previous RSFilter in a RSFilter-chain
 * @param filter A RSFilter
 * @param previous A previous RSFilter or NULL
 */
void
rs_filter_set_previous(RSFilter *filter, RSFilter *previous)
{
	filter_debug("rs_filter_set_previous(%p, %p)", filter, previous);
	g_assert(RS_IS_FILTER(filter));
	g_assert(RS_IS_FILTER(previous));

	filter->previous = previous;
	previous->next_filters = g_slist_append(previous->next_filters, filter);
}

/**
 * Signal that a filter has changed, filters depending on this will be invoked
 * This should only be called from filter code
 * @param filter The changed filter
 */
void
rs_filter_changed(RSFilter *filter)
{
	filter_debug("rs_filter_changed(%s [%p])", RS_FILTER_NAME(filter), filter);
	g_assert(RS_IS_FILTER(filter));

	gint i, n_next = g_slist_length(filter->next_filters);

	for(i=0; i<n_next; i++)
	{
		RSFilter *next = RS_FILTER(g_slist_nth_data(filter->next_filters, i));

		g_assert(RS_IS_FILTER(next));

		/* Notify "next" filter or try "next next" filter */
		if (RS_FILTER_GET_CLASS(next)->previous_changed)
			RS_FILTER_GET_CLASS(next)->previous_changed(next, filter);
		else
			rs_filter_changed(next);
	}

	g_signal_emit(G_OBJECT(filter), signals[CHANGED_SIGNAL], 0);
}

/**
 * Get the output image from a RSFilter
 * @param filter A RSFilter
 * @return A RS_IMAGE16, this must be unref'ed
 */
RS_IMAGE16 *
rs_filter_get_image(RSFilter *filter)
{
	filter_debug("rs_filter_get_image(%s [%p])", RS_FILTER_NAME(filter), filter);

	/* This timer-hack will break badly when multithreaded! */
	static gfloat last_elapsed = 0.0;
	static count = -1;
	gfloat elapsed;
	static GTimer *gt = NULL;

	RS_IMAGE16 *image;
	g_assert(RS_IS_FILTER(filter));

	if (count == -1)
		gt = g_timer_new();
	count++;

	if (RS_FILTER_GET_CLASS(filter)->get_image)
		image = RS_FILTER_GET_CLASS(filter)->get_image(filter);
	else
		image = rs_filter_get_image(filter->previous);

	elapsed = g_timer_elapsed(gt, NULL) - last_elapsed;

	printf("%s took: \033[32m%.0f\033[0mms", RS_FILTER_NAME(filter), elapsed*1000);
	if (elapsed > 0.001)
		printf(" [\033[33m%.01f\033[0mMpix/s]", ((gfloat)(image->w*image->h))/elapsed/1000000.0);
	printf("\n");
	last_elapsed += elapsed;

	g_assert(RS_IS_IMAGE16(image) || (image == NULL));

	count--;
	if (count == -1)
	{
		last_elapsed = 0.0;
		printf("Complete chain took: \033[32m%.0f\033[0mms\n\n", g_timer_elapsed(gt, NULL)*1000.0);
		g_timer_destroy(gt);
	}

	return image;
}

/**
 * Get the returned width of a RSFilter
 * @param filter A RSFilter
 * @return Width in pixels
 */
gint
rs_filter_get_width(RSFilter *filter)
{
	gint width;
	g_assert(RS_IS_FILTER(filter));

	if (RS_FILTER_GET_CLASS(filter)->get_width)
		width = RS_FILTER_GET_CLASS(filter)->get_width(filter);
	else
		width = rs_filter_get_width(filter->previous);

	return width;
}

/**
 * Get the returned height of a RSFilter
 * @param filter A RSFilter
 * @return Height in pixels
 */
gint
rs_filter_get_height(RSFilter *filter)
{
	gint height;
	g_assert(RS_IS_FILTER(filter));

	if (RS_FILTER_GET_CLASS(filter)->get_height)
		height = RS_FILTER_GET_CLASS(filter)->get_height(filter);
	else
		height = rs_filter_get_height(filter->previous);

	return height;
}

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

#include <stdlib.h> /* system() */
#include <rawstudio.h>
#include "rs-filter.h"

#if 0 /* Change to 1 to enable performance info */
#define filter_performance printf
#define FILTER_SHOW_PERFORMANCE
#else
#define filter_performance(...) {}
#endif

/* How much time should a filter at least have taken to show performance number */
#define FILTER_PERF_ELAPSED_MIN 0.001
#define CHAIN_PERF_ELAPSED_MIN 0.001

G_DEFINE_TYPE (RSFilter, rs_filter, G_TYPE_OBJECT)

enum {
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
dispose(GObject *obj)
{
	RSFilter *filter = RS_FILTER(obj);

	if (!filter->dispose_has_run)
	{
		filter->dispose_has_run = TRUE;
		if (filter->previous)
		{
			filter->previous->next_filters = g_slist_remove(filter->previous->next_filters, filter);
			g_object_unref(filter->previous);
		}
	}
}

static void
rs_filter_class_init(RSFilterClass *klass)
{
	RS_DEBUG(FILTERS, "rs_filter_class_init(%p)", klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	signals[CHANGED_SIGNAL] = g_signal_new ("changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, 
		NULL,                
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);

	klass->get_image = NULL;
	klass->get_image8 = NULL;
	klass->get_size = NULL;
	klass->previous_changed = NULL;

	object_class->dispose = dispose;
}

static void
rs_filter_init(RSFilter *self)
{
	RS_DEBUG(FILTERS, "rs_filter_init(%p)", self);
	self->previous = NULL;
	self->next_filters = NULL;
	self->enabled = TRUE;
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
	RS_DEBUG(FILTERS, "rs_filter_new(%s, %s [%p])", name, RS_FILTER_NAME(previous), previous);
	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail((previous == NULL) || RS_IS_FILTER(previous), NULL);

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
 * @param previous A previous RSFilter
 */
void
rs_filter_set_previous(RSFilter *filter, RSFilter *previous)
{
	RS_DEBUG(FILTERS, "rs_filter_set_previous(%p, %p)", filter, previous);
	g_return_if_fail(RS_IS_FILTER(filter));
	g_return_if_fail(RS_IS_FILTER(previous));

	/* We will only set the previous filter if it differs from current previous filter */
	if (filter->previous != previous)
	{
		if (filter->previous)
		{
			/* If we already got a previous filter, clean up */
			filter->previous->next_filters = g_slist_remove(filter->previous->next_filters, filter);
			g_object_unref(filter->previous);
		}
		filter->previous = g_object_ref(previous);

		previous->next_filters = g_slist_append(previous->next_filters, filter);
	}
}

/**
 * Signal that a filter has changed, filters depending on this will be invoked
 * This should only be called from filter code
 * @param filter The changed filter
 * @param mask A mask indicating what changed
 */
void
rs_filter_changed(RSFilter *filter, RSFilterChangedMask mask)
{
	RS_DEBUG(FILTERS, "rs_filter_changed(%s [%p], %04x)", RS_FILTER_NAME(filter), filter, mask);
	g_return_if_fail(RS_IS_FILTER(filter));

	gint i, n_next = g_slist_length(filter->next_filters);

	for(i=0; i<n_next; i++)
	{
		RSFilter *next = RS_FILTER(g_slist_nth_data(filter->next_filters, i));

		g_assert(RS_IS_FILTER(next));

		/* Notify "next" filter or try "next next" filter */
		if (RS_FILTER_GET_CLASS(next)->previous_changed)
			RS_FILTER_GET_CLASS(next)->previous_changed(next, filter, mask);
		else
			rs_filter_changed(next, mask);
	}

	g_signal_emit(G_OBJECT(filter), signals[CHANGED_SIGNAL], 0, mask);
}

/* Clamps ROI rectangle to image size */
/* Returns a new rectangle, or NULL if ROI was within bounds*/

static GdkRectangle* 
clamp_roi(const GdkRectangle *roi, RSFilter *filter, const RSFilterRequest *request)
{
	RSFilterResponse *response = rs_filter_get_size(filter, request);
	gint w = rs_filter_response_get_width(response);
	gint h = rs_filter_response_get_height(response);
	g_object_unref(response);

	if ((roi->x >= 0) && (roi->y >=0) && (roi->x + roi->width <= w) && (roi->y + roi->height <= h))
		return NULL;

	GdkRectangle* new_roi = g_new(GdkRectangle, 1);
	new_roi->x = MAX(0, roi->x);
	new_roi->y = MAX(0, roi->y);
	new_roi->width = MIN(w - new_roi->x, roi->width);
	new_roi->height = MAX(h - new_roi->y, roi->height);
	return new_roi;
}

/**
 * Get the output image from a RSFilter
 * @param filter A RSFilter
 * @param param A RSFilterRequest defining parameters for a image request
 * @return A RS_IMAGE16, this must be unref'ed
 */
RSFilterResponse *
rs_filter_get_image(RSFilter *filter, const RSFilterRequest *request)
{
	GdkRectangle* roi = NULL;
	RSFilterRequest *r = NULL;

	g_return_val_if_fail(RS_IS_FILTER(filter), NULL);
	g_return_val_if_fail(RS_IS_FILTER_REQUEST(request), NULL);

	RS_DEBUG(FILTERS, "rs_filter_get_image(%s [%p])", RS_FILTER_NAME(filter), filter);

	/* This timer-hack will break badly when multithreaded! */
	static gfloat last_elapsed = 0.0;
	static gint count = -1;
	gfloat elapsed;
	static GTimer *gt = NULL;

	RSFilterResponse *response;
	RS_IMAGE16 *image;

	if (count == -1)
		gt = g_timer_new();
	count++;

	if (filter->enabled && (roi = rs_filter_request_get_roi(request)))
	{
		roi = clamp_roi(roi, filter, request);
		if (roi)
		{
			r = rs_filter_request_clone(request);
			rs_filter_request_set_roi(r, roi);
			request = r;
		}
	}

	if (RS_FILTER_GET_CLASS(filter)->get_image && filter->enabled)
		response = RS_FILTER_GET_CLASS(filter)->get_image(filter, request);
	else
		response = rs_filter_get_image(filter->previous, request);

	g_assert(RS_IS_FILTER_RESPONSE(response));

	image = rs_filter_response_get_image(response);

	elapsed = g_timer_elapsed(gt, NULL) - last_elapsed;

	if (roi)
		g_free(roi);
	if (r)
		g_object_unref(r);

#ifdef FILTER_SHOW_PERFORMANCE
	if ((elapsed > FILTER_PERF_ELAPSED_MIN) && (image != NULL)) 
	{
		gint iw = image->w;
		gint ih = image->h;
		if (rs_filter_response_get_roi(response)) 
		{
			roi = rs_filter_response_get_roi(response);
			iw = roi->width;
			ih = roi->height;
		}
		filter_performance("%s took: \033[32m%.0f\033[0mms", RS_FILTER_NAME(filter), elapsed*1000);
		if ((elapsed > 0.001) && (image != NULL))
			filter_performance(" [\033[33m%.01f\033[0mMpix/s]", ((gfloat)(iw*ih))/elapsed/1000000.0);
		if (image)
			filter_performance(" [w: %d, h: %d, roi-w:%d, roi-h:%d, channels: %d, pixelsize: %d, rowstride: %d]",
				image->w, image->h, iw, ih, image->channels, image->pixelsize, image->rowstride);
		filter_performance("\n");
	}
#endif
	g_assert(RS_IS_IMAGE16(image) || (image == NULL));
	last_elapsed += elapsed;

	count--;
	if (count == -1)
	{
		last_elapsed = 0.0;
		if (g_timer_elapsed(gt,NULL) > CHAIN_PERF_ELAPSED_MIN)
			filter_performance("Complete 16 bit chain took: \033[32m%.0f\033[0mms\n\n", g_timer_elapsed(gt, NULL)*1000.0);
		rs_filter_param_set_float(RS_FILTER_PARAM(response), "16-bit-time", g_timer_elapsed(gt, NULL));
		g_timer_destroy(gt);
	}
	
	if (image)
		g_object_unref(image);

	return response;
}


/**
 * Get 8 bit output image from a RSFilter
 * @param filter A RSFilter
 * @param param A RSFilterRequest defining parameters for a image request
 * @return A RS_IMAGE16, this must be unref'ed
 */
RSFilterResponse *
rs_filter_get_image8(RSFilter *filter, const RSFilterRequest *request)
{
	g_return_val_if_fail(RS_IS_FILTER(filter), NULL);
	g_return_val_if_fail(RS_IS_FILTER_REQUEST(request), NULL);

	RS_DEBUG(FILTERS, "rs_filter_get_image8(%s [%p])", RS_FILTER_NAME(filter), filter);

	/* This timer-hack will break badly when multithreaded! */
	static gfloat last_elapsed = 0.0;
	static gint count = -1;
	gfloat elapsed, temp;
	static GTimer *gt = NULL;

	RSFilterResponse *response = NULL;
	GdkPixbuf *image = NULL;
	GdkRectangle* roi = NULL;
	RSFilterRequest *r = NULL;

	g_return_val_if_fail(RS_IS_FILTER(filter), NULL);
	g_return_val_if_fail(RS_IS_FILTER_REQUEST(request), NULL);

	if (count == -1)
		gt = g_timer_new();
	count++;

	if (filter->enabled && (roi = rs_filter_request_get_roi(request)))
	{
		roi = clamp_roi(roi, filter, request);
		if (roi)
		{
			r = rs_filter_request_clone(request);
			rs_filter_request_set_roi(r, roi);
			request = r;
		}
	}

	if (RS_FILTER_GET_CLASS(filter)->get_image8 && filter->enabled)
		response = RS_FILTER_GET_CLASS(filter)->get_image8(filter, request);
	else if (filter->previous)
		response = rs_filter_get_image8(filter->previous, request);

	g_assert(RS_IS_FILTER_RESPONSE(response));

	image = rs_filter_response_get_image8(response);
	elapsed = g_timer_elapsed(gt, NULL) - last_elapsed;

	/* Subtract 16 bit time */
	if (rs_filter_param_get_float(RS_FILTER_PARAM(response), "16-bit-time", &temp))
		elapsed -= temp;

	if (roi)
		g_free(roi);
	if (r)
		g_object_unref(r);

#ifdef FILTER_SHOW_PERFORMANCE
	if ((elapsed > FILTER_PERF_ELAPSED_MIN) && (image != NULL)) {
		gint iw = gdk_pixbuf_get_width(image);
		gint ih = gdk_pixbuf_get_height(image);
		if (rs_filter_response_get_roi(response)) 
		{
			GdkRectangle *roi = rs_filter_response_get_roi(response);
			iw = roi->width;
			ih = roi->height;
		}
		filter_performance("%s took: \033[32m%.0f\033[0mms", RS_FILTER_NAME(filter), elapsed * 1000);
		filter_performance(" [\033[33m%.01f\033[0mMpix/s]", ((gfloat)(iw * ih)) / elapsed / 1000000.0);
		filter_performance("\n");
	}
#endif

	last_elapsed += elapsed;

	g_assert(GDK_IS_PIXBUF(image) || (image == NULL));

	count--;
	if (count == -1)
	{
		last_elapsed = 0.0;
		rs_filter_param_get_float(RS_FILTER_PARAM(response), "16-bit-time", &last_elapsed);
		last_elapsed = g_timer_elapsed(gt, NULL)-last_elapsed;
		if (last_elapsed > CHAIN_PERF_ELAPSED_MIN)
			filter_performance("Complete 8 bit chain took: \033[32m%.0f\033[0mms\n\n", last_elapsed*1000.0);
		g_timer_destroy(gt);
		last_elapsed = 0.0;
	}

	if (image)
		g_object_unref(image);

	return response;
}

/**
 * Get predicted size of a RSFilter
 * @param filter A RSFilter
 * @param request A RSFilterRequest defining parameters for the request
 */
RSFilterResponse *
rs_filter_get_size(RSFilter *filter, const RSFilterRequest *request)
{
	RSFilterResponse *response = NULL;

	g_return_val_if_fail(RS_IS_FILTER(filter), NULL);
	g_return_val_if_fail(RS_IS_FILTER_REQUEST(request), NULL);

	if (RS_FILTER_GET_CLASS(filter)->get_size && filter->enabled)
		response = RS_FILTER_GET_CLASS(filter)->get_size(filter, request);
	else if (filter->previous)
		response = rs_filter_get_size(filter->previous, request);

	return response;
}

/**
 * Get predicted size of a RSFilter
 * @param filter A RSFilter
 * @param request A RSFilterRequest defining parameters for the request
 * @param width A pointer to a gint where the width will be written or NULL
 * @param height A pointer to a gint where the height will be written or NULL
 * @return TRUE if width/height is known, FALSE otherwise
 */
gboolean
rs_filter_get_size_simple(RSFilter *filter, const RSFilterRequest *request, gint *width, gint *height)
{
	gint w, h;
	RSFilterResponse *response;

	g_return_val_if_fail(RS_IS_FILTER(filter), FALSE);
	g_return_val_if_fail(RS_IS_FILTER_REQUEST(request), FALSE);

	response = rs_filter_get_size(filter, request);

	if (!RS_IS_FILTER_RESPONSE(response))
		return FALSE;

	w = rs_filter_response_get_width(response);
	h = rs_filter_response_get_height(response);
	if (width)
		*width = w;
	if (height)
		*height = h;

	g_object_unref(response);

	return ((w>0) && (h>0));
}

/**
 * Set a GObject property on zero or more filters above #filter recursively
 * @param filter A RSFilter
 * @param ... Pairs of property names and values followed by NULL
 */
void
rs_filter_set_recursive(RSFilter *filter, ...)
{
	va_list ap;
	gchar *property_name;
	RSFilter *current_filter;
	GParamSpec *spec;
	RSFilter *first_seen_here = NULL;
	GTypeValueTable *table = NULL;
	GType type = 0;
	union CValue {
		gint     v_int;
		glong    v_long;
		gint64   v_int64;
		gdouble  v_double;
		gpointer v_pointer;
	} value;

	g_return_if_fail(RS_IS_FILTER(filter));

	va_start(ap, filter);

	/* Loop through all properties */
	while ((property_name = va_arg(ap, gchar *)))
	{
		/* We set table to NULL for every property to indicate that we (again)
		 * have an "unknown" type */
		table = NULL;

		current_filter = filter;
		/* Iterate through all filters previous to filter */
		do {
			if ((spec = g_object_class_find_property(G_OBJECT_GET_CLASS(current_filter), property_name)))
				if (spec->flags & G_PARAM_WRITABLE)
				{
					/* If we got no GTypeValueTable at this point, we aquire
					 * one. We rely on all filters using the same type for all
					 * properties equally named */
					if (!table)
					{
						first_seen_here = current_filter;
						type = spec->value_type;
						table = g_type_value_table_peek(type);

						/* If we have no valuetable, we're screwed, bail out */
						if (!table)
							g_error("No GTypeValueTable found for '%s'", g_type_name(type));

						switch (table->collect_format[0])
						{
							case 'i': value.v_int = va_arg(ap, gint); break;
							case 'l': value.v_long = va_arg(ap, glong); break;
							case 'd': value.v_double = va_arg(ap, gdouble); break;
							case 'p': value.v_pointer = va_arg(ap, gpointer); break;
							default: g_error("Don't know how to collect for '%s'", g_type_name(type)); break;
						}
					}

					if (table)
					{
						/* We try to catch cases where different filters use
						 * the same property name for different types */
						if (type != spec->value_type)
							g_warning("Diverging types found for property '%s' (on filter '%s' and '%s')",
								property_name,
								RS_FILTER_NAME(first_seen_here),
								RS_FILTER_NAME(current_filter));

						switch (table->collect_format[0])
						{
							case 'i': g_object_set(current_filter, property_name, value.v_int, NULL); break;
							case 'l': g_object_set(current_filter, property_name, value.v_long, NULL); break;
							case 'd': g_object_set(current_filter, property_name, value.v_double, NULL); break;
							case 'p': g_object_set(current_filter, property_name, value.v_pointer, NULL); break;
							default: break;
						}
					}
				}
		} while (RS_IS_FILTER(current_filter = current_filter->previous));
		if (!table)
		{
//			g_warning("Property: %s could not be found in filter chain. Skipping further properties", property_name);
			va_end(ap);
			return;
		}
	}

	va_end(ap);
}

/**
 * Get a GObject property from a RSFilter chain recursively
 * @param filter A RSFilter
 * @param ... Pairs of property names and a return pointers followed by NULL
 */
void
rs_filter_get_recursive(RSFilter *filter, ...)
{
	va_list ap;
	gchar *property_name;
	gpointer property_ret;
	RSFilter *current_filter;

	g_return_if_fail(RS_IS_FILTER(filter));

	va_start(ap, filter);

	/* Loop through all properties */
	while ((property_name = va_arg(ap, gchar *)))
	{
		property_ret = va_arg(ap, gpointer);

		g_assert(property_ret != NULL);

		current_filter = filter;
		/* Iterate through all filter previous to filter */
		do {
			if (current_filter->enabled && g_object_class_find_property(G_OBJECT_GET_CLASS(current_filter), property_name))
			{
				g_object_get(current_filter, property_name, property_ret, NULL);
				break;
			}
		} while (RS_IS_FILTER(current_filter = current_filter->previous));
	}

	va_end(ap);
}

/**
 * Set enabled state of a RSFilter
 * @param filter A RSFilter
 * @param enabled TRUE to enable filter, FALSE to disable
 * @return Previous state
 */
gboolean
rs_filter_set_enabled(RSFilter *filter, gboolean enabled)
{
	gboolean previous_state;

	g_return_val_if_fail(RS_IS_FILTER(filter), FALSE);

	previous_state = filter->enabled;

	if (filter->enabled != enabled)
	{
		filter->enabled = enabled;
		rs_filter_changed(filter, RS_FILTER_CHANGED_PIXELDATA);
	}

	return previous_state;
}

/**
 * Get enabled state of a RSFilter
 * @param filter A RSFilter
 * @return TRUE if filter is enabled, FALSE if disabled
 */
gboolean
rs_filter_get_enabled(RSFilter *filter)
{
	g_return_val_if_fail(RS_IS_FILTER(filter), FALSE);

	return filter->enabled;
}

/**
 * Set a label for a RSFilter - only used for debugging
 * @param filter A RSFilter
 * @param label A new label for the RSFilter, this will NOT be copied
 */
extern void
rs_filter_set_label(RSFilter *filter, const gchar *label)
{
	g_return_if_fail(RS_IS_FILTER(filter));

	filter->label = label;	
}

/**
 * Get the label for a RSFilter
 * @param filter A RSFilter
 * @return The label for the RSFilter or NULL
 */
const gchar *
rs_filter_get_label(RSFilter *filter)
{
	g_return_val_if_fail(RS_IS_FILTER(filter), "");

	return filter->label;
}

static void
rs_filter_graph_helper(GString *str, RSFilter *filter)
{
	g_assert(str != NULL);
	g_assert(RS_IS_FILTER(filter));

	g_string_append_printf(str, "\"%p\" [\n\tshape=\"Mrecord\"\n", filter);
	
	if (!g_str_equal(RS_FILTER_NAME(filter), "RSCache"))
		g_string_append_printf(str, "\tcolor=grey\n\tstyle=filled\n");

	if (filter->enabled)
		g_string_append_printf(str, "\tcolor=\"#66ba66\"\n");
	else
		g_string_append_printf(str, "\tcolor=grey\n");
		
	g_string_append_printf(str, "\tlabel=<<table cellborder=\"0\" border=\"0\">\n");

	GObjectClass *klass = G_OBJECT_GET_CLASS(filter);
	GParamSpec **specs;
	gint i;
	guint n_specs = 0;

	/* Filter name (and label) */
	g_string_append_printf(str, "\t\t<tr>\n\t\t\t<td colspan=\"2\" bgcolor=\"black\"><font color=\"white\">%s", RS_FILTER_NAME(filter));
	if (filter->label)
		g_string_append_printf(str, " (%s)", filter->label);
	g_string_append_printf(str, "</font></td>\n\t\t</tr>\n");

	/* Parameter and value list */
	specs = g_object_class_list_properties(G_OBJECT_CLASS(klass), &n_specs);
	for(i=0; i<n_specs; i++)
	{
		gboolean boolean = FALSE;
		gint integer = 0;
		gfloat loat = 0.0;
		gchar *ostr = NULL;

		g_string_append_printf(str, "\t\t<tr>\n\t\t\t<td align=\"right\">%s:</td>\n\t\t\t<td align=\"left\">", specs[i]->name);
		/* We have to use if/else here, because RS_TYPE_* does not resolve to a constant */
		if (G_PARAM_SPEC_VALUE_TYPE(specs[i]) == RS_TYPE_LENS)
		{
			RSLens *lens;
			gchar *identifier;

			g_object_get(filter, specs[i]->name, &lens, NULL);
			if (lens)
			{
				g_object_get(lens, "identifier", &identifier, NULL);
				g_object_unref(lens);

				g_string_append_printf(str, "%s", identifier);

				g_free(identifier);
			}
			else
				g_string_append_printf(str, "n/a");
		}
		else if (G_PARAM_SPEC_VALUE_TYPE(specs[i]) == RS_TYPE_ICC_PROFILE)
		{
			RSIccProfile *profile;
			gchar *profile_filename;
			gchar *profile_basename;

			g_object_get(filter, specs[i]->name, &profile, NULL);
			g_object_get(profile, "filename", &profile_filename, NULL);
			g_object_unref(profile);
			profile_basename = g_path_get_basename (profile_filename);
			g_free(profile_filename);

			g_string_append_printf(str, "%s", profile_basename);
			g_free(profile_basename);
		}
		else
			switch (G_PARAM_SPEC_VALUE_TYPE(specs[i]))
			{
				case G_TYPE_BOOLEAN:
					g_object_get(filter, specs[i]->name, &boolean, NULL);
					g_string_append_printf(str, "%s", (boolean) ? "TRUE" : "FALSE");
					break;
				case G_TYPE_INT:
					g_object_get(filter, specs[i]->name, &integer, NULL);
					g_string_append_printf(str, "%d", integer);
					break;
				case G_TYPE_FLOAT:
					g_object_get(filter, specs[i]->name, &loat, NULL);
					g_string_append_printf(str, "%.05f", loat);
					break;
				case G_TYPE_STRING:
					g_object_get(filter, specs[i]->name, &ostr, NULL);
					g_string_append_printf(str, "%s", ostr);
					break;
				default:
					g_string_append_printf(str, "n/a");
					break;
			}
		g_string_append_printf(str, "</td>\n\t\t</tr>\n");
	}

	g_string_append_printf(str, "\t\t</table>>\n\t];\n");

	gint n_next = g_slist_length(filter->next_filters);

	for(i=0; i<n_next; i++)
	{
		RSFilter *next = RS_FILTER(g_slist_nth_data(filter->next_filters, i));
		RSFilterResponse *response = rs_filter_get_size(filter, RS_FILTER_REQUEST_QUICK);

		/* Edge - print dimensions along */
		g_string_append_printf(str, "\t\"%p\" -> \"%p\" [label=\" %dx%d\"];\n",
			filter, next,
			rs_filter_response_get_width(response), rs_filter_response_get_height(response));
		g_object_unref(response);

		/* Recursively call ourself for every "next" filter */
		rs_filter_graph_helper(str, next);
	}
}

/**
 * Draw a nice graph of the filter chain
 * note: Requires graphviz
 * @param filter The top-most filter to graph
 */
void
rs_filter_graph(RSFilter *filter)
{
	g_return_if_fail(RS_IS_FILTER(filter));
	GString *str = g_string_new("digraph G {\n");

	rs_filter_graph_helper(str, filter);

	g_string_append_printf(str, "}\n");
	g_file_set_contents("/tmp/rs-filter-graph", str->str, str->len, NULL);

	if (0 != system("dot -Tpng >/tmp/rs-filter-graph.png </tmp/rs-filter-graph"))
		g_warning("Calling dot failed");
	if (0 != system("gnome-open /tmp/rs-filter-graph.png"))
		g_warning("Calling gnome-open failed.");

	g_string_free(str, TRUE);
}

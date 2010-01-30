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

#define RS_TYPE_CACHE (rs_cache_type)
#define RS_CACHE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_CACHE, RSCache))
#define RS_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_CACHE, RSCacheClass))
#define RS_IS_CACHE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_CACHE))

typedef struct _RSCache RSCache;
typedef struct _RSCacheClass RSCacheClass;

struct _RSCache {
	RSFilter parent;

	RSFilterResponse *cached_image;
	gboolean ignore_changed;
	RSFilterChangedMask mask;
	gboolean ignore_roi;
	gint latency;
};

struct _RSCacheClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_cache, RSCache)

enum {
	PROP_0,
	PROP_LATENCY,
	PROP_IGNORE_ROI
};

static void finalize(GObject *object);
static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static RSFilterResponse *get_image8(RSFilter *filter, const RSFilterRequest *request);
static void flush(RSCache *cache);
static void previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_cache_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_cache_class_init(RSCacheClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->finalize = finalize;

	g_object_class_install_property(object_class,
		PROP_LATENCY, g_param_spec_int(
			"latency", "latency", "Signal propagation latency in milliseconds, this can be used to reduce signals from \"noisy\" filters.",
			0, 10000, 0,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_IGNORE_ROI, g_param_spec_boolean(
			"ignore-roi", "ignore-roi", "Ignore ROI parameter from request",
			FALSE,
			G_PARAM_READWRITE)
	);

	filter_class->name = "Listen for changes and caches image data";
	filter_class->get_image = get_image;
	filter_class->get_image8 = get_image8;
	filter_class->previous_changed = previous_changed;
}

static void
rs_cache_init(RSCache *cache)
{
	cache->ignore_changed = FALSE;
	cache->ignore_roi = FALSE;
	cache->latency = 0;
	cache->cached_image = rs_filter_response_new();
}

static void
finalize(GObject *object)
{
	RSCache *cache = RS_CACHE(object);
	flush(cache);
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSCache *cache = RS_CACHE(object);

	switch (property_id)
	{
		case PROP_LATENCY:
			g_value_set_int(value, cache->latency);
			break;
		case PROP_IGNORE_ROI:
			g_value_set_boolean(value, cache->ignore_roi);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSCache *cache = RS_CACHE(object);

	switch (property_id)
	{
		case PROP_LATENCY:
			cache->latency = g_value_get_int(value);
			break;
		case PROP_IGNORE_ROI:
			cache->ignore_roi = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gboolean
rectangle_is_inside(GdkRectangle *outer_rect, GdkRectangle *inner_rect)
{
	return inner_rect->x >= outer_rect->x &&
		inner_rect->x + inner_rect->width <= outer_rect->x + outer_rect->width &&
		inner_rect->y >= outer_rect->y && 
		inner_rect->y + inner_rect->height <= outer_rect->y + outer_rect->height;
}

static void
set_roi_to_full(RSCache *cache) {
	GdkRectangle *r = g_new(GdkRectangle, 1);
	r->x = 0;
	r->y = 0;

	if (rs_filter_response_has_image(cache->cached_image)) {
		RS_IMAGE16 *img = rs_filter_response_get_image(cache->cached_image);
		r->width = img->w;
		r->height = img->h;
		rs_filter_response_set_roi(cache->cached_image,r);
		g_object_unref(img);
	}

	if (rs_filter_response_has_image8(cache->cached_image)) {
		GdkPixbuf *img  =  rs_filter_response_get_image8(cache->cached_image);
		r->width = gdk_pixbuf_get_width(img);
		r->height = gdk_pixbuf_get_height(img);
		rs_filter_response_set_roi(cache->cached_image,r);
		g_object_unref(img);
	}
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *_request)
{
	RSCache *cache = RS_CACHE(filter);
	RSFilterRequest *request = rs_filter_request_clone(_request);
	GdkRectangle *roi = rs_filter_request_get_roi(request);

	if (roi && cache->ignore_roi)
	{
		roi = NULL;
		rs_filter_request_set_roi(request, NULL);
	}

	if (rs_filter_response_has_image(cache->cached_image)) {

		if (rs_filter_response_get_quick(cache->cached_image) && !rs_filter_request_get_quick(request))
			flush(cache);

		if (!rs_filter_response_get_roi(cache->cached_image) && roi)
			set_roi_to_full(cache);

		if (!cache->ignore_roi && roi)
			if (rs_filter_response_get_roi(cache->cached_image)) 
				if (!rectangle_is_inside(rs_filter_response_get_roi(cache->cached_image), roi))
					flush(cache);

		if (!roi && rs_filter_response_get_roi(cache->cached_image))
			flush(cache);
	}

	if (!rs_filter_response_has_image(cache->cached_image))
	{
		g_object_unref(cache->cached_image);
		cache->cached_image = rs_filter_get_image(filter->previous, request);
		rs_filter_response_set_roi(cache->cached_image, roi);
		if (rs_filter_request_get_quick(request))
			rs_filter_response_set_quick(cache->cached_image);
	}

	RSFilterResponse *fr = rs_filter_response_clone(cache->cached_image);
	RS_IMAGE16* img = rs_filter_response_get_image(cache->cached_image);
	rs_filter_response_set_image(fr, img);

	if (img)
		g_object_unref(img);

	g_object_unref(request);

	return fr;
}


static RSFilterResponse *
get_image8(RSFilter *filter, const RSFilterRequest *_request)
{
	RSCache *cache = RS_CACHE(filter);
	RSFilterRequest *request = rs_filter_request_clone(_request);
	GdkRectangle *roi = rs_filter_request_get_roi(request);

	if (roi && cache->ignore_roi)
	{
		roi = NULL;
		rs_filter_request_set_roi(request, NULL);
	}

	if (rs_filter_response_has_image8(cache->cached_image)) {

		if (rs_filter_response_get_quick(cache->cached_image) && !rs_filter_request_get_quick(request))
			flush(cache);

		if (!rs_filter_response_get_roi(cache->cached_image) && roi)
			set_roi_to_full(cache);

		if (!cache->ignore_roi && roi) 
			if (rs_filter_response_get_roi(cache->cached_image)) 
				if (!rectangle_is_inside(rs_filter_response_get_roi(cache->cached_image), roi))
					flush(cache);

		if (!roi && rs_filter_response_get_roi(cache->cached_image))
			flush(cache);
	}

	if (!rs_filter_response_has_image8(cache->cached_image))
	{
		g_object_unref(cache->cached_image);
		cache->cached_image = rs_filter_get_image8(filter->previous, request);
		rs_filter_response_set_roi(cache->cached_image, roi);
		if (rs_filter_request_get_quick(request))
			rs_filter_response_set_quick(cache->cached_image);
	}

	RSFilterResponse *fr = rs_filter_response_clone(cache->cached_image);
	GdkPixbuf* img = rs_filter_response_get_image8(cache->cached_image);
	rs_filter_response_set_image8(fr, img);

	if (img)
		g_object_unref(img);

	g_object_unref(request);

	return fr;
}

static gboolean
previous_changed_timeout_func(gpointer data)
{
	RS_CACHE(data)->ignore_changed = FALSE;

	rs_filter_changed(RS_FILTER(data), RS_CACHE(data)->mask);

	return FALSE;
}

static void
flush(RSCache *cache)
{
	g_object_unref(cache->cached_image);
	cache->cached_image = rs_filter_response_new();
}

static void
previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask)
{
	RSCache *cache = RS_CACHE(filter);

	if (mask & RS_FILTER_CHANGED_PIXELDATA)
		flush(cache);

	if (cache->latency > 0)
	{
		cache->mask = mask;
		if (!cache->ignore_changed)
		{
			cache->ignore_changed = TRUE;
			g_timeout_add(cache->latency, previous_changed_timeout_func, cache);
		}
	}
	else
		rs_filter_changed(filter, mask);
}

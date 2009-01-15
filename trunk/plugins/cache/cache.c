/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

	RS_IMAGE16 *image;
};

struct _RSCacheClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_cache, RSCache)

static RS_IMAGE16 *get_image(RSFilter *filter);
static void previous_changed(RSFilter *filter, RSFilter *parent);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_cache_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_cache_class_init(RSCacheClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);

	filter_class->name = "Listen for changes and caches image data";
	filter_class->get_image = get_image;
	filter_class->previous_changed = previous_changed;
}

static void
rs_cache_init(RSCache *cache)
{
	cache->image = NULL;
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSCache *cache = RS_CACHE(filter);

	if (!cache->image)
		cache->image = rs_filter_get_image(filter->previous);

	return (cache->image) ? g_object_ref(cache->image) : NULL;
}

static void
previous_changed(RSFilter *filter, RSFilter *parent)
{
	RSCache *cache = RS_CACHE(filter);

	if (cache->image)
		g_object_unref(cache->image);

	cache->image = NULL;

	/* Propagate */
	rs_filter_changed(filter);
}

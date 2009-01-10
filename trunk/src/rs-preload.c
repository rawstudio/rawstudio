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

#include <gtk/gtk.h>
#include <glib.h>
#include "rs-preload.h"
#include "rs-photo.h"

#define PRELOAD_DEBUG if (0) printf

typedef struct _rs_preloaded {
	gchar *filename;
	RS_IMAGE16 *image;
} RS_PRELOADED;

static void rs_preload(const gchar *filename);

static GThreadPool *pool = NULL;

/* Which images are "near" the current image */
static GStaticMutex near_lock = G_STATIC_MUTEX_INIT;
static GList *near = NULL;

/* The queue - to be processed */
static GStaticMutex queue_lock = G_STATIC_MUTEX_INIT;
static GList *queue = NULL;

/* Which images are currently preloaded */
static GStaticMutex preloaded_lock = G_STATIC_MUTEX_INIT;

static GList *preloaded = NULL; /* RS_PRELOADED * */
static size_t preloaded_memory_in_use = 0;

/* The queue - to be processed */
static GStaticMutex average_size_lock = G_STATIC_MUTEX_INIT;
static size_t average_size = 0;

/* The maximum amount of memory to use in bytes */
static size_t max_memory = 0; /* The maximum memory to use for preload cache */

/**
 * Searches in a GList of strings
 */
static gint
g_list_str_equal(gconstpointer a, gconstpointer b)
{
	if (!(a&&b)) return -1;
	return !g_str_equal(a, b);
}

/**
 * Searches for a specific filename in a GList of RS_PRELOADED *
 */
static gint
g_list_find_filename(gconstpointer a, gconstpointer b)
{
	if (!(a&&b)) return -1;
	return !g_str_equal(((RS_PRELOADED *)a)->filename, b);
}

/**
 * Removes exactly one image from preload-cache, trying to keep images that are "near"
 */
static void
_remove_one_image()
{
	gint len, pos;
	RS_PRELOADED *p = NULL;
	GList *remove = NULL;
	GList *l;

	len = g_list_length(preloaded);
	pos = len-1;

	/* Try to find the last cached - NOT in near list */
	while (pos >= 0)
	{
		l = g_list_nth(preloaded, pos);
		p = l->data;
		if(!(g_list_find_custom(near, p->filename, g_list_str_equal)))
		{
			PRELOAD_DEBUG("\033[34m[NOT NEAR] ");
			remove = l;
			break;
		}
		pos--;
	}

	/* If all are near, pick the one farest away */
	if (!remove)
	{
		len = g_list_length(near);
		pos = len-1;
		while(pos >= 0)
		{
			if((l = g_list_find_custom(preloaded, g_list_nth_data(near, pos), g_list_find_filename)))
			{
				PRELOAD_DEBUG("\033[34m[NEAR] ");
				remove = l;
				break;
			}
			pos--;
		}
	}

	if (remove)
	{
		p = remove->data;
		PRELOAD_DEBUG("Removed %s\033[0m\n", p->filename);

		preloaded_memory_in_use -= rs_image16_get_footprint(p->image);

		rs_image16_free(p->image);
		g_free(p->filename);
		g_free(p);

		preloaded = g_list_remove_link(preloaded, remove);
		g_list_free(remove);
	}
}

static void
worker_thread(gpointer data, gpointer bogus)
{
	gchar *filename = (gchar *) data;
	RS_PHOTO *photo = NULL;

	photo = rs_photo_load_from_file(filename, TRUE);
	if (photo)
	{
		GList *q = NULL;
		size_t footprint = rs_image16_get_footprint(photo->input);
		RS_PRELOADED *p = g_new0(RS_PRELOADED, 1);
		p->filename = filename;
		p->image = photo->input;
		PRELOAD_DEBUG("\033[34mPreloading %s\033[0m\n", filename);
		photo->input = NULL; /* EVIL hack to avoid freeing in rs_photo_free() */
		g_object_unref(photo);

		g_static_mutex_lock(&queue_lock);
		g_static_mutex_lock(&preloaded_lock);

		while((preloaded_memory_in_use + footprint) > max_memory)
			_remove_one_image();
		preloaded_memory_in_use += footprint;

		/* Move from queue to preloaded */
		if ((q = g_list_find_custom(queue, filename, g_list_str_equal)))
			queue = g_list_remove_link(queue, q);
		preloaded = g_list_prepend(preloaded, p);

		g_static_mutex_unlock(&preloaded_lock);
		g_static_mutex_unlock(&queue_lock);

		/* Calculate average image size */
		g_static_mutex_lock(&average_size_lock);
		if (average_size == 0)
			average_size = footprint;
		else
			average_size = (average_size+footprint+1)/2;
		g_static_mutex_unlock(&average_size_lock);
	}
	PRELOAD_DEBUG("\033[33m[%zd/%zdMB used] [%d tasks unprocessed] [%zdMB avg]\033[0m\n", preloaded_memory_in_use/(1024*1024), max_memory/(1024*1024), g_thread_pool_unprocessed(pool), average_size/(1024*1024));
}

/**
 * Remove all images from near list, this should be called before populating near
 */
void
rs_preload_near_remove_all()
{
	/* Clear the "near" array, will be filled by rs_preload_near_add() */
	g_static_mutex_lock(&near_lock);
	if (near)
	{
		g_list_foreach(near, (GFunc) g_free, NULL);
		g_list_free(near);
		near = NULL;
	}
	g_static_mutex_unlock(&near_lock);
}

/**
 * Add a near image, near images will be free'ed last,
 * rs_preload_near_remove_all() should be called first, to empty the list
 */ 
void
rs_preload_near_add(const gchar *filename)
{
	if (max_memory == 0) return;
	g_static_mutex_lock(&near_lock);
	near = g_list_prepend(near, g_strdup(filename));
	g_static_mutex_unlock(&near_lock);
	rs_preload(filename);
}

/**
 * Preload a file
 * @param filename The file to preload
 */
static void
rs_preload(const gchar *filename)
{
	if (max_memory == 0) return;

	g_static_mutex_lock(&queue_lock);
	g_static_mutex_lock(&preloaded_lock);
	if ((!g_list_find_custom(queue, filename, g_list_str_equal))
		&& (!g_list_find_custom(preloaded, filename, g_list_find_filename)))
	{
		queue = g_list_prepend(queue, g_strdup(filename));
		g_thread_pool_push(pool, g_strdup(filename), NULL);
	}
	g_static_mutex_unlock(&preloaded_lock);
	g_static_mutex_unlock(&queue_lock);
}

/**
 * Get a preloaded photo
 * @param filename A filename
 * @return The new RS_PHOTO or NULL if not preloaded
 */
RS_PHOTO *
rs_get_preloaded(const gchar *filename)
{
	RS_PHOTO *photo = NULL;

	if (max_memory == 0) return NULL;

	if (filename)
	{
		GList *l;
		RS_PRELOADED *p;
		g_static_mutex_lock(&preloaded_lock);
		if ((l = g_list_find_custom(preloaded, filename, g_list_find_filename)))
		{
			PRELOAD_DEBUG("\033[32m%s preloaded\033[0m\n", filename);
			photo = rs_photo_new();
			p = l->data;
			photo->input = rs_image16_copy_double(p->image, NULL);
			photo->filename = g_strdup(p->filename);
		}
		else
			PRELOAD_DEBUG("\033[31m%s NOT preloaded\033[0m\n", filename);
		g_static_mutex_unlock(&preloaded_lock);
	}

	return photo;
}

/**
 * Get a qualified guess about how many photos we would like to have marked as
 * near
 * @return A number of photos to mark as near
 */
gint
rs_preload_get_near_count()
{
	/* Defaults to 2, this should not saturate anyone */
	gint near = 2;

	if (max_memory == 0) return 0;

	g_static_mutex_lock(&average_size_lock);
	if (average_size > 0)
		near = ((max_memory*2)/3)/average_size-1;
	g_static_mutex_unlock(&average_size_lock);

	/* Bigger is not always better! */
	if (near > 3)
		near = 3;

	PRELOAD_DEBUG("near: %d\n", near);

	return near;
}

/**
 * Set the maximum amount of memory to be used for preload buffer
 * @param max A value in bytes, everything below 100MB will be interpreted as 0
 */
void
rs_preload_set_maximum_memory(size_t max)
{
	if (max < (100*1024*1024))
		max = 0;
	max_memory = max;

	/* Initialize thread pool */
	if ((max_memory > 0) && (!pool))
		pool = g_thread_pool_new(worker_thread, NULL, 2, TRUE, NULL);

	g_static_mutex_lock(&queue_lock);
	g_static_mutex_lock(&preloaded_lock);

	while(preloaded_memory_in_use > max_memory)
		_remove_one_image();

	g_static_mutex_unlock(&preloaded_lock);
	g_static_mutex_unlock(&queue_lock);
}

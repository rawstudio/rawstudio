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
#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "rs-preload.h"

static GAsyncQueue *queue = NULL;

static gpointer
worker(gpointer data)
{
	gchar *filename;
	gint fd;
	struct stat st;

	while (1)
	{
		data = g_async_queue_pop(queue);
		if (data == NULL)
			continue;

		filename = data;
		fd = open(filename, O_RDONLY);

		if (fd > 0)
		{
			stat(filename, &st);
			if (st.st_size > 0)
			{
#if __gnu_linux__
				readahead(fd, 0, st.st_size);
#else
				gint bytes_read = 0;
				gchar *tmp = g_new(gchar, st.st_size);
				while(bytes_read < st.st_size)
					bytes_read += read(fd, tmp, st.st_size-bytes_read);
				g_free(tmp);
#endif /* __gnu_linux__ */
			}
			close(fd);
		}

		g_free(data);
	}
	return NULL;
}

static void
init()
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (queue == NULL)
	{
		queue = g_async_queue_new_full(g_free);
		g_thread_create_full(worker, NULL, 0, FALSE, FALSE, G_THREAD_PRIORITY_LOW, NULL);
	}
	g_static_mutex_unlock(&lock);
}

/**
 * Empty the current queue
 */
extern void
rs_preload_cancel_all()
{
	init();

	while(g_async_queue_try_pop(queue));
}

/**
 * Preloads a file - this will add file content to the OS cache
 * @param filename A filename to preload
 */
void
rs_preload(const gchar *filename)
{
	init();

	g_async_queue_push(queue, g_strdup(filename));
}

/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
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

#include "rs-io.h"

static GStaticMutex init_lock = G_STATIC_MUTEX_INIT;
static GAsyncQueue *queue = NULL;
static GStaticRecMutex io_lock = G_STATIC_REC_MUTEX_INIT;
static gboolean pause_queue = FALSE;


static gint
queue_sort(gconstpointer a, gconstpointer b, gpointer user_data)
{
	gint id1 = 0;
	gint id2 = 0;

	if (a)
		id1 = RS_IO_JOB(a)->priority;
	if (b)
		id2 = RS_IO_JOB(b)->priority;

	return (id1 > id2 ? +1 : id1 == id2 ? 0 : -1);
}

static gpointer
queue_worker(gpointer data)
{
	GAsyncQueue *queue = data;
	RSIoJob *job;

	while (1)
	{
		if (pause_queue)
			g_usleep(1000);
		else
		{
			job = g_async_queue_pop(queue);

			/* If we somehow got NULL, continue. I'm not sure this will ever happen, but this is better than random segfaults :) */
			if (!job)
				continue;

			rs_io_job_execute(job);
			rs_io_job_do_callback(job);
		}
	}

	return NULL;
}

static void
init()
{
	int i;
	g_static_mutex_lock(&init_lock);
	if (!queue)
	{
		queue = g_async_queue_new();
		for (i = 0; i < rs_get_number_of_processor_cores(); i++)
			g_thread_create_full(queue_worker, queue, 0, FALSE, FALSE, G_THREAD_PRIORITY_LOW, NULL);
	}
	g_static_mutex_unlock(&init_lock);
}

/**
 * Add a RSIoJob to be executed later
 * @param job A RSIoJob. This will be unreffed upon completion
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @param priority Lower value means higher priority
 * @param user_data A pointer to pass to the callback
 */
void
rs_io_idle_add_job(RSIoJob *job, gint idle_class, gint priority, gpointer user_data)
{
	g_assert(RS_IS_IO_JOB(job));

	job->idle_class = idle_class;
	job->priority = priority;
	job->user_data = user_data;

	g_assert(job->idle_class != -1);
	g_async_queue_push_sorted(queue, job, queue_sort, NULL);
}

/**
 * Prefetch a file
 * @param path Absolute path to a file to prefetch
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_prefetch_file(const gchar *path, gint idle_class)
{
	init();

	RSIoJob *job = rs_io_job_prefetch_new(path);
	rs_io_idle_add_job(job, idle_class, 20, NULL);

	return job;
}

/**
 * Load metadata belonging to a photo
 * @param path Absolute path to a photo
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @param callback A callback to call when the data is ready or NULL
 * @param user_data Data to pass to the callback
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_read_metadata(const gchar *path, gint idle_class, RSGotMetadataCB callback, gpointer user_data)
{
	init();

	RSIoJob *job = rs_io_job_metadata_new(path, callback);
	rs_io_idle_add_job(job, idle_class, 10, user_data);

	return job;
}

/**
 * Compute a "Rawstudio checksum" of a file
 * @param path Absolute path to a file
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @param callback A callback to call when the data is ready or NULL
 * @param user_data Data to pass to the callback
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_read_checksum(const gchar *path, gint idle_class, RSGotChecksumCB callback, gpointer user_data)
{
	init();

	RSIoJob *job = rs_io_job_checksum_new(path, callback);
	rs_io_idle_add_job(job, idle_class, 30, user_data);

	return job;
}

/**
 * Cancel a complete class of idle requests
 * @param idle_class The class identifier
 */
void
rs_io_idle_cancel_class(gint idle_class)
{
	/* This behaves like rs_io_idle_cancel_class(), please see comments there */
	RSIoJob *current_job;
	RSIoJob *marker_job = rs_io_job_new();

	init();

	g_async_queue_lock(queue);

	/* Put a marker in the queue, we will rotate the complete queue, so we have to know when we're around */
	g_async_queue_push_unlocked(queue, marker_job);

	while((current_job = g_async_queue_pop_unlocked(queue)))
	{
		/* If current job matches marker, we're done */
		if (current_job == marker_job)
			break;

		/* Of the job's idle_class doesn't match the class to cancel, we put the job back in the queue */
		if (current_job->idle_class != idle_class)
		{
			g_async_queue_push_unlocked(queue, current_job);
		}
	}

	/* Make sure the queue is sorted */
	g_async_queue_sort_unlocked(queue, queue_sort, NULL);

	g_async_queue_unlock(queue);

	g_object_unref(marker_job);
}

/**
 * Cancel an idle request
 * @param request_id A request_id as returned by rs_io_idle_read_complete_file()
 */
void
rs_io_idle_cancel(RSIoJob *job)
{
	/* This behaves like rs_io_idle_cancel_class(), please see comments there */
	RSIoJob *current_job;
	RSIoJob *marker_job = rs_io_job_new();

	init();

	g_async_queue_lock(queue);

	/* Put a marker in the queue, we will rotate the complete queue, so we have to know when we're around */
	g_async_queue_push_unlocked(queue, marker_job);

	while((current_job = g_async_queue_pop_unlocked(queue)))
	{
		/* If current job matches marker, we're done */
		if (current_job == marker_job)
			break;

		if (current_job != job)
			g_async_queue_push_unlocked(queue, current_job);
	}

	/* Make sure the queue is sorted */
	g_async_queue_sort_unlocked(queue, queue_sort, NULL);

	g_async_queue_unlock(queue);

	g_object_unref(marker_job);
}

/**
 * Aquire the IO lock
 */
void
rs_io_lock()
{
	g_static_rec_mutex_lock(&io_lock);
}

/**
 * Release the IO lock
 */
void
rs_io_unlock()
{
	g_static_rec_mutex_unlock(&io_lock);
}

/**
 * Pause the worker threads
 */
void
rs_io_idle_pause()
{
	pause_queue = TRUE;
}

/**
 * Unpause the worker threads
 */
void
rs_io_idle_unpause()
{
	pause_queue = FALSE;
}

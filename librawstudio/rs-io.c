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

#include "rs-io.h"

static GMutex init_lock;
static GAsyncQueue *queue = NULL;
static GRecMutex io_lock;
static GTimer *io_lock_timer = NULL;
static gboolean pause_queue = FALSE;
static gint queue_active_count = 0;
static GMutex count_lock;


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
			g_mutex_lock(&count_lock);
			job = g_async_queue_try_pop(queue);
			if (job)
				queue_active_count++;
			g_mutex_unlock(&count_lock);

			/* If we somehow got NULL, continue. I'm not sure this will ever happen, but this is better than random segfaults :) */
			if (job)
			{
				rs_io_job_execute(job);
				rs_io_job_do_callback(job);
				g_mutex_lock(&count_lock);
				queue_active_count--;
				g_mutex_unlock(&count_lock);
			}
			else
			{
				/* Sleep 1 ms */
				g_usleep(1000);
			}
		}
	}

	return NULL;
}

static void
init(void)
{
	int i;
	g_mutex_lock(&init_lock);
	if (!queue)
	{
		queue = g_async_queue_new();
		for (i = 0; i < rs_get_number_of_processor_cores(); i++)
			g_thread_new("io worker", queue_worker, queue);

		io_lock_timer = g_timer_new();
	}
	g_mutex_unlock(&init_lock);
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
	g_return_if_fail(RS_IS_IO_JOB(job));

	job->idle_class = idle_class;
	job->priority = priority;
	job->user_data = user_data;

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
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(path), NULL);

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
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(path), NULL);

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
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(path), NULL);

	init();

	RSIoJob *job = rs_io_job_checksum_new(path, callback);
	rs_io_idle_add_job(job, idle_class, 30, user_data);

	return job;
}

/**
 * Restore tags of a new directory or add tags to a photo
 * @param filename Absolute path to a file to tags to
 * @param tag_id The id of the tag to add.
 * @param auto_tag Is the tag an automatically generated tag
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_add_tag(const gchar *filename, gint tag_id, gboolean auto_tag, gint idle_class)
{
	g_return_val_if_fail(filename != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(filename), NULL);

	init();

	RSIoJob *job = rs_io_job_tagging_new(filename, tag_id, auto_tag);
	rs_io_idle_add_job(job, idle_class, 50, NULL);

	return job;
}

/**
 * Restore tags of a new directory or add tags to a photo
 * @param path Absolute path to a directory to restore tags to
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_restore_tags(const gchar *path, gint idle_class)
{
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(path), NULL);

	init();

	RSIoJob *job = rs_io_job_tagging_new(path, -1, FALSE);
	rs_io_idle_add_job(job, idle_class, 50, NULL);

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
rs_io_lock_real(const gchar *source_file, gint line, const gchar *caller)
{
	RS_DEBUG(LOCKING, "[%s:%d %s()] \033[33mrequesting\033[0m IO lock (thread %p)",
		source_file, line, caller,
		(g_timer_start(io_lock_timer), g_thread_self()));

	/* Each loop tries approx every millisecond, so we wait 10 secs */
	int tries_left = 10*1000;

	while (FALSE == g_rec_mutex_trylock(&io_lock))
	{
		g_usleep(1000);
		if (--tries_left <= 0)
		{
			RS_DEBUG(LOCKING, "[%s:%d %s()] \033[31mIO Lock was not released after \033[36m%.2f\033[0mms\033[0m, ignoring IO lock (thread %p)",
				source_file, line, caller,
				g_timer_elapsed(io_lock_timer, NULL)*1000.0,
				(g_timer_start(io_lock_timer), g_thread_self()));
			return;
		}
	}

	RS_DEBUG(LOCKING, "[%s:%d %s()] \033[32mgot\033[0m IO lock after \033[36m%.2f\033[0mms (thread %p)",
		source_file, line, caller,
		g_timer_elapsed(io_lock_timer, NULL)*1000.0,
		(g_timer_start(io_lock_timer), g_thread_self()));
}

/**
 * Release the IO lock
 */
void
rs_io_unlock_real(const gchar *source_file, gint line, const gchar *caller)
{
	RS_DEBUG(LOCKING, "[%s:%d %s()] releasing IO lock after \033[36m%.2f\033[0mms (thread %p)",
		source_file, line, caller,
		g_timer_elapsed(io_lock_timer, NULL)*1000.0,
		g_thread_self());

	g_rec_mutex_unlock(&io_lock);
}

/**
 * Pause the worker threads
 */
void
rs_io_idle_pause(void)
{
	pause_queue = TRUE;
}

/**
 * Unpause the worker threads
 */
void
rs_io_idle_unpause(void)
{
	pause_queue = FALSE;
}

/**
 * Returns the number of jobs left
 */
gint
rs_io_get_jobs_left(void)
{
	g_mutex_lock(&count_lock);
	gint left = g_async_queue_length(queue) + queue_active_count;
	g_mutex_unlock(&count_lock);
	return left;
}
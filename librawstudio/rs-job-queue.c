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
#include <rawstudio.h>
#include "rs-job-queue.h"

struct _RSJobQueueSlot {
	GtkWidget *container;
	GtkWidget *label;
	GtkWidget *progress;
};

struct _RSJob {
	RSJobFunc func;
	RSJobQueue *job_queue;
	RSJobQueueSlot *slot;
	gpointer data;
	gpointer result;
	gboolean done;
	GCond *done_cond;
	GMutex *done_mutex;
};

struct _RSJobQueue {
	GObject parent;
	gboolean dispose_has_run;

	GMutex *lock;
	GThreadPool *pool;
	gint n_slots;
	GtkWidget *window;
	GtkWidget *box;
};

G_DEFINE_TYPE (RSJobQueue, rs_job_queue, G_TYPE_OBJECT)

static void job_consumer(gpointer data, gpointer unused);

static void
rs_job_queue_dispose (GObject *object)
{
	RSJobQueue *job_queue = RS_JOB_QUEUE(object);

	if (!job_queue->dispose_has_run)
	{
		job_queue->dispose_has_run = TRUE;

		g_mutex_free(job_queue->lock);
	}

	/* Chain up */
	G_OBJECT_CLASS(rs_job_queue_parent_class)->dispose(object);
}

static void
rs_job_queue_class_init (RSJobQueueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_job_queue_dispose;
}

static void
rs_job_queue_init(RSJobQueue *job_queue)
{
	job_queue->dispose_has_run = FALSE;
	job_queue->lock = g_mutex_new();
	job_queue->pool = g_thread_pool_new(((GFunc) job_consumer), NULL, rs_get_number_of_processor_cores(), TRUE, NULL);
	job_queue->n_slots = 0;
	job_queue->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	job_queue->box = gtk_vbox_new(TRUE, 1);

	gtk_container_add(GTK_CONTAINER(job_queue->window), job_queue->box);

	gtk_window_set_accept_focus(GTK_WINDOW(job_queue->window), FALSE);
	gtk_window_set_keep_above(GTK_WINDOW(job_queue->window), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(job_queue->window), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(job_queue->window), TRUE);
	gtk_window_set_title(GTK_WINDOW(job_queue->window), "");
	gtk_window_set_type_hint(GTK_WINDOW(job_queue->window), GDK_WINDOW_TYPE_HINT_NOTIFICATION);

	/* Let's spice it up a notch! :) */
#if GTK_CHECK_VERSION(2,12,0)
	gtk_window_set_opacity(GTK_WINDOW(job_queue->window), 0.75);
#endif

#if GTK_CHECK_VERSION(2,10,0)
	gtk_window_set_deletable(GTK_WINDOW(job_queue->window), FALSE);
#endif

	/* Set the gravity, so that resizes will still result in a window
	 * positioned in the lower left */
	gtk_window_set_gravity(GTK_WINDOW(job_queue->window), GDK_GRAVITY_SOUTH_EAST);

	/* Place the window in lower left corner of screen */
	gtk_window_move(GTK_WINDOW(job_queue->window), 0, gdk_screen_get_height(gdk_display_get_default_screen(gdk_display_get_default()))-50);
}

/**
 * Get a new RSJobQueue
 * @return A new RSJobQueue
 */
static RSJobQueue *
rs_job_queue_new(void)
{
	return g_object_new (RS_TYPE_JOB_QUEUE, NULL);
}

/**
 * Return the RSJobQueue singleton
 * @note THis function should be thread safe
 * @return A RSJobQueue singleton
 */
static RSJobQueue *
rs_job_queue_get_singleton(void)
{
	static RSJobQueue *singleton = NULL;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!singleton)
		singleton = rs_job_queue_new();
	g_static_mutex_unlock(&lock);

	g_assert(RS_IS_JOB_QUEUE(singleton));

	return singleton;
}

/**
 * Add a new processing slot to a RSJobQueue window
 * @param job_queue A RSJobQueue
 * @return A new RSJobQueueSlot
 */
static RSJobQueueSlot *
rs_job_queue_add_slot(RSJobQueue *job_queue)
{
	RSJobQueueSlot *slot = g_new0(RSJobQueueSlot, 1);

	g_mutex_lock(job_queue->lock);
	gdk_threads_enter();

	slot->container = gtk_vbox_new(FALSE, 0);
	slot->progress = gtk_progress_bar_new();
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(slot->progress), "Hello...");
	gtk_box_pack_start(GTK_BOX(slot->container), slot->progress, FALSE, TRUE, 1);

	gtk_box_pack_start(GTK_BOX(job_queue->box), slot->container, FALSE, TRUE, 1);

	/* If we previously got 0 slots open, position the window again */
	if (job_queue->n_slots == 0)
		gtk_window_move(GTK_WINDOW(job_queue->window), 0, gdk_screen_get_height(gdk_display_get_default_screen(gdk_display_get_default()))-50);

	/* For some reason this must be called everytime to trigger correct placement?! */
	gtk_widget_show_all(job_queue->window);

	job_queue->n_slots++;

	gdk_threads_leave();
	g_mutex_unlock(job_queue->lock);

	return slot;
}

/**
 * Remove and frees a RSJobQueueSlot from a RSJobQueue window
 * @param job_queue A RSJobQueue
 * @param slot The slot to remove and free
 */
static void
rs_job_queue_remove_slot(RSJobQueue *job_queue, RSJobQueueSlot *slot)
{
	g_mutex_lock(job_queue->lock);
	gdk_threads_enter();

	gtk_container_remove(GTK_CONTAINER(job_queue->box), slot->container);
	job_queue->n_slots--;

	/* If we got less than 1 slot left, we hide the window, no reason to
	 * show an empty window */
	if (job_queue->n_slots < 1)
		gtk_widget_hide_all(job_queue->window);

	/* We resize the window to 1,1 to make it as small as _possible_ to
	 * avoid blank space after removing a slot */
	gtk_window_resize(GTK_WINDOW(job_queue->window), 1, 1);

	gdk_threads_leave();
	g_mutex_unlock(job_queue->lock);
}

/**
 * A function to consume jobs, this should run in its own thread
 * @note Will never return
 */
static void
job_consumer(gpointer data, gpointer unused)
{
	RSJob *job = (RSJob *) data;
	RSJobQueueSlot *slot = rs_job_queue_add_slot(job->job_queue);
	
	/* Call the job */
	if (!job->done)
		job->result = job->func(slot, job->data);

	rs_job_queue_remove_slot(job->job_queue, slot);
	g_object_unref(job->job_queue);

	if (job->done_cond)
	{
		/* If we take this path, we shouldn't free the job, rs_job_queue_wait()
		 * must take care of that */
		g_mutex_lock(job->done_mutex);
		job->done = TRUE;
		g_cond_signal(job->done_cond);
		g_mutex_unlock(job->done_mutex);
	}
	else
		g_free(job);
}

/**
 * Add a new job to the job queue
 * @note When func is called, it WILL be from another thread, it may be
 *       required to acquire the GDK lock if any GTK+ stuff is done in the
 *       callback!
 * @param func A function to call for performing the job
 * @param data Data to pass to func
 * @param waitable If TRUE, rs_job_queue_wait() will wait until completion
 */
RSJob *
rs_job_queue_add_job(RSJobFunc func, gpointer data, gboolean waitable)
{
	RSJobQueue *job_queue = rs_job_queue_get_singleton();

    g_assert(func != NULL);

	g_mutex_lock(job_queue->lock);

	RSJob *job = g_new0(RSJob, 1);
    job->func = func;
	job->job_queue = g_object_ref(job_queue);
    job->data = data;
    job->done = FALSE;

	if (waitable)
    {
        job->done_cond = g_cond_new();
        job->done_mutex = g_mutex_new();
    }
    else
    {
        job->done_cond = NULL;
        job->done_mutex = NULL;
    }

    g_thread_pool_push(job_queue->pool, job, NULL);
	g_mutex_unlock(job_queue->lock);

	return job;
}

/**
 * Wait (hang) until a job is finished and then free the memory allocated to job
 * @param job The RSJob to wait for
 * @return The value returned by the func given to rs_job_queue_add()
 */
gpointer
rs_job_queue_wait(RSJob *job)
{
	gpointer result = NULL;

	g_assert(job != NULL);
	g_assert(job->done_cond != NULL);
	g_assert(job->done_mutex != NULL);

	/* Wait for it */
	g_mutex_lock(job->done_mutex);
	while(!job->done)
		g_cond_wait(job->done_cond, job->done_mutex);
	g_mutex_unlock(job->done_mutex);

	/* Free everything */
	g_cond_free(job->done_cond);
	g_mutex_free(job->done_mutex);
	g_free(job);

	result = job->result;

	return result;
}

/**
 * Update the job description
 * @note You should NOT have aquired the GDK thread lock when calling this
 *       function.
 * @param slot A job_slot as recieved in the job callback function
 * @param description The new description or NULL to show nothing
 */
void
rs_job_update_description(RSJobQueueSlot *slot, const gchar *description)
{
	gdk_threads_enter();

	if (description)
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(slot->progress), description);
	else
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(slot->progress), "");

	gdk_threads_leave();
}

/**
 * Update the job progress bar
 * @note You should NOT have aquired the GDK thread lock when calling this
 *       function.
 * @param slot A job_slot as recieved in the job callback function
 * @param fraction A value between 0.0 and 1.0 to set the progress bar at
 *                 the specific fraction or -1.0 to pulse the progress bar.
 */
void
rs_job_update_progress(RSJobQueueSlot *slot, const gdouble fraction)
{
	gdk_threads_enter();

	if (fraction < 0.0)
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(slot->progress));
	else
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(slot->progress), fraction);

	gdk_threads_leave();
}

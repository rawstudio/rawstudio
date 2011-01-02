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

#ifndef RS_JOB_QUEUE_H
#define RS_JOB_QUEUE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_JOB_QUEUE rs_job_queue_get_type()
#define RS_JOB_QUEUE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_JOB_QUEUE, RSJobQueue))
#define RS_JOB_QUEUE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_JOB_QUEUE, RSJobQueueClass))
#define RS_IS_JOB_QUEUE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_JOB_QUEUE))
#define RS_IS_JOB_QUEUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_JOB_QUEUE))
#define RS_JOB_QUEUE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_JOB_QUEUE, RSJobQueueClass))

typedef struct _RSJobQueueSlot RSJobQueueSlot;
typedef struct _RSJob RSJob;
typedef struct _RSJobQueue RSJobQueue;

typedef struct {
	GObjectClass parent_class;
} RSJobQueueClass;

typedef gpointer (*RSJobFunc)(RSJobQueueSlot *, gpointer);

GType rs_job_queue_get_type(void);

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
rs_job_queue_add_job(RSJobFunc func, gpointer data, gboolean waitable);

/**
 * Wait (hang) until a job is finished and then free the memory allocated to job
 * @param job The RSJob to wait for
 * @return The value returned by the func given to rs_job_queue_add()
 */
gpointer
rs_job_queue_wait(RSJob *job);

/**
 * Update the job description
 * @note You should NOT have aquired the GDK thread lock when calling this
 *       function.
 * @param slot A job_slot as recieved in the job callback function
 * @param description The new description or NULL to show nothing
 */
void
rs_job_update_description(RSJobQueueSlot *slot, const gchar *description);

/**
 * Update the job progress bar
 * @note You should NOT have aquired the GDK thread lock when calling this
 *       function.
 * @param slot A job_slot as recieved in the job callback function
 * @param fraction A value between 0.0 and 1.0 to set the progress bar at
 *                 the specific fraction or -1.0 to pulse the progress bar.
 */
void
rs_job_update_progress(RSJobQueueSlot *slot, const gdouble fraction);

G_END_DECLS

#endif /* RS_JOB_QUEUE_H */

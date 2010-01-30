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

#ifndef RS_IO_JOB_H
#define RS_IO_JOB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_IO_JOB rs_io_job_get_type()
#define RS_IO_JOB(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_IO_JOB, RSIoJob))
#define RS_IO_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_IO_JOB, RSIoJobClass))
#define RS_IS_IO_JOB(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_IO_JOB))
#define RS_IS_IO_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_IO_JOB))
#define RS_IO_JOB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_IO_JOB, RSIoJobClass))

typedef struct {
	GObject parent;

	gint idle_class;
	gint priority;
	gpointer user_data;
} RSIoJob;

typedef struct {
	GObjectClass parent_class;

	void (*execute)(RSIoJob *job);
	void (*do_callback)(RSIoJob *job);
} RSIoJobClass;

GType rs_io_job_get_type(void);

RSIoJob *rs_io_job_new(void);

void rs_io_job_execute(RSIoJob *job);

void rs_io_job_do_callback(RSIoJob *job);

G_END_DECLS

#endif /* RS_IO_JOB_H */

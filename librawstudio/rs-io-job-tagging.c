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

#include "rs-io-job-tagging.h"
#include "rawstudio.h"

typedef struct {
	RSIoJob parent;
	gboolean dispose_has_run;

	gchar *path;
} RSIoJobTagging;

G_DEFINE_TYPE(RSIoJobTagging, rs_io_job_tagging, RS_TYPE_IO_JOB)

static void
execute(RSIoJob *job)
{
	RSIoJobTagging *tagging = RS_IO_JOB_TAGGING(job);

	rs_library_restore_tags(tagging->path);
}

static void
rs_io_job_tagging_dispose(GObject *object)
{
	RSIoJobTagging *tagging = RS_IO_JOB_TAGGING(object);
	if (!tagging->dispose_has_run)
	{
		tagging->dispose_has_run = TRUE;

		g_free(tagging->path);
	}
	G_OBJECT_CLASS(rs_io_job_tagging_parent_class)->dispose(object);
}

static void
rs_io_job_tagging_class_init(RSIoJobTaggingClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	RSIoJobClass *job_class = RS_IO_JOB_CLASS(klass);

	object_class->dispose = rs_io_job_tagging_dispose;
	job_class->execute = execute;
}

static void
rs_io_job_tagging_init(RSIoJobTagging *tagging)
{
}

RSIoJob *
rs_io_job_tagging_new(const gchar *path)
{
	RSIoJobTagging *tagging = g_object_new (RS_TYPE_IO_JOB_TAGGING, NULL);

	tagging->path = g_strdup(path);

	return RS_IO_JOB(tagging);
}

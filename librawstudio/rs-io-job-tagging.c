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

#include "rs-io-job-tagging.h"
#include "rawstudio.h"

typedef struct {
	RSIoJob parent;
	gboolean dispose_has_run;

	gchar *path;
	gint tag_id;
	gboolean autotag;
} RSIoJobTagging;

G_DEFINE_TYPE(RSIoJobTagging, rs_io_job_tagging, RS_TYPE_IO_JOB)
static RSLibrary *library;

static void
execute(RSIoJob *job)
{
	RSIoJobTagging *tagging = RS_IO_JOB_TAGGING(job);

	if (tagging->tag_id == -2)
	{
		rs_library_backup_tags(library,tagging->path);
	}
	else if (tagging->tag_id == -1)
	{
		rs_library_restore_tags(tagging->path);
	}
	else
	{
		rs_library_photo_add_tag(library, tagging->path, tagging->tag_id, tagging->autotag);
	}
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
	if (!library)
		library = rs_library_get_singleton();
}

RSIoJob *
rs_io_job_tagging_new(const gchar *path, gint tag_id, gboolean autotag)
{
	RSIoJobTagging *tagging = g_object_new (RS_TYPE_IO_JOB_TAGGING, NULL);

	tagging->path = g_strdup(path);
	tagging->tag_id = tag_id;
	tagging->autotag = autotag;

	return RS_IO_JOB(tagging);
}

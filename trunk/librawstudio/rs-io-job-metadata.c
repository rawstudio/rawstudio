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

#include "rs-io-job-metadata.h"
#include "rawstudio.h"

typedef struct {
	RSIoJob parent;
	gboolean dispose_has_run;

	gchar *path;
	RSMetadata *metadata;
	RSGotMetadataCB callback;
} RSIoJobMetadata;

G_DEFINE_TYPE(RSIoJobMetadata, rs_io_job_metadata, RS_TYPE_IO_JOB)

static void
execute(RSIoJob *job)
{
	RSIoJobMetadata *metadata = RS_IO_JOB_METADATA(job);

	/* Don't lock IO, while reading metadata - filesizes too small for it to have any practical impact. */
	metadata->metadata = rs_metadata_new_from_file(metadata->path);
}

static void
do_callback(RSIoJob *job)
{
	RSIoJobMetadata *metadata = RS_IO_JOB_METADATA(job);

	if (metadata->callback && metadata->metadata)
		metadata->callback(metadata->metadata, job->user_data);
}

static void
rs_io_job_metadata_dispose(GObject *object)
{
	RSIoJobMetadata *metadata = RS_IO_JOB_METADATA(object);
	if (!metadata->dispose_has_run)
	{
		metadata->dispose_has_run = TRUE;

		g_free(metadata->path);
		g_object_unref(metadata->metadata);
	}
	G_OBJECT_CLASS(rs_io_job_metadata_parent_class)->dispose(object);
}

static void
rs_io_job_metadata_class_init(RSIoJobMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	RSIoJobClass *job_class = RS_IO_JOB_CLASS(klass);

	object_class->dispose = rs_io_job_metadata_dispose;
	job_class->execute = execute;
	job_class->do_callback = do_callback;
}

static void
rs_io_job_metadata_init(RSIoJobMetadata *metadata)
{
}

RSIoJob *
rs_io_job_metadata_new(const gchar *path, RSGotMetadataCB callback)
{
	RSIoJobMetadata *metadata = g_object_new (RS_TYPE_IO_JOB_METADATA, NULL);

	metadata->path = g_strdup(path);
	metadata->callback = callback;

	return RS_IO_JOB(metadata);
}

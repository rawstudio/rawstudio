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

#include "rs-io-job-checksum.h"
#include "rawstudio.h"

typedef struct {
	RSIoJob parent;
	gboolean dispose_has_run;

	gchar *path;
	gchar *checksum;
	RSGotChecksumCB callback;
} RSIoJobChecksum;

G_DEFINE_TYPE(RSIoJobChecksum, rs_io_job_checksum, RS_TYPE_IO_JOB)

static void
execute(RSIoJob *job)
{
	RSIoJobChecksum *checksum = RS_IO_JOB_CHECKSUM(job);

	rs_io_lock();
	checksum->checksum = rs_file_checksum(checksum->path);
	rs_io_unlock();
}

static void
do_callback(RSIoJob *job)
{
	RSIoJobChecksum *checksum = RS_IO_JOB_CHECKSUM(job);

	if (checksum->callback && checksum->checksum)
		checksum->callback(checksum->checksum, job->user_data);
}

static void
rs_io_job_checksum_dispose(GObject *object)
{
	RSIoJobChecksum *checksum = RS_IO_JOB_CHECKSUM(object);
	if (!checksum->dispose_has_run)
	{
		checksum->dispose_has_run = TRUE;

		g_free(checksum->path);
		g_free(checksum->checksum);
	}
	G_OBJECT_CLASS (rs_io_job_checksum_parent_class)->dispose(object);
}

static void
rs_io_job_checksum_class_init(RSIoJobChecksumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	RSIoJobClass *job_class = RS_IO_JOB_CLASS(klass);

	object_class->dispose = rs_io_job_checksum_dispose;
	job_class->execute = execute;
	job_class->do_callback = do_callback;
}

static void
rs_io_job_checksum_init(RSIoJobChecksum *checksum)
{
}

RSIoJob *
rs_io_job_checksum_new(const gchar *path, RSGotChecksumCB callback)
{
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(path), NULL);

	RSIoJobChecksum *checksum = g_object_new(RS_TYPE_IO_JOB_CHECKSUM, NULL);

	checksum->path = g_strdup(path);
	checksum->callback = callback;

	return RS_IO_JOB(checksum);
}

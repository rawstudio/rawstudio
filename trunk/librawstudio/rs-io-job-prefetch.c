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

/* readahead() on Linux */
#if __gnu_linux__
#define _GNU_SOURCE
#endif /* __gnu_linux__ */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "rs-io.h"
#include "rs-io-job-prefetch.h"

typedef struct {
	RSIoJob parent;
	gboolean dispose_has_run;

	gchar *path;
} RSIoJobPrefetch;

G_DEFINE_TYPE(RSIoJobPrefetch, rs_io_job_prefetch, RS_TYPE_IO_JOB)

static void
execute(RSIoJob *job)
{
	gint fd;
	struct stat st;
	RSIoJobPrefetch *prefetch = RS_IO_JOB_PREFETCH(job);

	stat(prefetch->path, &st);
	if (st.st_size > 0)
	{
		fd = open(prefetch->path, O_RDONLY);
		if (fd > 0)
		{
			gint bytes_read = 0;
#if __gnu_linux__
			while(bytes_read < st.st_size)
			{
				rs_io_lock();
				gint length = MIN(st.st_size-bytes_read, 1024*1024);
				readahead(fd, bytes_read, length);
				bytes_read += length;
				rs_io_unlock();
				// Sleep 5ms
				g_usleep(5000);
			}
#else
			gchar *tmp = g_new(gchar, st.st_size);

			while(bytes_read < st.st_size)
			{
				rs_io_lock();
				bytes_read += read(fd, tmp+bytes_read, MIN(st.st_size-bytes_read, 1024*1024));
				rs_io_unlock();
			}

			g_free(tmp);
#endif /* __gnu_linux__ */
			}
			close(fd);
		}
}

static void
rs_io_job_prefetch_dispose(GObject *object)
{
	RSIoJobPrefetch *prefetch = RS_IO_JOB_PREFETCH(object);
	if (!prefetch->dispose_has_run)
	{
		prefetch->dispose_has_run = TRUE;

		g_free(prefetch->path);
	}
	G_OBJECT_CLASS(rs_io_job_prefetch_parent_class)->dispose(object);
}

static void
rs_io_job_prefetch_class_init(RSIoJobPrefetchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	RSIoJobClass *job_class = RS_IO_JOB_CLASS(klass);

	object_class->dispose = rs_io_job_prefetch_dispose;
	job_class->execute = execute;
}

static void
rs_io_job_prefetch_init(RSIoJobPrefetch *prefetch)
{
}

RSIoJob *
rs_io_job_prefetch_new(const gchar *path)
{
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_path_is_absolute(path), NULL);

	RSIoJobPrefetch *prefetch = g_object_new(RS_TYPE_IO_JOB_PREFETCH, NULL);

	prefetch->path = g_strdup(path);

	return RS_IO_JOB(prefetch);
}

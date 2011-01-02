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

#include "rs-io-job.h"

G_DEFINE_TYPE(RSIoJob, rs_io_job, G_TYPE_OBJECT)

static void
rs_io_job_class_init(RSIoJobClass *klass)
{
}

static void
rs_io_job_init(RSIoJob *job)
{
}

RSIoJob *
rs_io_job_new(void)
{
	return g_object_new(RS_TYPE_IO_JOB, NULL);
}

void
rs_io_job_execute(RSIoJob *job)
{
	g_assert(RS_IS_IO_JOB(job));

	RSIoJobClass *klass = RS_IO_JOB_GET_CLASS(job);

	if (klass->execute)
		klass->execute(job);
}

void
rs_io_job_do_callback(RSIoJob *job)
{
	g_assert(RS_IS_IO_JOB(job));

	RSIoJobClass *klass = RS_IO_JOB_GET_CLASS(job);

	if (klass->do_callback)
		klass->do_callback(job);
}

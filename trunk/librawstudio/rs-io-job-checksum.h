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

#ifndef RS_IO_JOB_CHECKSUM_H
#define RS_IO_JOB_CHECKSUM_H

#include "rs-io-job.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_IO_JOB_CHECKSUM rs_io_job_checksum_get_type()
#define RS_IO_JOB_CHECKSUM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_IO_JOB_CHECKSUM, RSIoJobChecksum))
#define RS_IO_JOB_CHECKSUM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_IO_JOB_CHECKSUM, RSIoJobChecksumClass))
#define RS_IS_IO_JOB_CHECKSUM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_IO_JOB_CHECKSUM))
#define RS_IS_IO_JOB_CHECKSUM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_IO_JOB_CHECKSUM))
#define RS_IO_JOB_CHECKSUM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_IO_JOB_CHECKSUM, RSIoJobChecksumClass))

typedef void (*RSGotChecksumCB)(const gchar *checksum, gpointer user_data);

typedef struct {
	RSIoJobClass parent_class;
} RSIoJobChecksumClass;

GType rs_io_job_checksum_get_type(void);

RSIoJob *rs_io_job_checksum_new(const gchar *path, RSGotChecksumCB callback);

G_END_DECLS

#endif /* RS_IO_JOB_CHECKSUM_H */

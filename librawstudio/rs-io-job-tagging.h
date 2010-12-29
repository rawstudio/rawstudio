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

#ifndef RS_IO_JOB_TAGGING_H
#define RS_IO_JOB_TAGGING_H

#include "rs-types.h"
#include "rs-io-job.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_IO_JOB_TAGGING rs_io_job_tagging_get_type()
#define RS_IO_JOB_TAGGING(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_IO_JOB_TAGGING, RSIoJobTagging))
#define RS_IO_JOB_TAGGING_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_IO_JOB_TAGGING, RSIoJobTaggingClass))
#define RS_IS_IO_JOB_TAGGING(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_IO_JOB_TAGGING))
#define RS_IS_IO_JOB_TAGGING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_IO_JOB_TAGGING))
#define RS_IO_JOB_TAGGING_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_IO_JOB_TAGGING, RSIoJobTaggingClass))

typedef struct {
	RSIoJobClass parent_class;
} RSIoJobTaggingClass;

GType rs_io_job_tagging_get_type(void);

/* Do delayed loading of tags, or add tags to an image */
/* To load tagging data delayed set tag_id to -1 */
/* To backup tagging data delayed set tag_id to -2 */
/* To add a tag to an image, provide the image name as path and set the tag_id */
RSIoJob *rs_io_job_tagging_new(const gchar *path, gint tag_id, gboolean autotag);

G_END_DECLS

#endif /* RS_IO_JOB_TAGGING_H */

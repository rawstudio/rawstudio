/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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

#ifndef RS_JOB_H
#define RS_JOB_H

#include <glib.h>
#include "rawstudio.h"

typedef struct _RS_JOB RS_JOB;

/**
 * Cancels a job. If the job is in queue it will be removed. If the job is
 * currently running, the consumer thread will be signalled to abort
 * @param job A RS_JOB
 */
extern void
rs_job_cancel(RS_JOB *job);

/**
 * Adds a new demosaic job
 * @param image An RS_IMAGE16 to demosaic
 * @return A new RS_JOB, this should NOT be freed by caller
 */
extern RS_JOB *
rs_job_add_demosaic(RS_IMAGE16 *image);

/**
 * Adds a new sharpen job
 * @param in An RS_IMAGE16 to sharpen
 * @param out An RS_IMAGE16 for output after sharpen
 * @return A new RS_JOB, this should NOT be freed by caller
 */
extern RS_JOB *
rs_job_add_sharpen(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble amount);

/**
 * Adds a new render job
 * @param in An input image
 * @param out An output image
 * @param rct A color transform to use for the render
 */
RS_JOB *
rs_job_add_render(RS_IMAGE16 *in, GdkPixbuf *out, RS_COLOR_TRANSFORM *rct);

#endif /* RS_JOB_H */

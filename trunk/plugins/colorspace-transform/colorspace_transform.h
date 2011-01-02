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

/* Plugin tmpl version 5 */

#include <rawstudio.h>
#include <lcms.h>
#include "rs-cmm.h"

#define RS_TYPE_COLORSPACE_TRANSFORM (rs_colorspace_transform_type)
#define RS_COLORSPACE_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_COLORSPACE_TRANSFORM, RSColorspaceTransform))
#define RS_COLORSPACE_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_COLORSPACE_TRANSFORM, RSColorspaceTransformClass))
#define RS_IS_COLORSPACE_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_COLORSPACE_TRANSFORM))

typedef struct _RSColorspaceTransform RSColorspaceTransform;
typedef struct _RSColorspaceTransformClass RSColorspaceTransformClass;

typedef struct {
	RSColorspaceTransform *cst;
	GThread *threadid;
	gint start_x;
	gint start_y;
	gint end_x;
	gint end_y;
	RS_IMAGE16 *input;
	void *output;
	RSColorSpace *input_space;
	RSColorSpace *output_space;
	RS_MATRIX3 *matrix;
	gboolean gamma_correct;
	guchar* table8;
	gfloat output_gamma;
	GCond* run_transform;
	GMutex* run_transform_mutex;
	GCond* transform_finished;
	GMutex* transform_finished_mutex;
	gboolean do_run_transform;
} ThreadInfo;

/* SSE2 optimized functions */
void transform8_srgb_sse2(ThreadInfo* t);
void transform8_otherrgb_sse2(ThreadInfo* t);
gboolean cst_has_sse2();

/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and
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

#ifndef ___dcp_h_included___
#define ___dcp_h_included___

#include "config.h"
#include <rawstudio.h>
#define RS_TYPE_DCP (rs_dcp_type)
#define RS_DCP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DCP, RSDcp))
#define RS_DCP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DCP, RSDcpClass))
#define RS_IS_DCP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DCP))
#define RS_DCP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_DCP, RSDcpClass))

typedef struct _RSDcp RSDcp;
typedef struct _RSDcpClass RSDcpClass;

typedef struct {
	//Precalc:
	gfloat hScale[4] __attribute__ ((aligned (16)));
	gfloat sScale[4] __attribute__ ((aligned (16)));
	gfloat vScale[4] __attribute__ ((aligned (16)));
	gint maxHueIndex0[4] __attribute__ ((aligned (16)));
	gint maxSatIndex0[4] __attribute__ ((aligned (16)));
	gint maxValIndex0[4] __attribute__ ((aligned (16)));
	gint hueStep[4] __attribute__ ((aligned (16)));
	gint valStep[4] __attribute__ ((aligned (16)));
} PrecalcHSM;


struct _RSDcp {
	RSFilter parent;

	gfloat exposure;
	gfloat saturation;
	gfloat contrast;
	gfloat hue;

	RS_xy_COORD white_xy;

	gint nknots;
	gfloat *curve_samples;

	gfloat temp1;
	gfloat temp2;

	RSSpline *tone_curve;
	gfloat *tone_curve_lut;

	gboolean has_color_matrix1;
	gboolean has_color_matrix2;
	RS_MATRIX3 color_matrix1;
	RS_MATRIX3 color_matrix2;

	gboolean has_forward_matrix1;
	gboolean has_forward_matrix2;
	RS_MATRIX3 forward_matrix1;
	RS_MATRIX3 forward_matrix2;
	RS_MATRIX3 forward_matrix;

	RSHuesatMap *looktable;

	RSHuesatMap *huesatmap;
	RSHuesatMap *huesatmap1;
	RSHuesatMap *huesatmap2;
	RSHuesatMap *huesatmap_interpolated;

	RS_MATRIX3 camera_to_pcs;

	RS_VECTOR3 camera_white;
	RS_MATRIX3 camera_to_prophoto;

	gfloat exposure_slope;
	gfloat exposure_black;
	gfloat exposure_radius;
	gfloat exposure_qscale;

	PrecalcHSM huesatmap_precalc;
	PrecalcHSM looktable_precalc;
};

struct _RSDcpClass {
	RSFilterClass parent_class;
	RSColorSpace *prophoto;
	RSIccProfile *prophoto_profile;
};

typedef struct {
	RSDcp *dcp;
	GThread *threadid;
	gint start_x;
	gint start_y;
	gint end_y;
	RS_IMAGE16 *tmp;

} ThreadInfo;

gboolean render_SSE2(ThreadInfo* t);
void calc_hsm_constants(const RSHuesatMap *map, PrecalcHSM* table); 

#endif //___dcp_h_included___
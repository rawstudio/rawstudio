/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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

#ifndef RS_COLOR_TRANSFORM_H
#define RS_COLOR_TRANSFORM_H

#include "rawstudio.h"
#include "matrix.h"

#define COLOR_TRANSFORM(transform) void (transform) \
(RS_COLOR_TRANSFORM *rct, \
		gint width, gint height, \
		gushort *in, gint in_rowstride, \
		void *out, gint out_rowstride)

/* Public */
typedef struct _RS_COLOR_TRANSFORM_PRIVATE RS_COLOR_TRANSFORM_PRIVATE;

struct _RS_COLOR_TRANSFORM {
	RS_COLOR_TRANSFORM_PRIVATE *priv;
	COLOR_TRANSFORM(*transform);
};

extern RS_COLOR_TRANSFORM *rs_color_transform_new();
extern void rs_color_transform_free(RS_COLOR_TRANSFORM *rct);
extern gboolean rs_color_transform_set_gamma(RS_COLOR_TRANSFORM *rct, gdouble gamma);
extern gboolean rs_color_transform_set_contrast(RS_COLOR_TRANSFORM *rct, gdouble contrast);
extern gboolean rs_color_transform_set_premul(RS_COLOR_TRANSFORM *rct, gfloat *premul);
extern gboolean rs_color_transform_set_matrix(RS_COLOR_TRANSFORM *rct, RS_MATRIX4 *matrix);
void rs_color_transform_set_from_settings(RS_COLOR_TRANSFORM *rct, RS_SETTINGS_DOUBLE *settings, guint mask);
extern gboolean rs_color_transform_set_curve(RS_COLOR_TRANSFORM *rct, gfloat *curve);
extern void rs_color_transform_set_all(RS_COLOR_TRANSFORM *rct, gdouble gamma,
	gdouble contrast, gfloat *premul, RS_MATRIX4 *matrix, gfloat *curve);
extern void rs_color_transform_set_from_photo(RS_COLOR_TRANSFORM *rct, RS_PHOTO *photo, gint snapshot);
extern gboolean rs_color_transform_set_output_format(RS_COLOR_TRANSFORM *rct, guint bits_per_color);
extern void rs_color_transform_set_cms_transform(RS_COLOR_TRANSFORM *rct, void *transform);
extern void rs_color_transform_make_histogram(RS_COLOR_TRANSFORM *rct, RS_IMAGE16 *input, guint histogram[3][256]);

extern COLOR_TRANSFORM(*transform_nocms8);
extern COLOR_TRANSFORM(*transform_cms8);
extern COLOR_TRANSFORM(transform_nocms_c);
extern COLOR_TRANSFORM(transform_cms_c);

#if defined (__i386__) || defined (__x86_64__)
extern COLOR_TRANSFORM(transform_nocms8_sse);
extern COLOR_TRANSFORM(transform_nocms8_3dnow);
extern COLOR_TRANSFORM(transform_cms8_sse);
extern COLOR_TRANSFORM(transform_cms8_3dnow);
#endif /* __i386__ || __x86_64__ */

#endif /* RS_COLOR_TRANSFORM_H */

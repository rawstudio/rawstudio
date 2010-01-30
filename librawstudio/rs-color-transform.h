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

#ifndef RS_COLOR_TRANSFORM_H
#define RS_COLOR_TRANSFORM_H

#include <glib-object.h>
#include "rs-math.h"

#define RS_TYPE_COLOR_TRANSFORM rs_color_transform_get_type()
#define RS_COLOR_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_COLOR_TRANSFORM, RSColorTransform))
#define RS_COLOR_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_COLOR_TRANSFORM, RSColorTransformClass))
#define RS_IS_COLOR_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_COLOR_TRANSFORM))
#define RS_IS_COLOR_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_COLOR_TRANSFORM))
#define RS_COLOR_TRANSFORM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_COLOR_TRANSFORM, RSColorTransformClass))

/* RSColorTransform typedef'ed in application.h */

typedef struct _RSColorTransformClass {
  GObjectClass parent_class;
} RSColorTransformClass;

GType rs_color_transform_get_type (void);

#define COLOR_TRANSFORM(transform) void (transform) \
(RSColorTransform *rct, \
		gint width, gint height, \
		gushort *in, gint in_rowstride, \
		void *out, gint out_rowstride)

extern RSColorTransform *rs_color_transform_new();
extern COLOR_TRANSFORM(rs_color_transform_transform);
extern gboolean rs_color_transform_set_gamma(RSColorTransform *rct, gdouble gamma);
extern gboolean rs_color_transform_set_contrast(RSColorTransform *rct, gdouble contrast);
extern gboolean rs_color_transform_set_premul(RSColorTransform *rct, gfloat *premul);
extern gboolean rs_color_transform_set_matrix(RSColorTransform *rct, RS_MATRIX4 *matrix);
void rs_color_transform_set_from_settings(RSColorTransform *rct, RSSettings *settings, const RSSettingsMask mask);
extern gboolean rs_color_transform_set_curve(RSColorTransform *rct, gfloat *curve);
extern void rs_color_transform_set_all(RSColorTransform *rct, gdouble gamma,
	gdouble contrast, gfloat *premul, RS_MATRIX4 *matrix, gfloat *curve);
extern gboolean rs_color_transform_set_output_format(RSColorTransform *rct, guint bits_per_color);
extern void rs_color_transform_set_cms_transform(RSColorTransform *rct, void *transform);
extern void rs_color_transform_set_adobe_matrix(RSColorTransform *rct, RS_MATRIX4 *matrix);
extern void rs_color_transform_make_histogram(RSColorTransform *rct, RS_IMAGE16 *input, guint histogram[3][256]);

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

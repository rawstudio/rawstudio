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

#ifndef RS_RENDER_H
#define RS_RENDER_H

#include "rs-arch.h"
#include "matrix.h"

void rs_render_select(gboolean cms);
void rs_render_previewtable(const gdouble contrast, gfloat *curve, guchar *table8, gushort *table16);

#define DEFINE_RENDER(func) \
void (func) \
(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gint width, gint height, gushort *in, \
 gint in_rowstride, guchar *out, gint out_rowstride, \
 void *profile)

#define DECL_RENDER(func) \
extern DEFINE_RENDER(func)

#define DEFINE_RENDER16(func) \
void (func) \
(RS_MATRIX4 *matrix, gfloat *pre_mul, gushort *table16, gint width, gint height, gushort *in, \
 gint in_rowstride, gushort *out, gint out_rowstride, \
 void *profile)

#define DECL_RENDER16(func) \
extern DEFINE_RENDER16(func)

/* Main renderer - initialized by rs_render_select */
DECL_RENDER(*rs_render);
DECL_RENDER16(*rs_render16);

/* Sub type renderer - don't use directly, the right one will be binded to
 * rs_render */
DECL_RENDER(*rs_render_cms) __rs_optimized;
DECL_RENDER(*rs_render_nocms) __rs_optimized;
DECL_RENDER16(*rs_render16_cms);
DECL_RENDER16(*rs_render16_nocms);

/* Default C implementations */
DECL_RENDER(rs_render_cms_c);
DECL_RENDER(rs_render_nocms_c);
DECL_RENDER16(rs_render16_cms_c);
DECL_RENDER16(rs_render16_nocms_c);

/* Optimized renderers */
#if defined (__i386__) || defined (__x86_64__)
DECL_RENDER(rs_render_cms_sse);
DECL_RENDER(rs_render_cms_3dnow);
DECL_RENDER(rs_render_nocms_sse);
DECL_RENDER(rs_render_nocms_3dnow);
#endif

/* Histogram renderer */
extern void
(*rs_render_histogram_table)(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, RS_IMAGE16 *input, guint *output) __rs_optimized;

extern void rs_render_histogram_table_c(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, RS_IMAGE16 *input, guint *output);

/* Pixel renderer -  initialized by rs_render_select */
extern void
(*rs_render_pixel)(RS_MATRIX4 *matrix, gfloat *pre_mul, guchar *table, gushort *table16, gushort *in, guchar *out, void *profile);

#endif

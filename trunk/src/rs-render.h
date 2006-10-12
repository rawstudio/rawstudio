/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

void rs_render_select(gboolean cms);
void rs_render_previewtable(const double contrast);

#define DEFINE_RENDER(func) \
void (func) \
(RS_PHOTO *photo, gint width, gint height, gushort *in, \
 gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, \
 void *profile)

#define DECL_RENDER(func) \
extern void (func) \
(RS_PHOTO *photo, gint width, gint height, gushort *in, \
 gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, \
 void *profile)

#define DEFINE_RENDER16(func) \
void (func) \
(RS_PHOTO *photo, gint width, gint height, gushort *in, \
 gint in_rowstride, gint in_channels, gushort *out, gint out_rowstride, \
 void *profile)

#define DECL_RENDER16(func) \
extern void (func) \
(RS_PHOTO *photo, gint width, gint height, gushort *in, \
 gint in_rowstride, gint in_channels, gushort *out, gint out_rowstride, \
 void *profile)

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
(*rs_render_histogram_table)(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table) __rs_optimized;

extern void rs_render_histogram_table_c(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table);

#if defined (__i386__) || defined (__x86_64__)
extern void rs_render_histogram_table_cmov(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table);
#endif

/* Pixel renderer -  initialized by rs_render_select */
extern void
(*rs_render_pixel)(RS_PHOTO *photo, gushort *in, guchar *out, void *profile);

#endif

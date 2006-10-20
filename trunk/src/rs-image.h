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

#define rs_image16_scale(in, out, scale) rs_image16_scale_double(in, out, scale)

#define IS_PIXEL_WITHIN_IMAGE(image, x, y) (((x)<(image)->w)&&((y)<(image)->h))
#define IS_RECT_WITHIN_IMAGE(image, rect) \
(IS_PIXEL_WITHIN_IMAGE(image, rect->x1, rect->y1) \
&&IS_PIXEL_WITHIN_IMAGE(image, rect->x2, rect->y2))

RS_IMAGE16 *rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image16_free(RS_IMAGE16 *rsi);
RS_IMAGE8 *rs_image8_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image8_free(RS_IMAGE8 *rsi);
void rs_image16_orientation(RS_IMAGE16 *rsi, gint orientation);
RS_IMAGE16 *rs_image16_affine(RS_IMAGE16 *in, RS_IMAGE16 *out, RS_MATRIX3 *affine);
RS_IMAGE16 *rs_image16_scale_double(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale);
RS_IMAGE16 *rs_image16_copy(RS_IMAGE16 *rsi);
void rs_image16_crop(RS_IMAGE16 **rsi, RS_RECT *rect);
void rs_image16_uncrop(RS_IMAGE16 **image);
gboolean rs_image16_8_cmp_size(RS_IMAGE16 *a, RS_IMAGE8 *b);

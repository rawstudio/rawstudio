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

#ifndef RS_COLOR_H
#define RS_COLOR_H

extern const RS_MATRIX3 PCStoProPhoto;
extern const RS_XYZ_VECTOR XYZ_WP_D50;

extern void rs_color_whitepoint_to_temp(const RS_xy_COORD *xy, gfloat *temp, gfloat *tint);
extern RS_xy_COORD rs_color_temp_to_whitepoint(gfloat temp, gfloat tint);
extern RS_XYZ_VECTOR xy_to_XYZ(const RS_xy_COORD *xy);
extern RS_xy_COORD XYZ_to_xy(const RS_XYZ_VECTOR *XYZ);
extern RS_MATRIX3 rs_calculate_map_white_matrix(const RS_xy_COORD *from_xy, const RS_xy_COORD *to_xy);

#endif /* RS_COLOR_H */

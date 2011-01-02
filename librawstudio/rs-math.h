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

#ifndef RS_MATRIX_H
#define RS_MATRIX_H

#include "rs-types.h"

#define MATRIX_RESOLUTION (11) /* defined in bits! */
#define MATRIX_RESOLUTION_ROUNDER (1024) /* Half of fixed point precision */

extern void printmat3(RS_MATRIX3 *mat);
extern void printmat(RS_MATRIX4 *mat);
extern void printvec(const char *str, const RS_VECTOR3 *vec);
extern float vector3_max(const RS_VECTOR3 *vec);
extern RS_MATRIX3 vector3_as_diagonal(const RS_VECTOR3 *vec);
extern void matrix4_identity (RS_MATRIX4 *matrix);
extern void matrix4_multiply(const RS_MATRIX4 *left, RS_MATRIX4 *right, RS_MATRIX4 *result);
void matrix4_color_invert(const RS_MATRIX4 *in, RS_MATRIX4 *out);
extern void matrix4_to_matrix4int(RS_MATRIX4 *matrix, RS_MATRIX4Int *matrixi);
extern void matrix4_color_normalize(RS_MATRIX4 *mat);
extern void matrix4_color_saturate(RS_MATRIX4 *mat, double sat);
extern void matrix4_color_hue(RS_MATRIX4 *mat, double rot);
extern void matrix4_color_exposure(RS_MATRIX4 *mat, double exp);
extern void matrix3_to_matrix3int(RS_MATRIX3 *matrix, RS_MATRIX3Int *matrixi);
extern void matrix3_identity (RS_MATRIX3 *matrix);
extern RS_MATRIX3 matrix3_invert(const RS_MATRIX3 *matrix);
extern void matrix3_multiply(const RS_MATRIX3 *left, const RS_MATRIX3 *right, RS_MATRIX3 *result);
extern RS_VECTOR3 vector3_multiply_matrix(const RS_VECTOR3 *vec, const RS_MATRIX3 *matrix);
extern void matrix3_scale(RS_MATRIX3 *matrix, const float scale, RS_MATRIX3 *result);
extern float matrix3_max(const RS_MATRIX3 *matrix);
void matrix3_interpolate(const RS_MATRIX3 *a, const RS_MATRIX3 *b, const float alpha, RS_MATRIX3 *result);
extern float matrix3_weight(const RS_MATRIX3 *mat);
extern void matrix3_affine_invert(RS_MATRIX3 *mat);
extern void matrix3_affine_scale(RS_MATRIX3 *matrix, double xscale, double yscale);
extern void matrix3_affine_translate(RS_MATRIX3 *matrix, double xtrans, double ytrans);
extern void matrix3_affine_rotate(RS_MATRIX3 *matrix, double degrees);
extern void matrix3_affine_transform_point(RS_MATRIX3 *matrix, double x, double y, double *x2, double *y2);
extern void matrix3_affine_transform_point_int(RS_MATRIX3 *matrix, int x, int y, int *x2, int *y2);
extern void matrix3_affine_get_minmax(RS_MATRIX3 *matrix, double *minx, double *miny, double *maxx, double *maxy,
	double x1, double y1, double x2, double y2);

/**
 * Interpolate an array of unsigned integers
 * @param input_dataset An array of unsigned integers to be inperpolated
 * @param input_length The length of the input array
 * @param output_dataset An array of unsigned integers for output or NULL
 * @param output_length The length of the output array
 * @param max A pointer to an unsigned int or NULL
 * @return the interpolated dataset
 */
unsigned int *
interpolate_dataset_int(unsigned int *input_dataset, unsigned int input_length, unsigned int *output_dataset, unsigned int output_length, unsigned int *max);

#endif /* RS_MATRIX_H */

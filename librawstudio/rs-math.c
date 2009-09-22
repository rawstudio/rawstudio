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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "rs-math.h"

/* luminance weights, notice that these is used for linear data */
#define RLUM (0.3086)
#define GLUM (0.6094)
#define BLUM (0.0820)

static void matrix4_zshear (RS_MATRIX4 *matrix, double dx, double dy);
static void matrix4_xrotate(RS_MATRIX4 *matrix, double rs, double rc);
static void matrix4_yrotate(RS_MATRIX4 *matrix, double rs, double rc);
static void matrix4_zrotate(RS_MATRIX4 *matrix, double rs, double rc);
static void matrix4_affine_transform_3dpoint(RS_MATRIX4 *matrix, double x, double y, double z, double *tx, double *ty, double *tz);

void
printvec(const char *str, const RS_VECTOR3 *vec)
{
	printf("%s: [ %.05f %.05f %.05f ]\n", str, vec->x, vec->y, vec->z);
}

void
printmat3(RS_MATRIX3 *mat)
{
	int x, y;

	printf("M: matrix(\n");
	for(y=0; y<3; y++)
	{
		printf("\t[ %f, ",mat->coeff[y][0]);
		printf("%f, ",mat->coeff[y][1]);
		printf("%f ",mat->coeff[y][2]);
		printf("],\n");
	}
	printf(")\n");
}

void
printmat(RS_MATRIX4 *mat)
{
	int x, y;

	for(y=0; y<4; y++)
	{
		for(x=0; x<4; x++)
			printf("%f ",mat->coeff[y][x]);
		printf("\n");
	}
	printf("\n");
}

float
vector3_max(const RS_VECTOR3 *vec)
{
	float max = 0.0;

	max = MAX(max, vec->x);
	max = MAX(max, vec->y);
	max = MAX(max, vec->z);

	return max;
}

RS_MATRIX3
vector3_as_diagonal(const RS_VECTOR3 *vec)
{
	RS_MATRIX3 result;
	matrix3_identity(&result);
	result.coeff[0][0] = vec->x;
	result.coeff[1][1] = vec->y;
	result.coeff[2][2] = vec->z;

	return result;
}

void
matrix4_identity (RS_MATRIX4 *matrix)
{
  static const RS_MATRIX4 identity = { { { 1.0, 0.0, 0.0, 0.0 },
                                          { 0.0, 1.0, 0.0, 0.0 },
                                          { 0.0, 0.0, 1.0, 0.0 },
                                          { 0.0, 0.0, 0.0, 1.0 } } };

  *matrix = identity;
}

void
matrix4_multiply(const RS_MATRIX4 *left, RS_MATRIX4 *right, RS_MATRIX4 *result)
{
  int i, j;
  RS_MATRIX4 tmp;
  double t1, t2, t3, t4;

  for (i = 0; i < 4; i++)
    {
      t1 = left->coeff[i][0];
      t2 = left->coeff[i][1];
      t3 = left->coeff[i][2];
      t4 = left->coeff[i][3];

      for (j = 0; j < 4; j++)
        {
          tmp.coeff[i][j]  = t1 * right->coeff[0][j];
          tmp.coeff[i][j] += t2 * right->coeff[1][j];
          tmp.coeff[i][j] += t3 * right->coeff[2][j];
          tmp.coeff[i][j] += t4 * right->coeff[3][j];
        }
    }
  *result = tmp;
}

/* copied almost verbatim from  dcraw.c:pseudoinverse()
 - but this one doesn't transpose */
void
matrix4_color_invert(const RS_MATRIX4 *in, RS_MATRIX4 *out)
{
	RS_MATRIX4 tmp;
	double work[3][6], num;
	int i, j, k;

	matrix4_identity(&tmp);

	for (i=0; i < 3; i++)
	{
		for (j=0; j < 6; j++)
			work[i][j] = j == i+3;
		for (j=0; j < 3; j++)
			for (k=0; k < 3; k++)
				work[i][j] += in->coeff[k][i] * in->coeff[k][j];
	}
	for (i=0; i < 3; i++)
	{
		num = work[i][i];
		for (j=0; j < 6; j++)
			work[i][j] /= num;
		for (k=0; k < 3; k++)
		{
			if (k==i)
				continue;
			num = work[k][i];
			for (j=0; j < 6; j++)
				work[k][j] -= work[i][j] * num;
		}
	}
	for (i=0; i < 3; i++)
		for (j=0; j < 3; j++)
			for (tmp.coeff[i][j]=k=0; k < 3; k++)
				tmp.coeff[i][j] += work[j][k+3] * in->coeff[i][k];
	for (i=0; i < 4; i++)
		for (j=0; j < 4; j++)
			out->coeff[i][j] = tmp.coeff[j][i];
}

static void
matrix4_zshear (RS_MATRIX4 *matrix, double dx, double dy)
{
  RS_MATRIX4 zshear;

  zshear.coeff[0][0] = 1.0;
  zshear.coeff[1][0] = 0.0;
  zshear.coeff[2][0] = dx;
  zshear.coeff[3][0] = 0.0;

  zshear.coeff[0][1] = 0.0;
  zshear.coeff[1][1] = 1.0;
  zshear.coeff[2][1] = dy;
  zshear.coeff[3][1] = 0.0;

  zshear.coeff[0][2] = 0.0;
  zshear.coeff[1][2] = 0.0;
  zshear.coeff[2][2] = 1.0;
  zshear.coeff[3][2] = 0.0;

  zshear.coeff[0][3] = 0.0;
  zshear.coeff[1][3] = 0.0;
  zshear.coeff[2][3] = 0.0;
  zshear.coeff[3][3] = 1.0;

  matrix4_multiply(&zshear, matrix, matrix);
}

void
matrix4_to_matrix4int(RS_MATRIX4 *matrix, RS_MATRIX4Int *matrixi)
{
  int a,b;
  for(a=0;a<4;a++)
    for(b=0;b<4;b++)
      matrixi->coeff[a][b] = (int) (matrix->coeff[a][b] * (double) (1<<MATRIX_RESOLUTION));
  return;
}

static void
matrix4_xrotate(RS_MATRIX4 *matrix, double rs, double rc)
{
  RS_MATRIX4 tmp;

  tmp.coeff[0][0] = 1.0;
  tmp.coeff[1][0] = 0.0;
  tmp.coeff[2][0] = 0.0;
  tmp.coeff[3][0] = 0.0;

  tmp.coeff[0][1] = 0.0;
  tmp.coeff[1][1] = rc;
  tmp.coeff[2][1] = rs;
  tmp.coeff[3][1] = 0.0;

  tmp.coeff[0][2] = 0.0;
  tmp.coeff[1][2] = -rs;
  tmp.coeff[2][2] = rc;
  tmp.coeff[3][2] = 0.0;

  tmp.coeff[0][3] = 0.0;
  tmp.coeff[1][3] = 0.0;
  tmp.coeff[2][3] = 0.0;
  tmp.coeff[3][3] = 1.0;

  matrix4_multiply(&tmp, matrix, matrix);
}

static void
matrix4_yrotate(RS_MATRIX4 *matrix, double rs, double rc)
{
  RS_MATRIX4 tmp;

  tmp.coeff[0][0] = rc;
  tmp.coeff[1][0] = 0.0;
  tmp.coeff[2][0] = -rs;
  tmp.coeff[3][0] = 0.0;

  tmp.coeff[0][1] = 0.0;
  tmp.coeff[1][1] = 1.0;
  tmp.coeff[2][1] = 0.0;
  tmp.coeff[3][1] = 0.0;

  tmp.coeff[0][2] = rs;
  tmp.coeff[1][2] = 0.0;
  tmp.coeff[2][2] = rc;
  tmp.coeff[3][2] = 0.0;

  tmp.coeff[0][3] = 0.0;
  tmp.coeff[1][3] = 0.0;
  tmp.coeff[2][3] = 0.0;
  tmp.coeff[3][3] = 1.0;

  matrix4_multiply(&tmp, matrix, matrix);
}

static void
matrix4_zrotate(RS_MATRIX4 *matrix, double rs, double rc)
{
  RS_MATRIX4 tmp;

  tmp.coeff[0][0] = rc;
  tmp.coeff[1][0] = rs;
  tmp.coeff[2][0] = 0.0;
  tmp.coeff[3][0] = 0.0;

  tmp.coeff[0][1] = -rs;
  tmp.coeff[1][1] = rc;
  tmp.coeff[2][1] = 0.0;
  tmp.coeff[3][1] = 0.0;

  tmp.coeff[0][2] = 0.0;
  tmp.coeff[1][2] = 0.0;
  tmp.coeff[2][2] = 1.0;
  tmp.coeff[3][2] = 0.0;

  tmp.coeff[0][3] = 0.0;
  tmp.coeff[1][3] = 0.0;
  tmp.coeff[2][3] = 0.0;
  tmp.coeff[3][3] = 1.0;

  matrix4_multiply(&tmp, matrix, matrix);
}

void
matrix4_color_normalize(RS_MATRIX4 *mat)
{
	int row,col;
	double lum;

	for(row=0;row<3;row++)
	{
		lum = 0.0;
		for(col=0;col<3;col++)
			lum += mat->coeff[row][col];
		for(col=0;col<3;col++)
			mat->coeff[row][col] /= lum;
	}
	return;
}

void
matrix4_color_saturate(RS_MATRIX4 *mat, double sat)
{
	RS_MATRIX4 tmp;

	if (sat == 1.0) return;

	tmp.coeff[0][0] = (1.0-sat)*RLUM + sat;
	tmp.coeff[1][0] = (1.0-sat)*RLUM;
	tmp.coeff[2][0] = (1.0-sat)*RLUM;
	tmp.coeff[3][0] = 0.0;

	tmp.coeff[0][1] = (1.0-sat)*GLUM;
	tmp.coeff[1][1] = (1.0-sat)*GLUM + sat;
	tmp.coeff[2][1] = (1.0-sat)*GLUM;
	tmp.coeff[3][1] = 0.0;

	tmp.coeff[0][2] = (1.0-sat)*BLUM;
	tmp.coeff[1][2] = (1.0-sat)*BLUM;
	tmp.coeff[2][2] = (1.0-sat)*BLUM + sat;
	tmp.coeff[3][2] = 0.0;

	tmp.coeff[0][3] = 0.0;
	tmp.coeff[1][3] = 0.0;
	tmp.coeff[2][3] = 0.0;
	tmp.coeff[3][3] = 1.0;
	matrix4_multiply(mat, &tmp, mat);
}

static void
matrix4_affine_transform_3dpoint(RS_MATRIX4 *matrix, double x, double y, double z, double *tx, double *ty, double *tz)
{
	*tx = x*matrix->coeff[0][0] + y*matrix->coeff[0][1]
		+ z*matrix->coeff[0][2] + matrix->coeff[0][3];
	*ty = x*matrix->coeff[1][0] + y*matrix->coeff[1][1]
		+ z*matrix->coeff[1][2] + matrix->coeff[1][3];
	*tz = x*matrix->coeff[2][0] + y*matrix->coeff[2][1]
		+ z*matrix->coeff[2][2] + matrix->coeff[2][3];
}

void
matrix4_color_hue(RS_MATRIX4 *mat, double rot)
{
	RS_MATRIX4 tmp;
	double mag;
	double lx, ly, lz;
	double xrs, xrc;
	double yrs, yrc;
	double zrs, zrc;
	double zsx, zsy;

	if (rot==0.0) return;

	matrix4_identity(&tmp);

	/* rotate the grey vector into positive Z */
	mag = sqrt(2.0);
	xrs = 1.0/mag;
	xrc = 1.0/mag;
	matrix4_xrotate(&tmp, xrs, xrc);

	mag = sqrt(3.0);
	yrs = -1.0/mag;
	yrc = sqrt(2.0)/mag;
	matrix4_yrotate(&tmp, yrs ,yrc);

	/* shear the space to make the luminance plane horizontal */
	matrix4_affine_transform_3dpoint(&tmp,RLUM,GLUM,BLUM,&lx,&ly,&lz);
	zsx = lx/lz;
	zsy = ly/lz;
	matrix4_zshear(&tmp, zsx, zsy);

	/* rotate the hue */
	zrs = sin(rot*M_PI/180.0);
	zrc = cos(rot*M_PI/180.0);
	matrix4_zrotate(&tmp, zrs, zrc);

	/* unshear the space to put the luminance plane back */
	matrix4_zshear(&tmp, -zsx, -zsy);

	/* rotate the grey vector back into place */
	matrix4_yrotate(&tmp,-yrs,yrc);
	matrix4_xrotate(&tmp,-xrs,xrc);
	matrix4_multiply(mat,&tmp,mat);
}

void
matrix4_color_exposure(RS_MATRIX4 *mat, double exp)
{
	double expcom = pow(2.0, exp);
	mat->coeff[0][0] *= expcom;
	mat->coeff[0][1] *= expcom;
	mat->coeff[0][2] *= expcom;
	mat->coeff[1][0] *= expcom;
	mat->coeff[1][1] *= expcom;
	mat->coeff[1][2] *= expcom;
	mat->coeff[2][0] *= expcom;
	mat->coeff[2][1] *= expcom;
	mat->coeff[2][2] *= expcom;
	return;
}

void
matrix3_identity (RS_MATRIX3 *matrix)
{
  static const RS_MATRIX3 identity = { { { 1.0, 0.0, 0.0 },
                                          { 0.0, 1.0, 0.0 },
                                          { 0.0, 0.0, 1.0 } } };

  *matrix = identity;
}

RS_MATRIX3
matrix3_invert(const RS_MATRIX3 *matrix)
{
	int j,k;

	double a00 = matrix->coeff[0][0];
	double a01 = matrix->coeff[0][1];
	double a02 = matrix->coeff[0][2];
	double a10 = matrix->coeff[1][0];
	double a11 = matrix->coeff[1][1];
	double a12 = matrix->coeff[1][2];
	double a20 = matrix->coeff[2][0];
	double a21 = matrix->coeff[2][1];
	double a22 = matrix->coeff[2][2];

	double temp[3][3];

	temp[0][0] = a11 * a22 - a21 * a12;
	temp[0][1] = a21 * a02 - a01 * a22;
	temp[0][2] = a01 * a12 - a11 * a02;
	temp[1][0] = a20 * a12 - a10 * a22;
	temp[1][1] = a00 * a22 - a20 * a02;
	temp[1][2] = a10 * a02 - a00 * a12;
	temp[2][0] = a10 * a21 - a20 * a11;
	temp[2][1] = a20 * a01 - a00 * a21;
	temp[2][2] = a00 * a11 - a10 * a01;

	double det = (a00 * temp[0][0] + a01 * temp[1][0] + a02 * temp[2][0]);

	RS_MATRIX3 B;

	for (j = 0; j < 3; j++)
		for (k = 0; k < 3; k++)
			B.coeff[j][k] = temp[j][k] / det;

	return B;
}

void
matrix3_multiply(const RS_MATRIX3 *left, const RS_MATRIX3 *right, RS_MATRIX3 *result)
{
  int i, j;
  RS_MATRIX3 tmp;
  double t1, t2, t3;

  for (i = 0; i < 3; i++)
    {
      t1 = left->coeff[i][0];
      t2 = left->coeff[i][1];
      t3 = left->coeff[i][2];

      for (j = 0; j < 3; j++)
        {
          tmp.coeff[i][j]  = t1 * right->coeff[0][j];
          tmp.coeff[i][j] += t2 * right->coeff[1][j];
          tmp.coeff[i][j] += t3 * right->coeff[2][j];
        }
    }
	*result = tmp;
	return;
}

RS_VECTOR3
vector3_multiply_matrix(const RS_VECTOR3 *vec, const RS_MATRIX3 *matrix)
{
	RS_VECTOR3 result;

	result.x = vec->x * matrix->coeff[0][0] + vec->y * matrix->coeff[0][1] + vec->z * matrix->coeff[0][2];
	result.y = vec->x * matrix->coeff[1][0] + vec->y * matrix->coeff[1][1] + vec->z * matrix->coeff[1][2];
	result.z = vec->x * matrix->coeff[2][0] + vec->y * matrix->coeff[2][1] + vec->z * matrix->coeff[2][2];

	return result;
}

float
matrix3_max(const RS_MATRIX3 *matrix)
{
	gfloat max = 0.0;
	int i, j;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			max = MAX(max, matrix->coeff[i][j]);

	return max;
}

void
matrix3_scale(RS_MATRIX3 *matrix, const float scale, RS_MATRIX3 *result)
{
	int i, j;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			result->coeff[i][j] = matrix->coeff[i][j] * scale;
}

void
matrix3_interpolate(const RS_MATRIX3 *a, const RS_MATRIX3 *b, const float alpha, RS_MATRIX3 *result)
{
#define LERP(a,b,g) (((b) - (a)) * (g) + (a))
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			result->coeff[i][j] = LERP(a->coeff[i][j], b->coeff[i][j], alpha);
#undef LERP
}

float
matrix3_weight(const RS_MATRIX3 *mat)
{
	float weight = 
		mat->coeff[0][0]
		+ mat->coeff[0][1]
		+ mat->coeff[0][2]
		+ mat->coeff[1][0]
		+ mat->coeff[1][1]
		+ mat->coeff[1][2]
		+ mat->coeff[2][0]
		+ mat->coeff[2][1]
		+ mat->coeff[2][2];
	return(weight);
}

void
matrix3_to_matrix3int(RS_MATRIX3 *matrix, RS_MATRIX3Int *matrixi)
{
  int a,b;
  for(a=0;a<3;a++)
    for(b=0;b<3;b++)
      matrixi->coeff[a][b] = (int) (matrix->coeff[a][b] * (double) (1<<MATRIX_RESOLUTION));
  return;
}

/*

Affine transformations

The following transformation matrix is used:

 [  a  b u ]
 [  c  d v ]
 [ tx ty w ]

a: x scale
b: y skew
c: x skew
d: y scale
tx: x translation (offset)
ty: y translation (offset)

(u, v & w unused)

x' = x*a + y*c + tx
y' = x*b + y*d + ty

*/

void
matrix3_affine_invert(RS_MATRIX3 *mat)
{
	RS_MATRIX3 tmp;
	const double reverse_det = 1.0/(mat->coeff[0][0]*mat->coeff[1][1] - mat->coeff[0][1]*mat->coeff[1][0]);
	matrix3_identity(&tmp);
	tmp.coeff[0][0] =  mat->coeff[1][1] * reverse_det;
	tmp.coeff[0][1] = -mat->coeff[0][1] * reverse_det;
	tmp.coeff[1][0] = -mat->coeff[1][0] * reverse_det;
	tmp.coeff[1][1] =  mat->coeff[0][0] * reverse_det;
	tmp.coeff[2][0] =  (mat->coeff[1][0]*mat->coeff[2][1] - mat->coeff[1][1]*mat->coeff[2][0])/
		(mat->coeff[0][0]*mat->coeff[1][1] - mat->coeff[0][1]*mat->coeff[1][0]);
	tmp.coeff[2][1] = -(mat->coeff[0][0]*mat->coeff[2][1] - mat->coeff[0][1]*mat->coeff[2][0])/
		(mat->coeff[0][0]*mat->coeff[1][1] - mat->coeff[0][1]*mat->coeff[1][0]);
	*mat = tmp;
}

void
matrix3_affine_scale(RS_MATRIX3 *matrix, double xscale, double yscale)
{
	RS_MATRIX3 tmp;
	matrix3_identity(&tmp);
	tmp.coeff[0][0] *= xscale;
	tmp.coeff[1][1] *= yscale;
	matrix3_multiply(matrix, &tmp, matrix);
	return;
}

void
matrix3_affine_translate(RS_MATRIX3 *matrix, double xtrans, double ytrans)
{
	matrix->coeff[2][0] += xtrans;
	matrix->coeff[2][1] += ytrans;
	return;
}

void
matrix3_affine_rotate(RS_MATRIX3 *matrix, double degrees)
{
	RS_MATRIX3 tmp;
	const double s = sin (degrees * M_PI / 180.0);
	const double c = cos (degrees * M_PI / 180.0);

	matrix3_identity(&tmp);
	tmp.coeff[0][0] = c;
	tmp.coeff[0][1] = s;
	tmp.coeff[1][0] = -s;
	tmp.coeff[1][1] = c;
	matrix3_multiply(matrix, &tmp, matrix);
	return;
}

inline void
matrix3_affine_transform_point(RS_MATRIX3 *matrix, double x, double y, double *x2, double *y2)
{
	const double x_tmp = x*matrix->coeff[0][0] + y*matrix->coeff[1][0] + matrix->coeff[2][0];
	const double y_tmp = x*matrix->coeff[0][1] + y*matrix->coeff[1][1] + matrix->coeff[2][1];
	*x2 = x_tmp;
	*y2 = y_tmp;
	return;
}

inline void
matrix3_affine_transform_point_int(RS_MATRIX3 *matrix, int x, int y, int *x2, int *y2)
{
	const int x_tmp = (int) (x*matrix->coeff[0][0] + y*matrix->coeff[1][0] + matrix->coeff[2][0]);
	const int y_tmp = (int) (x*matrix->coeff[0][1] + y*matrix->coeff[1][1] + matrix->coeff[2][1]);
	*x2 = x_tmp;
	*y2 = y_tmp;
	return;
}

void
matrix3_affine_get_minmax(RS_MATRIX3 *matrix,
	double *minx, double *miny,
	double *maxx, double *maxy,
	double x1, double y1,
	double x2, double y2)
{
	double x,y;
	*minx = *miny = 100000.0;
	*maxx = *maxy = 0.0;

	matrix3_affine_transform_point(matrix, x1, y1, &x, &y);
	if (x<*minx) *minx=x;
	if (x>*maxx) *maxx=x;
	if (y<*miny) *miny=y;
	if (y>*maxy) *maxy=y;
	matrix3_affine_transform_point(matrix, x2, y1, &x, &y);
	if (x<*minx) *minx=x;
	if (x>*maxx) *maxx=x;
	if (y<*miny) *miny=y;
	if (y>*maxy) *maxy=y;
	matrix3_affine_transform_point(matrix, x1, y2, &x, &y);
	if (x<*minx) *minx=x;
	if (x>*maxx) *maxx=x;
	if (y<*miny) *miny=y;
	if (y>*maxy) *maxy=y;
	matrix3_affine_transform_point(matrix, x2, y2, &x, &y);
	if (x<*minx) *minx=x;
	if (x>*maxx) *maxx=x;
	if (y<*miny) *miny=y;
	if (y>*maxy) *maxy=y;

	return;
}

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
interpolate_dataset_int(unsigned int *input_dataset, unsigned int input_length, unsigned int *output_dataset, unsigned int output_length, unsigned int *max)
{
	const double scale = ((double)input_length) / ((double)output_length);
	int i, source1, source2;
	float source;
	float weight1, weight2;

	if (output_dataset == NULL)
		output_dataset = malloc(sizeof(unsigned int)*output_length);

	for(i=0;i<output_length;i++)
	{
		source = ((float)i) * scale;
		source1 = (int) floor(source);
		source2 = source1+1;
		weight1 = 1.0f - (source - floor(source));
		weight2 = 1.0f - weight1;
		output_dataset[i] = (unsigned int) (((float)input_dataset[source1]) * weight1
					+ ((float)input_dataset[source2]) * weight2);
		if (max)
			if (output_dataset[i] > (*max))
				*max = output_dataset[i];
	}

	return output_dataset;
}

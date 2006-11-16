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

#include <stdio.h>
#include <math.h>
#include "color.h"
#include "matrix.h"

static void matrix4_mult(const RS_MATRIX4 *matrix1, RS_MATRIX4 *matrix2) __attribute__ ((deprecated));
static void matrix4_multiply(const RS_MATRIX4 *left, RS_MATRIX4 *right, RS_MATRIX4 *result);
static void matrix4_zshear (RS_MATRIX4 *matrix, double dx, double dy);
static void matrix4_xrotate(RS_MATRIX4 *matrix, double rs, double rc);
static void matrix4_yrotate(RS_MATRIX4 *matrix, double rs, double rc);
static void matrix4_zrotate(RS_MATRIX4 *matrix, double rs, double rc);
static void xformpnt(RS_MATRIX4 *matrix, double x, double y, double z, double *tx, double *ty, double *tz);

void
printmat(RS_MATRIX4 *mat)
{
	int x, y;

	for(y=0; y<4; y++)
	{
		for(x=0; x<4; x++)
			printf("%f ",mat->coeff[x][y]);
		printf("\n");
	}
	printf("\n");
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
matrix4_mult(const RS_MATRIX4 *matrix1, RS_MATRIX4 *matrix2)
{
  int i, j;
  RS_MATRIX4 tmp;
  double t1, t2, t3, t4;

  for (i = 0; i < 4; i++)
    {
      t1 = matrix1->coeff[i][0];
      t2 = matrix1->coeff[i][1];
      t3 = matrix1->coeff[i][2];
      t4 = matrix1->coeff[i][3];

      for (j = 0; j < 4; j++)
        {
          tmp.coeff[i][j]  = t1 * matrix2->coeff[0][j];
          tmp.coeff[i][j] += t2 * matrix2->coeff[1][j];
          tmp.coeff[i][j] += t3 * matrix2->coeff[2][j];
          tmp.coeff[i][j] += t4 * matrix2->coeff[3][j];
        }
    }
  *matrix2 = tmp;
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

void
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

void
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

void
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

void
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

void
xformpnt(RS_MATRIX4 *matrix, double x, double y, double z, double *tx, double *ty, double *tz)
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
	xformpnt(&tmp,RLUM,GLUM,BLUM,&lx,&ly,&lz);
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

void
matrix3_mult(const RS_MATRIX3 *matrix1, RS_MATRIX3 *matrix2)
{
	int i, j;
	RS_MATRIX3 tmp;
	double t1, t2, t3;

	for (i = 0; i < 3; i++)
	{
		t1 = matrix1->coeff[i][0];
		t2 = matrix1->coeff[i][1];
		t3 = matrix1->coeff[i][2];
		for (j = 0; j < 3; j++)
		{
			tmp.coeff[i][j]  = t1 * matrix2->coeff[0][j];
			tmp.coeff[i][j] += t2 * matrix2->coeff[1][j];
			tmp.coeff[i][j] += t3 * matrix2->coeff[2][j];
		}
	}
	*matrix2 = tmp;
	return;
}

void
matrix3_multiply(const RS_MATRIX3 *left, RS_MATRIX3 *right, RS_MATRIX3 *result)
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

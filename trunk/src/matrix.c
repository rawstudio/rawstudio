#include <stdio.h>
#include <math.h>
#include "color.h"
#include "matrix.h"

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

  matrix4_mult(&zshear, matrix);
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

  matrix4_mult(&tmp, matrix);
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

  matrix4_mult(&tmp, matrix);
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

  matrix4_mult(&tmp, matrix);
}

void
matrix4_color_saturate(RS_MATRIX4 *mat, double sat)
{
	RS_MATRIX4 tmp;
	double a, b, c, d, e, f, g, h, i;
	double rwgt, gwgt, bwgt;

	if (sat == 1.0) return;

	rwgt = RLUM;
	gwgt = GLUM;
	bwgt = BLUM;

	a = (1.0-sat)*rwgt + sat;
	b = (1.0-sat)*rwgt;
	c = (1.0-sat)*rwgt;
	d = (1.0-sat)*gwgt;
	e = (1.0-sat)*gwgt + sat;
	f = (1.0-sat)*gwgt;
	g = (1.0-sat)*bwgt;
	h = (1.0-sat)*bwgt;
	i = (1.0-sat)*bwgt + sat;
	tmp.coeff[0][0] = a;
	tmp.coeff[1][0] = b;
	tmp.coeff[2][0] = c;
	tmp.coeff[3][0] = 0.0;

	tmp.coeff[0][1] = d;
	tmp.coeff[1][1] = e;
	tmp.coeff[2][1] = f;
	tmp.coeff[3][1] = 0.0;

	tmp.coeff[0][2] = g;
	tmp.coeff[1][2] = h;
	tmp.coeff[2][2] = i;
	tmp.coeff[3][2] = 0.0;

	tmp.coeff[0][3] = 0.0;
	tmp.coeff[1][3] = 0.0;
	tmp.coeff[2][3] = 0.0;
	tmp.coeff[3][3] = 1.0;
	matrix4_mult(&tmp,mat);
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
	matrix4_mult(&tmp,mat);
}

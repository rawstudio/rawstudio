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

#include <rawstudio.h>
#include <math.h>
#include "rs-color.h"

/* We use XYZ with a D50 whitepoint as PCS (as recommended by ICC) */

const RS_XYZ_VECTOR XYZ_WP_D50 = {{0.964296}, {1.0}, {0.825105}}; /* Computed by DNG SDK */

/* Scale factor between distances in uv space to a more user friendly "tint" parameter. */
static const gdouble tint_scale = -3000.0;

/* Table from Wyszecki & Stiles, "Color Science", second edition, page 228. */

struct ruvt {
	gdouble r;
	gdouble u;
	gdouble v;
	gdouble t;
} temp_table[] = {
	{   0, 0.18006, 0.26352, -0.24341 },
	{  10, 0.18066, 0.26589, -0.25479 },
	{  20, 0.18133, 0.26846, -0.26876 },
	{  30, 0.18208, 0.27119, -0.28539 },
	{  40, 0.18293, 0.27407, -0.30470 },
	{  50, 0.18388, 0.27709, -0.32675 },
	{  60, 0.18494, 0.28021, -0.35156 },
	{  70, 0.18611, 0.28342, -0.37915 },
	{  80, 0.18740, 0.28668, -0.40955 },
	{  90, 0.18880, 0.28997, -0.44278 },
	{ 100, 0.19032, 0.29326, -0.47888 },
	{ 125, 0.19462, 0.30141, -0.58204 },
	{ 150, 0.19962, 0.30921, -0.70471 },
	{ 175, 0.20525, 0.31647, -0.84901 },
	{ 200, 0.21142, 0.32312, -1.0182 },
	{ 225, 0.21807, 0.32909, -1.2168 },
	{ 250, 0.22511, 0.33439, -1.4512 },
	{ 275, 0.23247, 0.33904, -1.7298 },
	{ 300, 0.24010, 0.34308, -2.0637 },
	{ 325, 0.24702, 0.34655, -2.4681 },
	{ 350, 0.25591, 0.34951, -2.9641 },
	{ 375, 0.26400, 0.35200, -3.5814 },
	{ 400, 0.27218, 0.35407, -4.3633 },
	{ 425, 0.28039, 0.35577, -5.3762 },
	{ 450, 0.28863, 0.35714, -6.7262 },
	{ 475, 0.29685, 0.35823, -8.5955 },
	{ 500, 0.30505, 0.35907, -11.324 },
	{ 525, 0.31320, 0.35968, -15.628 },
	{ 550, 0.32129, 0.36011, -23.325 },
	{ 575, 0.32931, 0.36038, -40.770 },
	{ 600, 0.33724, 0.36051, -116.45 }
};

void
rs_color_whitepoint_to_temp(const RS_xy_COORD *xy, gfloat *temp, gfloat *tint)
{
	/* Convert to uv space. */
	gdouble u = 2.0 * xy->x / (1.5 - xy->x + 6.0 * xy->y);
    gdouble v = 3.0 * xy->y / (1.5 - xy->x + 6.0 * xy->y);

	/* Search for line pair coordinate is between. */
    gdouble last_dt = 0.0;

    gdouble last_dv = 0.0;
    gdouble last_du = 0.0;

	guint index;
	for (index = 1; index <= 30; index++)
	{
		/* Convert slope to delta-u and delta-v, with length 1. */
		gdouble du = 1.0;
		gdouble dv = temp_table[index] . t;

		gdouble len = sqrt (1.0 + dv * dv);

		du /= len;
		dv /= len;

		/* Find delta from black body point to test coordinate. */
		gdouble uu = u - temp_table[index] . u;
		gdouble vv = v - temp_table[index] . v;

		/* Find distance above or below line. */

		gdouble dt = - uu * dv + vv * du;

		/* If below line, we have found line pair. */

		if (dt <= 0.0 || index == 30)
		{
			/* Find fractional weight of two lines. */
			if (dt > 0.0)
				dt = 0.0;

			dt = -dt;

			gdouble f;

			if (index == 1)
				f = 0.0;
			else
				f = dt / (last_dt + dt);

			/* Interpolate the temperature. */
			if (temp)
				*temp = 1.0E6 / (temp_table[index - 1] . r * f + temp_table[index    ] . r * (1.0 - f));

			/* Find delta from black body point to test coordinate. */
			uu = u - (temp_table[index - 1] . u * f + temp_table[index] . u * (1.0 - f));
			vv = v - (temp_table[index - 1] . v * f + temp_table[index] . v * (1.0 - f));

			/* Interpolate vectors along slope. */
			du = du * (1.0 - f) + last_du * f;
			dv = dv * (1.0 - f) + last_dv * f;

			len = sqrt (du * du + dv * dv);

			du /= len;
			dv /= len;

			/* Find distance along slope. */
			if (tint)
				*tint = (uu * du + vv * dv) * tint_scale;

			break;
		}

		/* Try next line pair. */

		last_dt = dt;

		last_du = du;
		last_dv = dv;
	}
}

RS_xy_COORD
rs_color_temp_to_whitepoint(gfloat temp, gfloat tint)
{
	RS_xy_COORD xy = XYZ_to_xy(&XYZ_WP_D50);

	/* Find inverse temperature to use as index. */
	gdouble  r = 1.0E6 / temp;

	/* Convert tint to offset is uv space. */

	gdouble offset = tint * (1.0 / tint_scale);

	/* Search for line pair containing coordinate. */

	guint index;
	for (index = 0; index <= 29; index++)
	{
		if (r < temp_table[index + 1] . r || index == 29)
		{
			/* Find relative weight of first line. */

			gdouble f = (temp_table[index + 1].r - r) / (temp_table[index + 1].r - temp_table[index].r);

			/* Interpolate the black body coordinates. */

			gdouble u = temp_table[index].u * f + temp_table[index + 1].u * (1.0 - f);

			gdouble v = temp_table[index].v * f + temp_table[index + 1].v * (1.0 - f);

			/* Find vectors along slope for each line. */

			gdouble uu1 = 1.0;
			gdouble vv1 = temp_table[index].t;

			gdouble uu2 = 1.0;
			gdouble vv2 = temp_table[index + 1].t;

			gdouble len1 = sqrt (1.0 + vv1 * vv1);
			gdouble len2 = sqrt (1.0 + vv2 * vv2);

			uu1 /= len1;
			vv1 /= len1;

			uu2 /= len2;
			vv2 /= len2;

			// Find vector from black body point.
			gdouble uu3 = uu1 * f + uu2 * (1.0 - f);
			gdouble vv3 = vv1 * f + vv2 * (1.0 - f);

			gdouble len3 = sqrt (uu3 * uu3 + vv3 * vv3);

			uu3 /= len3;
			vv3 /= len3;

			// Adjust coordinate along this vector.

			u += uu3 * offset;
			v += vv3 * offset;

			// Convert to xy coordinates.

			xy.x = 1.5 * u / (u - 4.0 * v + 2.0);
			xy.y =       v / (u - 4.0 * v + 2.0);

			break;
		}
	}

	return xy;
}

RS_XYZ_VECTOR
xy_to_XYZ(const RS_xy_COORD *xy)
{
	RS_XYZ_VECTOR XYZ;

	/* Watch out for division by zero and clipping */
	gdouble x = CLAMP(xy->x, 0.000001, 0.999999);
	gdouble y = CLAMP(xy->y, 0.000001, 0.999999);

	/* Correct out of range color coordinates */
	if ((x + y) > 0.999999)
	{
		gdouble scale = 0.999999 / (x + y);
		x *= scale;
		y *= scale;
	}

	XYZ.X = x / y;
	XYZ.Y = 1.0;
	XYZ.Z = (1.0 - x - y) / y;

	return XYZ;
}

RS_xy_COORD
XYZ_to_xy(const RS_XYZ_VECTOR *XYZ)
{
	RS_xy_COORD xy;

	gfloat sum = XYZ->X + XYZ->Y + XYZ->Z;

	if (sum > 0.0)
	{
		xy.x = XYZ->X / sum;
		xy.y = XYZ->Y / sum;
	}
	else
		xy = XYZ_to_xy(&XYZ_WP_D50);

	return xy;
}

/**
 * http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
 */
RS_MATRIX3
rs_calculate_map_white_matrix(const RS_xy_COORD *from_xy, const RS_xy_COORD *to_xy)
{
	RS_MATRIX3 adapt;

	/* Bradford adaption matrix */
	RS_MATRIX3 bradford = {{
		{ 0.8951,  0.2664, -0.1614, },
		{ -0.7502,  1.7135,  0.0367, },
		{ 0.0389, -0.0685,  1.0296 }
	}};

	RS_XYZ_VECTOR from_xyz;
	RS_XYZ_VECTOR to_xyz;
	RS_VECTOR3 ws, wd;
	RS_MATRIX3 tmp;

	from_xyz = xy_to_XYZ(from_xy);
	to_xyz = xy_to_XYZ(to_xy);

	ws = vector3_multiply_matrix(&from_xyz, &bradford);
	wd = vector3_multiply_matrix(&to_xyz, &bradford);

	ws.x = MAX(ws.x, 0.0);
	ws.y = MAX(ws.y, 0.0);
	ws.z = MAX(ws.z, 0.0);

	wd.x = MAX(wd.x, 0.0);
	wd.y = MAX(wd.y, 0.0);
	wd.z = MAX(wd.z, 0.0);

	matrix3_identity(&tmp);

	tmp.coeff[0][0] = CLAMP(0.1, ws.x > 0.0 ? wd.x / ws.x : 10.0, 10.0);
	tmp.coeff[1][1] = CLAMP(0.1, ws.y > 0.0 ? wd.y / ws.y : 10.0, 10.0);
	tmp.coeff[2][2] = CLAMP(0.1, ws.z > 0.0 ? wd.z / ws.z : 10.0, 10.0);

	adapt = matrix3_invert(&bradford);
	matrix3_multiply(&adapt, &tmp, &adapt);
	matrix3_multiply(&adapt, &bradford, &adapt);

	return adapt;
}

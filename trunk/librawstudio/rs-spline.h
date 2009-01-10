/*****************************************************************************
 * Bicubic spline implementation
 * 
 * Copyright (C) 2007 Edouard Gomez <ed.gomez@free.fr>
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
 ****************************************************************************/

#ifndef RS_SPLINE_H
#define RS_SPLINE_H

#include <glib.h>

/** Spline curve runout type */
typedef enum rs_spline_runout_type_t
{
	NATURAL=1,
	PARABOLIC,
	CUBIC,
} rs_spline_runout_type_t;

/** Spline curve - Opaque declaration */
typedef struct rs_spline_t rs_spline_t;

/**
 * Cubic spline constructor.
 * @param knots Array of knots, can be NULL if no knots yet
 * @param n Number of knots
 * @param runout_type Type of the runout
 * @return Spline
 */
extern rs_spline_t *
rs_spline_new(
	const gfloat *const knots,
	const gint n,
	const rs_spline_runout_type_t runout_type);

/**
 * Attribute getter
 * @return Number of knots
 */
extern guint
rs_spline_length(rs_spline_t *spline);

/**
 * Adds a knot to the curve
 * @param spline Spline to be used
 * @param x X coordinate
 * @param y Y coordinate
 */
extern void
rs_spline_add(rs_spline_t *spline, gfloat x, gfloat y);

/**
 * Moves a knot in the curve
 * @param spline Spline to be used
 * @param n Which knot to move
 * @param x X coordinate
 * @param y Y coordinate
 */
extern void
rs_spline_move(struct rs_spline_t *spline, gint n, gfloat x, gfloat y);

/**
 * Deletes a knot in the curve
 * @param spline Spline to be used
 * @param n Which knot to delete
 */
extern void
rs_spline_delete(struct rs_spline_t *spline, gint n);

/**
 * Computes value of the spline at the x abissa
 * @param spline Spline to be used
 * @param x Cubic spline parametric (in)
 * @param y Interpolated value (out)
 * @return 0 if failed, can happen when the spline is to be calculated again.
 */
extern gint
rs_spline_interpolate(rs_spline_t *spline, gfloat x, gfloat *y);

/**
 * Gets a copy of the internal sorted knot array (gfloat[2])
 * @param spline Spline to be used
 * @param knots Output knots (out, allocated with g_malloc)
 * @param n Output number of knots (out)
 */
extern void
rs_spline_get_knots(rs_spline_t *spline, gfloat **knots, guint *n);

/**
 * Sample the curve
 * @param spline Spline to be used
 * @param samples Pointer to output array or NULL
 * @param nbsamples number of samples
 * @return Sampled curve or NULL if failed
 */
gfloat *
rs_spline_sample(struct rs_spline_t *spline, gfloat *samples, guint nbsamples);

/**
 * Cubic spline destructor.
 * @param spline Spline structure to be destroyed
 */
extern void
rs_spline_destroy(rs_spline_t *spline);

/**
 * Print a spline on the stdout
 * @param spline Spline curve
 */
extern void
rs_spline_print(rs_spline_t *spline);

#endif /* RS_SPLINE_H */

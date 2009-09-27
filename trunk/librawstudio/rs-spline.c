/*****************************************************************************
 * Cubic spline implementation
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

#include "rs-spline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Spline curve - Real definition */
struct _RSSpline {
	GObject parent;
	gboolean dispose_has_run;

	/** Number of knots */
	guint n;

	/** Runout type */
	rs_spline_runout_type_t type;

	/** Knots as an array of gfloat[2].
	 * x is [0] and y is [1] */
	gfloat *knots;

	/** Cubic curves as an array of gfloat[4].
	 * The cubic a*x^3 + b*x^2 + c*x + d is stored as follows:
	 * a is [0], b is [1], c is [2], d is [3]*/
	gfloat *cubics;

	/** Tells if the curve needs internal update before using the cubics
	 * attribute */
	gint dirty;

	/** Knots (gfloat[2]) added and not yet merged in the ordered knots
	 * array attribute */
	GSList *added;
};

G_DEFINE_TYPE(RSSpline, rs_spline, G_TYPE_OBJECT)

static void knot_free(gpointer knot, gpointer userdata);

static void
rs_spline_dispose(GObject *object)
{
	RSSpline *spline = RS_SPLINE(object);

	if (!spline->dispose_has_run)
	{
		spline->dispose_has_run = TRUE;
		g_free(spline->knots);

		g_free(spline->cubics);

		g_slist_foreach(spline->added, (GFunc)knot_free, NULL);
		g_slist_free(spline->added);
	}
	G_OBJECT_CLASS(rs_spline_parent_class)->dispose(object);
}

static void
rs_spline_class_init(RSSplineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_spline_dispose;
}

static void
rs_spline_init(RSSpline *spline)
{
}

#define MERGE_KNOTS    (1<<0)
#define SORT_KNOTS     (1<<1)
#define COMPUTE_CUBICS (1<<2)
#define DIRTY(a, b) do { (a) |= (b); } while (0)
#define UNDIRTY(a, b) do { (a) &= ~(b); } while (0)
#define ISDIRTY(a, b) (!!((a)&(b)))

/*
 * Tridiagonal matrix solver.
 * 
 * Solving a tridiagonal NxN matrix can be explained as followed:
 *
 * [b0 c0  0  0  0  0  0  0  0   0  ] [  d0]   [  r0]
 * [a1 b1 c1  0                  0  ] [  d1]   [  r1]
 * [ 0 a2 b2 c2  0               0  ] [  d2]   [  . ]
 * [ 0  0 a3 b3 c3   0           0  ] [  . ]   [  . ]
 * [ 0     0  .  .  .    0       0  ]x[  . ] = [  . ]  (1)
 * [ 0        0  .   .   .   0   0  ] [dN-4]   [rN-4]
 * [ 0   0       0  .      .     0  ] [dN-3]   [rN-3]
 * [ 0              0 aN-2 bN-2 cN-2] [dN-2]   [rN-2]
 * [ 0  0  0  0  0  0    0 aN-1 bN-1] [dN-1]   [rN-1]
 *
 * Let's call a(n) the sub diagonal coefficients suite, b(n) the diagonal
 * suite, c(n) the super diagonal suite, and r(n) the result vector (right
 * member of the matrix equation (1)
 *
 * We will prove we can solve this recursively:
 * For n=0, we have:
 * b0*d0 + c0*d1 = r0 <=> d0 = r0/b0 - c0/b0*d1
 *                        d0 = f0 - g0*d1
 *
 * With:
 *   f0 = r0/b0                                        (2)
 *   g0 = c0/b0                                        (3)
 *
 * Let's suppose this for 0<n<N-1:
 *   dn-1 = fn-1 - gn-1*dn                             (4)
 *
 * We must prove it's still true at order n+1.
 * Using (1)
 *     rn = an*dn-1 + bn*dn + cn*dn+1
 * <=> rn = an*(fn-1 - gn-1*dn) + bn*dn + cn*dn+1
 * <=> rn = an*fn-1 + (bn - an*gn-1)*dn + cn*dn+1
 * <=> dn = [(rn - an*fn-1) - cn*dn+1]/(bn - an*gn-1)
 * <=> dn = fn - gn*dn+1
 *
 * Where fn = (rn - an*fn-1)/(bn - an*gn-1)           (5)
 *   and gn = cn/(bn - an*gn-1)                       (6)
 *
 * Proved :-)
 *
 * Let's see the final line of the matrix:
 *     aN-1*dN-2 + bN-1*dN-1 = rN-1
 * <=> aN-1(fN-2 - gN-2*dN-1) + bN-1*dN-1 = rN-1
 * <=> dN-1 = (rN-1 - aN-1*fN-2)/(bN-1 - aN-1*gN-2)   (7)
 *
 * So we can extend the recursive proof by one with just fixing g[n-1] = 0.
 * and keeping in mind that dN-1 = fN-1
 *
 * In the end to solve the tridiagonal matrix we must determine the f(n) suite
 * and g(n) suite using (5), (6) and the initial values in (2), (3). Then we
 * will be able to cascade the result bottom-top using (4)
 *
 * Make just sure that bn-an*gn is never 0 :-) which just doesn't happen if
 * |bn| > |an| + |cn|
 */

/**
 * Tridiagonal matrix solver.
 * @param d Solution vector (out)
 * @param r Result vector (in)
 * @param a a(n) suite of sub diagonal coefficients (a[0] is not used) (in)
 * @param b b(n) suite of diagonal coefficients (in)
 * @param c c(n) suite of sub diagonal coefficients (c[n-1] is not used) (in)
 * @param n Vector size (in)
 * @return 0 if failed, 1 if succeeded
 */
static gint
matrix_tridiagonal_solve(
	gfloat *d,
	gint n,
	const gfloat *const r,
	const gfloat *const a,
	const gfloat *const b,
	const gfloat *const c)
{
	/* Iterator */
	gint i;

	/* Temporary that will hold the (bn - an*gn) value */
	gfloat bet;

	/* 1st pass: contains g(n) */
	gfloat *g;

	/* Alloc space */
	g = g_malloc(n*sizeof(gfloat));

	/* 1st pass: Decomposition and forward substitution */

	/* First element is special - see (2), (3) */
	bet = b[0];
	d[0] = r[0]/bet;
	g[0] = c[0]/bet;
 
	/* Treat all rows it appears we can treat them all the same way for
	 * i>=1, see (5), (6) and (7) */
	for (i=1; i<n; i++) {
		/* Compute the common divisor */
		bet = b[i] - a[i]*g[i-1];
		if (bet == 0.0f) {
			/* This algorithm cannot work */
			g_free(g);
			return 0;
		}

		/* Use (5) */
		d[i] = (r[i] - a[i]*d[i-1])/bet;

		/* Use (6) */
		g[i] = c[i]/bet;
	}

	/* Use (4) backward to cascade the results bottom/up.
	 * We already know d[n-1] holds the solution, see (7) */
	for (i=n-2; i>=0; i--) {
		d[i] -= g[i]*d[i+1];
	}

#ifdef RSSplineEST
#define EPSILON (0.01f)
	g[0] = b[0]*d[0] + c[0]*d[1];
	for (i=1; i<n-1; i++) {
		g[i] = a[i]*d[i-1] + b[i]*d[i] + c[i]*d[i+1];
	}
	g[n-1] = a[n-1]*d[n-2] + b[n-1]*d[n-1];
	for (i=0; i<n; i++) {
		g_assert(g[i] >= (r[i]-EPSILON) && g[i] <= (r[i]+EPSILON));
	}
#endif

	g_free(g);

	return 1;
}

/* A cubic spline is a function defined as follows:
 *
 *        { s0(x) for x0<=x<x1
 * f(x) = { s1(x) for x1<=x<x2
 *        { ...
 *        { sN-1(x) for xN-2<=x<xN-1
 *
 * Where the series of functions sn(x) can be written as
 * sn(x) = an*(x-xn)^3 + bn*(x-xn)^2 + cn*(x-xn) + dn
 *
 * With an additional property:
 *  - f(x) is C2 continuous
 *  - f(xn) = yn (the sampling points)
 *
 * From these two constraints, we will deduce most of the stuff we need:
 * 1)      f(xn) = yn
 *    <=> sn(xn) = yn
 *    <=>     dn = yn
 * 2) The function is continuous so:
 *        sn+1(xn+1) = sn(xn+1)
 *    <=>       dn+1 = an*(xn+1-xn)^3 + bn*(xn+1-xn)^2 + cn*(xn+1-xn) +dn
 *    <=>       dn+1 = an*hn^3 + bn*hn^2 + cn*hn + dn
 *    with hn = xn+1 - xn, for 0<=n<N-1
 * 3) The first derivative is continuous so:
 *        s'n+1(xn+1) = s'n(xn+1)
 *    <=>        cn+1 = 3*an*hn^2 + 2*bn*hn + cn
 * 4) The second derivative is continuous so:
 *        s''n+1(xn+1) = s''n(xn+1)
 *    <=>        2*bn+1 = 6*an*hn + 2*bn
 * 5) With all this in mind, we will simplify the writings of the 4 previous
 *    equations introducing Mn as the value of the second derivative of f(x)
 *    at the sampling points positions:
 *        s''n(xn) = 2*bn = Mn
 *    <=>              bn = Mn/2
 *
 *  Rewriting 4)
 *  Mn+1 = 6*an*hn + Mn <=> an = (Mn+1-Mn)/(6*hn)
 *
 *  Rewriting 2)
 *       dn+1 = an*hn^3 + bn*hn^2 + cn*hn + dn
 *  <=>  yn+1 = hn^3*(Mn+1-Mn)/(6*hn) + Mn/2*hn^2 + cn*hn + yn
 *  <=> cn*hn = (Mn+1+2*Mn)/6*hn^2 + (yn+1-yn)
 *  <=>    cn = hn*(Mn+1+2*Mn)/6 + (yn+1-yn)/hn
 *
 *  So we end up having
 *   an = (Mn+1-Mn)/(6*hn)
 *   bn = Mn/2
 *   cn = hn*(Mn+1+2*Mn)/6 + (yn+1-yn)/hn
 *   dn = yn
 *
 *  Replacing all these coefficients definition in 3) gives us this nice
 *  equation:
 *  hn*Mn + 2*(hn+hn+1)*Mn+1 + hn+1*Mn+2 = 6*[(yn+2-yn+1)/hn+1 - (yn+1-yn)/hn]
 *
 *  Which can be represented as a linear equation set like this:
 * [h0 2*(h0+h1) h1  0  0  0  0  0  0  0  0   0   ] [  M0]   [6*(dy1/h1 - dy0/h0)] 
 * [ 0 h1 2*(h1+h2) h2  0                     0   ] [  M1]   [6*(dy2/h2 - dy1/h1)]
 * [ 0 0 h2 2*(h2+h3) h3  0                   0   ] [  M2]   [6*(dy3/h3 - dy2/h2)]
 * [ 0    0 h3 2*(h3+h4) h4   0               0   ] [  . ]   [          .        ]
 * [ 0      0  .    .        .   0            0   ]x[  . ] = [          .        ]  (1)
 * [ 0        0    .     .   .        0       0   ] [MN-4]   [          .        ]
 * [ 0   0       0    .         .        0    0   ] [MN-3]   [6*(      ...      )]
 * [ 0              0 hN-3 2*(hN-3+hN-2) hN-2 0   ] [MN-2]   [6*(      ...      )]
 * [ 0  0  0  0  0  0    0 hN-2 2*(hN-2+hN-1) hN-1] [MN-1]   [6*(      ...      )]
 *                N columns x (N-2) rows matrix    x N length vector   = N-2 length vector
 *
 * So it's still necessary to fix 2 more conditions to have a solvable system.
 * It's usual to fix these conditions on the two ends of the spline to control
 * its behavior... that's the runout type, natural, parabolic, cubic etc...
 *
 *   Natural runout: M0 = 0 and Mn-1 = 0
 * Parabolic runout: M0 = M1 and Mn-1 = Mn-2
 *     Cubic runout: M0 = 2*M1-M2 and Mn-1 = 2*Mn-2 - Mn-3
 */
/* Some macros we'll use when dealing with the cubics or the knots */
#define _a(w) (spline->cubics[4*(w)])
#define _b(w) (spline->cubics[4*(w)+1])
#define _c(w) (spline->cubics[4*(w)+2])
#define _d(w) (spline->cubics[4*(w)+3])
#define _x(w) (spline->knots[2*(w)])
#define _y(w) (spline->knots[2*(w)+1])

/**
 * Compare x coordinate of a knot, for sorting purposes
 * @param arg1 First knot
 * @param arg2 Second knot
 * @return <ul>
 * <li>-1 when arg1&lt;arg2</li>
 * <li>0 when equal</li>
 * <li>1 when arg1&gt;arg2</li>
 * </ul>
 */
static int
compare_knot(const void *arg1, const void *arg2)
{
	gfloat *knot1 = (gfloat*)arg1;
	gfloat *knot2 = (gfloat*)arg2;

	if (knot1[0] == knot2[0]) {
		return 0;
	}
	if (knot1[0] > knot2[0]) {
		return 1;
	}
	return -1;
}

/**
 * Copy a knot at last position in the knots array attribute and updates the n
 * attribute accordingly. The knots array is supposed to have enough space for
 * the copy.
 * @param data knot to be copied
 * @param useradata Expected to be a RSSpline
 */
static void
knot_copy(gpointer data, gpointer userdata)
{
	RSSpline *spline = (RSSpline *)userdata;
	gfloat *knot = (gfloat*)data;

	spline->knots[2*spline->n]   = knot[0];
	spline->knots[2*spline->n+1] = knot[1];
	spline->n++;
}

/**
 * Free the space allocated for a knot
 * @param knot Knot to be freed
 * @param userdata Nothing
 */
static void
knot_free(gpointer knot, gpointer userdata)
{
	g_free(knot);
}

/**
 * Prepare the knots list. It merges the new knots inserted since last time
 * this method has been called and sort the knot array
 * @param spline Spline
 */
static void
knots_prepare(RSSpline *spline)
{
	if (ISDIRTY(spline->dirty, MERGE_KNOTS)) {
		guint nbadded = g_slist_length(spline->added);

		/* Merge the new points into the knots array */
		spline->knots = g_realloc(spline->knots, (spline->n +
					  nbadded)*sizeof(gfloat)*2);
		g_slist_foreach(spline->added, (GFunc)knot_copy, (gpointer)spline);
		g_slist_foreach(spline->added, (GFunc)knot_free, NULL);
		g_slist_free(spline->added);
		spline->added = NULL;
		UNDIRTY(spline->dirty, MERGE_KNOTS);
		DIRTY(spline->dirty, SORT_KNOTS);
	}

	if (ISDIRTY(spline->dirty, SORT_KNOTS) && spline->knots != NULL) {
		/* Order the knots */
		qsort(spline->knots, spline->n, sizeof(gfloat)*2, &compare_knot);
		UNDIRTY(spline->dirty, SORT_KNOTS);
		DIRTY(spline->dirty, COMPUTE_CUBICS);
	}
}
 
/**
 * Cubic Spline solver
 * @param spline Spline structure to be updated. The knots, n, and type
 * attributes must be initialized correctly.
 * @return 0 if failed
 */
static gint
spline_compute_cubics(RSSpline *spline)
{
	/* Sub diagonal */
	gfloat *a = NULL;

	/* Diagonal */
	gfloat *b = NULL;

	/* Super diagonal */
	gfloat *c = NULL;

	/* Solution */
	gfloat *m = NULL;

	/* Result */
	gfloat *r = NULL;

	/* Iterator */
	gint i;

	/* Deal with stupid cases */
	if (spline->n <= 1) {
		return 0;
	}

	/* Prepare the knots array */
	knots_prepare(spline);

	/* Let(s see if we have work to do */
	if (!ISDIRTY(spline->dirty, COMPUTE_CUBICS)) {
		return 1;
	}

	/* The case spline->n == 2 is special - Use linear interpolation */
	if (spline->n == 2) {
		/* Prepare the cubic coefficients directly */
		if (spline->cubics != NULL) {
			/* Free old cubics */
			g_free(spline->cubics);
			spline->cubics = NULL;
		}

		spline->cubics = g_malloc(sizeof(gfloat)*4);
		_a(0) = 0;
		_b(0) = 0;
		_c(0) = (_y(1) - _y(0))/(_x(1) -_x(0));
		_d(0) = _y(0);

		return 1;
	}

	/* Prepare the tridiagonal matrix resolution */
	r = g_malloc(sizeof(gfloat)*(spline->n-2));
	a = g_malloc(sizeof(gfloat)*(spline->n-2));
	b = g_malloc(sizeof(gfloat)*(spline->n-2));
	c = g_malloc(sizeof(gfloat)*(spline->n-2));
	m = g_malloc(sizeof(gfloat)*spline->n);
	for (i=0; i<(spline->n-2); i++) {
		gfloat dx1 = _x(i+1) - _x(i);
		gfloat dy1 = _y(i+1) - _y(i);
		gfloat dx2 = _x(i+2) - _x(i+1);
		gfloat dy2 = _y(i+2) - _y(i+1);
		r[i] = 6.0f*(dy2/dx2 - dy1/dx1);
		a[i] = dx1;
		b[i] = 2.0f*(dx1+dx2);
		c[i] = dx2;
	}

	/* Adapt matrix coefficients according to runout type */
	switch (spline->type) {
	case PARABOLIC:
		/* TODO */
		break;
	case CUBIC:
		/* TODO */
		break;
	case NATURAL:
	default:
		/* Nothing to do :-) */
		break;
	}

	/* Solve the tridiagonal matrix */
	i = matrix_tridiagonal_solve(&m[1], spline->n-2, r, a, b, c);
	g_free(r);
	g_free(a);
	g_free(b);
	g_free(c);
	if (!i) {
		g_free(m);
		return 0;
	}

	/* According to runout type, fill in the 0 and n element of m array */
	switch (spline->type) {
	case PARABOLIC:
		m[0]           = m[1];
		m[spline->n-1] = m[spline->n-2];
		break;
	case CUBIC:
		m[0]           = 2*m[1] - m[2];
		m[spline->n-1] = 2*m[spline->n-2] - m[spline->n-3];
		break;
	case NATURAL:
	default:
		m[0]           = 0.0f;
		m[spline->n-1] = 0.0f;
		break;
	}

	/* Prepare the cubic coefficients */
	if (spline->cubics != NULL) {
		/* Free old cubics */
		g_free(spline->cubics);
		spline->cubics = NULL;
	}
	spline->cubics = g_malloc(sizeof(gfloat)*(spline->n-1)*4);
	for (i=0; i<(spline->n-1); i++) {
		gfloat h = _x(i+1) - _x(i);
		_a(i) = (m[i+1] - m[i])/(6.0f*h);
		_b(i) = m[i]/2.0f;
		_c(i) = (_y(i+1) - _y(i))/h - h*(m[i+1] + 2.0f*m[i])/6.0f;
		_d(i) = _y(i);
	}

	/* We're done with m */
	g_free(m);

	/* UnMark the cubics bit */
	UNDIRTY(spline->dirty, COMPUTE_CUBICS);

	return 1;
}

/**
 * Attribute getter
 * @return Number of knots
 */
guint
rs_spline_length(RSSpline *spline)
{
	return spline->n + g_slist_length(spline->added);
}

/**
 * Adds a knot to the curve
 * @param spline Spline to be used
 * @param x X coordinate
 * @param y Y coordinate
 */
void
rs_spline_add(RSSpline *spline, gfloat x, gfloat y)
{
	gfloat *knot = g_malloc(sizeof(gfloat)*2);
	knot[0] = x;
	knot[1] = y;
	spline->added = g_slist_prepend(spline->added, knot);
	DIRTY(spline->dirty, MERGE_KNOTS);
}

/**
 * Moves a knot in the curve
 * @param spline Spline to be used
 * @param n Which knot to move
 * @param x X coordinate
 * @param y Y coordinate
 */
void
rs_spline_move(RSSpline *spline, gint n, gfloat x, gfloat y)
{
	spline->knots[n*2+0] = x;
	spline->knots[n*2+1] = y;

	DIRTY(spline->dirty, SORT_KNOTS);
	DIRTY(spline->dirty, COMPUTE_CUBICS);
}

/**
 * Deletes a knot in the curve
 * @param spline Spline to be used
 * @param n Which knot to delete
 */
extern void
rs_spline_delete(RSSpline *spline, gint n)
{
	gfloat *old_knots = spline->knots;
	gint i, target = 0;

	/* Allocate new array */
	spline->knots = g_new(gfloat, (spline->n-1)*2);

	/* Simply copy the old values, no fancy stuff */
	for(i=0;i<spline->n;i++)
	{
		if (i != n) /* copy everything but n */
		{
			spline->knots[target*2+0] = old_knots[i*2+0];
			spline->knots[target*2+1] = old_knots[i*2+1];
			target++;
		}
	}
	spline->n--;

	/* Free the old array */
	g_free(old_knots);

	/* There should be no need to force resort */
	DIRTY(spline->dirty, COMPUTE_CUBICS);
}

/**
 * Computes value of the spline at the x abissa
 * @param spline Spline to be used
 * @param x Cubic spline parametric (in)
 * @param y Interpolated value (out)
 * @return 0 if failed, can happen when the spline is to be calculated again.
 */
gint
rs_spline_interpolate(RSSpline *spline, gfloat x, gfloat *y)
{
	/* Iterator */
	gint j;

	/* Compute the spline */
	if (!spline_compute_cubics(spline)) {
		return 0;
	}

	/* Find the right cubics to interpolate from */
	for (j=0; j<(spline->n-1); j++) {
		if (x >= _x(j) && x < _x(j+1)) {
			break;
		}
	}

	/* Don't forget to compute x-xn */
	x -= _x(j);

	/* Fill in the out value */
	*y = x*(x*(x*_a(j) + _b(j)) + _c(j)) + _d(j);

	/* Success */
	return 1;
}

/**
 * Gets a copy of the internal sorted knot array (gfloat[2])
 * @param spline Spline to be used
 * @param knots Output knots (out)
 * @param n Output number of knots (out)
 */
void
rs_spline_get_knots(RSSpline *spline, gfloat **knots, guint *n)
{
	knots_prepare(spline);
	*n = rs_spline_length(spline);
	*knots = g_malloc(*n*sizeof(gfloat)*2);
	memcpy(*knots, spline->knots, *n*sizeof(gfloat)*2);
}

/**
 * Sample the curve
 * @param spline Spline to be used
 * @param samples Pointer to output array or NULL
 * @param nbsamples number of samples
 * @return Sampled curve or NULL if failed
 */
gfloat *
rs_spline_sample(RSSpline *spline, gfloat *samples, guint nbsamples)
{
	/* Iterator */
	guint i;

	/* Output array */
	if (!samples)
		samples = g_malloc(sizeof(gfloat)*nbsamples);
		
	/* Compute everything required */
	if (!spline_compute_cubics(spline)) {
		return NULL;
	}

	if ((spline->n>1) && spline->knots)
	{
		/* Find the sample number for first and last knot */
		const gint start = spline->knots[0*2+0]*((gfloat)nbsamples);
		const gint stop = spline->knots[(spline->n-1)*2+0]*((gfloat)nbsamples);

		/* Allocate space for output, if not given */
		if (!samples)
			samples = g_new(gfloat, nbsamples);

		/* Sample between knots */
		for (i=0; i<(stop-start); i++)
		{
			gfloat x = ((gfloat)i)*(_x(spline->n-1) - _x(0))/(gfloat)(stop-start) + _x(0);
			/* We can safely ignore the return value because the call to
		 	* compute_spline has successfully returned a few lines
		 	* upper */
			rs_spline_interpolate(spline, x, &(samples+start)[i]);
		}

		/* Sample flat curve before first knot */
		for(i=0;i<start;i++)
			samples[i] = spline->knots[0*2+1];

		/* Sample flat curve after last knot */
		for(i=stop;i<nbsamples;i++)
			samples[i] = spline->knots[(spline->n-1)*2+1];
	}

	return samples;
}

/**
 * Cubic spline constructor.
 * @param knots Array of knots
 * @param n Number of knots
 * @param runout_type Type of the runout
 * @return Spline
 */
RSSpline *
rs_spline_new(
	const gfloat *const knots,
	const gint n,
	const rs_spline_runout_type_t runout_type)
{
	/* Ordered knots */
	gfloat *k = NULL;

	/* Copy the knots */
	if (knots != NULL) {
		k = g_malloc(sizeof(gfloat)*2*n);
		memcpy(k, knots, sizeof(gfloat)*2*n);
	}

	/* Prepare the result */
	RSSpline *new = g_object_new(RS_TYPE_SPLINE, NULL);
	new->knots = k;
	new->cubics = NULL;
	new->n = (k!=NULL) ? n: 0;
	new->type = runout_type;
	new->added = NULL;
	new->dirty = 0;
	DIRTY(new->dirty, SORT_KNOTS);
	DIRTY(new->dirty, COMPUTE_CUBICS);

	return new;
}

/**
 * Print a spline on the stdout
 * @param spline Spline curve
 */
void
rs_spline_print(RSSpline *spline)
{
	/* Iterator */
	guint i;

	/* Samples */
	gfloat *samples = rs_spline_sample(spline, NULL, 512);

	/* Print the spline */
	printf("\n\n# Spline\n");
	for (i=0; i<(spline->n-1); i++) {
		printf("# [(%.2f,%.2f) (%.2f,%.2f)] an=%.2f bn=%.2f cn=%.2f dn=%.2f\n",
		       _x(i), _y(i), _x(i+1), _y(i+1), _a(i), _b(i), _c(i), _d(i));
	}
	for (i=0; i<512; i++) {
		printf("%f\n", samples[i]);
	}

	g_free(samples);
}
#undef _a
#undef _b
#undef _c
#undef _d
#undef _x
#undef _y

#ifdef RSSplineTEST
typedef struct test_t
{
	gint size;
	const gfloat *knots;
} test_t;

typedef const test_t *const test_t_ptr;

int
main(int argc, char **argv)
{
	/* A simple S-curve */
	const gfloat scurve_knots[] = {
		0.625f, 0.75f,
		0.125f, 0.25f,
		0.5f, 0.5f,
		0.0f, 0.0f,
		1.0f, 1.0f
	};

	const test_t scurve = {
		sizeof(scurve_knots)/(sizeof(scurve_knots[0])*2),
		scurve_knots
	};

	/* A simple line */
	const gfloat line_knots[] = {
		0.0f, 0.0f,
		0.25f, 0.25f,
		0.5f, 0.5f,
		0.75f, 0.75f,
		1.0f, 1.0f
	};
	const test_t line = {
		sizeof(line_knots)/(sizeof(line_knots[0])*2),
		line_knots
	};

	/* Tests */
	test_t_ptr tests[] = {
		&line,
		&scurve
	};

	/* All types */
	const rs_spline_runout_type_t runouts[] = {
		NATURAL,
		PARABOLIC,
		CUBIC,
	};

	/* Spline result */
	RSSpline *spline;

	/* Iterators */
	gint i;
	gint j;

	for (i=0; i<sizeof(tests)/sizeof(tests[0]); i++) {
		for (j=0; j<sizeof(runouts)/sizeof(runouts[0]); j++) {
			guint size = tests[i]->size;

			/* Create the spline */
			spline = rs_spline_new(tests[i]->knots, size-1, j);

			/* Make sure everything is ok */
			if (spline == NULL) {
				g_warning("The spline could not be computed\n");
				continue;
			}

			/* Add the last point */
			rs_spline_add(spline,
				      tests[i]->knots[(size-1)*2],
				      tests[i]->knots[(size-1)*2 + 1]);

			/* Print it to stdio */
			rs_spline_print(spline);

			/* Destory it */
			g_object_unref(spline);
		}
	}
	return 0;
}
#endif /* RSSplineTEST */

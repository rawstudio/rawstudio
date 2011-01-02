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

#include "rs-1d-function.h"

G_DEFINE_TYPE(RS1dFunction, rs_1d_function, G_TYPE_OBJECT)

static void
rs_1d_function_class_init(RS1dFunctionClass *klass)
{
}

static void
rs_1d_function_init(RS1dFunction *func)
{
}

/**
 * Instantiate a new RS1dFunction, it will behave as an identity function (y = x)
 * @return A new RS1dFunction with arefcount of 1.
 */
RS1dFunction *
rs_1d_function_new(void)
{
	return g_object_new (RS_TYPE_1D_FUNCTION, NULL);
}

/**
 * Behaves like #rs_1d_function_new but returns a singleton
 * @return A new RS1dFunction singleton which should not be unreffed
 */
const RS1dFunction *
rs_1d_function_new_singleton(void)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static RS1dFunction *func = NULL;

	g_static_mutex_lock(&lock);
	if (!func)
		func = rs_1d_function_new();
	g_static_mutex_unlock(&lock);

	return func;
}

/**
 * Map x to a new y value
 * @param func A RS1dFunction
 * @param x An input parameter in the range 0.0-1.0
 * @return Mapped value for x
 */
gdouble
rs_1d_function_evaluate(const RS1dFunction *func, const gdouble x)
{
	g_assert(RS_IS_1D_FUNCTION(func));

	RS1dFunctionEvaluate *evaluate = RS_1D_FUNCTION_GET_CLASS(func)->evaluate;

	if (evaluate)
		return evaluate(func, x);
	else
		return x;
}

/**
 * Map y to a new x value
 * @param func A RS1dFunction
 * @param x An input parameter in the range 0.0-1.0
 * @return Inverse value for y
 */
gdouble
rs_1d_function_evaluate_inverse(const RS1dFunction *func, const gdouble y)
{
	g_assert(RS_IS_1D_FUNCTION(func));

	RS1dFunctionEvaluate *evaluate_inverse = RS_1D_FUNCTION_GET_CLASS(func)->evaluate_inverse;

	if (evaluate_inverse)
		return evaluate_inverse(func, y);
	else
		return y;
}

/**
 * Return TRUE if rs_1d_function_evaluate(#func, x) == x for all x
 * @param func A RS1dFunction
 * @return TRUE if rs_1d_function_evaluate(#func, x) == x for all x, FALSE otherwise
 */
gboolean
rs_1d_function_is_identity(const RS1dFunction *func)
{
	g_assert(RS_IS_1D_FUNCTION(func));

	RS1dFunctionIsIdentity *is_identity = RS_1D_FUNCTION_GET_CLASS(func)->is_identity;
	RS1dFunctionEvaluate *evaluate = RS_1D_FUNCTION_GET_CLASS(func)->evaluate;

	if (!is_identity && !evaluate)
		return TRUE;
	else if (is_identity)
		return is_identity(func);
	else
		return FALSE;
}

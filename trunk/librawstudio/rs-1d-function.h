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

#ifndef RS_1D_FUNCTION_H
#define RS_1D_FUNCTION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_1D_FUNCTION rs_1d_function_get_type()
#define RS_1D_FUNCTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_1D_FUNCTION, RS1dFunction))
#define RS_1D_FUNCTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_1D_FUNCTION, RS1dFunctionClass))
#define RS_IS_1D_FUNCTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_1D_FUNCTION))
#define RS_IS_1D_FUNCTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_1D_FUNCTION))
#define RS_1D_FUNCTION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_1D_FUNCTION, RS1dFunctionClass))

typedef struct {
	GObject parent;
} RS1dFunction;

typedef gdouble (RS1dFunctionEvaluate)(const RS1dFunction *func, const gdouble);
typedef gboolean (RS1dFunctionIsIdentity)(const RS1dFunction *func);

typedef struct {
	GObjectClass parent_class;

	RS1dFunctionIsIdentity *is_identity;
	RS1dFunctionEvaluate *evaluate;
	RS1dFunctionEvaluate *evaluate_inverse;
} RS1dFunctionClass;

GType rs_1d_function_get_type(void);

/**
 * Instantiate a new RS1dFunction, it will behave as an identity function (y = x)
 * @return A new RS1dFunction with arefcount of 1.
 */
RS1dFunction *
rs_1d_function_new(void);

/**
 * Behaves like #rs_1d_function_new but returns a singleton
 * @return A new RS1dFunction singleton which should not be unreffed
 */
const RS1dFunction *
rs_1d_function_new_singleton(void);

/**
 * Map x to a new y value
 * @param func A RS1dFunction
 * @param x An input parameter in the range 0.0-1.0
 * @return Mapped value for x
 */
gdouble
rs_1d_function_evaluate(const RS1dFunction *func, gdouble x);

/**
 * Map y to a new x value
 * @param func A RS1dFunction
 * @param x An input parameter in the range 0.0-1.0
 * @return Inverse value for y
 */
gdouble
rs_1d_function_evaluate_inverse(const RS1dFunction *func, gdouble y);

/**
 * Return TRUE if rs_1d_function_evaluate(#func, x) == x for all x
 * @param func A RS1dFunction
 * @return TRUE if rs_1d_function_evaluate(#func, x) == x for all x, FALSE otherwise
 */
gboolean
rs_1d_function_is_identity(const RS1dFunction *func);

G_END_DECLS

#endif /* RS_1D_FUNCTION_H */

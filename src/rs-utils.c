/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

#include <glib.h>

/**
 * A version of atof() that isn't locale specific
 * @note This doesn't do any error checking!
 * @param str A NULL terminated string representing a number
 * @return The number represented by str or 0.0 if str is NULL
 */
gdouble
rs_atof(const gchar *str)
{
	gdouble result = 0.0f;
	gdouble div = 1.0f;
	gboolean point_passed = FALSE;

	gchar *ptr = (gchar *) str;

	while(str && *ptr)
	{
		if (g_ascii_isdigit(*ptr))
		{
			result = result * 10.0f + g_ascii_digit_value(*ptr);
			if (point_passed)
				div *= 10.0f;
		}
		else if (*ptr == '-')
			div *= -1.0f;
		else if (g_ascii_ispunct(*ptr))
			point_passed = TRUE;
		ptr++;
	}

	return result / div;
}


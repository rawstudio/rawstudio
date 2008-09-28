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

#ifndef RS_UTILS_H
#define RS_UTILS_H

#include <glib.h>

/**
 * A version of atof() that isn't locale specific
 * @note This doesn't do any error checking!
 * @param str A NULL terminated string representing a number
 * @return The number represented by str or 0.0 if str is NULL
 */
extern gdouble rs_atof(const gchar *str);

/**
 * A convenience function to convert an EXIF timestamp to a unix timestamp.
 * @note This will only work until 2038 unless glib fixes its GTime
 * @param str A NULL terminated string containing a timestamp in the format "YYYY:MM:DD HH:MM:SS" (EXIF 2.2 section 4.6.4)
 * @return A unix timestamp or -1 on error
 */
GTime
rs_exiftime_to_unixtime(const gchar *str);

/**
 * A convenience function to convert an unix timestamp to an EXIF timestamp.
 * @note This will only work until 2038 unless glib fixes its GTime
 * @param timestamp A unix timestamp
 * @return A string formatted as specified in EXIF 2.2 section 4.6.4
 */
gchar *
rs_unixtime_to_exiftime(GTime timestamp);

/**
 * Constrains a box to fill a bounding box without changing aspect
 * @param target_width The width of the bounding box
 * @param target_height The height of the bounding box
 * @param width The input and output width
 * @param height The input and output height
 */
extern void
rs_constrain_to_bounding_box(gint target_width, gint target_height, gint *width, gint *height);

/**
 * Try to count the number of processor cores in a system.
 * @note This currently only works for systems with /proc/cpuinfo
 * @return The numver of cores or 1 if the system is unsupported
 */
extern gint
rs_get_number_of_processor_cores();

/**
 * Return a path to the current config directory for Rawstudio - this is the
 * .rawstudio direcotry in home
 * @return A path to an existing directory
 */
extern const gchar *
rs_confdir_get();

/**
 * Return a cache directory for filename
 * @param filename A complete path to a photo
 * @return A directory to hold the cache. This is guarenteed to exist
 */
extern gchar *
rs_dotdir_get(const gchar *filename);

/**
 * Normalize a RS_RECT, ie makes sure that x1 < x2 and y1<y2
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 */
extern void
rs_rect_normalize(RS_RECT *in, RS_RECT *out);

/**
 * Flip a RS_RECT
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 * @param w The width of the data OUTSIDE the RS_RECT
 * @param h The height of the data OUTSIDE the RS_RECT
 */
extern void
rs_rect_flip(RS_RECT *in, RS_RECT *out, gint w, gint h);

/**
 * Mirrors a RS_RECT
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 * @param w The width of the data OUTSIDE the RS_RECT
 * @param h The height of the data OUTSIDE the RS_RECT
 */
extern void
rs_rect_mirror(RS_RECT *in, RS_RECT *out, gint w, gint h);

/**
 * Rotate a RS_RECT in 90 degrees steps
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 * @param w The width of the data OUTSIDE the RS_RECT
 * @param h The height of the data OUTSIDE the RS_RECT
 * @param quarterturns How many times to turn the rect clockwise
 */
extern void
rs_rect_rotate(RS_RECT *in, RS_RECT *out, gint w, gint h, gint quarterturns);

#endif /* RS_UTILS_H */

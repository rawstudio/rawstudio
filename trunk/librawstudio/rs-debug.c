/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>,
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

#include "rs-debug.h"

guint rs_debug_flags = 0;

static const GDebugKey rs_debug_keys[] = {
	{ "all", RS_DEBUG_ALL },
	{ "plugins", RS_DEBUG_PLUGINS },
	{ "filters", RS_DEBUG_FILTERS },
	{ "performance", RS_DEBUG_PERFORMANCE },
	{ "processing", RS_DEBUG_PROCESSING }
};

void
rs_debug_setup(const gchar *debug_string)
{
	rs_debug_flags = g_parse_debug_string(debug_string, rs_debug_keys, G_N_ELEMENTS(rs_debug_keys));
}

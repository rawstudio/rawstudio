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

#ifndef RS_DEBUG_H
#define RS_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	RS_DEBUG_ALL         = 0xffffffff,
	RS_DEBUG_PLUGINS     = 1 << 0,
	RS_DEBUG_FILTERS     = 1 << 1,
	RS_DEBUG_PERFORMANCE = 1 << 2,
	RS_DEBUG_PROCESSING  = 1 << 3,
	RS_DEBUG_LIBRARY     = 1 << 4,
} RSDebugFlag;

#define RS_DEBUG(type,x,a...) \
G_STMT_START { \
	if (G_UNLIKELY (rs_debug_flags & RS_DEBUG_##type)) \
	{ \
		printf("* Debug [" #type "] " G_STRLOC ": " x "\n", ##a); \
	} \
} G_STMT_END

void
rs_debug_setup(const gchar *debug_string);

extern guint rs_debug_flags;

G_END_DECLS

#endif /* RS_DEBUG_H */

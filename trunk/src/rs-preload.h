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

#ifndef RS_PRELOAD_H
#define RS_PRELOAD_H
#include "rawstudio.h"

/**
 * Empty the near list
 */
extern void
rs_preload_near_remove_all();

/**
 * Add a filename to the near list, the near list is a previous or following
 * number of photos, given by rs_preload_get_near_count() from the currently
 * selected image
 * @param filename A filename
 */
extern void
rs_preload_near_add(const gchar *filename);

/**
 * Get a preloaded photo
 * @param filename A filename
 * @return The new RS_PHOTO or NULL if not preloaded
 */
extern RS_PHOTO *
rs_get_preloaded(const gchar *filename);

/**
 * Get a qualified guess about how many photos we would like to have marked as
 * near
 * @return A number of photos to mark as near
 */
extern gint
rs_preload_get_near_count();

/**
 * Set the maximum amount of memory to be used for preload buffer
 * @param max A value in bytes, everything below 100MB will be interpreted as 0
 */
void
rs_preload_set_maximum_memory(size_t max);

#endif /* RS_PRELOAD_H */

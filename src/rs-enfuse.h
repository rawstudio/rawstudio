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

#ifndef RS_ENFUSE_H
#define RS_ENFUSE_H

#include "application.h"


#define ENFUSE_METHOD_EXPOSURE_BLENDING "Exposure blending"
#define ENFUSE_METHOD_EXPOSURE_BLENDING_ID 0
#define ENFUSE_OPTIONS_EXPOSURE_BLENDING "--exposure-weight=1 --saturation-weight=0.2 --contrast-weight=0 --soft-mask"

#define ENFUSE_METHOD_FOCUS_STACKING "Focus stacking"
#define ENFUSE_METHOD_FOCUS_STACKING_ID 1
#define ENFUSE_OPTIONS_FOCUS_STACKING "--exposure-weight=0 --saturation-weight=0 --contrast-weight=1 --hard-mask"


extern gchar * rs_enfuse(RS_BLOB *rs, GList *files, gboolean quick, gint boundingbox);
extern gboolean rs_has_enfuse (gint major, gint minor);

#endif /* RS_ENFUSE_H  */

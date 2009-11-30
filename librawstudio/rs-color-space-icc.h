/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#ifndef RS_COLOR_SPACE_ICC_H
#define RS_COLOR_SPACE_ICC_H

#include <glib-object.h>
#include "rs-color-space.h"

G_BEGIN_DECLS

#define RS_TYPE_COLOR_SPACE_ICC rs_color_space_icc_get_type()
#define RS_COLOR_SPACE_ICC(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_COLOR_SPACE_ICC, RSColorSpaceIcc))
#define RS_COLOR_SPACE_ICC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_COLOR_SPACE_ICC, RSColorSpaceIccClass))
#define RS_IS_COLOR_SPACE_ICC(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_COLOR_SPACE_ICC))
#define RS_IS_COLOR_SPACE_ICC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_COLOR_SPACE_ICC))
#define RS_COLOR_SPACE_ICC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_COLOR_SPACE_ICC, RSColorSpaceIccClass))

typedef struct {
	RSColorSpace parent;
	gboolean dispose_has_run;

	RSIccProfile *icc_profile;
} RSColorSpaceIcc;

typedef struct {
	RSColorSpaceClass parent_class;
} RSColorSpaceIccClass;

GType rs_color_space_icc_get_type(void);

RSColorSpaceIcc *rs_color_space_icc_new_from_profile(RSIccProfile *icc_profile);

RSColorSpace *rs_color_space_icc_new_from_file(const gchar *path);

G_END_DECLS

#endif /* RS_COLOR_SPACE_ICC_H */

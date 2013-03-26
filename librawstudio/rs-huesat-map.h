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

#ifndef RS_HUESAT_MAP_H
#define RS_HUESAT_MAP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_HUESAT_MAP rs_huesat_map_get_type()
#define RS_HUESAT_MAP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_HUESAT_MAP, RSHuesatMap))
#define RS_HUESAT_MAP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_HUESAT_MAP, RSHuesatMapClass))
#define RS_IS_HUESAT_MAP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_HUESAT_MAP))
#define RS_IS_HUESAT_MAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_HUESAT_MAP))
#define RS_HUESAT_MAP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_HUESAT_MAP, RSHuesatMapClass))

typedef struct {
	GObject parent;

	guint hue_divisions;
	guint sat_divisions;
	guint val_divisions;

	guint hue_step;
	guint val_step;

	RS_VECTOR3 *deltas;
	gint v_encoding;	
} RSHuesatMap;

typedef struct {
	GObjectClass parent_class;
} RSHuesatMapClass;

GType rs_huesat_map_get_type(void);

RSHuesatMap *rs_huesat_map_new(guint hue_divisions, guint sat_division, guint val_divisions);

RSHuesatMap *rs_huesat_map_new_from_dcp(RSTiff *tiff, const guint ifd, const gushort dims_tag, const gushort table_tag);

guint rs_huesat_map_get_deltacount(RSHuesatMap *map);

void rs_huesat_map_get_delta(RSHuesatMap *map, const guint hue_div, const guint sat_div, const guint val_div, RS_VECTOR3 *modify);
void rs_huesat_map_set_delta(RSHuesatMap *map, const guint hue_div, const guint sat_div, const guint val_div, const RS_VECTOR3 *modify);

G_END_DECLS

#endif /* RS_HUESAT_MAP_H */

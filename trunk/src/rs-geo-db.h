/*
 * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>,
 * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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

#ifndef RS_GEO_DB_H
#define RS_GEO_DB_H

#include <rawstudio.h>
#include "application.h"
#include <sqlite3.h>
#include "osm-gps-map.h"

G_BEGIN_DECLS

#define RS_TYPE_GEO_DB rs_geo_db_get_type()
#define RS_GEO_DB(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_GEO_DB, RSGeoDb))
#define RS_GEO_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_GEO_DB, RSGeoDbClass))
#define RS_IS_GEO_DB(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_GEO_DB))
#define RS_IS_GEO_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_GEO_DB))
#define RS_GEO_DB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_GEO_DB, RSGeoDbClass))

typedef struct _RSGeoDb RSGeoDb;

typedef struct {
  GtkScrolledWindowClass parent_class;
} RSGeoDbClass;

GType rs_geo_db_get_type (void);

extern RSGeoDb *
rs_geo_db_new (void);
RSGeoDb *rs_geo_db_get_singleton(void);

extern GtkWidget * rs_geo_db_get_widget(RSGeoDb *geodb);
extern void rs_geo_db_set_coordinates(RSGeoDb *geodb, RS_PHOTO *photo);
extern void rs_geo_db_set_coordinates_manual(RSGeoDb *geodb, RS_PHOTO *photo, gdouble lon, gdouble lat);
void rs_geo_db_find_coordinate(RSGeoDb *geodb, gint timestamp);
void rs_geo_db_set_offset(RSGeoDb *geodb, RS_PHOTO *time_offset, gint offset);

#endif /* RS_GEO_DB */




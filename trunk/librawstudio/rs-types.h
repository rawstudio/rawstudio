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

#ifndef RS_TYPES_H
#define RS_TYPES_H

#include <gtk/gtk.h>

/* Defined in rawfile.c */
typedef struct _RAWFILE RAWFILE;

/* Defined in rs-image.c */
typedef struct _RSImage RSImage;

/* Defined in rs-image16.h */
typedef struct _rs_image16 RS_IMAGE16;

/* Defined in rs-metadata.h */
typedef struct _RSMetadata RSMetadata;

/* Defined in rs-color-transform.c */
typedef struct _RSColorTransform RSColorTransform;

typedef struct {
	GObject parent;

	gboolean dispose_has_run;

	gchar *filename;
	guchar *map;
	gsize map_length;

	gushort byte_order;
	guchar tiff_version;
	guint first_ifd_offset;
	guint num_ifd;
	GList *ifds;

	GType ifd_type;
	GType ifd_entry_type;
} RSTiff;

typedef struct {double coeff[3][3]; } RS_MATRIX3;
typedef struct {int coeff[3][3]; } RS_MATRIX3Int;
typedef struct {double coeff[4][4]; } RS_MATRIX4;
typedef struct {int coeff[4][4]; } RS_MATRIX4Int;

typedef struct {
		union { gfloat x; gfloat X; gfloat R; gfloat h; gfloat fHueShift; };
		union { gfloat y; gfloat Y; gfloat G; gfloat s; gfloat fSatScale; };
		union { gfloat z; gfloat Z; gfloat B; gfloat v; gfloat fValScale; };
} RS_VECTOR3;

typedef struct {
	gfloat x;
	gfloat y;
	gfloat padding_to_match_RS_VECTOR3;
} RS_xy_COORD;

typedef RS_VECTOR3 RS_XYZ_VECTOR;

typedef struct {
	gint x1;
	gint y1;
	gint x2;
	gint y2;
} RS_RECT;

typedef enum {
	R = 0,
	G = 1,
	B = 2,
	G2 = 3
} RSColor;

#endif /* RS_TYPES_H */


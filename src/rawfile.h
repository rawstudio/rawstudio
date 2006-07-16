/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

#define ENDIANSWAP4(a) (((a) & 0x000000FF) << 24 | ((a) & 0x0000FF00) << 8 | ((a) & 0x00FF0000) >> 8) | (((a) & 0xFF000000) >> 24)
#define ENDIANSWAP2(a) (((a) & 0x00FF) << 8) | (((a) & 0xFF00) >> 8)

typedef struct _rawfile {
	gint fd;
	guint size;
	void *map;
	gushort byteorder;
	guint first_ifd_offset;
} RAWFILE;

void raw_init();
RAWFILE *raw_open_file(const gchar *filename);
gboolean raw_get_uint(RAWFILE *rawfile, guint pos, guint *target);
gboolean raw_get_ushort(RAWFILE *rawfile, guint pos, gushort *target);
gboolean raw_get_float(RAWFILE *rawfile, guint pos, gfloat *target);
gboolean raw_get_uchar(RAWFILE *rawfile, guint pos, guchar *target);
gboolean raw_strcmp(RAWFILE *rawfile, guint pos, const gchar *needle, gint len);
gboolean raw_strcpy(RAWFILE *rawfile, guint pos, void *target, gint len);
GdkPixbuf *raw_get_pixbuf(RAWFILE *rawfile, guint pos, guint length);
void raw_close_file(RAWFILE *rawfile);

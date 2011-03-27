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
#ifndef RAWFILE_H
#define RAWFILE_H

#define ENDIANSWAP4(a) (((a) & 0x000000FF) << 24 | ((a) & 0x0000FF00) << 8 | ((a) & 0x00FF0000) >> 8) | (((a) & 0xFF000000) >> 24)
#define ENDIANSWAP2(a) (((a) & 0x00FF) << 8) | (((a) & 0xFF00) >> 8)

#include "rs-types.h"

void raw_init(void);
RAWFILE *raw_open_file(const gchar *filename);
RAWFILE *raw_create_from_memory(void *memory, guint size, guint first_ifd_offset, gushort byteorder);
guchar raw_init_file_tiff(RAWFILE *rawfile, guint pos);
gboolean raw_get_uint(RAWFILE *rawfile, guint pos, guint *target);
gboolean raw_get_ushort(RAWFILE *rawfile, guint pos, gushort *target);
gushort raw_get_ushort_from_string(RAWFILE *rawfile, gchar *source);
gboolean raw_get_short(RAWFILE *rawfile, guint pos, gshort *target);
gshort raw_get_short_from_string(RAWFILE *rawfile, gchar *source);
gboolean raw_get_float(RAWFILE *rawfile, guint pos, gfloat *target);
gboolean raw_get_uchar(RAWFILE *rawfile, guint pos, guchar *target);
gboolean raw_get_rational(RAWFILE *rawfile, guint pos, gfloat *target);
gboolean raw_strcmp(RAWFILE *rawfile, guint pos, const gchar *needle, gint len);
gboolean raw_strcpy(RAWFILE *rawfile, guint pos, void *target, gint len);
gchar *raw_strdup(RAWFILE *rawfile, guint pos, gint len);
GdkPixbuf *raw_get_pixbuf(RAWFILE *rawfile, guint pos, guint length);
void raw_close_file(RAWFILE *rawfile);
void raw_reset_base(RAWFILE *rawfile);
gint raw_get_base(RAWFILE *rawfile);
gushort raw_get_byteorder(RAWFILE *rawfile);
void raw_set_byteorder(RAWFILE *rawfile, gushort byteorder);
guint get_first_ifd_offset(RAWFILE *rawfile);
void *raw_get_map(RAWFILE *rawfile);
guint raw_get_filesize(RAWFILE *rawfile);

#endif /* RAWFILE_H */

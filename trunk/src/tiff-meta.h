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

void rs_tiff_load_meta(const gchar *filename, RS_METADATA *meta);
GdkPixbuf *rs_tiff_load_thumb(const gchar *src);

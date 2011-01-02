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

#ifndef RS_TIFF_H
#define RS_TIFF_H

extern gboolean rs_tiff8_save(GdkPixbuf *pixbuf, const gchar *filename, const gchar *profile_filename, gboolean uncompressed);
extern gboolean rs_tiff16_save(RS_IMAGE16 *image, const gchar *filename, const gchar *profile_filename, gboolean uncompressed);
RS_IMAGE16 *rs_tiff16_load(const gchar *filename);

#endif

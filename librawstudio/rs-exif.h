/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
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

#ifndef RS_EXIF_H
#define RS_EXIF_H

#ifdef  __cplusplus

extern "C" {
#endif /* __cplusplus */
#include <glib.h>
#include <rawstudio.h>

typedef void RS_EXIF_DATA;

extern RS_EXIF_DATA *rs_exif_load_from_file(const gchar *);
extern RS_EXIF_DATA *rs_exif_load_from_rawfile(RAWFILE *rawfile);
extern void rs_exif_add_to_file(RS_EXIF_DATA *d, const gchar *filename);
extern void rs_exif_free(RS_EXIF_DATA *d);
extern gboolean rs_exif_copy(const gchar *input_filename, const gchar *output_filename);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* RS_EXIF_H */

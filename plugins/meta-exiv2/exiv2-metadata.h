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

#ifndef exiv2_metadata_h__
#define exiv2_metadata_h__
#include <rawstudio.h>

#ifdef _unix_
G_BEGIN_DECLS
#endif

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

extern gboolean
exiv2_load_meta_interface(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta);


#ifdef _unix_
G_END_DECLS
#endif
#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif


#endif // exiv2_metadata_h__

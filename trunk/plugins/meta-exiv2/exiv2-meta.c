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

#include <rawstudio.h>
#include "exiv2-metadata.h"

static gboolean
exiv2_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
  return exiv2_load_meta_interface(service, rawfile, offset, meta);
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_meta_loader(".jpg", "8 bit JPEG File", exiv2_load_meta, 100, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".jpeg", "8 bit JPEG File", exiv2_load_meta, 100, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".png", "PNG File", exiv2_load_meta, 100, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".tiff", "TIFF File", exiv2_load_meta, 100, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".tif", "TIFF File", exiv2_load_meta, 100, RS_LOADER_FLAGS_RAW);
}

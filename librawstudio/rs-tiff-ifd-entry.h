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

#ifndef RS_TIFF_IFD_ENTRY_H
#define RS_TIFF_IFD_ENTRY_H

#include <rs-types.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_TIFF_IFD_ENTRY rs_tiff_ifd_entry_get_type()
#define RS_TIFF_IFD_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TIFF_IFD_ENTRY, RSTiffIfdEntry))
#define RS_TIFF_IFD_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TIFF_IFD_ENTRY, RSTiffIfdEntryClass))
#define RS_IS_TIFF_IFD_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TIFF_IFD_ENTRY))
#define RS_IS_TIFF_IFD_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_TIFF_IFD_ENTRY))
#define RS_TIFF_IFD_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_TIFF_IFD_ENTRY, RSTiffIfdEntryClass))

typedef struct {
	GObject parent;

	gushort tag;
	gushort type;
	guint count;
	guint value_offset;
} RSTiffIfdEntry;

typedef struct {
	GObjectClass parent_class;
} RSTiffIfdEntryClass;

GType rs_tiff_ifd_entry_get_type(void);

RSTiffIfdEntry *rs_tiff_ifd_entry_new(RSTiff *tiff, guint offset);

G_END_DECLS

#endif /* RS_TIFF_IFD_ENTRY_H */

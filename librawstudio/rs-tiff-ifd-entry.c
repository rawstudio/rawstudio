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

#include "rs-tiff-ifd-entry.h"
#include "rs-tiff.h"

G_DEFINE_TYPE (RSTiffIfdEntry, rs_tiff_ifd_entry, G_TYPE_OBJECT)

static void
rs_tiff_ifd_entry_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
rs_tiff_ifd_entry_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
rs_tiff_ifd_entry_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_tiff_ifd_entry_parent_class)->dispose (object);
}

static void
rs_tiff_ifd_entry_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_tiff_ifd_entry_parent_class)->finalize (object);
}

static void
rs_tiff_ifd_entry_class_init(RSTiffIfdEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = rs_tiff_ifd_entry_get_property;
	object_class->set_property = rs_tiff_ifd_entry_set_property;
	object_class->dispose = rs_tiff_ifd_entry_dispose;
	object_class->finalize = rs_tiff_ifd_entry_finalize;
}

static void
rs_tiff_ifd_entry_init(RSTiffIfdEntry *self)
{
}

RSTiffIfdEntry *
rs_tiff_ifd_entry_new(RSTiff *tiff, guint offset)
{
	RSTiffIfdEntry *entry = g_object_new(RS_TYPE_TIFF_IFD_ENTRY, NULL);

	entry->tag = rs_tiff_get_ushort(tiff, offset+0);
	entry->type = rs_tiff_get_ushort(tiff, offset+2);
	entry->count = rs_tiff_get_uint(tiff, offset+4);
	entry->value_offset = rs_tiff_get_uint(tiff, offset+8);

	return entry;
}

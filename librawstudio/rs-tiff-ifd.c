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

#include <rawstudio.h>
#include "rs-tiff-ifd.h"

G_DEFINE_TYPE (RSTiffIfd, rs_tiff_ifd, G_TYPE_OBJECT)

static void read_entries(RSTiffIfd *ifd);

enum {
	PROP_0,
	PROP_TIFF,
	PROP_OFFSET,
	PROP_NEXT_IFD,
};

static void
rs_tiff_ifd_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSTiffIfd *ifd = RS_TIFF_IFD(object);

	switch (property_id)
	{
		case PROP_NEXT_IFD:
			g_value_set_uint(value, ifd->next_ifd);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
rs_tiff_ifd_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSTiffIfd *ifd = RS_TIFF_IFD(object);

	switch (property_id)
	{
		case PROP_TIFF:
			ifd->tiff = g_object_ref(g_value_get_object(value));
			if (ifd->tiff && ifd->offset)
				RS_TIFF_IFD_GET_CLASS(ifd)->read(ifd);
			break;
		case PROP_OFFSET:
			ifd->offset = g_value_get_uint(value);
			if (ifd->tiff && ifd->offset)
				RS_TIFF_IFD_GET_CLASS(ifd)->read(ifd);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
rs_tiff_ifd_dispose(GObject *object)
{
	RSTiffIfd *ifd = RS_TIFF_IFD(object);

	if (!ifd->dispose_has_run)
	{
		ifd->dispose_has_run = TRUE;
		g_object_unref(ifd->tiff);
		g_list_foreach(ifd->entries, (GFunc)g_object_unref, NULL);
		g_list_free(ifd->entries);
	}

	G_OBJECT_CLASS(rs_tiff_ifd_parent_class)->dispose (object);
}

static void
rs_tiff_ifd_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_tiff_ifd_parent_class)->finalize (object);
}

static void
rs_tiff_ifd_class_init(RSTiffIfdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = rs_tiff_ifd_get_property;
	object_class->set_property = rs_tiff_ifd_set_property;
	object_class->dispose = rs_tiff_ifd_dispose;
	object_class->finalize = rs_tiff_ifd_finalize;

	g_object_class_install_property(object_class,
		PROP_TIFF, g_param_spec_object(
			"tiff", "tiff", "The RSTiff associated with the RSTiffIfd",
			RS_TYPE_TIFF, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

	g_object_class_install_property(object_class,
		PROP_OFFSET, g_param_spec_uint(
			"offset", "offset", "IFD offset",
			0, G_MAXUINT, 0, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

	g_object_class_install_property(object_class,
		PROP_NEXT_IFD, g_param_spec_uint(
			"next-ifd", "next-ifd", "Offset for next ifd",
			0, G_MAXUINT, 0, G_PARAM_READABLE));

	klass->read = read_entries;
}

static void
rs_tiff_ifd_init(RSTiffIfd *self)
{
}

RSTiffIfd *
rs_tiff_ifd_new(RSTiff *tiff, guint offset)
{
	g_assert(RS_IS_TIFF(tiff));

	return g_object_new(RS_TYPE_TIFF_IFD, "tiff", tiff, "offset", offset, NULL);
}

guint
rs_tiff_ifd_get_next(RSTiffIfd *ifd)
{
	g_assert(RS_IS_TIFF_IFD(ifd));

	return ifd->next_ifd;
}

static void
read_entries(RSTiffIfd *ifd)
{
	gint i;

	ifd->num_entries = rs_tiff_get_ushort(ifd->tiff, ifd->offset);
	ifd->next_ifd = rs_tiff_get_uint(ifd->tiff, ifd->offset + 2 + ifd->num_entries*12);

	if (ifd->next_ifd == ifd->offset)
		ifd->next_ifd = 0;
	else if (ifd->next_ifd > (ifd->tiff->map_length-12))
		ifd->next_ifd = 0;

	/* Read all entries */
	for(i=0;i<ifd->num_entries;i++)
		ifd->entries = g_list_append(ifd->entries, rs_tiff_ifd_entry_new(ifd->tiff, ifd->offset + 2 + i * 12));
}

static gint
_tag_search(RSTiffIfdEntry *entry, gushort tag)
{
	return entry->tag - tag;
}

RSTiffIfdEntry *
rs_tiff_ifd_get_entry_by_tag(RSTiffIfd *ifd, gushort tag)
{
	g_assert(RS_IS_TIFF_IFD(ifd));
	GList *found;
	RSTiffIfdEntry *ret = NULL;

	found = g_list_find_custom(ifd->entries, GUINT_TO_POINTER((guint) tag), (GCompareFunc) _tag_search);

	if (found)
		ret = g_object_ref(found->data);

	return ret;
}

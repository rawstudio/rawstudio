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
#include <sys/stat.h>
#include <string.h>
#include "rs-tiff.h"

G_DEFINE_TYPE (RSTiff, rs_tiff, G_TYPE_OBJECT)

static gboolean read_file_header(RSTiff *tiff);
static gboolean read_from_file(RSTiff *tiff);

enum {
	PROP_0,
	PROP_FILENAME,
};

static void
rs_tiff_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSTiff *tiff = RS_TIFF(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, tiff->filename);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
rs_tiff_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSTiff *tiff = RS_TIFF(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			tiff->filename = g_value_dup_string(value);
			read_from_file(tiff);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
rs_tiff_dispose(GObject *object)
{
	RSTiff *tiff = RS_TIFF(object);

	if (!tiff->dispose_has_run)
	{
		tiff->dispose_has_run = TRUE;
		if (tiff->map)
			g_free(tiff->map);
		g_list_foreach(tiff->ifds, (GFunc)g_object_unref, NULL);
		g_list_free(tiff->ifds);
	}
	G_OBJECT_CLASS(rs_tiff_parent_class)->dispose(object);
}

static void
rs_tiff_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_tiff_parent_class)->finalize(object);
}

static void
rs_tiff_class_init(RSTiffClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = rs_tiff_get_property;
	object_class->set_property = rs_tiff_set_property;
	object_class->dispose = rs_tiff_dispose;
	object_class->finalize = rs_tiff_finalize;

	g_object_class_install_property(object_class,
		PROP_FILENAME, g_param_spec_string(
			"filename", "Filename", "The filename to load",
			NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	klass->read_file_header = read_file_header;
}

static void
rs_tiff_init(RSTiff *self)
{
}

static gboolean
read_file_header(RSTiff *tiff)
{
	gboolean ret = TRUE;
	guint next_ifd;

	if (tiff->map_length < 16)
		return FALSE;

	/* Read endianness */
	if ((tiff->map[0] == 'I') && (tiff->map[1] == 'I'))
		tiff->byte_order = G_LITTLE_ENDIAN;
	else if ((tiff->map[0] == 'M') && (tiff->map[1] == 'M'))
		tiff->byte_order = G_BIG_ENDIAN;
	else /* Not a TIFF file */
		ret = FALSE;

	/* Read TIFF identifier */
	gushort magic = rs_tiff_get_ushort(tiff, 2);
	if (magic != 42 && magic != 0x4352)
		ret = FALSE;

	tiff->first_ifd_offset = rs_tiff_get_uint(tiff, 4);

	next_ifd = tiff->first_ifd_offset;
	while(next_ifd)
	{
		RSTiffIfd *ifd = rs_tiff_ifd_new(tiff, next_ifd);
		if (ifd)
		{
			tiff->num_ifd++;
			tiff->ifds = g_list_append(tiff->ifds, ifd);
			next_ifd = rs_tiff_ifd_get_next(ifd);
		}
		else
			break;
	}

	return ret;
}

static gboolean
read_from_file(RSTiff *tiff)
{
	gboolean ret = TRUE;
	GError *error = NULL;

	g_file_get_contents(tiff->filename, (gchar **)&tiff->map, &tiff->map_length, &error);

	if (error)
	{
		g_warning("GError: '%s'", error->message);
		g_error_free(error);
		ret = FALSE;
	}

	return ret && RS_TIFF_GET_CLASS(tiff)->read_file_header(tiff);
}

RSTiff *
rs_tiff_new_from_file(const gchar *filename)
{
	return g_object_new(RS_TYPE_TIFF, "filename", filename, NULL);
}

const gchar *
rs_tiff_get_filename(RSTiff *tiff)
{
	g_return_val_if_fail(RS_IS_TIFF(tiff), "");

	return tiff->filename;
}

const gchar *
rs_tiff_get_filename_nopath(RSTiff *tiff)
{
	g_return_val_if_fail(RS_IS_TIFF(tiff), "");

	return strrchr(tiff->filename,'/') + 1;
}

RSTiffIfdEntry *
rs_tiff_get_ifd_entry(RSTiff *tiff, guint ifd_num, gushort tag)
{
	RSTiffIfd *ifd = NULL;
	RSTiffIfdEntry *ret = NULL;

	g_return_val_if_fail(RS_IS_TIFF(tiff), NULL);

	if (tiff->ifds == 0)
		if (!read_from_file(tiff))
			return NULL;
		
	if (ifd_num <= tiff->num_ifd)
		ifd = g_list_nth_data(tiff->ifds, ifd_num);

	if (ifd)
		ret = rs_tiff_ifd_get_entry_by_tag(ifd, tag);

	return ret;
}

gchar *
rs_tiff_get_ascii(RSTiff *tiff, guint ifd_num, gushort tag)
{
	gchar *ret = NULL;
	RSTiffIfdEntry *entry = NULL;

	g_return_val_if_fail(RS_IS_TIFF(tiff), NULL);

	entry = rs_tiff_get_ifd_entry(tiff, ifd_num, tag);
	if (entry && entry->type && entry->count)
	{
		if ((entry->value_offset + entry->count) <= tiff->map_length)
			ret = g_strndup((gchar *) tiff->map + entry->value_offset , entry->count);
	}

	return ret;
}

void
rs_tiff_free_data(RSTiff * tiff)
{
	g_return_if_fail(RS_IS_TIFF(tiff));

	if (tiff->map)
		g_free(tiff->map);
	tiff->map = NULL;

	g_list_foreach(tiff->ifds, (GFunc)g_object_unref, NULL);
	g_list_free(tiff->ifds);
	tiff->ifds = 0;
}

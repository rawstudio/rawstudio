/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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

#include <config.h>
#include "gettext.h"
#include "rs-metadata.h"
#include "rawstudio.h"
#include "rs-math.h"

G_DEFINE_TYPE (RSMetadata, rs_metadata, G_TYPE_OBJECT)

static void
rs_metadata_dispose (GObject *object)
{
	RSMetadata *metadata = RS_METADATA(object);

	if (!metadata->dispose_has_run)
	{
		metadata->dispose_has_run = TRUE;
	
		if (metadata->make_ascii)
			g_free(metadata->make_ascii);
		if (metadata->model_ascii)
			g_free(metadata->model_ascii);
		if (metadata->time_ascii)
			g_free(metadata->time_ascii);
		if (metadata->thumbnail)
			g_object_unref(metadata->thumbnail);
	}

	/* Chain up */
	if (G_OBJECT_CLASS (rs_metadata_parent_class)->dispose)
		G_OBJECT_CLASS (rs_metadata_parent_class)->dispose (object);
}

static void
rs_metadata_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (rs_metadata_parent_class)->finalize)
		G_OBJECT_CLASS (rs_metadata_parent_class)->finalize (object);
}

static void
rs_metadata_class_init (RSMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_metadata_dispose;
	object_class->finalize = rs_metadata_finalize;
}

static void
rs_metadata_init (RSMetadata *metadata)
{
	gint i;

	metadata->dispose_has_run = FALSE;
	metadata->make = MAKE_UNKNOWN;
	metadata->make_ascii = NULL;
	metadata->model_ascii = NULL;
	metadata->time_ascii = NULL;
	metadata->timestamp = -1;
	metadata->orientation = 0;
	metadata->aperture = -1.0;
	metadata->iso = 0;
	metadata->shutterspeed = -1.0;
	metadata->thumbnail_start = 0;
	metadata->thumbnail_length = 0;
	metadata->preview_start = 0;
	metadata->preview_length = 0;
	metadata->preview_planar_config = 0;
	metadata->preview_width = 0;
	metadata->preview_height = 0;
	metadata->cam_mul[0] = -1.0;
	metadata->contrast = -1.0;
	metadata->saturation = -1.0;
	metadata->color_tone = -1.0;
	metadata->focallength = -1;
	for(i=0;i<4;i++)
		metadata->cam_mul[i] = 1.0f;
	matrix4_identity(&metadata->adobe_coeff);
	metadata->thumbnail = NULL;
}

RSMetadata*
rs_metadata_new (void)
{
	return g_object_new (RS_TYPE_METADATA, NULL);
}

RSMetadata *
rs_metadata_new_from_file(const gchar *filename)
{
	RSMetadata *metadata = rs_metadata_new();

	rs_metadata_load_from_file(metadata, filename);

	return metadata;
}

gboolean
rs_metadata_load_from_file(RSMetadata *metadata, const gchar *filename)
{
	gboolean ret = FALSE;
	RS_FILETYPE *filetype;

	g_assert(filename != NULL);
	g_assert(RS_IS_METADATA(metadata));

	filetype = rs_filetype_get(filename, TRUE);

	if (filetype && filetype->load_meta)
	{
		filetype->load_meta(filename, metadata);
		ret = TRUE;
	}

	return ret;
}

void
rs_metadata_normalize_wb(RSMetadata *metadata)
{
	gdouble div;

	g_assert(RS_IS_METADATA(metadata));

	if ((metadata->cam_mul[1]+metadata->cam_mul[3])!=0.0)
	{
		div = 2/(metadata->cam_mul[1]+metadata->cam_mul[3]);
		metadata->cam_mul[0] *= div;
		metadata->cam_mul[1] = 1.0;
		metadata->cam_mul[2] *= div;
		metadata->cam_mul[3] = 1.0;
	}
	return;
}

gchar *
rs_metadata_get_short_description(RSMetadata *metadata)
{
	GString *label = g_string_new("");
	gchar *ret = NULL;

	g_assert(RS_IS_METADATA(metadata));

	if (metadata->focallength>0)
		g_string_append_printf(label, _("%dmm "), metadata->focallength);
	if (metadata->shutterspeed > 0.0 && metadata->shutterspeed < 4) 
		g_string_append_printf(label, _("%.1fs "), 1/metadata->shutterspeed);
	else if (metadata->shutterspeed >= 4)
		g_string_append_printf(label, _("1/%.0fs "), metadata->shutterspeed);
	if (metadata->aperture!=0.0)
		g_string_append_printf(label, _("F/%.1f "), metadata->aperture);
	if (metadata->iso!=0)
		g_string_append_printf(label, _("ISO%d"), metadata->iso);

	ret = label->str;

	g_string_free(label, FALSE);

	return ret;
}

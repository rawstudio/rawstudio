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
#include <glib/gstdio.h> /* g_unlink() */
#include <config.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "gettext.h"

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
		if (metadata->lens_identifier)
			g_free(metadata->lens_identifier);

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
	metadata->thumbnail = NULL;

	/* Lens info */
	metadata->lens_id = -1;
	metadata->lens_min_focal = -1.0;
	metadata->lens_max_focal = -1.0;
	metadata->lens_min_aperture = -1.0;
	metadata->lens_max_aperture = -1.0;
	metadata->lens_identifier = NULL;
	metadata->fixed_lens_identifier = NULL;
}

RSMetadata*
rs_metadata_new (void)
{
	return g_object_new (RS_TYPE_METADATA, NULL);
}

#define METACACHEVERSION 6
void
rs_metadata_cache_save(RSMetadata *metadata, const gchar *filename)
{
	gchar *basename;
	gchar *dotdir = rs_dotdir_get(filename);
	gchar *cache_filename;
	gchar *thumb_filename;
	xmlTextWriterPtr writer;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	if (!dotdir)
		return;

	g_static_mutex_lock(&lock);
	basename = g_path_get_basename(filename);

	cache_filename = g_strdup_printf("%s/%s.metacache.xml", dotdir, basename);

	writer = xmlNewTextWriterFilename(cache_filename, 0);
	if (writer)
	{
		xmlTextWriterSetIndent(writer, 1);

		xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
		xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-metadata");
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "version", "%d", METACACHEVERSION);
		if (metadata->make != MAKE_UNKNOWN)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "make", "%d", metadata->make);
		if (metadata->make_ascii)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "make_ascii","%s", metadata->make_ascii);
		if (metadata->model_ascii)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "model_ascii", "%s", metadata->model_ascii);
		if (metadata->time_ascii)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "time_ascii", "%s", metadata->time_ascii);
		if (metadata->timestamp > -1)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "timestamp", "%d", metadata->timestamp);
		/* Can we make orientation conditional? */
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "orientation", "%u", metadata->orientation);
		if (metadata->aperture > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "aperture", "%f", metadata->aperture);
		if (metadata->iso > 0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "iso", "%u", metadata->iso);
		if (metadata->shutterspeed > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "shutterspeed", "%f", metadata->shutterspeed);
		if (metadata->cam_mul[0] > 0.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "cam_mul", "%f %f %f %f", metadata->cam_mul[0], metadata->cam_mul[1], metadata->cam_mul[2], metadata->cam_mul[3]);
		if (metadata->contrast > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "contrast", "%f", metadata->contrast);
		if (metadata->saturation > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "saturation", "%f", metadata->saturation);
		if (metadata->color_tone > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "color_tone", "%f", metadata->color_tone);
		if (metadata->focallength > 0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "focallength", "%d", metadata->focallength);
		if (metadata->lens_id > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "lens_id", "%d", metadata->lens_id);
		if (metadata->lens_min_focal > -1)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "lens_min_focal", "%f", metadata->lens_min_focal);
		if (metadata->lens_max_focal > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "lens_max_focal", "%f", metadata->lens_max_focal);
		if (metadata->lens_min_aperture > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "lens_min_aperture", "%f", metadata->lens_min_aperture);
		if (metadata->lens_max_aperture > -1.0)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "lens_max_aperture", "%f", metadata->lens_max_aperture);
		if (metadata->fixed_lens_identifier)
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "fixed_lens_identifier", "%s", metadata->fixed_lens_identifier);
		xmlTextWriterEndDocument(writer);
		xmlFreeTextWriter(writer);
	}
	g_free(cache_filename);
	g_static_mutex_unlock(&lock);

	if (metadata->thumbnail)
	{
		thumb_filename = g_strdup_printf("%s/%s.thumb.jpg", dotdir, basename);
		gdk_pixbuf_save(metadata->thumbnail, thumb_filename, "jpeg", NULL, "quality", "90", NULL);
		g_free(thumb_filename);
	}

	g_free(basename);
}

static gboolean
rs_metadata_cache_load(RSMetadata *metadata, const gchar *filename)
{
	gboolean ret = FALSE;
	gchar *basename;
	gchar *dotdir = rs_dotdir_get(filename);
	gchar *cache_filename;
	gchar *thumb_filename;
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gint version = 0;

	if (!dotdir)
		return FALSE;

	basename = g_path_get_basename(filename);

	cache_filename = g_strdup_printf("%s/%s.metacache.xml", dotdir, basename);
	if (!g_file_test(cache_filename, G_FILE_TEST_IS_REGULAR))
	{
		g_free(basename);
		g_free(cache_filename);
		return FALSE;
	}

	doc = xmlParseFile(cache_filename);
	if(!doc)
		return FALSE;

	cur = xmlDocGetRootElement(doc);

	if ((!xmlStrcmp(cur->name, BAD_CAST "rawstudio-metadata")))
	{
		val = xmlGetProp(cur, BAD_CAST "version");
		if (val)
			version = atoi((gchar *) val);
	}

	if (version == METACACHEVERSION)
	{
		cur = cur->xmlChildrenNode;
		while(cur)
		{
			if ((!xmlStrcmp(cur->name, BAD_CAST "make")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->make = atoi((gchar *) val);
				xmlFree(val);
			}

			else if ((!xmlStrcmp(cur->name, BAD_CAST "make_ascii")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->make_ascii = g_strdup((gchar *)val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "model_ascii")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->model_ascii = g_strdup((gchar *)val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "time_ascii")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->time_ascii = g_strdup((gchar *)val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "timestamp")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->timestamp = atoi((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "orientation")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->orientation = atoi((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "aperture")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->aperture = rs_atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "iso")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->iso = atoi((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "shutterspeed")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->shutterspeed = rs_atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "cam_mul")))
			{
				gchar **vals;
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				vals = g_strsplit((gchar *)val, " ", 4);
				if (vals[0])
				{
					metadata->cam_mul[0] = rs_atof((gchar *) vals[0]);
					if (vals[1])
					{
						metadata->cam_mul[1] = rs_atof((gchar *) vals[1]);
						if (vals[2])
						{
							metadata->cam_mul[2] = rs_atof((gchar *) vals[2]);
							if (vals[3])
								metadata->cam_mul[3] = rs_atof((gchar *) vals[3]);
						}
					}
				}
				g_strfreev(vals);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "contrast")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->contrast = rs_atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "saturation")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->saturation = rs_atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "color_tone")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->color_tone = rs_atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "focallength")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->focallength = atoi((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "lens_id")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->lens_id = atoi((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "lens_min_focal")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->lens_min_focal = atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "lens_max_focal")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->lens_max_focal = atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "lens_min_aperture")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->lens_min_aperture = atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "lens_max_aperture")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->lens_max_aperture = atof((gchar *) val);
				xmlFree(val);
			}
			else if ((!xmlStrcmp(cur->name, BAD_CAST "fixed_lens_identifier")))
			{
				val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				metadata->fixed_lens_identifier = g_strdup((gchar *)val);
				xmlFree(val);
			}

			cur = cur->next;
		}
		ret = TRUE;
	}

	xmlFreeDoc(doc);
	g_free(cache_filename);

	/* If the version is less than 4, delete the PNG thunbnail, we're using JPEG now */
	if (version < 4)
	{
		thumb_filename = g_strdup_printf("%s/%s.thumb.png", dotdir, basename);
		g_unlink(thumb_filename);
		g_free(thumb_filename);
	}

	if (ret == TRUE)
	{
		thumb_filename = g_strdup_printf("%s/%s.thumb.jpg", dotdir, basename);
		metadata->thumbnail = gdk_pixbuf_new_from_file(thumb_filename, NULL);
		g_free(thumb_filename);
		if (!metadata->thumbnail)
			ret = FALSE;
	}

	g_free(basename);

	return ret;
}
#undef METACACHEVERSION


static void generate_lens_identifier(RSMetadata *meta)
{
	/* Check if we already have an identifier from camera */
	if (meta->fixed_lens_identifier)
	{
		meta->lens_identifier = meta->fixed_lens_identifier;
		return;
	}
	/* These lenses are identified with varying aperture for lens depending on actual focal length. We fix this by
	   setting the correct aperture values, so the lens only will show up once in the lens db editor */
	rs_lens_fix(meta);

	/* Build identifier string */
	GString *identifier = g_string_new("");
	if (meta->lens_id > 0)
		g_string_append_printf(identifier, "ID:%d ",meta->lens_id);
	if (meta->lens_max_focal > 0)
		g_string_append_printf(identifier, "maxF:%.0f ",meta->lens_max_focal);
	if (meta->lens_min_focal > 0)
		g_string_append_printf(identifier, "minF:%.0f ",meta->lens_min_focal);
	if (meta->lens_max_aperture > 0)
		g_string_append_printf(identifier, "maxF:%.1f ",meta->lens_max_aperture);
	if (meta->lens_min_aperture > 0)
		g_string_append_printf(identifier, "minF:%.0f ",meta->lens_min_aperture);
	if (identifier->len > 0)
		meta->lens_identifier = g_strdup(identifier->str);
	else
	{
		/* Most likely a hacked compact */
		if (meta->make_ascii > 0)
			g_string_append_printf(identifier, "make:%s ",meta->make_ascii);
		if (meta->model_ascii > 0)
			g_string_append_printf(identifier, "model:%s ",meta->model_ascii);
		if (identifier->len > 0)
			meta->lens_identifier = g_strdup(identifier->str);
	}
	g_string_free(identifier, TRUE);
}

RSMetadata *
rs_metadata_new_from_file(const gchar *filename)
{
	RSMetadata *metadata = rs_metadata_new();

	if (!rs_metadata_cache_load(metadata, filename))
	{
		rs_metadata_load_from_file(metadata, filename);
		rs_metadata_cache_save(metadata, filename);
	}

	generate_lens_identifier(metadata);
	return metadata;
}

gboolean
rs_metadata_load(RSMetadata *metadata, const gchar *filename)
{
	if (!rs_metadata_cache_load(metadata, filename))
	{
		if (rs_metadata_load_from_file(metadata, filename))
		{
			rs_metadata_cache_save(metadata, filename);
			generate_lens_identifier(metadata);
			return TRUE;
		}
		return FALSE;
	}
	generate_lens_identifier(metadata);
	return TRUE;
}

gboolean
rs_metadata_load_from_file(RSMetadata *metadata, const gchar *filename)
{
	gboolean ret = FALSE;
	RAWFILE *rawfile;

	g_assert(filename != NULL);
	g_assert(RS_IS_METADATA(metadata));

	rawfile = raw_open_file(filename);
	if (rawfile)
	{
		/* FIXME: Fix the damned return value from meta-loaders! */
		ret = TRUE;
		rs_filetype_meta_load(filename, metadata, rawfile, 0);

		raw_close_file(rawfile);
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

GdkPixbuf *
rs_metadata_get_thumbnail(RSMetadata *metadata)
{
	g_assert(RS_IS_METADATA(metadata));

	if (metadata->thumbnail)
		g_object_ref(metadata->thumbnail);

	return metadata->thumbnail;
}

/**
 * Deletes the on-disk cache (if any) for a photo
 * @param filename The full path to the photo - not the cache itself
 */
void
rs_metadata_delete_cache(const gchar *filename)
{
	gchar *basename;
	gchar *dotdir = rs_dotdir_get(filename);
	gchar *cache_filename;
	gchar *thumb_filename;

	if (!dotdir)
		return;

	g_assert(filename);

	basename = g_path_get_basename(filename);

	/* Delete the metadata cache itself */
	cache_filename = g_strdup_printf("%s/%s.metacache.xml", dotdir, basename);
	g_unlink(cache_filename);
	g_free(cache_filename);

	/* Delete the thumbnail */
	thumb_filename = g_strdup_printf("%s/%s.thumb.jpg", dotdir, basename);
	g_unlink(thumb_filename);
	g_free(thumb_filename);

	/* Clean up please */
	g_free(dotdir);
	g_free(basename);
}

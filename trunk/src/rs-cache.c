/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

#include <gtk/gtk.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"

gchar *
rs_cache_get_name(const gchar *src)
{
	gchar *ret=NULL;
	gchar *dotdir, *filename;
	GString *out;
	dotdir = rs_dotdir_get(src);
	filename = g_path_get_basename(src);
	if (dotdir)
	{
		out = g_string_new(dotdir);
		out = g_string_append(out, "/");
		out = g_string_append(out, filename);
		out = g_string_append(out, ".cache.xml");
		ret = out->str;
		g_string_free(out, FALSE);
		g_free(dotdir);
	}
	g_free(filename);
	return(ret);
}

void
rs_cache_init()
{
	static gboolean init=FALSE;
	if (!init)
		LIBXML_TEST_VERSION /* yep, it should look like this */
	init = TRUE;
	return;
}

void
rs_cache_save(RS_PHOTO *photo)
{
	gint id;
	xmlTextWriterPtr writer;
	gchar *cachename;

	if(!photo->active) return;
	cachename = rs_cache_get_name(photo->filename);
	if (!cachename) return;
	writer = xmlNewTextWriterFilename(cachename, 0); /* fixme, check for errors */
	xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-cache");
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "priority", "%d",
		photo->priority);
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "orientation", "%d",
		photo->orientation);
	for(id=0;id<3;id++)
	{
		xmlTextWriterStartElement(writer, BAD_CAST "settings");
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "id", "%d", id);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "exposure", "%f",
			photo->settings[id]->exposure);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "saturation", "%f",
			photo->settings[id]->saturation);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "hue", "%f",
			photo->settings[id]->hue);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "contrast", "%f",
			photo->settings[id]->contrast);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "warmth", "%f",
			photo->settings[id]->warmth);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "tint", "%f",
			photo->settings[id]->tint);
		xmlTextWriterEndElement(writer);
	}
	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	g_free(cachename);
	return;
}

void
rs_cache_load_setting(RS_SETTINGS_DOUBLE *rss, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar *val;
	gdouble *target=NULL;
	while(cur)
	{
		target = NULL;
		if ((!xmlStrcmp(cur->name, BAD_CAST "exposure")))
			target = &rss->exposure;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "saturation")))
			target = &rss->saturation;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "hue")))
			target = &rss->hue;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "contrast")))
			target = &rss->contrast;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "warmth")))
			target = &rss->warmth;
		else if ((!xmlStrcmp(cur->name, BAD_CAST "tint")))
			target = &rss->tint;

		if (target)
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			*target =  g_strtod((gchar *) val, NULL);
			xmlFree(val);
		}
		cur = cur->next;
	}
}

void
rs_cache_load(RS_PHOTO *photo)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gchar *cachename;
	gint id;

	cachename = rs_cache_get_name(photo->filename);
	if (!cachename) return;
	if (!g_file_test(cachename, G_FILE_TEST_IS_REGULAR)) return;
	doc = xmlParseFile(cachename);
	if(doc==NULL) return;

	cur = xmlDocGetRootElement(doc);

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "settings")))
		{
			val = xmlGetProp(cur, BAD_CAST "id");
			id = atoi((gchar *) val);
			xmlFree(val);
			if (id>2) id=0;
			if (id<0) id=0;
			rs_cache_load_setting(photo->settings[id], doc, cur->xmlChildrenNode);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "priority")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			photo->priority = atoi((gchar *) val);
			xmlFree(val);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "orientation")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			photo->orientation = atoi((gchar *) val);
			xmlFree(val);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	g_free(cachename);
	return;
}

void
rs_cache_load_quick(const gchar *filename, gint *priority)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gchar *cachename;

	cachename = rs_cache_get_name(filename);
	if (!cachename) return;
	if (!g_file_test(cachename, G_FILE_TEST_IS_REGULAR)) return;
	doc = xmlParseFile(cachename);
	if(doc==NULL) return;

	cur = xmlDocGetRootElement(doc);

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "priority")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			*priority = atoi((gchar *) val);
			xmlFree(val);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	g_free(cachename);
	return;
}

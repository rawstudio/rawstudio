/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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
#include "rawstudio.h"
#include "rs-cache.h"

static void rs_cache_load_setting(RS_SETTINGS_DOUBLE *rss, xmlDocPtr doc, xmlNodePtr cur);

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
		out = g_string_append(out, G_DIR_SEPARATOR_S);
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
rs_cache_save(RS_PHOTO *photo)
{
	gint id, i;
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
	if (photo->exported)
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "exported", "yes");
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "orientation", "%d",
		photo->orientation);
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "angle", "%f",
		photo->angle);
	if (photo->crop)
	{
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "crop", "%d %d %d %d",
			photo->crop->x1, photo->crop->y1,
			photo->crop->x2, photo->crop->y2);
	}
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
		if (photo->settings[id]->curve_nknots > 0)
		{
			xmlTextWriterStartElement(writer, BAD_CAST "curve");
			xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "num", "%d", photo->settings[id]->curve_nknots);
			for(i=0;i<photo->settings[id]->curve_nknots;i++)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "knot", "%f %f",
					photo->settings[id]->curve_knots[i*2+0],
					photo->settings[id]->curve_knots[i*2+1]);
			xmlTextWriterEndElement(writer);
		}
		xmlTextWriterEndElement(writer);
	}
	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	g_free(cachename);
	return;
}

static void
rs_cache_load_setting(RS_SETTINGS_DOUBLE *rss, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar *val;
	gdouble *target=NULL;
	xmlNodePtr curve = NULL;
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
		else if ((!xmlStrcmp(cur->name, BAD_CAST "curve")))
		{
			gchar **vals;
			gint num;
			gfloat x,y;

			val = xmlGetProp(cur, BAD_CAST "num");
			if (val)
				num = atoi((gchar *) val);
			else
				num = 0;

			rss->curve_knots = g_new(gfloat, 2*num);
			rss->curve_nknots = 0;
			curve = cur->xmlChildrenNode;
			while (curve && num)
			{
				if ((!xmlStrcmp(curve->name, BAD_CAST "knot")))
				{
					val = xmlNodeListGetString(doc, curve->xmlChildrenNode, 1);
					vals = g_strsplit((gchar *)val, " ", 4);
					if (vals[0] && vals[1])
					{
						x = atof((gchar *) vals[0]);
						y = atof((gchar *) vals[1]);
						rss->curve_knots[rss->curve_nknots*2+0] = x;
						rss->curve_knots[rss->curve_nknots*2+1] = y;
					}
					g_strfreev(vals);
					xmlFree(val);
				}
				curve = curve->next;
				rss->curve_nknots++;
				if (rss->curve_nknots > num) break;
			}
		}

		if (target)
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			*target =  g_strtod((gchar *) val, NULL);
			xmlFree(val);
		}
		cur = cur->next;
	}
}

gboolean
rs_cache_load(RS_PHOTO *photo)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gchar *cachename;
	gint id;

	cachename = rs_cache_get_name(photo->filename);
	if (!cachename) return(FALSE);
	if (!g_file_test(cachename, G_FILE_TEST_IS_REGULAR)) return FALSE;
	photo->exported = FALSE;
	doc = xmlParseFile(cachename);
	if(doc==NULL) return(FALSE);

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
		else if ((!xmlStrcmp(cur->name, BAD_CAST "angle")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			photo->angle = g_strtod((gchar *) val, NULL);
			xmlFree(val);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "exported")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (g_ascii_strcasecmp((gchar *) val, "yes")==0)
				photo->exported = TRUE;
			xmlFree(val);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "crop")))
		{
			gchar **vals;

			if (!photo->crop)
				photo->crop = (RS_RECT *) g_malloc(sizeof(RS_RECT));
			
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			vals = g_strsplit((gchar *)val, " ", 4);
			if (vals[0])
			{
				photo->crop->x1 = atoi((gchar *) vals[0]);
				if (vals[1])
				{
					photo->crop->y1 = atoi((gchar *) vals[1]);
					if (vals[2])
					{
						photo->crop->x2 = atoi((gchar *) vals[2]);
						if (vals[3])
							photo->crop->y2 = atoi((gchar *) vals[3]);
					}
				}
			}
			g_strfreev(vals);
			xmlFree(val);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	g_free(cachename);
	return(TRUE);
}

void
rs_cache_load_quick(const gchar *filename, gint *priority, gboolean *exported)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *val;
	gchar *cachename;

	if (!filename) return;
	cachename = rs_cache_get_name(filename);
	if (!cachename) return;
	if (!g_file_test(cachename, G_FILE_TEST_IS_REGULAR)) return;
	doc = xmlParseFile(cachename);
	if(doc==NULL) return;
	if (exported) *exported = FALSE;

	cur = xmlDocGetRootElement(doc);

	cur = cur->xmlChildrenNode;
	while(cur)
	{
		if (priority && (!xmlStrcmp(cur->name, BAD_CAST "priority")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			*priority = atoi((gchar *) val);
			xmlFree(val);
		}
		if (exported && (!xmlStrcmp(cur->name, BAD_CAST "exported")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (g_ascii_strcasecmp((gchar *) val, "yes")==0)
				*exported = TRUE;
			xmlFree(val);
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	g_free(cachename);
	return;
}

void
rs_cache_save_flags(const gchar *filename, const guint *priority, const gboolean *exported)
{
	RS_PHOTO *photo;

	g_assert(filename != NULL);

	if (!(priority || exported)) return;

	/* Aquire a "fake" RS_PHOTO */
	photo = rs_photo_new();
	photo->active = TRUE;
	photo->filename = (gchar *) filename;

	if (rs_cache_load(photo))
	{
		/* If we got a cache file, save as normal */
		if (priority)
			photo->priority = *priority;
		if (exported)
			photo->exported = *exported;
		rs_cache_save(photo);
	}
	else
	{
		/* If we're creating a new file, only save what we know */
		xmlTextWriterPtr writer;
		gchar *cachename = rs_cache_get_name(photo->filename);

		if (cachename)
		{
			writer = xmlNewTextWriterFilename(cachename, 0); /* fixme, check for errors */
			g_free(cachename);

			xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
			xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-cache");

			if (priority)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "priority", "%d",
					*priority);

			if (exported && *exported)
				xmlTextWriterWriteFormatElement(writer, BAD_CAST "exported", "yes");

			xmlTextWriterEndDocument(writer);
			xmlFreeTextWriter(writer);
		}
	}

	/* Free the photo */
	photo->filename = NULL;
	rs_photo_free(photo);

	return;
}

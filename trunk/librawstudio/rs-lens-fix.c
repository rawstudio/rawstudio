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
#include <libxml/encoding.h>
#include "config.h"

static GHashTable *lens_fix_hash_table;

typedef struct {
	gdouble max;
	gdouble min;
} LensAperture;

gchar *
lens_fix_str_hash(gint id, gdouble min_focal, gdouble max_focal)
{
	return g_strdup_printf("%d:%0.1f:%0.1f", id, min_focal, max_focal);
}

LensAperture *
lens_fix_find(gint id, gdouble min_focal, gdouble max_focal)
{
	gchar *str_hash = lens_fix_str_hash(id, min_focal, max_focal);
	LensAperture *lens_aperture = g_hash_table_lookup(lens_fix_hash_table, str_hash);
	g_free(str_hash);
	return lens_aperture;
}

gboolean
lens_fix_insert(gint id, gdouble min_focal, gdouble max_focal, gdouble max_aperture, gdouble min_aperture)
{
	gchar *str_hash = lens_fix_str_hash(id, min_focal, max_focal); /* May NOT be freed */
	LensAperture *lens_aperture = g_new(LensAperture, 1);
	lens_aperture->max = max_aperture;
	lens_aperture->min = min_aperture;

	if (!lens_fix_find(id, min_focal, max_focal))
		g_hash_table_insert(lens_fix_hash_table, str_hash, lens_aperture);
  
	return TRUE;
}

gboolean
rs_lens_fix_init()
{
	lens_fix_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr entry = NULL;
	xmlChar *val;

	gint lens_id;
	gdouble min_focal = 0.0, max_focal = 0.0;
	gdouble max_aperture = 0.0, min_aperture = 0.0;

	gchar *filename = g_build_filename(PACKAGE_DATA_DIR, PACKAGE, "lens_fix.xml", NULL);

	if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR))
	{
		g_warning("Cannot read lens fix file: %s ", filename);
		return FALSE;
	}

	doc = xmlParseFile(filename);
	if (!doc)
	{
		g_warning("Error parsing lens fix file: %s ", filename);
		return FALSE;
	}

	g_free(filename);

	cur = xmlDocGetRootElement(doc);
	if (cur && (xmlStrcmp(cur->name, BAD_CAST "rawstudio-lens-fix") == 0))
	{
		cur = cur->xmlChildrenNode;
		while(cur)
		{
			if ((!xmlStrcmp(cur->name, BAD_CAST "lens")))
			{
				lens_id = atoi((char *) xmlGetProp(cur, BAD_CAST "id"));
				min_focal = rs_atof((char *) xmlGetProp(cur, BAD_CAST "min-focal"));
				max_focal = rs_atof((char *) xmlGetProp(cur, BAD_CAST "max-focal"));

				entry = cur->xmlChildrenNode;
				while (entry)
				{
					if (!xmlStrcmp(entry->name, BAD_CAST "max-aperture"))
					{
						val = xmlNodeListGetString(doc, entry->xmlChildrenNode, 1);
						max_aperture = rs_atof((char *) val);
						xmlFree(val);
					}
					else if (!xmlStrcmp(entry->name, BAD_CAST "min-aperture"))
					{
						val = xmlNodeListGetString(doc, entry->xmlChildrenNode, 1);
						min_aperture = rs_atof((char *) val);
						xmlFree(val);
					}
					entry = entry->next;
				}

				lens_fix_insert(lens_id, min_focal, max_focal, max_aperture, min_aperture);
			}
			cur = cur->next;
		}
	}
	else
		g_warning("Did not recognize the format in %s", filename);

	xmlFreeDoc(doc);
	return FALSE;
}

gboolean
rs_lens_fix(RSMetadata *meta)
{
	if (!lens_fix_hash_table)
	{
		g_warning("rs_lens_fix_init() has not been run - lens \"fixing\" will is disabled.");
		return FALSE;
	}

	LensAperture *lens_aperture = lens_fix_find(meta->lens_id, meta->lens_min_focal, meta->lens_max_focal);
	if (!lens_aperture)
		return FALSE;

	meta->lens_max_aperture = lens_aperture->max;
	meta->lens_min_aperture = lens_aperture->min;

	return TRUE;
}

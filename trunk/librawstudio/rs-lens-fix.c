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
#include <libxml/encoding.h>
#include "config.h"

gboolean
rs_lens_fix(RSMetadata *meta)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr entry = NULL;
	xmlChar *val;

	gint lens_id;
	gdouble min_focal, max_focal;

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
				min_focal = atof((char *) xmlGetProp(cur, BAD_CAST "min-focal"));
				max_focal = atof((char *) xmlGetProp(cur, BAD_CAST "max-focal"));

				if (lens_id == meta->lens_id && min_focal == meta->lens_min_focal && max_focal == meta->lens_max_focal)
				{
					entry = cur->xmlChildrenNode;
					while (entry)
					{
						val = xmlNodeListGetString(doc, entry->xmlChildrenNode, 1);
						if (!xmlStrcmp(entry->name, BAD_CAST "max-aperture"))
							meta->lens_max_aperture = atof((char *) val);
						else if (!xmlStrcmp(entry->name, BAD_CAST "min-aperture"))
							meta->lens_min_aperture = atof((char *) val);
						xmlFree(val);
						entry = entry->next;
					}
				xmlFreeDoc(doc);
				return TRUE;
				}
			}
			cur = cur->next;
		}
	}
	else
		g_warning("Did not recognize the format in %s", filename);

	xmlFreeDoc(doc);
	return FALSE;
}

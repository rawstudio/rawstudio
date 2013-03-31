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

static gchar *
lens_fix_str_hash(RS_MAKE make, gint id, gdouble min_focal, gdouble max_focal)
{
	return g_strdup_printf("%d %d:%0.1f:%0.1f", (int)make, id, min_focal, max_focal);
}

static const gchar*
lens_fix_find(RS_MAKE make, gint id, gdouble min_focal, gdouble max_focal)
{
	gchar *str_hash = lens_fix_str_hash(make, id, min_focal, max_focal);
	const gchar* lens_name = g_hash_table_lookup(lens_fix_hash_table, str_hash);
	g_free(str_hash);
	return lens_name;
}

static gboolean
lens_fix_insert(RS_MAKE make, gint id, gdouble min_focal, gdouble max_focal, const gchar* name)
{
	gchar *str_hash = lens_fix_str_hash(make, id, min_focal, max_focal); /* May NOT be freed */

	if (!lens_fix_find(make, id, min_focal, max_focal))
		g_hash_table_insert(lens_fix_hash_table, str_hash, g_strdup(name));
	else
		g_free(str_hash);

	lens_fix_find(make, id, min_focal, max_focal);
	return TRUE;
}

/* These are lenses for Canon, where there is no known 3rd party lenses */
/* This table is mainly used for Canon cameras, where there is no text indication of lenses */

static gchar* 
get_canon_name_from_lens_id(gint lens_id)
{
	gchar* id = NULL;
	switch (lens_id)
	{
		case 1: id = g_strdup("Canon EF 50mm f/1.8"); break;
		case 2: id = g_strdup("Canon EF 28mm f/2.8"); break;
		case 3: id = g_strdup("Canon EF 135mm f/2.8 Soft"); break;
		case 5: id = g_strdup("Canon EF 35-70mm f/3.5-4.5"); break;
		case 7: id = g_strdup("Canon EF 100-300mm f/5.6L"); break;
		case 9: id = g_strdup("Canon EF 70-210mm f/4"); break;
		case 11: id = g_strdup("Canon EF 35mm f/2"); break;
		case 13: id = g_strdup("Canon EF 15mm f/2.8 Fisheye"); break;
		case 14: id = g_strdup("Canon EF 50-200mm f/3.5-4.5L"); break;
		case 15: id = g_strdup("Canon EF 50-200mm f/3.5-4.5"); break;
		case 16: id = g_strdup("Canon EF 35-135mm f/3.5-4.5"); break;
		case 17: id = g_strdup("Canon EF 35-70mm f/3.5-4.5A"); break;
		case 18: id = g_strdup("Canon EF 28-70mm f/3.5-4.5"); break;
		case 20: id = g_strdup("Canon EF 100-200mm f/4.5A"); break;
		case 21: id = g_strdup("Canon EF 80-200mm f/2.8L"); break;
		case 23: id = g_strdup("Canon EF 35-105mm f/3.5-4.5"); break;
		case 24: id = g_strdup("Canon EF 35-80mm f/4-5.6 Power Zoom"); break;
		case 25: id = g_strdup("Canon EF 35-80mm f/4-5.6 Power Zoom"); break;
		case 27: id = g_strdup("Canon EF 35-80mm f/4-5.6"); break;
		case 29: id = g_strdup("Canon EF 50mm f/1.8 II"); break;
		case 30: id = g_strdup("Canon EF 35-105mm f/4.5-5.6"); break;
		case 33: id = g_strdup("Voigtlander or Zeiss Lens"); break;
		case 35: id = g_strdup("Canon EF 35-80mm f/4-5.6"); break;
		case 36: id = g_strdup("Canon EF 38-76mm f/4.5-5.6"); break;
		case 38: id = g_strdup("Canon EF 80-200mm f/4.5-5.6"); break;
		case 39: id = g_strdup("Canon EF 75-300mm f/4-5.6"); break;
		case 40: id = g_strdup("Canon EF 28-80mm f/3.5-5.6"); break;
		case 41: id = g_strdup("Canon EF 28-90mm f/4-5.6"); break;
		case 43: id = g_strdup("Canon EF 28-105mm f/4-5.6"); break;
		case 44: id = g_strdup("Canon EF 90-300mm f/4.5-5.6"); break;
		case 45: id = g_strdup("Canon EF-S 18-55mm f/3.5-5.6 [II]"); break;
		case 46: id = g_strdup("Canon EF 28-90mm f/4-5.6"); break;
		case 48: id = g_strdup("Canon EF-S 18-55mm f/3.5-5.6 IS"); break;
		case 49: id = g_strdup("Canon EF-S 55-250mm f/4-5.6 IS"); break;
		case 50: id = g_strdup("Canon EF-S 18-200mm f/3.5-5.6 IS"); break;
		case 51: id = g_strdup("Canon EF-S 18-135mm f/3.5-5.6 IS"); break;
		case 52: id = g_strdup("Canon EF-S 18-55mm f/3.5-5.6 IS II"); break;
		case 94: id = g_strdup("Canon TS-E 17mm f/4L"); break;
		case 95: id = g_strdup("Canon TS-E 24.0mm f/3.5 L II"); break;
		case 124: id = g_strdup("Canon MP-E 65mm f/2.8 1-5x Macro Photo"); break;
		case 125: id = g_strdup("Canon TS-E 24mm f/3.5L"); break;
		case 126: id = g_strdup("Canon TS-E 45mm f/2.8"); break;
		case 127: id = g_strdup("Canon TS-E 90mm f/2.8"); break;
		case 129: id = g_strdup("Canon EF 300mm f/2.8L"); break;
		case 130: id = g_strdup("Canon EF 50mm f/1.0L"); break;
		case 132: id = g_strdup("Canon EF 1200mm f/5.6L"); break;
		case 134: id = g_strdup("Canon EF 600mm f/4L IS"); break;
		case 135: id = g_strdup("Canon EF 200mm f/1.8L"); break;
		case 136: id = g_strdup("Canon EF 300mm f/2.8L"); break;
		case 138: id = g_strdup("Canon EF 28-80mm f/2.8-4L"); break;
		case 139: id = g_strdup("Canon EF 400mm f/2.8L"); break;
		case 140: id = g_strdup("Canon EF 500mm f/4.5L"); break;
		case 141: id = g_strdup("Canon EF 500mm f/4.5L"); break;
		case 142: id = g_strdup("Canon EF 300mm f/2.8L IS"); break;
		case 143: id = g_strdup("Canon EF 500mm f/4L IS"); break;
		case 144: id = g_strdup("Canon EF 35-135mm f/4-5.6 USM"); break;
		case 145: id = g_strdup("Canon EF 100-300mm f/4.5-5.6 USM"); break;
		case 146: id = g_strdup("Canon EF 70-210mm f/3.5-4.5 USM"); break;
		case 147: id = g_strdup("Canon EF 35-135mm f/4-5.6 USM"); break;
		case 148: id = g_strdup("Canon EF 28-80mm f/3.5-5.6 USM"); break;
		case 149: id = g_strdup("Canon EF 100mm f/2 USM"); break;
		case 151: id = g_strdup("Canon EF 200mm f/2.8L"); break;
		case 154: id = g_strdup("Canon EF 20mm f/2.8 USM"); break;
		case 155: id = g_strdup("Canon EF 85mm f/1.8 USM"); break;
		case 162: id = g_strdup("Canon EF 200mm f/2.8L"); break;
		case 163: id = g_strdup("Canon EF 300mm f/4L"); break;
		case 164: id = g_strdup("Canon EF 400mm f/5.6L"); break;
		case 165: id = g_strdup("Canon EF 70-200mm f/2.8 L"); break;
		case 166: id = g_strdup("Canon EF 70-200mm f/2.8 L + 1.4x"); break;
		case 167: id = g_strdup("Canon EF 70-200mm f/2.8 L + 2x"); break;
		case 168: id = g_strdup("Canon EF 28mm f/1.8 USM"); break;
		case 170: id = g_strdup("Canon EF 200mm f/2.8L II"); break;
		case 171: id = g_strdup("Canon EF 300mm f/4L"); break;
		case 172: id = g_strdup("Canon EF 400mm f/5.6L"); break;
		case 175: id = g_strdup("Canon EF 400mm f/2.8L"); break;
		case 176: id = g_strdup("Canon EF 24-85mm f/3.5-4.5 USM"); break;
		case 177: id = g_strdup("Canon EF 300mm f/4L IS"); break;
		case 178: id = g_strdup("Canon EF 28-135mm f/3.5-5.6 IS"); break;
		case 179: id = g_strdup("Canon EF 24mm f/1.4L"); break;
		case 180: id = g_strdup("Canon EF 35mm f/1.4L"); break;
		case 181: id = g_strdup("Canon EF 100-400mm f/4.5-5.6L IS + 1.4x"); break;
		case 182: id = g_strdup("Canon EF 100-400mm f/4.5-5.6L IS + 2x"); break;
		case 183: id = g_strdup("Canon EF 100-400mm f/4.5-5.6L IS"); break;
		case 184: id = g_strdup("Canon EF 400mm f/2.8L + 2x"); break;
		case 185: id = g_strdup("Canon EF 600mm f/4L IS"); break;
		case 186: id = g_strdup("Canon EF 70-200mm f/4L"); break;
		case 187: id = g_strdup("Canon EF 70-200mm f/4L + 1.4x"); break;
		case 188: id = g_strdup("Canon EF 70-200mm f/4L + 2x"); break;
		case 189: id = g_strdup("Canon EF 70-200mm f/4L + 2.8x"); break;
		case 190: id = g_strdup("Canon EF 100mm f/2.8 Macro"); break;
		case 191: id = g_strdup("Canon EF 400mm f/4 DO IS"); break;
		case 193: id = g_strdup("Canon EF 35-80mm f/4-5.6 USM"); break;
		case 194: id = g_strdup("Canon EF 80-200mm f/4.5-5.6 USM"); break;
		case 195: id = g_strdup("Canon EF 35-105mm f/4.5-5.6 USM"); break;
		case 196: id = g_strdup("Canon EF 75-300mm f/4-5.6 USM"); break;
		case 197: id = g_strdup("Canon EF 75-300mm f/4-5.6 IS USM"); break;
		case 198: id = g_strdup("Canon EF 50mm f/1.4 USM"); break;
		case 199: id = g_strdup("Canon EF 28-80mm f/3.5-5.6 USM"); break;
		case 200: id = g_strdup("Canon EF 75-300mm f/4-5.6 USM"); break;
		case 201: id = g_strdup("Canon EF 28-80mm f/3.5-5.6 USM"); break;
		case 202: id = g_strdup("Canon EF 28-80mm f/3.5-5.6 USM IV"); break;
		case 208: id = g_strdup("Canon EF 22-55mm f/4-5.6 USM"); break;
		case 209: id = g_strdup("Canon EF 55-200mm f/4.5-5.6"); break;
		case 210: id = g_strdup("Canon EF 28-90mm f/4-5.6 USM"); break;
		case 211: id = g_strdup("Canon EF 28-200mm f/3.5-5.6 USM"); break;
		case 212: id = g_strdup("Canon EF 28-105mm f/4-5.6 USM"); break;
		case 213: id = g_strdup("Canon EF 90-300mm f/4.5-5.6 USM"); break;
		case 214: id = g_strdup("Canon EF-S 18-55mm f/3.5-5.6 USM"); break;
		case 215: id = g_strdup("Canon EF 55-200mm f/4.5-5.6 II USM"); break;
		case 224: id = g_strdup("Canon EF 70-200mm f/2.8L IS"); break;
		case 225: id = g_strdup("Canon EF 70-200mm f/2.8L IS + 1.4x"); break;
		case 226: id = g_strdup("Canon EF 70-200mm f/2.8L IS + 2x"); break;
		case 227: id = g_strdup("Canon EF 70-200mm f/2.8L IS + 2.8x"); break;
		case 228: id = g_strdup("Canon EF 28-105mm f/3.5-4.5 USM"); break;
		case 229: id = g_strdup("Canon EF 16-35mm f/2.8L"); break;
		case 230: id = g_strdup("Canon EF 24-70mm f/2.8L"); break;
		case 231: id = g_strdup("Canon EF 17-40mm f/4L"); break;
		case 232: id = g_strdup("Canon EF 70-300mm f/4.5-5.6 DO IS USM"); break;
		case 233: id = g_strdup("Canon EF 28-300mm f/3.5-5.6L IS"); break;
		case 234: id = g_strdup("Canon EF-S 17-85mm f4-5.6 IS USM"); break;
		case 235: id = g_strdup("Canon EF-S 10-22mm f/3.5-4.5 USM"); break;
		case 236: id = g_strdup("Canon EF-S 60mm f/2.8 Macro USM"); break;
		case 237: id = g_strdup("Canon EF 24-105mm f/4L IS"); break;
		case 238: id = g_strdup("Canon EF 70-300mm f/4-5.6 IS USM"); break;
		case 239: id = g_strdup("Canon EF 85mm f/1.2L II"); break;
		case 240: id = g_strdup("Canon EF-S 17-55mm f/2.8 IS USM"); break;
		case 241: id = g_strdup("Canon EF 50mm f/1.2L"); break;
		case 242: id = g_strdup("Canon EF 70-200mm f/4L IS"); break;
		case 243: id = g_strdup("Canon EF 70-200mm f/4L IS + 1.4x"); break;
		case 244: id = g_strdup("Canon EF 70-200mm f/4L IS + 2x"); break;
		case 245: id = g_strdup("Canon EF 70-200mm f/4L IS + 2.8x"); break;
		case 246: id = g_strdup("Canon EF 16-35mm f/2.8L II"); break;
		case 247: id = g_strdup("Canon EF 14mm f/2.8L II USM"); break;
		case 248: id = g_strdup("Canon EF 200mm f/2L IS"); break;
		case 249: id = g_strdup("Canon EF 800mm f/5.6L IS"); break;
		case 250: id = g_strdup("Canon EF 24 f/1.4L II"); break;
		case 251: id = g_strdup("Canon EF 70-200mm f/2.8L IS II USM"); break;
		case 254: id = g_strdup("Canon EF 100mm f/2.8L Macro IS USM"); break;
		case 488: id = g_strdup("Canon EF-S 15-85mm f/3.5-5.6 IS USM"); break;
	}
	return id;
}

static RS_MAKE 
translate_maker_name(const gchar *maker)
{
	if (0 == g_strcmp0(maker, "canon"))
		return MAKE_CANON;
	if (0 == g_strcmp0(maker, "cikon"))
		return MAKE_NIKON;
	if (0 == g_strcmp0(maker, "casio"))
		return MAKE_CASIO;
	if (0 == g_strcmp0(maker, "olympus"))
		return MAKE_OLYMPUS;
	if (0 == g_strcmp0(maker, "kodak"))
		return MAKE_KODAK;
	if (0 == g_strcmp0(maker, "leica"))
		return MAKE_LEICA;
	if (0 == g_strcmp0(maker, "minolta"))
		return MAKE_MINOLTA;
	if (0 == g_strcmp0(maker, "hasselblad"))
		return MAKE_HASSELBLAD;
	if (0 == g_strcmp0(maker, "panasonic"))
		return MAKE_PANASONIC;
	if (0 == g_strcmp0(maker, "pentax"))
		return MAKE_PENTAX;
	if (0 == g_strcmp0(maker, "fujifilm"))
		return MAKE_FUJIFILM;
	if (0 == g_strcmp0(maker, "phase one"))
		return MAKE_PHASEONE;
	if (0 == g_strcmp0(maker, "ricoh"))
		return MAKE_RICOH;
	if (0 == g_strcmp0(maker, "sony"))
		return MAKE_SONY;
	g_debug("Warning: Could not identify camera in lens-fix DB: %s", maker);
	return MAKE_UNKNOWN;
}


gboolean
rs_lens_fix_init(void)
{
	lens_fix_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlNodePtr entry = NULL;
	xmlChar *val;

	gint lens_id;
	gdouble min_focal = 0.0, max_focal = 0.0;
	gchar *camera_make = NULL;
	gchar *lens_name = NULL;

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
				camera_make = g_ascii_strdown((gchar *) xmlGetProp(cur, BAD_CAST "make"), -1);

				entry = cur->xmlChildrenNode;
				while (entry)
				{
					if (!xmlStrcmp(entry->name, BAD_CAST "name"))
					{
						val = xmlNodeListGetString(doc, entry->xmlChildrenNode, 1);
						lens_name = g_strdup((gchar *) val);
						xmlFree(val);
					}
					entry = entry->next;
				}
				if (lens_name)
				{
					RS_MAKE camera_maker_id = translate_maker_name(camera_make);
					lens_fix_insert(camera_maker_id, lens_id, min_focal, max_focal, lens_name);
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

gboolean
rs_lens_fix(RSMetadata *meta)
{
	g_return_val_if_fail(RS_IS_METADATA(meta), FALSE);

	if (!lens_fix_hash_table)
	{
		g_warning("rs_lens_fix_init() has not been run - lens \"fixing\" will is disabled.");
		return FALSE;
	}

	if (meta->make == MAKE_CANON && meta->lens_id > 0)
	{
		gchar *lens = get_canon_name_from_lens_id(meta->lens_id);
		if (lens)
		{
			meta->lens_identifier = lens;
			return TRUE;
		}
	}

	const gchar* lens_name = lens_fix_find(meta->make, meta->lens_id, meta->lens_min_focal, meta->lens_max_focal);
	if (lens_name)
	{
		meta->lens_identifier = g_strdup(lens_name);
		return TRUE;
	}
	return TRUE;
}

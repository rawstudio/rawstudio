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

G_DEFINE_TYPE (RSHuesatMap, rs_huesat_map, G_TYPE_OBJECT)

static void
rs_huesat_map_finalize(GObject *object)
{
	RSHuesatMap *map = RS_HUESAT_MAP(object);

	if (map->deltas)
		g_free(map->deltas);

	G_OBJECT_CLASS (rs_huesat_map_parent_class)->finalize (object);
}

static void
rs_huesat_map_class_init(RSHuesatMapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = rs_huesat_map_finalize;
}

static void
rs_huesat_map_init(RSHuesatMap *self)
{
}

RSHuesatMap *
rs_huesat_map_new(guint hue_divisions, guint sat_division, guint val_divisions)
{
	RSHuesatMap *map = g_object_new(RS_TYPE_HUESAT_MAP, NULL);

	if (val_divisions == 0)
		val_divisions = 1;

	map->hue_divisions = hue_divisions;
	map->sat_divisions = sat_division;
	map->val_divisions = val_divisions;

	map->hue_step = sat_division;
	map->val_step = hue_divisions * map->hue_step;

	map->deltas = g_new0(RS_VECTOR3, rs_huesat_map_get_deltacount(map));
	map->v_encoding = 0;

	return map;
}

RSHuesatMap *
rs_huesat_map_new_from_dcp(RSTiff *tiff, const guint ifd, const gushort dims_tag, const gushort table_tag)
{
	RSHuesatMap *map = NULL;
	RSTiffIfdEntry *entry;
	guint hue_count = 0, sat_count = 0, val_count = 0;

	g_assert(RS_IS_TIFF(tiff));

	entry = rs_tiff_get_ifd_entry(tiff, ifd, dims_tag);
	if (entry && (entry->count > 1))
	{
		hue_count = rs_tiff_get_uint(tiff, entry->value_offset);
		sat_count = rs_tiff_get_uint(tiff, entry->value_offset+4);

		if (entry->count > 2)
			val_count = rs_tiff_get_uint(tiff, entry->value_offset+8);

		entry = rs_tiff_get_ifd_entry(tiff, ifd, table_tag);
		if (entry && (entry->count == (hue_count * sat_count * val_count * 3)))
		{
			gboolean skipSat0 = FALSE; /* FIXME */
			gint val, hue, sat;
			gint offset = entry->value_offset;

			map = rs_huesat_map_new(hue_count, sat_count, val_count);
			for (val = 0; val < val_count; val++)
			{
				for (hue = 0; hue < hue_count; hue++)
				{
					for (sat = (skipSat0) ? 1 : 0; sat < sat_count; sat++)
					{
						RS_VECTOR3 modify;

						modify.h = rs_tiff_get_float(tiff, offset);
						modify.s = rs_tiff_get_float(tiff, offset+4);
						modify.v = rs_tiff_get_float(tiff, offset+8);
						offset += 12;

						rs_huesat_map_set_delta(map, hue, sat, val, &modify);
					}
				}
			}
		}
	}

	if (NULL == map)
		return map;

	entry = NULL;
	if (table_tag == 50938 || table_tag == 50939) /* ProfileHueSatMapData1  or  ProfileHueSatMapData2*/
		entry = rs_tiff_get_ifd_entry(tiff, ifd, 51107); /*ProfileHueSatMapEncoding */
	else if (table_tag == 50982) /* ProfileLookTableData */
		entry = rs_tiff_get_ifd_entry(tiff, ifd, 51108);  /*ProfileLookTableEncoding*/

	if (NULL != entry && entry->count == 1)
		map->v_encoding = entry->value_offset;

	return map;
}

RSHuesatMap *
rs_huesat_map_new_interpolated(const RSHuesatMap *map1, RSHuesatMap *map2, gfloat weight1)
{
	RSHuesatMap *map = NULL;

	g_assert(RS_IS_HUESAT_MAP(map1));
	g_assert(RS_IS_HUESAT_MAP(map2));

	if (weight1 >= 1.0)
		return RS_HUESAT_MAP(g_object_ref(G_OBJECT(map1)));
	else if (weight1 <= 0.0)
		return RS_HUESAT_MAP(g_object_ref(G_OBJECT(map2)));

	if ((map1->hue_divisions == map2->hue_divisions) &&
	    (map1->sat_divisions == map2->sat_divisions) &&
	    (map1->val_divisions == map2->val_divisions))
	{
		map = rs_huesat_map_new(map1->hue_divisions, map1->sat_divisions, map1->val_divisions);
		gfloat weight2 = 1.0 - weight1;

		const RS_VECTOR3 *data1 = map1->deltas;
		const RS_VECTOR3 *data2 = map1->deltas;
		RS_VECTOR3 *data3 = map1->deltas;
		gint count = map1->hue_divisions * map1->sat_divisions * map1->val_divisions;

		gint index;
		for (index = 0; index < count; index++)
		{
			data3->h = weight1 * data1->h + weight2 * data2->h;
			data3->s = weight1 * data1->s + weight2 * data2->s;
			data3->v = weight1 * data1->v + weight2 * data2->v;

			data1++;
			data2++;
			data3++;
		}
		map->v_encoding = map1->v_encoding;
	}

	return map;
}

guint
rs_huesat_map_get_deltacount(RSHuesatMap *map)
{
	return map->val_divisions * map->hue_divisions * map->sat_divisions;
}

void
rs_huesat_map_get_delta(RSHuesatMap *map, const guint hue_div, const guint sat_div, const guint val_div, RS_VECTOR3 *modify)
{
	g_assert(RS_IS_HUESAT_MAP(map));
	if (hue_div >= map->hue_divisions || sat_div >= map->sat_divisions || val_div >= map->val_divisions)
	{
		modify->h = 0.0;
		modify->s = 1.0;
		modify->v = 1.0;

		return;
	}

	gint offset = val_div * map->val_step + hue_div * map->hue_step + sat_div;

	*modify = map->deltas[offset];
}

void
rs_huesat_map_set_delta(RSHuesatMap *map, const guint hue_div, const guint sat_div, const guint val_div, const RS_VECTOR3 *modify)
{
	g_assert(RS_IS_HUESAT_MAP(map));

	if (hue_div >= map->hue_divisions || sat_div >= map->sat_divisions || val_div >= map->val_divisions)
		return;

	gint offset = val_div * map->val_step + hue_div * map->hue_step + sat_div;

	map->deltas[offset] = *modify;

	if (sat_div == 0)
		map->deltas[offset].v = 1.0;

	else if (sat_div == 1)
	{
		RS_VECTOR3 zero_sat_modify;
		rs_huesat_map_get_delta(map, hue_div, 0, val_div, &zero_sat_modify);
		if (zero_sat_modify.v != 1.0f)
		{
			zero_sat_modify = *modify;
			zero_sat_modify.v = 1.0;
			rs_huesat_map_set_delta(map, hue_div, 0, val_div, &zero_sat_modify);
		}
	}
}

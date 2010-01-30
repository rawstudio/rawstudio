/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#include "rs-dcp-file.h"

struct _RSDcpFile {
	RSTiff parent;
	gboolean dispose_has_run;

	gchar *model;
	gchar *signature;
	gchar *name;
	gchar *copyright;
	gchar *id;
};

static gboolean read_file_header(RSTiff *tiff);

G_DEFINE_TYPE (RSDcpFile, rs_dcp_file, RS_TYPE_TIFF)

static void
rs_dcp_file_dispose(GObject *object)
{
	RSDcpFile *dcp_file = RS_DCP_FILE(object);

	if (!dcp_file->dispose_has_run)
	{
		dcp_file->dispose_has_run = TRUE;

		g_free(dcp_file->model);
		g_free(dcp_file->signature);
		g_free(dcp_file->name);
		g_free(dcp_file->copyright);
		g_free(dcp_file->id);
	}

	G_OBJECT_CLASS(rs_dcp_file_parent_class)->dispose(object);
}

static void
rs_dcp_file_class_init(RSDcpFileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	RSTiffClass *tiff_class = RS_TIFF_CLASS(klass);

	tiff_class->read_file_header = read_file_header;
	object_class->dispose = rs_dcp_file_dispose;
}

static void
rs_dcp_file_init(RSDcpFile *dcp_file)
{
}

static gboolean
read_file_header(RSTiff *tiff)
{
	gboolean ret = TRUE;

	/* Parse TIFF */
	RS_TIFF_CLASS(rs_dcp_file_parent_class)->read_file_header(tiff);

	/* Read DCP Magic */
	if (rs_tiff_get_ushort(tiff, 2) != 0x4352)
		ret = TRUE;

	return ret;
}

static gboolean
read_matrix(RSDcpFile *dcp_file, guint ifd, gushort tag, RS_MATRIX3 *matrix)
{
	RSTiff *tiff = RS_TIFF(dcp_file);
	gboolean ret = FALSE;
	RSTiffIfdEntry *entry = rs_tiff_get_ifd_entry(tiff, ifd, tag);

	if (entry && matrix)
	{
		matrix->coeff[0][0] = rs_tiff_get_rational(tiff, entry->value_offset);
		matrix->coeff[0][1] = rs_tiff_get_rational(tiff, entry->value_offset+8);
		matrix->coeff[0][2] = rs_tiff_get_rational(tiff, entry->value_offset+16);
		matrix->coeff[1][0] = rs_tiff_get_rational(tiff, entry->value_offset+24);
		matrix->coeff[1][1] = rs_tiff_get_rational(tiff, entry->value_offset+32);
		matrix->coeff[1][2] = rs_tiff_get_rational(tiff, entry->value_offset+40);
		matrix->coeff[2][0] = rs_tiff_get_rational(tiff, entry->value_offset+48);
		matrix->coeff[2][1] = rs_tiff_get_rational(tiff, entry->value_offset+56);
		matrix->coeff[2][2] = rs_tiff_get_rational(tiff, entry->value_offset+64);
		ret = TRUE;
	}

	return ret;
}

static gfloat
read_illuminant(RSDcpFile *dcp_file, guint ifd, gushort tag)
{
	RSTiffIfdEntry *entry = rs_tiff_get_ifd_entry(RS_TIFF(dcp_file), ifd, tag);

	if (!entry)
		return 5000.0;

	enum {
		lsUnknown                   =  0,

		lsDaylight                  =  1,
		lsFluorescent               =  2,
		lsTungsten                  =  3,
		lsFlash                     =  4,
		lsFineWeather               =  9,
		lsCloudyWeather             = 10,
		lsShade                     = 11,
		lsDaylightFluorescent       = 12,       // D 5700 - 7100K
		lsDayWhiteFluorescent       = 13,       // N 4600 - 5400K
		lsCoolWhiteFluorescent      = 14,       // W 3900 - 4500K
		lsWhiteFluorescent          = 15,       // WW 3200 - 3700K
		lsStandardLightA            = 17,
		lsStandardLightB            = 18,
		lsStandardLightC            = 19,
		lsD55                       = 20,
		lsD65                       = 21,
		lsD75                       = 22,
		lsD50                       = 23,
		lsISOStudioTungsten         = 24,

		lsOther                     = 255
	};

	switch (entry->value_offset)
	{
		case lsStandardLightA:
		case lsTungsten:
			return 2850.0;

		case lsISOStudioTungsten:
			return 3200.0;

		case lsD50:
			return 5000.0;

		case lsD55:
		case lsDaylight:
		case lsFineWeather:
		case lsFlash:
		case lsStandardLightB:
			return 5500.0;
		case lsD65:
		case lsStandardLightC:
		case lsCloudyWeather:
			return 6500.0;

		case lsD75:
		case lsShade:
			return 7500.0;

		case lsDaylightFluorescent:
			return (5700.0 + 7100.0) * 0.5;

		case lsDayWhiteFluorescent:
			return (4600.0 + 5400.0) * 0.5;

		case lsCoolWhiteFluorescent:
		case lsFluorescent:
			return (3900.0 + 4500.0) * 0.5;

		case lsWhiteFluorescent:
			return (3200.0 + 3700.0) * 0.5;

		default:
			return 0.0;
	}

	return 5000.0;
}

const gchar *
read_ascii(RSDcpFile *dcp_file, guint ifd, gushort tag, gchar **cache)
{
	GStaticMutex lock = G_STATIC_MUTEX_INIT;
	g_static_mutex_lock(&lock);
	if (!*cache)
		*cache = rs_tiff_get_ascii(RS_TIFF(dcp_file), ifd, tag);
	g_static_mutex_unlock(&lock);

	return *cache;
}

RSDcpFile *
rs_dcp_file_new_from_file(const gchar *path)
{
	return g_object_new(RS_TYPE_DCP_FILE, "filename", path, NULL);
}

const gchar *
rs_dcp_file_get_model(RSDcpFile *dcp_file)
{
	return read_ascii(dcp_file, 0, 0xc614, &dcp_file->model);
}

gboolean
rs_dcp_file_get_color_matrix1(RSDcpFile *dcp_file, RS_MATRIX3 *matrix)
{
	return read_matrix(dcp_file, 0, 0xc621, matrix);
}

gboolean
rs_dcp_file_get_color_matrix2(RSDcpFile *dcp_file, RS_MATRIX3 *matrix)
{
	return read_matrix(dcp_file, 0, 0xc622, matrix);
}

gfloat
rs_dcp_file_get_illuminant1(RSDcpFile *dcp_file)
{
	return read_illuminant(dcp_file, 0, 0xc65a);
}

gfloat
rs_dcp_file_get_illuminant2(RSDcpFile *dcp_file)
{
	return read_illuminant(dcp_file, 0, 0xc65b);
}

const gchar *
rs_dcp_file_get_signature(RSDcpFile *dcp_file)
{
	return read_ascii(dcp_file, 0, 0xc6f4, &dcp_file->signature);
}

const gchar *
rs_dcp_file_get_name(RSDcpFile *dcp_file)
{
	return read_ascii(dcp_file, 0, 0xc6f8, &dcp_file->name);
}

RSHuesatMap *
rs_dcp_file_get_huesatmap1(RSDcpFile *dcp_file)
{
	return rs_huesat_map_new_from_dcp(RS_TIFF(dcp_file), 0, 0xc6f9, 0xc6fa);
}

RSHuesatMap *
rs_dcp_file_get_huesatmap2(RSDcpFile *dcp_file)
{
	return rs_huesat_map_new_from_dcp(RS_TIFF(dcp_file), 0, 0xc6f9, 0xc6fb);
}

RSSpline *
rs_dcp_file_get_tonecurve(RSDcpFile *dcp_file)
{
	RSSpline *ret = NULL;
	RSTiff *tiff = RS_TIFF(dcp_file);

	RSTiffIfdEntry *entry = rs_tiff_get_ifd_entry(tiff, 0, 0xc6fc);

	if (entry)
	{
		gint i;
		gint num_knots = entry->count / 2;
		gfloat *knots = g_new0(gfloat, entry->count);

		for(i = 0; i < entry->count; i++)
			knots[i] = rs_tiff_get_float(tiff, (entry->value_offset+(i*4)));

		ret = rs_spline_new(knots, num_knots, NATURAL);
		g_free(knots);
	}

	return ret;
}

const gchar *
rs_dcp_file_get_copyright(RSDcpFile *dcp_file)
{
	return read_ascii(dcp_file, 0, 0xc6fe, &dcp_file->copyright);
}

gboolean
rs_dcp_file_get_forward_matrix1(RSDcpFile *dcp_file, RS_MATRIX3 *matrix)
{
	return read_matrix(dcp_file, 0, 0xc714, matrix);
}

gboolean
rs_dcp_file_get_forward_matrix2(RSDcpFile *dcp_file, RS_MATRIX3 *matrix)
{
	return read_matrix(dcp_file, 0, 0xc715, matrix);
}

RSHuesatMap *
rs_dcp_file_get_looktable(RSDcpFile *dcp_file)
{
	return rs_huesat_map_new_from_dcp(RS_TIFF(dcp_file), 0, 0xc725, 0xc726);
}

const gchar *
rs_dcp_get_id(RSDcpFile *dcp_file)
{
	g_assert(RS_IS_DCP_FILE(dcp_file));

	if (dcp_file->id)
		return dcp_file->id;

	const gchar *dcp_filename = rs_tiff_get_filename_nopath(RS_TIFF(dcp_file));
	const gchar *dcp_model = rs_dcp_file_get_model(dcp_file);
	const gchar *dcp_name = rs_dcp_file_get_name(dcp_file);
	
	/* Concat all three elements */
	gchar *id = g_strconcat(dcp_filename, dcp_model, dcp_name, NULL);

	/* Convert to lower case to eliminate case mismatches */
	dcp_file->id = g_ascii_strdown(id, -1);
	g_free(id);

	return dcp_file->id;
}

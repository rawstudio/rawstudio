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

#include <sys/stat.h>
#include <arpa/inet.h> /* ntohl() */
#include <glib.h>
#include <glib/gstdio.h>
#include "rs-icc-profile.h"

struct _RSIccProfile {
	GObject parent;
	gboolean dispose_has_run;

	gchar *filename;
    gchar *map;
    gsize map_length;
	RSIccProfile_ColorSpace colorspace;
	RSIccProfile_Class profile_class;
};

G_DEFINE_TYPE (RSIccProfile, rs_icc_profile, G_TYPE_OBJECT)

static void get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean read_from_file(RSIccProfile *profile, const gchar *path);
static gboolean read_from_memory(RSIccProfile *profile, gchar *map, gsize map_length, gboolean copy);

GType
rs_icc_colorspace_get_type(void)
{
	static GType icc_colorspace_type = 0;

	static const GEnumValue icc_colorspace[] = {
		{RS_ICC_COLORSPACE_UNDEFINED, "Colorspace not available", "n/a"},
		{RS_ICC_COLORSPACE_XYZ, "XYZ", "xyz"},
		{RS_ICC_COLORSPACE_LAB, "Lab", "lab"},
		{RS_ICC_COLORSPACE_LUV, "Luv", "luv"},
		{RS_ICC_COLORSPACE_YCBCR, "YCbCr", "ycbcr"},
		{RS_ICC_COLORSPACE_YXY, "Yxy", "YXY"},
		{RS_ICC_COLORSPACE_RGB, "RGB", "rgb"},
		{RS_ICC_COLORSPACE_GREY, "Grey", "grey"},
		{RS_ICC_COLORSPACE_HSV, "HSV", "hsv"},
		{RS_ICC_COLORSPACE_HLS, "HLS", "hls"},
		{RS_ICC_COLORSPACE_CMYK, "CMYK", "cmyk"},
		{RS_ICC_COLORSPACE_CMY, "CMY", "cmy"},
		{0, NULL, NULL}
	};

	if (!icc_colorspace_type)
		icc_colorspace_type = g_enum_register_static("RSIccColorSpace", icc_colorspace);

	return icc_colorspace_type;
}

GType
rs_icc_profile_class_get_type(void)
{
	static GType icc_profile_class_type = 0;

	static const GEnumValue icc_profile_class[] = {
		{RS_ICC_PROFILE_UNDEFINED, "Profile class undefined", "n/a"},
		{RS_ICC_PROFILE_INPUT, "Input Device profile", "input"},
		{RS_ICC_PROFILE_DISPLAY, "Display Device profile", "display"},
		{RS_ICC_PROFILE_OUTPUT, "Output Device profile", "output"},
		{RS_ICC_PROFILE_DEVICELINK, "DeviceLinkprofile", "devicelink"},
		{RS_ICC_PROFILE_COLORSPACE_CONVERSION, "ColorSpace Conversion profile", "colorspace-conversion"},
		{RS_ICC_PROFILE_ACSTRACT, "Abstract profile", "abstract"},
		{RS_ICC_PROFILE_NAMED_COLOR, "Named colour profile", "named-color"},
		{0, NULL, NULL}
	};

	if (!icc_profile_class_type)
		icc_profile_class_type = g_enum_register_static("RSIccProfileClass", icc_profile_class);

	return icc_profile_class_type;
}

GType
rs_icc_intent_get_type(void)
{
	static GType rs_icc_intent_type = 0;

	static const GEnumValue rs_icc_intents[] = {
		{RS_ICC_INTENT_PERCEPTUAL, "Perceptual intent", "perceptual"},
		{RS_ICC_INTENT_RELATIVE_COLORIMETRIC, "Relative colorimetric", "relative"},
		{RS_ICC_INTENT_SATURATION, "Saturation", "saturation"},
		{RS_ICC_INTENT_ABSOLUTE_COLORIMETRIC, "Absolute colorimetric", "absolute"},
		{0, NULL, NULL},
	};

	if (!rs_icc_intent_type)
		rs_icc_intent_type = g_enum_register_static("RSIccIntent", rs_icc_intents);

	return rs_icc_intent_type;
}

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_COLORSPACE,
	PROP_CLASS
};

static void
dispose(GObject *object)
{
	RSIccProfile *profile = RS_ICC_PROFILE(object);

	if (!profile->dispose_has_run)
	{
		g_free(profile->filename);
		g_free(profile->map);

		profile->dispose_has_run = TRUE;
	}

	/* Chain up */
	G_OBJECT_CLASS(rs_icc_profile_parent_class)->dispose(object);
}

static void
finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_icc_profile_parent_class)->finalize(object);
}

static void
rs_icc_profile_class_init(RSIccProfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_FILENAME, g_param_spec_string(
			"filename", "Filename", "The filename of the loaded profile",
			NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
		PROP_COLORSPACE, g_param_spec_enum(
			"colorspace", "colorspace", "Profile colorspace",
			RS_TYPE_ICC_COLORSPACE, RS_ICC_COLORSPACE_UNDEFINED, G_PARAM_READABLE));

	g_object_class_install_property(object_class,
		PROP_CLASS, g_param_spec_enum(
			"profile-class", "profile-class", "Profile class",
			RS_TYPE_ICC_PROFILE_CLASS, RS_ICC_PROFILE_UNDEFINED, G_PARAM_READABLE));

	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSIccProfile *profile = RS_ICC_PROFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, profile->filename);
			break;
		case PROP_COLORSPACE:
			g_value_set_enum(value, profile->colorspace);
			break;
		case PROP_CLASS:
			g_value_set_enum(value, profile->profile_class);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSIccProfile *profile = RS_ICC_PROFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_free(profile->filename);
			profile->filename = g_value_dup_string(value);
			read_from_file(profile, profile->filename);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
rs_icc_profile_init(RSIccProfile *profile)
{
	profile->dispose_has_run = FALSE;
	profile->filename = NULL;
	profile->map = NULL;
	profile->map_length = 0;
	profile->colorspace = RS_ICC_COLORSPACE_UNDEFINED;
	profile->profile_class = 0;
}

static gboolean
read_from_file(RSIccProfile *profile, const gchar *path)
{
	gboolean ret = FALSE;
	struct stat st;
	GError *error;

	g_stat(path, &st);

	/* We only accept files below 10MiB */
	if (st.st_size > (10*1024*1024))
		return ret;

	/* Or more than the ICC header... */
	if (st.st_size < 128)
		return ret;

	if (profile->map)
		g_free(profile->map);

	error = NULL;
	if(g_file_get_contents(path, &profile->map, &profile->map_length, &error))
		ret = read_from_memory(profile, profile->map, profile->map_length, FALSE);
	else if (error)
		g_warning("GError: '%s'", error->message);

	return ret;
}

static gboolean
read_from_memory(RSIccProfile *profile, gchar *map, gsize map_length, gboolean copy)
{
	/* For now, we don't fail :) */
	gboolean ret = TRUE;

	if (copy)
		profile->map = g_memdup(map, map_length);
	else
		profile->map = map;

	profile->map_length = map_length;

	/* Macro for reading unsigned integers from ICC profiles */
#define _GUINT(map, offset) (ntohl(*((guint *)(&map[offset]))))
	profile->colorspace = _GUINT(profile->map, 16);
	profile->profile_class = _GUINT(profile->map, 12);
#undef _GUINT

	return ret;
}

/**
 * Construct new RSIccProfile from an ICC profile on disk
 * @param path An absolute path to an ICC profile
 * @return A new RSIccProfile object or NULL on error
 */
RSIccProfile *
rs_icc_profile_new_from_file(const gchar *path)
{
	g_assert(path != NULL);
	g_assert(path[0] == G_DIR_SEPARATOR);

	RSIccProfile *profile = g_object_new (RS_TYPE_ICC_PROFILE, "filename", path, NULL);

	return profile;
}

/**
 * Construct new RSIccProfile from an in-memory ICC profile
 * @param map A pointer to a complete ICC profile
 * @param map_length The length of the profile in bytes
 * @param copy TRUE if the data should be copied, FALSE otherwise
 * @return A new RSIccProfile object or NULL on error
 */
RSIccProfile *
rs_icc_profile_new_from_memory(gchar *map, gsize map_length, gboolean copy)
{
	g_assert(map != NULL);
	g_assert(map_length >= 0);

	RSIccProfile *profile = g_object_new (RS_TYPE_ICC_PROFILE, NULL);

	if (!read_from_memory(profile, map, map_length, copy))
	{
		g_object_unref(profile);
		profile = NULL;
	}

	return profile;
}

/**
 * Get binary profile data
 * @param profile A RSIccProfile
 * @param map A pointer to a gchar pointer
 * @param map_length A pointer to a gsize, the length of the profile will be written here
 */
gboolean
rs_icc_profile_get_data(const RSIccProfile *profile, gchar **map, gsize *map_length)
{
	gboolean ret = FALSE;

	g_assert(RS_IS_ICC_PROFILE(profile));
	g_assert(map != NULL);
	g_assert(map_length != NULL);

	if (profile->map)
	{
		*map = g_memdup(profile->map, profile->map_length);
		*map_length = profile->map_length;
		ret = TRUE;
	}

	return ret;
}

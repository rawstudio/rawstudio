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

#ifndef RS_ICC_PROFILE_H
#define RS_ICC_PROFILE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_ICC_PROFILE rs_icc_profile_get_type()
#define RS_ICC_PROFILE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_ICC_PROFILE, RSIccProfile))
#define RS_ICC_PROFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_ICC_PROFILE, RSIccProfileClass))
#define RS_IS_ICC_PROFILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_ICC_PROFILE))
#define RS_IS_ICC_PROFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_ICC_PROFILE))
#define RS_ICC_PROFILE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_ICC_PROFILE, RSIccProfileClass))

typedef struct _RSIccProfile RSIccProfile;

typedef struct {
	GObjectClass parent_class;
} RSIccProfileClass;

GType rs_icc_profile_get_type(void);

typedef enum {
	RS_ICC_COLORSPACE_UNDEFINED = 0x0,
	RS_ICC_COLORSPACE_XYZ       = 0x58595A20,
	RS_ICC_COLORSPACE_LAB       = 0x4C616220,
	RS_ICC_COLORSPACE_LUV       = 0x4C757620,
	RS_ICC_COLORSPACE_YCBCR     = 0x59436272,
	RS_ICC_COLORSPACE_YXY       = 0x59787920,
	RS_ICC_COLORSPACE_RGB       = 0x52474220,
	RS_ICC_COLORSPACE_GREY      = 0x47524159,
	RS_ICC_COLORSPACE_HSV       = 0x48535620,
	RS_ICC_COLORSPACE_HLS       = 0x484C5320,
	RS_ICC_COLORSPACE_CMYK      = 0x434D594B,
	RS_ICC_COLORSPACE_CMY       = 0x434D5920
} RSIccProfile_ColorSpace;

#define RS_TYPE_ICC_COLORSPACE rs_icc_colorspace_get_type()
GType rs_icc_colorspace_get_type(void);

typedef enum {
	RS_ICC_PROFILE_UNDEFINED             = 0x0,
	RS_ICC_PROFILE_INPUT                 = 0x73636E72,
	RS_ICC_PROFILE_DISPLAY               = 0x6D6E7472,
	RS_ICC_PROFILE_OUTPUT                = 0x70727472,
	RS_ICC_PROFILE_DEVICELINK            = 0x6C696E6B,
	RS_ICC_PROFILE_COLORSPACE_CONVERSION = 0x73706163,
	RS_ICC_PROFILE_ACSTRACT              = 0x61627374,
	RS_ICC_PROFILE_NAMED_COLOR           = 0x6E6D636C
} RSIccProfile_Class;

#define RS_TYPE_ICC_PROFILE_CLASS rs_icc_profile_class_get_type()
GType rs_icc_profile_class_get_type(void);

typedef enum {
	RS_ICC_INTENT_PERCEPTUAL            = 0,
	RS_ICC_INTENT_RELATIVE_COLORIMETRIC = 1,
	RS_ICC_INTENT_SATURATION            = 2,
	RS_ICC_INTENT_ABSOLUTE_COLORIMETRIC = 3
} RSIccIntent;

#define RS_TYPE_ICC_INTENT rs_icc_intent_get_type()
GType rs_icc_intent_get_type(void);

/**
 * Construct new RSIccProfile from an ICC profile on disk
 * @param path An absolute path to an ICC profile
 * @return A new RSIccProfile object or NULL on error
 */
RSIccProfile *
rs_icc_profile_new_from_file(const gchar *path);

/**
 * Construct new RSIccProfile from an in-memory ICC profile
 * @param map A pointer to a complete ICC profile
 * @param map_length The length of the profile in bytes
 * @param copy TRUE if the data should be copied, FALSE otherwise
 * @return A new RSIccProfile object or NULL on error
 */
RSIccProfile *
rs_icc_profile_new_from_memory(gchar *map, gsize map_length, gboolean copy);

/**
 * Get binary profile data
 * @param profile A RSIccProfile
 * @param map A pointer to a gchar pointer
 * @param map_length A pointer to a gsize, the length of the profile will be written here
 */
gboolean
rs_icc_profile_get_data(const RSIccProfile *icc, gchar **data, gsize *length);

const gchar *
rs_icc_profile_get_description(const RSIccProfile *profile);

G_END_DECLS

#endif /* RS_ICC_PROFILE_H */

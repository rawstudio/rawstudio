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

#ifndef RS_CMS_H
#define RS_CMS_H

typedef enum {
	TRANSFORM_DISPLAY = 0,
	TRANSFORM_EXPORT,
	TRANSFORM_EXPORT16,
	TRANSFORM_SRGB,
	TRANSFORMS
} CMS_TRANSFORM;

typedef enum {
	PROFILE_INPUT = 0,
	PROFILE_DISPLAY,
	PROFILE_EXPORT,
	PROFILES
} CMS_PROFILE;

typedef struct RS_CMS {
	gushort loadtable[65536];
	gboolean enabled;
	gint intent;
	void *genericLoadProfile;
	void *genericRGBProfile;
	void *transforms[TRANSFORMS];
	void *profiles[PROFILES];
	gchar *profile_filenames[PROFILES];
} RS_CMS;

extern void rs_cms_enable(RS_CMS *cms, gboolean enable);
extern gboolean rs_cms_is_profile_valid(const gchar *path);
extern void rs_cms_set_profile(RS_CMS *cms, CMS_PROFILE profile, const gchar *filename);
extern gchar *rs_cms_get_profile_filename(RS_CMS *cms, CMS_PROFILE profile);
extern void rs_cms_set_intent(RS_CMS *cms, gint intent);
extern gint rs_cms_get_intent(RS_CMS *cms);
extern void *rs_cms_get_transform(RS_CMS *cms, CMS_TRANSFORM transform);
extern void rs_cms_prepare_transforms(RS_CMS *cms);
extern RS_CMS *rs_cms_init(void);

#endif /* RS_CMS_H */

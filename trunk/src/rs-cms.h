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
	CMS_PROFILE_INPUT = 0,
	CMS_PROFILE_DISPLAY,
	CMS_PROFILE_EXPORT,
	CMS_PROFILES
} CMS_PROFILE;

typedef struct _RS_CMS RS_CMS;

extern void rs_cms_enable(RS_CMS *cms, gboolean enable);
extern gboolean rs_cms_is_profile_valid(const gchar *path, const CMS_PROFILE profile);
extern void rs_cms_set_profile(RS_CMS *cms, CMS_PROFILE profile, const gchar *filename);
extern void rs_cms_set_intent(RS_CMS *cms, gint intent);
extern gint rs_cms_get_intent(RS_CMS *cms);
extern void *rs_cms_get_transform(RS_CMS *cms, CMS_TRANSFORM transform);
extern void rs_cms_prepare_transforms(RS_CMS *cms);
extern void rs_cms_do_transform(gpointer transform, gpointer input, gpointer output, guint size);
extern RS_CMS *rs_cms_init(void);
extern gboolean cms_get_profile_info_from_file(const gchar *filename, gchar **name, gchar **info, gchar **description);

#endif /* RS_CMS_H */

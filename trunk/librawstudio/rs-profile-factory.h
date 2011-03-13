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

#ifndef RS_PROFILE_FACTORY_H
#define RS_PROFILE_FACTORY_H

#include <glib-object.h>
#include "rs-dcp-file.h"
#include "rs-icc-profile.h"

G_BEGIN_DECLS

#define RS_TYPE_PROFILE_FACTORY rs_profile_factory_get_type()
#define RS_PROFILE_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PROFILE_FACTORY, RSProfileFactory))
#define RS_PROFILE_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PROFILE_FACTORY, RSProfileFactoryClass))
#define RS_IS_PROFILE_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PROFILE_FACTORY))
#define RS_IS_PROFILE_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_PROFILE_FACTORY))
#define RS_PROFILE_FACTORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_PROFILE_FACTORY, RSProfileFactoryClass))

enum {
	RS_PROFILE_FACTORY_STORE_MODEL,
	RS_PROFILE_FACTORY_STORE_PROFILE,
	RS_PROFILE_FACTORY_NUM_FIELDS
};

struct _RSProfileFactory {
	GObject parent;

	GtkListStore *profiles;
};

typedef struct _RSProfileFactory RSProfileFactory;

typedef struct {
	GObjectClass parent_class;
} RSProfileFactoryClass;

GType rs_profile_factory_get_type(void);

void rs_profile_factory_load_profiles(RSProfileFactory *factory, const gchar *path, gboolean load_dcp, gboolean load_icc);

RSProfileFactory *rs_profile_factory_new(const gchar *search_path);

RSProfileFactory *rs_profile_factory_new_default(void);

const gchar *rs_profile_factory_get_user_profile_directory(void);

gboolean rs_profile_factory_add_profile(RSProfileFactory *factory, const gchar *path);

GtkTreeModelFilter *rs_dcp_factory_get_compatible_as_model(RSProfileFactory *factory, const gchar *unique_id);

RSDcpFile *rs_profile_factory_find_from_id(RSProfileFactory *factory, const gchar *path);

RSIccProfile *rs_profile_factory_find_icc_from_filename(RSProfileFactory *factory, const gchar *path);

GSList *rs_profile_factory_find_from_model(RSProfileFactory *factory, const gchar *id);

void rs_profile_factory_set_embedded_profile(RSProfileFactory *factory, const RSIccProfile *profile);

G_END_DECLS

#endif /* RS_PROFILE_FACTORY_H */

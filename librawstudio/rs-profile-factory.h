#ifndef RS_PROFILE_FACTORY_H
#define RS_PROFILE_FACTORY_H

#include <glib-object.h>
#include "rs-dcp-file.h"

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

typedef struct _RSProfileFactory RSProfileFactory;

typedef struct {
	GObjectClass parent_class;
} RSProfileFactoryClass;

GType rs_profile_factory_get_type(void);

RSProfileFactory *rs_profile_factory_new(const gchar *search_path);

RSProfileFactory *rs_profile_factory_new_default(void);

const gchar *rs_profile_factory_get_user_profile_directory(void);

gboolean rs_profile_factory_add_profile(RSProfileFactory *factory, const gchar *path);

GtkTreeModelFilter *rs_dcp_factory_get_compatible_as_model(RSProfileFactory *factory, const gchar *unique_id);

RSDcpFile *rs_profile_factory_find_from_id(RSProfileFactory *factory, const gchar *path);

G_END_DECLS

#endif /* RS_PROFILE_FACTORY_H */

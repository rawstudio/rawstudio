#ifndef RS_FILTER_PARAM_H
#define RS_FILTER_PARAM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_FILTER_PARAM rs_filter_param_get_type()
#define RS_FILTER_PARAM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_FILTER_PARAM, RSFilterParam))
#define RS_FILTER_PARAM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_FILTER_PARAM, RSFilterParamClass))
#define RS_IS_FILTER_PARAM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_FILTER_PARAM))
#define RS_IS_FILTER_PARAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_FILTER_PARAM))
#define RS_FILTER_PARAM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_FILTER_PARAM, RSFilterParamClass))

typedef struct {
	GObject parent;
	gboolean dispose_has_run;

	GHashTable *properties;
} RSFilterParam;

typedef struct {
	GObjectClass parent_class;
} RSFilterParamClass;

GType rs_filter_param_get_type(void);

RSFilterParam *rs_filter_param_new(void);

void
rs_filter_param_clone(RSFilterParam *destination, const RSFilterParam *source);

/**
 * Set a string property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param str NULL-terminated string to set (will be copied)
 */
void rs_filter_param_set_string(RSFilterParam *filter_param, const gchar *name, const gchar *str);

/**
 * Get a string property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param str A pointer to a string pointer where the value of the property can be saved. Should not be freed
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_string(const RSFilterParam *filter_param, const gchar *name, const gchar ** const str);

/**
 * Set a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A value to store
 */
void
rs_filter_param_set_float(RSFilterParam *filter_param, const gchar *name, const gfloat value);

/**
 * Get a float property
 * @param filter_param A RSFilterParam
 * @param name The name of the property
 * @param value A pointer to a gfloat where the value will be stored
 * @return TRUE if the property was found, FALSE otherwise
 */
gboolean rs_filter_param_get_float(const RSFilterParam *filter_param, const gchar *name, gfloat *value);

G_END_DECLS

#endif /* RS_FILTER_PARAM_H */

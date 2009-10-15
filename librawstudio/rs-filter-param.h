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
} RSFilterParam;

typedef struct {
	GObjectClass parent_class;
} RSFilterParamClass;

GType rs_filter_param_get_type(void);

RSFilterParam *rs_filter_param_new(void);

G_END_DECLS

#endif /* RS_FILTER_PARAM_H */

#include "rs-filter-param.h"

G_DEFINE_TYPE(RSFilterParam, rs_filter_param, G_TYPE_OBJECT)

static void
rs_filter_param_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_filter_param_parent_class)->dispose (object);
}

static void
rs_filter_param_finalize(GObject *object)
{
	G_OBJECT_CLASS(rs_filter_param_parent_class)->finalize (object);
}

static void
rs_filter_param_class_init(RSFilterParamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = rs_filter_param_dispose;
	object_class->finalize = rs_filter_param_finalize;
}

static void
rs_filter_param_init(RSFilterParam *param)
{
}

RSFilterParam *
rs_filter_param_new(void)
{
	return g_object_new (RS_TYPE_FILTER_PARAM, NULL);
}

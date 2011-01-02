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

/* Plugin tmpl version 5 */

#include <rawstudio.h>

#define RS_TYPE_TEMPLATE (rs_template_type)
#define RS_TEMPLATE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TEMPLATE, RSTemplate))
#define RS_TEMPLATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TEMPLATE, RSTemplateClass))
#define RS_IS_TEMPLATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TEMPLATE))

typedef struct _RSTemplate RSTemplate;
typedef struct _RSTemplateClass RSTemplateClass;

struct _RSTemplate {
	RSFilter parent;

	gchar *changeme;
};

struct _RSTemplateClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_template, RSTemplate)

enum {
	PROP_0,
	PROP_CHANGEME
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);

static RSFilterClass *rs_template_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_template_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_template_class_init(RSTemplateClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_template_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_CHANGEME, g_param_spec_string (
			"changeme",
			"Changeme nick",
			"Changeme blurb",
			NULL,
			G_PARAM_READWRITE)
	);

	filter_class->name = "Template filter";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
}

static void
rs_template_init(RSTemplate *template)
{
	template->changeme = NULL;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSTemplate *template = RS_TEMPLATE(object);

	switch (property_id)
	{
		case PROP_CHANGEME:
			g_value_get_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSTemplate *template = RS_TEMPLATE(object);

	switch (property_id)
	{
		case PROP_CHANGEME:
			g_free(template->changeme);
			template->changeme = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSTemplate *template = RS_TEMPLATE(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = rs_image16_copy(input, FALSE);
	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	/* Process output */

	g_object_unref(input);
	return response;
}

static gint
get_width(RSFilter *filter)
{
	RSTemplate *template = RS_TEMPLATE(filter);

	return -1;
}

static gint
get_height(RSFilter *filter)
{
	RSTemplate *template = RS_TEMPLATE(filter);

	return -1;
}


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

/* Output plugin tmpl version 1 */

#include <rawstudio.h>
#include <gettext.h>
#include "config.h"

#define RS_TYPE_TEMPLATE (rs_template_type)
#define RS_TEMPLATE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TEMPLATE, RSTemplate))
#define RS_TEMPLATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TEMPLATE, RSTemplateClass))
#define RS_IS_TEMPLATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TEMPLATE))

typedef struct _RSTemplate RSTemplate;
typedef struct _RSTemplateClass RSTemplateClass;

struct _RSTemplate {
	RSOutput parent;

	gchar *filename;
	gint quality;
};

struct _RSTemplateClass {
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT(rs_template, RSTemplate)

enum {
	PROP_0,
	PROP_FILENAME
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean execute8(RSOutput *output, GdkPixbuf *pixbuf);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_template_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_template_class_init(RSTemplateClass *klass)
{
	RSOutputClass *output_class = RS_OUTPUT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_FILENAME, g_param_spec_string(
			"filename", "filename", "Full export path",
			NULL, G_PARAM_READWRITE)
	);

	output_class->execute8 = execute8;
	output_class->display_name = _("Change this");
}

static void
rs_template_init(RSTemplate *template)
{
	template->filename = NULL;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSTemplate *template = RS_TEMPLATE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, template->filename);
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
		case PROP_FILENAME:
			template->filename = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gboolean
execute8(RSOutput *output, GdkPixbuf *pixbuf)
{
	RSTemplate *template = RS_TEMPLATE(output);

	/* Do everything */

	return TRUE;
}


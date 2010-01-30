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

#define RS_TYPE_PNGFILE (rs_pngfile_type)
#define RS_PNGFILE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PNGFILE, RSPngfile))
#define RS_PNGFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PNGFILE, RSPngfileClass))
#define RS_IS_PNGFILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PNGFILE))

typedef struct _RSPngfile RSPngfile;
typedef struct _RSPngfileClass RSPngfileClass;

struct _RSPngfile {
	RSOutput parent;

	gchar *filename;
	gint quality;
};

struct _RSPngfileClass {
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT(rs_pngfile, RSPngfile)

enum {
	PROP_0,
	PROP_FILENAME
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean execute(RSOutput *output, RSFilter *filter);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_pngfile_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_pngfile_class_init(RSPngfileClass *klass)
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

	output_class->execute = execute;
	output_class->extension = "png";
	output_class->display_name = _("PNG (Portable Network Graphics)");
}

static void
rs_pngfile_init(RSPngfile *pngfile)
{
	pngfile->filename = NULL;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSPngfile *pngfile = RS_PNGFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, pngfile->filename);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSPngfile *pngfile = RS_PNGFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			pngfile->filename = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gboolean
execute(RSOutput *output, RSFilter *filter)
{
	gboolean ret;
	RSFilterResponse *response;
	RSPngfile *pngfile = RS_PNGFILE(output);
	response = rs_filter_get_image8(filter, NULL);
	GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);

	ret = gdk_pixbuf_save(pixbuf, pngfile->filename, "png", NULL, NULL);

	g_object_unref(response);
	g_object_unref(pixbuf);

	gchar *input_filename = NULL;
	rs_filter_get_recursive(filter, "filename", &input_filename, NULL);
	rs_exif_copy(input_filename, pngfile->filename);
	g_free(input_filename);

	return ret;
}

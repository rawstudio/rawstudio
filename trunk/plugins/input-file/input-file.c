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

#include <rawstudio.h>

#define RS_TYPE_INPUT_FILE (rs_input_file_type)
#define RS_INPUT_FILE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_INPUT_FILE, RSInputFile))
#define RS_INPUT_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_INPUT_FILE, RSInputFileClass))
#define RS_IS_INPUT_FILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_INPUT_FILE))

typedef struct _RSInputFile RSInputFile;
typedef struct _RSInputFileClass RSInputFileClass;

struct _RSInputFile {
	RSFilter parent;

	gchar *filename;
	RS_IMAGE16 *image;
	RSColorSpace *colorspace;	
};

struct _RSInputFileClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_input_file, RSInputFile)

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_COLOR_SPACE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static RSFilterResponse *get_size(RSFilter *filter, const RSFilterRequest *request);

static RSFilterClass *rs_input_file_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_input_file_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_input_file_class_init (RSInputFileClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_input_file_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_FILENAME, g_param_spec_string (
			"filename",
			"Filename",
			"The filename of the file to open",
			NULL,
			G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_COLOR_SPACE, g_param_spec_object(
			"color-space", "color-space", "A colorspace to assign input",
			RS_TYPE_COLOR_SPACE, G_PARAM_READWRITE));

	filter_class->name = "File loader based on rs_filetypes";
	filter_class->get_image = get_image;
	filter_class->get_size = get_size;
}

static void
rs_input_file_init (RSInputFile *filter)
{
	filter->filename = NULL;
	filter->image = NULL;
}

static void
get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSInputFile *input = RS_INPUT_FILE(object);
	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_get_string (value);
			break;
		case PROP_COLOR_SPACE:
			g_value_set_object(value, input->colorspace);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSInputFile *input = RS_INPUT_FILE(object);
	switch (property_id)
	{
		case PROP_FILENAME:
			g_free (input->filename);
			input->filename = g_value_dup_string (value);
			if (input->image)
				g_object_unref(input->image);
			input->image = NULL;
			RSFilterResponse *response = rs_filetype_load(input->filename);
			if (response && RS_IS_FILTER_RESPONSE(response))
			{
				if (rs_filter_response_has_image(response))
					input->image = rs_filter_response_get_image(response);
				g_object_unref(response);
				rs_filter_changed(RS_FILTER(input), RS_FILTER_CHANGED_DIMENSION);
			}
			break;
		case PROP_COLOR_SPACE:
			if (input->colorspace)
				g_object_unref(input->colorspace);
			input->colorspace = g_object_ref(g_value_get_object(value));
			rs_filter_changed(RS_FILTER(input), RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSFilterResponse *response = rs_filter_response_new();
	RSInputFile *input = RS_INPUT_FILE(filter);

	if (RS_IS_COLOR_SPACE(input->colorspace))
		rs_filter_param_set_object(RS_FILTER_PARAM(response), "colorspace", input->colorspace);

	if (input->image)
	{
		rs_filter_response_set_image(response, input->image);
		g_object_unref(input->image);
	}

	return response;
}

static RSFilterResponse *
get_size(RSFilter *filter, const RSFilterRequest *request)
{
	RSFilterResponse *response = rs_filter_response_new();
	RSInputFile *input = RS_INPUT_FILE(filter);

	if (input->image)
	{
		rs_filter_response_set_width(response, input->image->w);
		rs_filter_response_set_height(response, input->image->h);
	}

	return response;
}

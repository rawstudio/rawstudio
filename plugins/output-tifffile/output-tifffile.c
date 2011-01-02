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

#include <rawstudio.h>
#include <tiffio.h>
#include <gettext.h>
#include "config.h"

#define RS_TYPE_TIFFFILE (rs_tifffile_type)
#define RS_TIFFFILE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TIFFFILE, RSTifffile))
#define RS_TIFFFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TIFFFILE, RSTifffileClass))
#define RS_IS_TIFFFILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TIFFFILE))

typedef struct _RSTifffile RSTifffile;
typedef struct _RSTifffileClass RSTifffileClass;

struct _RSTifffile {
	RSOutput parent;

	gchar *filename;
	gboolean uncompressed;
	gboolean save16bit;
	RSColorSpace *color_space;
};

struct _RSTifffileClass {
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT(rs_tifffile, RSTifffile)

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_UNCOMPRESSED,
	PROP_16BIT,
	PROP_COLORSPACE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean execute(RSOutput *output, RSFilter *filter);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_tifffile_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_tifffile_class_init(RSTifffileClass *klass)
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

	g_object_class_install_property(object_class,
		PROP_UNCOMPRESSED, g_param_spec_boolean(
			"uncompressed", "Uncompressed TIFF", _("Save uncompressed TIFF"),
			FALSE, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_16BIT, g_param_spec_boolean(
			"save16bit", "16 bit TIFF", _("Save 16 bit TIFF"),
			FALSE, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_COLORSPACE, g_param_spec_object(
			"colorspace", "Output colorspace", "Color space used for saving",
			RS_TYPE_COLOR_SPACE, G_PARAM_READWRITE)
	);

	output_class->execute = execute;
	output_class->extension = "tif";
	output_class->display_name = _("TIFF (Tagged Image File Format)");
}

static void
rs_tifffile_init(RSTifffile *tifffile)
{
	tifffile->filename = NULL;
	tifffile->uncompressed = FALSE;
	tifffile->save16bit = FALSE;
	tifffile->color_space = rs_color_space_new_singleton("RSSrgb");
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSTifffile *tifffile = RS_TIFFFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, tifffile->filename);
			break;
		case PROP_UNCOMPRESSED:
			g_value_set_boolean(value, tifffile->uncompressed);
			break;
		case PROP_16BIT:
			g_value_set_boolean(value, tifffile->save16bit);
			break;
		case PROP_COLORSPACE:
			g_value_set_object(value, tifffile->color_space);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSTifffile *tifffile = RS_TIFFFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			tifffile->filename = g_value_dup_string(value);
			break;
		case PROP_UNCOMPRESSED:
			tifffile->uncompressed = g_value_get_boolean(value);
			break;
		case PROP_16BIT:
			tifffile->save16bit = g_value_get_boolean(value);
			break;
		case PROP_COLORSPACE:
			if (tifffile->color_space)
				g_object_unref(tifffile->color_space);
			tifffile->color_space = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void rs_tiff_generic_init(TIFF *output, guint w, guint h, const guint samples_per_pixel, const RSIccProfile *profile, gboolean uncompressed);

static void
rs_tiff_generic_init(TIFF *output, guint w, guint h, const guint samples_per_pixel, const RSIccProfile *profile, gboolean uncompressed)
{
	TIFFSetField(output, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(output, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(output, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
	TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	if (uncompressed)
		TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	else
	{
		TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
		TIFFSetField(output, TIFFTAG_ZIPQUALITY, 9);
	}

	if (profile)
	{
		gchar *buffer = NULL;
		gsize length = 0;

		if (rs_icc_profile_get_data(profile, &buffer, &length))
		{
			TIFFSetField(output, TIFFTAG_ICCPROFILE, length, buffer);
			g_free(buffer);
		}

	}
	TIFFSetField(output, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(output, 0));
}

static gboolean
execute(RSOutput *output, RSFilter *filter)
{
	RSFilterResponse *response;
	RSTifffile *tifffile = RS_TIFFFILE(output);
	const RSIccProfile *profile = NULL;
	TIFF *tiff;
	gint row;

	if((tiff = TIFFOpen(tifffile->filename, "w")) == NULL)
		return(FALSE);

	if (tifffile->color_space)
		profile = rs_color_space_get_icc_profile(tifffile->color_space, tifffile->save16bit);

	RSFilterRequest *request = rs_filter_request_new();
	rs_filter_request_set_quick(request, FALSE);
	rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", tifffile->color_space);

	if (tifffile->save16bit)
	{
		gint col;
		response = rs_filter_get_image(filter, request);
		RS_IMAGE16 *image = rs_filter_response_get_image(response);
		rs_tiff_generic_init(tiff, image->w, image->h, 3, profile, tifffile->uncompressed);
		gushort *line = g_new(gushort, image->w*3);

		g_assert(image->channels == 3);
		g_assert(image->pixelsize == 4);

		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 16);
		printf("pixelsize: %d\n", image->pixelsize);
		for(row=0;row<image->h;row++)
		{
			gushort *buf = GET_PIXEL(image, 0, row);
			for(col=0;col<image->w; col++)
			{
				line[col*3 + R] = buf[col*4 + R];
				line[col*3 + G] = buf[col*4 + G];
				line[col*3 + B] = buf[col*4 + B];
			}
			TIFFWriteScanline(tiff, line, row, 0);
		}

		g_object_unref(image);
		g_object_unref(response);
		g_free(line);
	}
	else
	{
		gint col;
		response = rs_filter_get_image8(filter, request);
		GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);
		gint width = gdk_pixbuf_get_width(pixbuf);
		gint height = gdk_pixbuf_get_height(pixbuf);
		gint input_channels = gdk_pixbuf_get_n_channels(pixbuf);
		rs_tiff_generic_init(tiff, width, height, 3, profile, tifffile->uncompressed);
		gchar *line = g_new(gchar, width * 3);

		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
		for(row=0;row<height;row++)
		{
			guchar *buf = GET_PIXBUF_PIXEL(pixbuf, 0, row);
			for(col=0; col<width; col++)
			{
				line[col*3 + R] = buf[col*input_channels + R];
				line[col*3 + G] = buf[col*input_channels + G];
				line[col*3 + B] = buf[col*input_channels + B];
			}
			TIFFWriteScanline(tiff, line, row, 0);
		}

		g_free(line);
		g_object_unref(pixbuf);
		g_object_unref(response);
	}
	g_object_unref(request);

	TIFFClose(tiff);

	gchar *input_filename = NULL;
	rs_filter_get_recursive(filter, "filename", &input_filename, NULL);

	rs_exif_copy(input_filename, tifffile->filename,  G_OBJECT_TYPE_NAME(tifffile->color_space), RS_EXIF_FILE_TYPE_TIFF);
	g_free(input_filename);

	return(TRUE);
}

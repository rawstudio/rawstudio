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
};

struct _RSTifffileClass {
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT(rs_tifffile, RSTifffile)

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_UNCOMPRESSED,
	PROP_16BIT
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
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void rs_tiff_generic_init(TIFF *output, guint w, guint h, const guint samples_per_pixel, const gchar *profile_filename, gboolean uncompressed);

static void
rs_tiff_generic_init(TIFF *output, guint w, guint h, const guint samples_per_pixel, const gchar *profile_filename, gboolean uncompressed)
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
	if (profile_filename)
	{
		gchar *buffer = NULL;
		gsize length = 0;

		if (g_file_get_contents(profile_filename, &buffer, &length, NULL))
			TIFFSetField(output, TIFFTAG_ICCPROFILE, length, buffer);

		g_free(buffer);
	}
	TIFFSetField(output, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(output, 0));
}

static gboolean
execute(RSOutput *output, RSFilter *filter)
{
	RSFilterResponse *response;
	RSTifffile *tifffile = RS_TIFFFILE(output);
	const gchar *profile_filename = NULL;
	TIFF *tiff;
	gint row;

	if((tiff = TIFFOpen(tifffile->filename, "w")) == NULL)
		return(FALSE);

	rs_tiff_generic_init(tiff, rs_filter_get_width(filter), rs_filter_get_height(filter), 3, profile_filename, tifffile->uncompressed);

	if (tifffile->save16bit)
	{
		gint width = rs_filter_get_width(filter);
		gint col;
		response = rs_filter_get_image(filter, NULL);
		RS_IMAGE16 *image = rs_filter_response_get_image(response);
		gushort *line = g_new(gushort, width*3);

		g_assert(image->channels == 3);
		g_assert(image->pixelsize == 4);

		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 16);
		printf("pixelsize: %d\n", image->pixelsize);
		for(row=0;row<image->h;row++)
		{
			gushort *buf = GET_PIXEL(image, 0, row);
			for(col=0;col<width; col++)
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
		response = rs_filter_get_image8(filter, NULL);
		GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);

		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
		for(row=0;row<gdk_pixbuf_get_height(pixbuf);row++)
		{
			guchar *buf = GET_PIXBUF_PIXEL(pixbuf, 0, row);
			TIFFWriteScanline(tiff, buf, row, 0);
		}

		g_object_unref(pixbuf);
		g_object_unref(response);
	}

	TIFFClose(tiff);

	gchar *input_filename = NULL;
	rs_filter_get_recursive(filter, "filename", &input_filename, NULL);
	rs_exif_copy(input_filename, tifffile->filename);
	g_free(input_filename);

	return(TRUE);
}
#if 0
/* FIXME: Fix tiff 16 bit .. NOW! */
static gboolean
rs_tiff16_save(RS_IMAGE16 *image, const gchar *filename, const gchar *profile_filename, gboolean uncompressed)
{
	TIFF *output;
	gint row;

	if ((!g_file_test(filename, G_FILE_TEST_EXISTS)) && (output = TIFFOpen(filename, "w")))
	{
		rs_tiff_generic_init(output, image->w, image->h, image->channels, profile_filename, uncompressed);
		TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, 16);
		for(row=0;row<image->h;row++)
		{
			gushort *buf = GET_PIXEL(image, 0, row);
			TIFFWriteScanline(output, buf, row, 0);
		}
		TIFFClose(output);
	}
	else
		return FALSE;

	return TRUE;
}
#endif

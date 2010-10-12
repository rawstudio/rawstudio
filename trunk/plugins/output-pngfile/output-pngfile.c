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
#include <png.h>

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
	RSColorSpace *color_space;
	gboolean save16bit;
};

struct _RSPngfileClass {
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT(rs_pngfile, RSPngfile)

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_16BIT,
	PROP_COLORSPACE
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
	g_object_class_install_property(object_class,
		PROP_COLORSPACE, g_param_spec_object(
			"colorspace", "Output colorspace", "Color space used for saving",
			RS_TYPE_COLOR_SPACE, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_16BIT, g_param_spec_boolean(
			"save16bit", "16 bit PNG", _("Save 16 bit linear PNG"),
			FALSE, G_PARAM_READWRITE)
	);

	output_class->execute = execute;
	output_class->extension = "png";
	output_class->display_name = _("PNG (Portable Network Graphics)");
}

static void
rs_pngfile_init(RSPngfile *pngfile)
{
	pngfile->filename = NULL;
	pngfile->color_space = rs_color_space_new_singleton("RSSrgb");
	pngfile->save16bit = FALSE;
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
		case PROP_COLORSPACE:
			g_value_set_object(value, pngfile->color_space);
			break;
		case PROP_16BIT:
			g_value_set_boolean(value, pngfile->save16bit);
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
		case PROP_COLORSPACE:
			if (pngfile->color_space)
				g_object_unref(pngfile->color_space);
			pngfile->color_space = g_value_get_object(value);
			break;
		case PROP_16BIT:
			pngfile->save16bit = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gboolean
execute(RSOutput *output, RSFilter *filter)
{
	RSPngfile *pngfile = RS_PNGFILE(output);
	png_bytep *row_pointers;
	FILE *fp = fopen(pngfile->filename, "wb");
	if (!fp)
	  return FALSE;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);

	if (!png_ptr)
		return FALSE;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
       return FALSE;
    }

	png_init_io(png_ptr, fp);
	/* set the zlib compression level */
	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);


	if (pngfile->color_space == rs_color_space_new_singleton("RSSrgb") && !pngfile->save16bit)
	{
		png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);
	}
	else
	{
		gchar *data;
		gsize data_length;
		const RSIccProfile *profile = rs_color_space_get_icc_profile(pngfile->color_space, pngfile->save16bit);
		rs_icc_profile_get_data(profile, &data, &data_length);

		// FIXME: Insert correct profile name
		png_set_iCCP(png_ptr, info_ptr, "Profile name", PNG_COMPRESSION_TYPE_BASE, data, data_length);
		if (pngfile->save16bit)
			png_set_gAMA(png_ptr, info_ptr, 1.0);
	}

	RSFilterResponse *response;
	RSFilterRequest *request = rs_filter_request_new();
	rs_filter_request_set_quick(RS_FILTER_REQUEST(request), FALSE);
	rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", pngfile->color_space);

	if (pngfile->save16bit)
	{
		response = rs_filter_get_image(filter, request);
		RS_IMAGE16 *image = rs_filter_response_get_image(response);

		gint n_channels = image->pixelsize;
		gint width = image->w;
		gint height = image->h;

		png_set_IHDR(png_ptr, info_ptr, width, height,
					16, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
					PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		png_write_info(png_ptr, info_ptr);

		row_pointers = g_malloc(sizeof(png_bytep*)*height);
		gint i;
		for( i = 0; i < height; i++)
			row_pointers[i] = (png_bytep)GET_PIXEL(image, 0, i);

		if (n_channels == 4)
			png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
#ifdef G_BIG_ENDIAN
		png_set_swap(png_ptr);
#endif
		png_write_image(png_ptr, row_pointers);
		g_object_unref(image);
	}
	else  // 8 bit
	{
		response = rs_filter_get_image8(filter, request);
		GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);

		gint n_channels = gdk_pixbuf_get_n_channels (pixbuf);
		gint width = gdk_pixbuf_get_width (pixbuf);
		gint height = gdk_pixbuf_get_height (pixbuf);
		gint rowstride = gdk_pixbuf_get_rowstride (pixbuf);
		guchar* pixels = gdk_pixbuf_get_pixels (pixbuf);

		png_set_IHDR(png_ptr, info_ptr, width, height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		png_write_info(png_ptr, info_ptr);

		row_pointers = g_malloc(sizeof(png_bytep*)*height);
		gint i;
		for( i = 0; i < height; i++)
			row_pointers[i] = (png_bytep)(pixels + i * rowstride);

		if (n_channels == 4)
			png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
		
		png_write_image(png_ptr, row_pointers);
		g_object_unref(pixbuf);
	}

	png_write_end(png_ptr, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	g_object_unref(request);
	g_object_unref(response);
	g_free(row_pointers);

	gchar *input_filename = NULL;
	rs_filter_get_recursive(filter, "filename", &input_filename, NULL);
	rs_exif_copy(input_filename, pngfile->filename, G_OBJECT_TYPE_NAME(pngfile->color_space));
	g_free(input_filename);

	return TRUE;
}

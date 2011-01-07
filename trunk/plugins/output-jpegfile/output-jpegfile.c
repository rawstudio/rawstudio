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
#ifdef WIN32
#define HAVE_BOOLEAN
#define _BASETSD_H_
#endif
#include <jpeglib.h>
#include <gettext.h>
#include "config.h"

/* stat() */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* open() */
#include <fcntl.h>

#define RS_TYPE_JPEGFILE (rs_jpegfile_type)
#define RS_JPEGFILE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_JPEGFILE, RSJpegfile))
#define RS_JPEGFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_JPEGFILE, RSJpegfileClass))
#define RS_IS_JPEGFILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_JPEGFILE))

typedef struct _RSJpegfile RSJpegfile;
typedef struct _RSJpegfileClass RSJpegfileClass;

struct _RSJpegfile {
	RSOutput parent;

	gchar *filename;
	gint quality;
	RSColorSpace *color_space;
};

struct _RSJpegfileClass {
	RSOutputClass parent_class;
};

RS_DEFINE_OUTPUT(rs_jpegfile, RSJpegfile)

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_QUALITY,
	PROP_COLORSPACE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean execute(RSOutput *output, RSFilter *filter);

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_jpegfile_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_jpegfile_class_init(RSJpegfileClass *klass)
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
		PROP_QUALITY, g_param_spec_int(
			"quality", "JPEG Quality", _("JPEG Quality"),
			10, 100, 90, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_COLORSPACE, g_param_spec_object(
			"colorspace", "Output colorspace", "Color space used for saving",
			RS_TYPE_COLOR_SPACE, G_PARAM_READWRITE)
	);

	output_class->execute = execute;
	output_class->extension = "jpg";
	output_class->display_name = _("JPEG (Joint Photographic Experts Group)");
}

static void
rs_jpegfile_init(RSJpegfile *jpegfile)
{
	jpegfile->filename = NULL;
	jpegfile->quality = 90;
	jpegfile->color_space = rs_color_space_new_singleton("RSSrgb");
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSJpegfile *jpegfile = RS_JPEGFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			g_value_set_string(value, jpegfile->filename);
			break;
		case PROP_QUALITY:
			g_value_set_int(value, jpegfile->quality);
			break;
		case PROP_COLORSPACE:
			g_value_set_object(value, jpegfile->color_space);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSJpegfile *jpegfile = RS_JPEGFILE(object);

	switch (property_id)
	{
		case PROP_FILENAME:
			jpegfile->filename = g_value_dup_string(value);
			break;
		case PROP_QUALITY:
			jpegfile->quality = g_value_get_int(value);
			break;
		case PROP_COLORSPACE:
			if (jpegfile->color_space)
				g_object_unref(jpegfile->color_space);
			jpegfile->color_space = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

/* This following function is an almost verbatim copy from little cms. Thanks Marti, you rock! */

#define ICC_MARKER  (JPEG_APP0 + 2) /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN  14        /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER  65533  /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER  (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)
#define ICC_MARKER_IDENT "ICC_PROFILE"

static void rs_jpeg_write_icc_profile(j_compress_ptr cinfo, const JOCTET *icc_data_ptr, guint icc_data_len);

static void
rs_jpeg_write_icc_profile(j_compress_ptr cinfo, const JOCTET *icc_data_ptr, guint icc_data_len)
{
	gchar *ident = ICC_MARKER_IDENT;
	guint num_markers; /* total number of markers we'll write */
	gint cur_marker = 1;       /* per spec, counting starts at 1 */
	guint length;      /* number of bytes to write in this marker */

	num_markers = icc_data_len / MAX_DATA_BYTES_IN_MARKER;
	if (num_markers * MAX_DATA_BYTES_IN_MARKER != icc_data_len)
		num_markers++;
	while (icc_data_len > 0)
	{
		length = icc_data_len;
		if (length > MAX_DATA_BYTES_IN_MARKER)
			length = MAX_DATA_BYTES_IN_MARKER;
		icc_data_len -= length;
		jpeg_write_m_header(cinfo, ICC_MARKER, (guint) (length + ICC_OVERHEAD_LEN));

		do {
			jpeg_write_m_byte(cinfo, *ident);
		} while(*ident++);
		jpeg_write_m_byte(cinfo, cur_marker);
		jpeg_write_m_byte(cinfo, (gint) num_markers);

		while (length--)
		{
			jpeg_write_m_byte(cinfo, *icc_data_ptr);
			icc_data_ptr++;
		}
		cur_marker++;
	}
	return;
}

static gboolean
execute(RSOutput *output, RSFilter *filter)
{
	RSJpegfile *jpegfile = RS_JPEGFILE(output);
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE * outfile;
	JSAMPROW row_pointer[1];
	gint x,y;
	
	
	RSFilterRequest *request = rs_filter_request_new();
	rs_filter_request_set_quick(RS_FILTER_REQUEST(request), FALSE);
	rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", jpegfile->color_space);
	RSFilterResponse *response = rs_filter_get_image8(filter, request);
	
	g_object_unref(request);
	GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);
	g_object_unref(response);

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	if ((outfile = fopen(jpegfile->filename, "wb")) == NULL)
		return(FALSE);
	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = gdk_pixbuf_get_width(pixbuf);
	cinfo.image_height = gdk_pixbuf_get_height(pixbuf);
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, jpegfile->quality, TRUE);
	rs_io_lock();
	jpeg_start_compress(&cinfo, TRUE);
	if (jpegfile->color_space && !g_str_equal(G_OBJECT_TYPE_NAME(jpegfile->color_space), "RSSrgb"))
	{
		const RSIccProfile *profile = rs_color_space_get_icc_profile(jpegfile->color_space, FALSE);
		if (profile)
		{
			gchar *data;
			gsize data_length;
			rs_icc_profile_get_data(profile, &data, &data_length);
			rs_jpeg_write_icc_profile(&cinfo, (guchar *) data, data_length);
			g_free(data);
		}
	}

	if (gdk_pixbuf_get_n_channels(pixbuf) == 4)
	{
		GdkPixbuf *out = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, cinfo.image_width, cinfo.image_height);
		for( y = 0; y < cinfo.image_height; y++){
			gint* in = (gint*)GET_PIXBUF_PIXEL(pixbuf, 0, y);
			guchar* o = GET_PIXBUF_PIXEL(out, 0, y);
			for( x = 0; x < cinfo.image_width ; x++) {
				guint i = *in++;
				o[0] = i&0xff;
				o[1] = (i>>8)&0xff;
				o[2] = (i>>16)&0xff;
				o+=3;
			}
		}
		g_object_unref(pixbuf);
		pixbuf = out;
	}

	while (cinfo.next_scanline < cinfo.image_height)
	{
		row_pointer[0] = GET_PIXBUF_PIXEL(pixbuf, 0, cinfo.next_scanline);
		if (jpeg_write_scanlines(&cinfo, row_pointer, 1) != 1)
			break;
	}
	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
	g_object_unref(pixbuf);

	gchar *input_filename = NULL;
	rs_filter_get_recursive(filter, "filename", &input_filename, NULL);
	rs_exif_copy(input_filename, jpegfile->filename, G_OBJECT_TYPE_NAME(jpegfile->color_space), RS_EXIF_FILE_TYPE_JPEG);
	rs_io_unlock();
	g_free(input_filename);

	return(TRUE);
}

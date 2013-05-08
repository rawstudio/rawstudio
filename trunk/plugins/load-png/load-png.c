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
#include <math.h> /* pow() */
#include <png.h>
#include <setjmp.h>
#include "exiv2-colorspace.h"
#include <config.h>
#if defined(HAVE_LCMS2)
#include <lcms2.h>
#elif defined(HAVE_LCMS)
#include <lcms.h>
#else
#error "LCMS v1 or LCMS v2 required"
#endif

/**
 * Open an image using libpng
 * @param filename The filename to open
 * @return The newly created RS_IMAGE16 or NULL on error
 */
static RSFilterResponse*
load_png(const gchar *filename)
{
  gfloat gamma_guess = 2.2f;
  RSColorSpace *input_space = exiv2_get_colorspace(filename, &gamma_guess);

  int width, height, rowbytes;
  png_byte color_type;
  png_byte bit_depth;

  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep * row_pointers;

  unsigned char header[8];    // 8 is the maximum size that can be checked

  /* open file and test for it being a png */
  FILE *fp = fopen(filename, "rb");
  if (!fp)
    return NULL;

  if (!fread(header, 1, 8, fp))
    return NULL;

  if (png_sig_cmp(header, 0, 8))
    return NULL;

  /* initialize stuff */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
    return NULL;

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    return NULL;

  if (setjmp(png_jmpbuf(png_ptr)))
    return NULL;

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);

  png_read_info(png_ptr, info_ptr);

  width = png_get_image_width(png_ptr, info_ptr);
  height = png_get_image_height(png_ptr, info_ptr);
  color_type = png_get_color_type(png_ptr, info_ptr);
  bit_depth = png_get_bit_depth(png_ptr, info_ptr);

#ifdef DEBUG
  printf("width: %u\n", (guint32) width);
  printf("height: %u\n", (guint32) height);
  printf("bit_depth: %d\n", bit_depth);
  printf("color_type: %d\n", color_type);
#endif

  /* currently we only support 16BIT RGBA */
  if (color_type != PNG_COLOR_TYPE_RGB_ALPHA || bit_depth != 16)
    return NULL;

  png_read_update_info(png_ptr, info_ptr);

  /* read file */
  if (setjmp(png_jmpbuf(png_ptr)))
    return NULL;

  row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);

  if (bit_depth == 16)
    rowbytes = width*8;
  else
    rowbytes = width*4;

  gint y,x;
  gint dest = 0;

  for (y=0; y<height; y++)
    row_pointers[y] = (png_byte*) malloc(rowbytes);

  png_read_image(png_ptr, row_pointers);

  RS_IMAGE16 *image = rs_image16_new(width, height, 3, 4);

  for (y=0; y<height; y++) {
    png_byte* row = row_pointers[y];
    for (x=0; x<width; x++) {
      png_byte* ptr = &(row[x*8]);
      image->pixels[dest++] = CLAMP((ptr[0]<<8)|ptr[1], 0, 65535);
      image->pixels[dest++] = CLAMP((ptr[2]<<8)|ptr[3], 0, 65535);
      image->pixels[dest++] = CLAMP((ptr[4]<<8)|ptr[5], 0, 65335);
      dest++;
    }
  }

  RSFilterResponse* response = rs_filter_response_new();
  if (image)
    {
      rs_filter_response_set_image(response, image);
      rs_filter_response_set_width(response, image->w);
      rs_filter_response_set_height(response, image->h);
      g_object_unref(image);
      rs_filter_param_set_object(RS_FILTER_PARAM(response), "embedded-colorspace", input_space);
      rs_filter_param_set_boolean(RS_FILTER_PARAM(response), "is-premultiplied", TRUE);
    }
  return response;

  return NULL;
}

/* We don't load actual metadata, but we will load thumbnail and return FALSE to pass these on */
static gboolean
rs_png_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	meta->thumbnail = gdk_pixbuf_new_from_file_at_size(service, 128, 128, NULL);
	return FALSE;
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	/* Our own extension for RS Enfused PNGs - metadata is handled by exiv2, but thumbnail is handled as 8bit */
	rs_filetype_register_loader(".rse", "JPEG", load_png, 5, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".rse", "PNG", rs_png_load_meta, 5, RS_LOADER_FLAGS_8BIT);

	rs_filetype_register_loader(".png", "JPEG", load_png, 5, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_meta_loader(".png", "PNG", rs_png_load_meta, 5, RS_LOADER_FLAGS_8BIT);
}

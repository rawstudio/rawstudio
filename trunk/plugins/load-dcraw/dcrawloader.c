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
#include <math.h>
#include "dcraw_api.h"

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2
 */
#define FC(row,col) \
	(int)(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

static int
fc_INDI (const unsigned int filters, const int row, const int col)
{
  static const char filter[16][16] =
  { { 2,1,1,3,2,3,2,0,3,2,3,0,1,2,1,0 },
    { 0,3,0,2,0,1,3,1,0,1,1,2,0,3,3,2 },
    { 2,3,3,2,3,1,1,3,3,1,2,1,2,0,0,3 },
    { 0,1,0,1,0,2,0,2,2,0,3,0,1,3,2,1 },
    { 3,1,1,2,0,1,0,2,1,3,1,3,0,1,3,0 },
    { 2,0,0,3,3,2,3,1,2,0,2,0,3,2,2,1 },
    { 2,3,3,1,2,1,2,1,2,1,1,2,3,0,0,1 },
    { 1,0,0,2,3,0,0,3,0,3,0,3,2,1,2,3 },
    { 2,3,3,1,1,2,1,0,3,2,3,0,2,3,1,3 },
    { 1,0,2,0,3,0,3,2,0,1,1,2,0,1,0,2 },
    { 0,1,1,3,3,2,2,1,1,3,3,0,2,1,3,2 },
    { 2,3,2,0,0,1,3,0,2,0,1,2,3,0,1,0 },
    { 1,3,1,2,3,2,3,2,0,2,0,1,1,0,3,0 },
    { 0,2,0,3,1,0,0,1,1,3,3,2,3,2,2,1 },
    { 2,1,3,2,3,1,2,1,0,3,0,2,0,2,0,2 },
    { 0,3,1,0,0,2,0,3,2,1,3,1,1,3,1,3 } };

  if (filters != 1) return FC(row,col);
  /* Assume that we are handling the Leaf CatchLight with
   * top_margin = 8; left_margin = 18; */
//  return filter[(row+top_margin) & 15][(col+left_margin) & 15];
  return filter[(row+8) & 15][(col+18) & 15];
}

static RS_IMAGE16 *
convert(dcraw_data *raw)
{
	RS_IMAGE16 *image = NULL;
	gint row, col;
	gushort *output;
    gint shift;
	gint temp;

	g_assert(raw != NULL);

	shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	/* Allocate a 1-channel RS_IMAGE16 */
	if (raw->filters != 0)
	{
		image = rs_image16_new(raw->raw.width*2, raw->raw.height*2, 1, 1);

		g_assert(raw->filters != 0);
		g_assert(raw->fourColorFilters != 0);
		g_assert(image->pixelsize == 1);

		image->filters = raw->filters;

		for(row=0 ; row < image->h ; row++)
		{
			output = GET_PIXEL(image, 0, row);
			for(col=0 ; col < image->w ; col++)
			{
				/* Extract the correct color from the raw image */
				temp = raw->raw.image[(row>>1) * raw->raw.width + (col>>1)][fc_INDI(raw->fourColorFilters, row, col)];

				/* Subtract black as calculated by dcraw */
				temp -= raw->black;

				/* Clamp */
				temp = MAX(0, temp);

				/* Shift our data to fit 16 bits */
				*output = temp<<shift;

				/* Advance output by one pixel */
				output++;
			}
		}
	}
	else if (raw->raw.colors == 3)
	{
		/* For foveon sensors, no demosaic is needed */
		gint i;
		gint max = 0;
		gint rawsize = raw->raw.width * raw->raw.height * 3;
		dcraw_image_type *input;

		g_assert(raw->black == 0); /* raw->black is always zero for foveon - I think :) */

		image = rs_image16_new(raw->raw.width, raw->raw.height, 3, 4);

		/* dcraw calculates 'wrong' rgbMax for Sigma's, let's calculate our own */
		for(i=0;i<rawsize;i++)
			max = MAX(((gushort *)raw->raw.image)[i], max);
		
		shift = (gint) (16.0-log((gdouble) max)/log(2.0));

		for(row=0 ; row < image->h ; row++)
		{
			output = GET_PIXEL(image, 0, row);
			input = raw->raw.image+row*raw->raw.width;
			for(col=0 ; col < image->w ; col++)
			{
				/* Copy and shift our data to fill 16 bits */
				output[R] = (*input)[R] << shift;
				output[G] = (*input)[G] << shift;
				output[B] = (*input)[B] << shift;

				/* Advance input by one dcraw_image_type */
				input++;

				/* Advance output by one pixel */
				output += image->pixelsize;
			}
		}
	}
	else if (raw->raw.colors == 1)
	{
		dcraw_image_type *input;

		image = rs_image16_new(raw->raw.width, raw->raw.height, 3, 4);

		for(row=0 ; row < image->h ; row++)
		{
			output = GET_PIXEL(image, 0, row);
			input = raw->raw.image+row*raw->raw.width;
			for(col=0 ; col < image->w ; col++)
			{
				/* Copy and shift our data to fill 16 bits */
				output[R] = *(*input) << shift;
				output[G] = *(*input) << shift;
				output[B] = *(*input) << shift;

				/* Advance input by one dcraw_image_type */
				input++;

				/* Advance output by one pixel */
				output += image->pixelsize;
			}
		}
	}

	return image;
}

static RSFilterResponse *
open_dcraw(const gchar *filename)
{
	dcraw_data *raw = g_new0(dcraw_data, 1);
	RS_IMAGE16 *image = NULL;

	RSFilterResponse* response = rs_filter_response_new();

	rs_io_lock();
	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);
		rs_io_unlock();
		rs_filter_param_set_integer(RS_FILTER_PARAM(response), "fuji-width", raw->fuji_width);
		image = convert(raw);
		dcraw_close(raw);
	}
	else
		rs_io_unlock();
	g_free(raw);

	rs_filter_response_set_image(response, image);
	rs_filter_response_set_width(response, image->w);
	rs_filter_response_set_height(response, image->h);
	g_object_unref(image);
	return response;
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_loader(".cr2", "Canon CR2", open_dcraw,  10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".crw", "Canon CIFF", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".nef", "Nikon NEF", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".nrw", "Nikon NEF 2", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".mrw", "Minolta raw", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".tif", "Canon TIFF", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".rwl", "Leica", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".arw", "Sony", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".sr2", "Sony", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".srf", "Sony", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".kdc", "Kodak", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".dcr", "Kodak", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".x3f", "Sigma", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".orf", "Olympus", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".raw", "Panasonic raw", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".rw2", "Panasonic raw v.2", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".pef", "Pentax raw", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".dng", "Adobe Digital negative", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".mef", "Mamiya", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".3fr", "Hasselblad", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".erf", "Epson", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_loader(".raf", "Fujifilm", open_dcraw, 10, RS_LOADER_FLAGS_RAW);
}

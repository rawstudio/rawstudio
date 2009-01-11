/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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

/* FIXME: Optimize these for MMX/SSE */

static void
open_dcraw_apply_black_and_shift(dcraw_data *raw, RS_IMAGE16 *image)
{
	gushort *dst1, *dst2, *src;
	gint row, col;
	gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

	for(row=0;row<(raw->raw.height*2);row+=2)
	{
		src = (gushort *) raw->raw.image + row/2 * raw->raw.width * 4;
		dst1 = GET_PIXEL(image, 0, row);
		dst2 = GET_PIXEL(image, 0, row+1);
		col = raw->raw.width;
		while(col--)
		{
			register gint r, g, b, g2;
			r  = *src++ - raw->black;
			g  = *src++ - raw->black;
			b  = *src++ - raw->black;
			g2 = *src++ - raw->black;
			r  = MAX(0, r);
			g  = MAX(0, g);
			b  = MAX(0, b);
			g2 = MAX(0, g2);
			*dst1++ = (gushort)( r<<shift);
			*dst1++ = (gushort)( g<<shift);
			*dst1++ = (gushort)( b<<shift);
			*dst1++ = (gushort)(g2<<shift);
			*dst1++ = (gushort)( r<<shift);
			*dst1++ = (gushort)( g<<shift);
			*dst1++ = (gushort)( b<<shift);
			*dst1++ = (gushort)(g2<<shift);
			*dst2++ = (gushort)( r<<shift);
			*dst2++ = (gushort)( g<<shift);
			*dst2++ = (gushort)( b<<shift);
			*dst2++ = (gushort)(g2<<shift);
			*dst2++ = (gushort)( r<<shift);
			*dst2++ = (gushort)( g<<shift);
			*dst2++ = (gushort)( b<<shift);
			*dst2++ = (gushort)(g2<<shift);
		}
	}
}

static void
open_dcraw_apply_black_and_shift_half_size(dcraw_data *raw, RS_IMAGE16 *image)
{
    gushort *dst, *src;
    gint row, col;
    gint64 shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);

    for(row=0;row<(raw->raw.height);row++)
    {
        src = (gushort *) raw->raw.image + row * raw->raw.width * 4;
        dst = GET_PIXEL(image, 0, row);
        col = raw->raw.width;
        while(col--)
        {
            register gint r, g, b, g2;
            r  = *src++ - raw->black;
            g  = *src++ - raw->black;
            b  = *src++ - raw->black;
            g2 = *src++ - raw->black;
            r  = MAX(0, r);
            g  = MAX(0, g);
            b  = MAX(0, b);
            g2 = MAX(0, g2);
            *dst++ = (gushort)( r<<shift);
            *dst++ = (gushort)( g<<shift);
            *dst++ = (gushort)( b<<shift);
            *dst++ = (gushort)(g2<<shift);
        }
    }
}

static RS_IMAGE16 *
open_dcraw(const gchar *filename, const gboolean half_size)
{
	dcraw_data *raw = g_new0(dcraw_data, 1);
	RS_IMAGE16 *image = NULL;

	if (!dcraw_open(raw, (char *) filename))
	{
		dcraw_load_raw(raw);

		if (half_size)
		{
			image = rs_image16_new(raw->raw.width, raw->raw.height, raw->raw.colors, 4);
			open_dcraw_apply_black_and_shift_half_size(raw, image);
		}
		else
		{
			image = rs_image16_new(raw->raw.width*2, raw->raw.height*2, raw->raw.colors, 4);
			open_dcraw_apply_black_and_shift(raw, image);
		}

		image->filters = raw->filters;
		image->fourColorFilters = raw->fourColorFilters;
		dcraw_close(raw);
	}
	g_free(raw);

	return image;
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_loader(".cr2", "Canon CR2", open_dcraw,  10);
	rs_filetype_register_loader(".crw", "Canon CIFF", open_dcraw, 10);
	rs_filetype_register_loader(".nef", "Nikon NEF", open_dcraw, 10);
	rs_filetype_register_loader(".mrw", "Minolta raw", open_dcraw, 10);
	rs_filetype_register_loader(".tif", "Canon TIFF", open_dcraw, 10);
	rs_filetype_register_loader(".arw", "Sony", open_dcraw, 10);
	rs_filetype_register_loader(".sr2", "Sony", open_dcraw, 10);
	rs_filetype_register_loader(".srf", "Sony", open_dcraw, 10);
	rs_filetype_register_loader(".kdc", "Kodak", open_dcraw, 10);
	rs_filetype_register_loader(".dcr", "Kodak", open_dcraw, 10);
	rs_filetype_register_loader(".x3f", "Sigma", open_dcraw, 10);
	rs_filetype_register_loader(".orf", "Olympus", open_dcraw, 10);
	rs_filetype_register_loader(".raw", "Panasonic raw", open_dcraw, 10);
	rs_filetype_register_loader(".rw2", "Panasonic raw v.2", open_dcraw, 10);
	rs_filetype_register_loader(".pef", "Pentax raw", open_dcraw, 10);
	rs_filetype_register_loader(".dng", "Adobe Digital negative", open_dcraw, 10);
	rs_filetype_register_loader(".mef", "Mamiya", open_dcraw, 10);
	rs_filetype_register_loader(".3fr", "Hasselblad", open_dcraw, 10);
	rs_filetype_register_loader(".erf", "Epson", open_dcraw, 10);
}

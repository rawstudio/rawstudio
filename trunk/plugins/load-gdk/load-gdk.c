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
#include "exiv2-colorspace.h"
#include <lcms.h>


/**
 * Open an image using the GDK-engine
 * @param filename The filename to open
 * @return The newly created RS_IMAGE16 or NULL on error
 */
static RSFilterResponse*
load_gdk(const gchar *filename)
{
	gushort gammatable[256];
	RS_IMAGE16 *image = NULL;
	GdkPixbuf *pixbuf;
	guchar *pixels;
	gint rowstride;
	gint width, height;
	gint row,col, src, dest;
	gint alpha=0;
	gint n;
	gdouble nd, res;
	gfloat gamma_guess = 2.2f;

	RSColorSpace *input_space = exiv2_get_colorspace(filename, &gamma_guess);

	if (G_OBJECT_TYPE(input_space) == RS_TYPE_COLOR_SPACE_ICC)
	{
		gchar *data;
		gsize length;
		RSIccProfile *profile = RS_COLOR_SPACE_ICC(input_space)->icc_profile;
		
		if (rs_icc_profile_get_data(profile, &data, &length))
		{
			cmsHPROFILE *lcms_target = cmsOpenProfileFromMem(data, length);
			if (lcms_target)
			{
				LPGAMMATABLE curve = NULL;
				if (cmsIsTag(lcms_target, icSigGrayTRCTag))
					curve = cmsReadICCGamma(lcms_target, icSigGrayTRCTag);

				if (NULL== curve && cmsIsTag(lcms_target, icSigRedTRCTag))
					curve = cmsReadICCGamma(lcms_target, icSigRedTRCTag);
				if (curve)
				{
					double gamma = cmsEstimateGamma(curve);
					if (gamma>0.0)
						gamma_guess = gamma;
				}
			}
		}

		/* This may seem very strange, but ICC profiles are basically treated as */
		/* being either gamma 1.0 or gamma 2.2, this is then reversely applied to */
		/* the profile, and therefore the actual gamma of the profile will be */
		/* applied at that stage of the process. */
		if (gamma_guess > 1.1)
			gamma_guess = 2.2;
		else
			gamma_guess = 1.0;
	}

	for(n=0;n<256;n++)
	{
		nd = ((gdouble) n) * (1.0/255.0);
		res = (gint) (pow(nd, gamma_guess) * 65535.0);
		_CLAMP65535(res);
		gammatable[n] = res;
	}

	if ((pixbuf = gdk_pixbuf_new_from_file(filename, NULL)))
	{
		rowstride = gdk_pixbuf_get_rowstride(pixbuf);
		pixels = gdk_pixbuf_get_pixels(pixbuf);
		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);
		if (gdk_pixbuf_get_has_alpha(pixbuf))
			alpha = 1;
		image = rs_image16_new(width, height, 3, 4);
		for(row=0;row<image->h;row++)
		{
			dest = row * image->rowstride;
			src = row * rowstride;
			for(col=0;col<image->w;col++)
			{
				image->pixels[dest++] = gammatable[pixels[src++]];
				image->pixels[dest++] = gammatable[pixels[src++]];
				image->pixels[dest++] = gammatable[pixels[src++]];
				dest++;
				src+=alpha;
			}
		}
		g_object_unref(pixbuf);
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
}

/* We don't load actual metadata, but we will load thumbnail and return FALSE to pass these on */
static gboolean
rs_gdk_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	meta->thumbnail = gdk_pixbuf_new_from_file_at_size(service, 128, 128, NULL);
	return FALSE;
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_loader(".jpg", "JPEG", load_gdk, 10, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_loader(".jpeg", "JPEG", load_gdk, 10, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_loader(".png", "JPEG", load_gdk, 10, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_loader(".tif", "JPEG", load_gdk, 20, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_loader(".tiff", "JPEG", load_gdk, 20, RS_LOADER_FLAGS_8BIT);

	/* Take care of thumbnailing too */
	rs_filetype_register_meta_loader(".jpg", "JPEG", rs_gdk_load_meta, 10, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_meta_loader(".jpeg", "JPEG", rs_gdk_load_meta, 10, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_meta_loader(".png", "PNG", rs_gdk_load_meta, 10, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_meta_loader(".tif", "TIFF", rs_gdk_load_meta, 20, RS_LOADER_FLAGS_8BIT);
	rs_filetype_register_meta_loader(".tiff", "TIFF", rs_gdk_load_meta, 20, RS_LOADER_FLAGS_8BIT);
}

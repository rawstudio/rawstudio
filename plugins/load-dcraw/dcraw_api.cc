/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * dcraw_api.cc - API for DCRaw
 * Copyright 2004-2010 by Udi Fuchs
 *
 * based on dcraw by Dave Coffin
 * http://www.cybercom.net/~dcoffin/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h> /* for sqrt() */
#include <setjmp.h>
#include <errno.h>
#include <float.h>
#include <glib.h>
#include <glib/gi18n.h> /*For _(String) definition - NKBJ*/
#include <sys/types.h>
#include "dcraw_api.h"
#include "dcraw.h"

#ifdef WITH_MMAP_HACK
#include "mmap-hack.h"
#endif
#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)
#define FC(filters,row,col) \
    (filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)
extern "C" {

int dcraw_open(dcraw_data *h, char *filename)
{
    DCRaw *d = new DCRaw;
    int c, i;

#ifndef LOCALTIME
    putenv (const_cast<char *>("TZ=UTC"));
#endif
    g_free(d->messageBuffer);
    d->messageBuffer = NULL;
    d->lastStatus = DCRAW_SUCCESS;
    d->verbose = 1;
    d->ifname = g_strdup(filename);
    d->ifname_display = g_filename_display_name(d->ifname);
    if (setjmp(d->failure)) {
	d->dcraw_message(DCRAW_ERROR,_("Fatal internal error\n"));
	h->message = d->messageBuffer;
	delete d;
	return DCRAW_ERROR;
    }
    if (!(d->ifp = fopen (d->ifname, "rb"))) {
	gchar *err_u8 = g_locale_to_utf8(strerror(errno), -1, NULL, NULL, NULL);
	d->dcraw_message(DCRAW_OPEN_ERROR,_("Cannot open file %s: %s\n"),
		d->ifname_display, err_u8);
	g_free(err_u8);
	h->message = d->messageBuffer;
	delete d;
	return DCRAW_OPEN_ERROR;
    }
	try
	{
    d->identify();
    /* We first check if dcraw recognizes the file, this is equivalent
     * to 'dcraw -i' succeeding */
    if (!d->make[0]) {
	d->dcraw_message(DCRAW_OPEN_ERROR,_("%s: unsupported file format.\n"),
		d->ifname_display);
	fclose(d->ifp);
	h->message = d->messageBuffer;
	delete d;
	return DCRAW_OPEN_ERROR;
    }
    /* Next we check if dcraw can decode the file */
    if (!d->is_raw) {
	d->dcraw_message(DCRAW_OPEN_ERROR,_("Cannot decode file %s\n"),
		d->ifname_display);
	fclose(d->ifp);
	h->message = d->messageBuffer;
	delete d;
	return DCRAW_OPEN_ERROR;
    }
    if (d->load_raw == &DCRaw::kodak_ycbcr_load_raw) {
	d->height += d->height & 1;
	d->width += d->width & 1;
    }
    /* Pass class variables to the handler on two conditions:
     * 1. They are needed at this stage.
     * 2. They where set in identify() and won't change in load_raw() */
    h->dcraw = d;
    h->ifp = d->ifp;
    h->height = d->height;
    h->width = d->width;
    h->fuji_width = d->fuji_width;
    h->fuji_step = sqrt(0.5);
    h->colors = d->colors;
    h->filters = d->filters;
    h->raw_color = d->raw_color;
    memcpy(h->cam_mul, d->cam_mul, sizeof d->cam_mul);
    // maximun and black might change during load_raw. We need them for the
    // camera-wb. If they'll change we will recalculate the camera-wb.
    h->rgbMax = d->maximum;
    i = d->cblack[3];
    FORC3 if ((unsigned)i > d->cblack[c]) i = d->cblack[c];
    FORC4 d->cblack[c] -= i;
    d->black += i;
    h->black = d->black;
    h->shrink = d->shrink = (h->filters!=0);
    h->pixel_aspect = d->pixel_aspect;
    /* copied from dcraw's main() */
    switch ((d->flip+3600) % 360) {
	case 270: d->flip = 5; break;
	case 180: d->flip = 3; break;
	case  90: d->flip = 6;
    }
    h->flip = d->flip;
    h->toneCurveSize = d->tone_curve_size;
    h->toneCurveOffset = d->tone_curve_offset;
    h->toneModeOffset = d->tone_mode_offset;
    h->toneModeSize = d->tone_mode_size;
    g_strlcpy(h->make, d->make, 80);
    g_strlcpy(h->model, d->model, 80);
    h->iso_speed = d->iso_speed;
    h->shutter = d->shutter;
    h->aperture = d->aperture;
    h->focal_len = d->focal_len;
    h->timestamp = d->timestamp;
    h->raw.image = NULL;
    h->thumbType = unknown_thumb_type;
    h->message = d->messageBuffer;
	} catch (...) 
	{
		delete d;
		return DCRAW_OPEN_ERROR;
	}
    return d->lastStatus;
}

void dcraw_image_dimensions(dcraw_data *raw, int flip, int shrink,
	int *height, int *width)
{
    // Effect of dcraw_finilize_shrink()
    *width = raw->width / shrink;
    *height = raw->height / shrink;
    // Effect of fuji_rotate_INDI() */
    if (raw->fuji_width) {
	int fuji_width = raw->fuji_width / shrink - 1;
	*width = fuji_width / raw->fuji_step;
	*height = (*height - fuji_width) / raw->fuji_step;
    }
    // Effect of dcraw_image_stretch()
    if (raw->pixel_aspect < 1)
        *height = *height / raw->pixel_aspect + 0.5;
    if (raw->pixel_aspect > 1)
        *width = *width * raw->pixel_aspect + 0.5;

    // Effect of dcraw_flip_image()
    if (flip & 4) {
	int tmp = *height;
	*height = *width;
	*width = tmp;
    }
}

int dcraw_load_raw(dcraw_data *h)
{
    DCRaw *d = (DCRaw *)h->dcraw;
    int c, i, j;
    double dmin;

    g_free(d->messageBuffer);
    d->messageBuffer = NULL;
    d->lastStatus = DCRAW_SUCCESS;
    if (setjmp(d->failure)) {
	d->dcraw_message(DCRAW_ERROR,_("Fatal internal error\n"));
	h->message = d->messageBuffer;
	delete d;
	return DCRAW_ERROR;
    }
    h->raw.height = d->iheight = (h->height+h->shrink) >> h->shrink;
    h->raw.width = d->iwidth = (h->width+h->shrink) >> h->shrink;
    h->raw.image = d->image = g_new0(dcraw_image_type, d->iheight * d->iwidth
	    + d->meta_length);
    d->meta_data = (char *) (d->image + d->iheight*d->iwidth);
    /* copied from the end of dcraw's identify() */
    if (d->filters && d->colors == 3) {
	d->filters |= ((d->filters >> 2 & 0x22222222) |
		(d->filters << 2 & 0x88888888)) & d->filters << 1;
    }
    h->raw.colors = d->colors;
    h->fourColorFilters = d->filters;
    d->dcraw_message(DCRAW_VERBOSE,_("Loading %s %s image from %s ...\n"),
	    d->make, d->model, d->ifname_display);
    fseek(d->ifp, 0, SEEK_END);
    d->ifpSize = ftell(d->ifp);
    fseek(d->ifp, d->data_offset, SEEK_SET);
    try
    {
    (d->*d->load_raw)();
    if (!--d->data_error) d->lastStatus = DCRAW_ERROR;
    if (d->zero_is_bad) d->remove_zeroes();
    d->bad_pixels(NULL);
    if (d->is_foveon) {
	d->foveon_interpolate();
	h->raw.width = h->width = d->width;
	h->raw.height = h->height = d->height;
    }
    // TODO: Go over the following settings to see if they change during
    // load_raw. If they change, document where. If not, move to dcraw_open().
    h->rgbMax = d->maximum;
    i = d->cblack[3];
    FORC3 if ((unsigned)i > d->cblack[c]) i = d->cblack[c];
    FORC4 d->cblack[c] -= i;
    d->black += i;
    h->black = d->black;
    d->dcraw_message(DCRAW_VERBOSE,_("Black: %d, Maximum: %d\n"),
	    d->black, d->maximum);
    dmin = DBL_MAX;
    for (i=0; i<h->colors; i++) if (dmin > d->pre_mul[i]) dmin = d->pre_mul[i];
    for (i=0; i<h->colors; i++) h->pre_mul[i] = d->pre_mul[i]/dmin;
    if (h->colors==3) h->pre_mul[3] = 0;
    memcpy(h->rgb_cam, d->rgb_cam, sizeof d->rgb_cam);

    double rgb_cam_transpose[4][3];
    for (i=0; i<4; i++) for (j=0; j<3; j++)
	rgb_cam_transpose[i][j] = d->rgb_cam[j][i];
    d->pseudoinverse (rgb_cam_transpose, h->cam_rgb, d->colors);
	} catch (...)
	{
		d->dcraw_message(DCRAW_ERROR,_("Dcraw could not load image.\n"));
		h->message = d->messageBuffer;
		fclose(d->ifp);
		h->ifp = NULL;
		delete d;
		return DCRAW_ERROR;
	}
	fclose(d->ifp);
	h->ifp = NULL;

    h->message = d->messageBuffer;
    return d->lastStatus;
}

void dcraw_close(dcraw_data *h)
{
    DCRaw *d = (DCRaw *)h->dcraw;
    g_free(h->raw.image);
    delete d;
}

void DCRaw::dcraw_message(int code, const char *format, ...)
{
}

void dcraw_message(void *dcraw, int code, char *format, ...)
{
    char *message;
    DCRaw *d = (DCRaw *)dcraw;
    va_list ap;
    va_start(ap, format);
    message = g_strdup_vprintf(format, ap);
    d->dcraw_message(code, message);
    va_end(ap);
    g_free(message);
}

} /*extern "C"*/

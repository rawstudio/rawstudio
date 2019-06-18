/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * dcraw_api.cc - API for DCRaw
 * Copyright 2004-2016 by Udi Fuchs
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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h> /* for sqrt() */
#include <setjmp.h>
#include <errno.h>
#include <float.h>
#include "uf_glib.h"
#include <glib/gi18n.h> /*For _(String) definition - NKBJ*/
#include <sys/types.h>
#include "dcraw_api.h"
#include "dcraw.h"

#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)

#define LIM(x,min,max) MAX(min,MIN(x,max))
#define CLIP(x) LIM((int)(x),0,65535)

extern "C" {
    int fcol_INDI(const unsigned filters, const int row, const int col,
                  const int top_margin, const int left_margin,
                  /*const*/ char xtrans[6][6]);
    void wavelet_denoise_INDI(gushort(*image)[4], const int black,
                              const int iheight, const int iwidth, const int height, const int width,
                              const int colors, const int shrink, const float pre_mul[4],
                              const float threshold, const unsigned filters);
    void scale_colors_INDI(const int maximum, const int black,
                           const int use_camera_wb, const float cam_mul[4], const int colors,
                           float pre_mul[4], const unsigned filters, /*const*/ gushort white[8][8],
                           const char *ifname_display, void *dcraw);
    void lin_interpolate_INDI(gushort(*image)[4], const unsigned filters,
                              const int width, const int height,
                              const int colors, void *dcraw, dcraw_data *h);
    void vng_interpolate_INDI(gushort(*image)[4], const unsigned filters,
                              const int width, const int height, const int colors, const int rgb_max,
                              void *dcraw, dcraw_data *h);
    void xtrans_interpolate_INDI(ushort(*image)[4], const unsigned filters,
                                 const int width, const int height,
                                 const int colors, const float rgb_cam[3][4],
                                 void *dcraw, dcraw_data *hh, const int passes);
    void ahd_interpolate_INDI(gushort(*image)[4], const unsigned filters,
                              const int width, const int height, const int colors, float rgb_cam[3][4],
                              void *dcraw, dcraw_data *h);
    void color_smooth(gushort(*image)[4], const int width, const int height,
                      const int passes);
    void ppg_interpolate_INDI(gushort(*image)[4], const unsigned filters,
                              const int width, const int height, const int colors, void *dcraw, dcraw_data *h);
    void flip_image_INDI(gushort(*image)[4], int *height_p, int *width_p,
                         const int flip);
    void fuji_rotate_INDI(gushort(**image_p)[4], int *height_p, int *width_p,
                          int *fuji_width_p, const int colors, const double step, void *dcraw);

    int dcraw_open(dcraw_data *h, char *filename)
    {
        DCRaw *d = new DCRaw;
        int c, i;

#ifndef LOCALTIME
        putenv(const_cast<char *>("TZ=UTC"));
#endif
        g_free(d->messageBuffer);
        d->messageBuffer = NULL;
        d->lastStatus = DCRAW_SUCCESS;
        d->verbose = 1;
        d->ifname = g_strdup(filename);
        d->ifname_display = g_filename_display_name(d->ifname);
        if (setjmp(d->failure)) {
            d->dcraw_message(DCRAW_ERROR, _("Fatal internal error\n"));
            h->message = d->messageBuffer;
            delete d;
            return DCRAW_ERROR;
        }
        if (!(d->ifp = g_fopen(d->ifname, "rb"))) {
            gchar *err_u8 = g_locale_to_utf8(strerror(errno), -1, NULL, NULL, NULL);
            d->dcraw_message(DCRAW_OPEN_ERROR, _("Cannot open file %s: %s\n"),
                             d->ifname_display, err_u8);
            g_free(err_u8);
            h->message = d->messageBuffer;
            delete d;
            return DCRAW_OPEN_ERROR;
        }
        d->identify();
        /* We first check if dcraw recognizes the file, this is equivalent
         * to 'dcraw -i' succeeding */
        if (!d->make[0]) {
            d->dcraw_message(DCRAW_OPEN_ERROR, _("%s: unsupported file format.\n"),
                             d->ifname_display);
            fclose(d->ifp);
            h->message = d->messageBuffer;
            int lastStatus = d->lastStatus;
            delete d;
            return lastStatus;
        }
        /* Next we check if dcraw can decode the file */
        if (!d->is_raw) {
            d->dcraw_message(DCRAW_OPEN_ERROR, _("Cannot decode file %s\n"),
                             d->ifname_display);
            fclose(d->ifp);
            h->message = d->messageBuffer;
            int lastStatus = d->lastStatus;
            delete d;
            return lastStatus;
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
        h->fuji_dr = d->fuji_dr;
        h->colors = d->colors;
        h->filters = d->filters;
        h->raw_color = d->raw_color;
        h->top_margin = d->top_margin;
        h->left_margin = d->left_margin;
        memcpy(h->cam_mul, d->cam_mul, sizeof d->cam_mul);
        // maximum and black might change during load_raw. We need them for the
        // camera-wb. If they'll change we will recalculate the camera-wb.
        h->rgbMax = d->maximum;
        i = d->cblack[3];
        FORC3 if ((unsigned)i > d->cblack[c]) i = d->cblack[c];
        FORC4 d->cblack[c] -= i;
        d->black += i;
        i = d->cblack[6];
        FORC(d->cblack[4] * d->cblack[5])
        if (i > d->cblack[6 + c]) i = d->cblack[6 + c];
        FORC(d->cblack[4] * d->cblack[5])
        d->cblack[6 + c] -= i;
        d->black += i;
        h->black = d->black;
        h->shrink = d->shrink = (h->filters == 1 || h->filters > 1000);
        h->pixel_aspect = d->pixel_aspect;
        /* copied from dcraw's main() */
        switch ((d->flip + 3600) % 360) {
            case 270:
                d->flip = 5;
                break;
            case 180:
                d->flip = 3;
                break;
            case  90:
                d->flip = 6;
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
        memcpy(h->xtrans, d->xtrans, sizeof d->xtrans);
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

    void fuji_merge(DCRaw *d, ushort *saved_raw_image, float saved_cam_mul[4], int saved_fuji_dr)
    {
        int i, j, c, s;
        unsigned b;
        float  S, R, w, l, m, th, tl, mul[4][4];

        if (d->fuji_width) { /* Super CCD SR */

            /* Populate a small array for converting the whitebalance */
            /* of the second image to that of the first one.          */

            if (d->fuji_layout) {
                /* First generation Super CCD SR (S20Pro, F700, F710)         */
                /* Many of these sensors are defective and have a colourcast. */

                /* RBRB */
                /* GGGG */
                /* BRBR */
                /* GGGG */
                mul[1][1] = mul[1][0] = mul[1][2] = mul[1][3] = 1;
                mul[3][1] = mul[3][0] = mul[3][2] = mul[3][3] = 1;
                mul[0][0] = mul[0][2] = mul[2][1] = mul[2][3] = d->cam_mul[0] / saved_cam_mul[0];
                mul[0][1] = mul[0][3] = mul[2][0] = mul[2][2] = d->cam_mul[2] / saved_cam_mul[2];

            } else { /* Super CCD SR II (S3Pro, S5Pro) */

                /* RGBG */
                /* BGRG */
                /* RGBG */
                /* BGRG */
                mul[0][1] = mul[0][3] = mul[1][1] = mul[1][3] = 1;
                mul[2][1] = mul[2][3] = mul[3][1] = mul[3][3] = 1;
                mul[0][0] = mul[1][2] = mul[2][0] = mul[3][2] = d->cam_mul[0] / saved_cam_mul[0];
                mul[0][2] = mul[1][0] = mul[2][2] = mul[3][0] = d->cam_mul[2] / saved_cam_mul[2];
            }

            for (i = 0 ; i < d->raw_height; i++)
                for (j = 0 ; j < d->raw_width; j++) {

                    S = saved_raw_image[i * d->raw_width + j];
                    R = d->raw_image[i * d->raw_width + j] * mul[i & 3][j & 3] * 16;

                    /* Fade from S to R in one stop. */
                    /* Response of these sensors appears to be non-linear, */
                    /* causing a slight colourcast in the transition zone. */
                    if (S > 0x1f00) {
                        if (S < 0x3e00) {
                            w = (S - 0x1f00) / 0x1f00;
                            S = (1 - w) * S + w * R;
                        } else
                            S = R;
                    }

                    d->raw_image[i * d->raw_width + j] = CLIP((S * 0xffff / 0x2f000));
                }

            d->maximum = 0xffff;

            FORC4 d->cam_mul[c] = saved_cam_mul[c];

            d->fuji_dr = -400;

        } else { /* EXR */

            if (d->black)
                b = d->black;
            else
                b = d->cblack[6];

            s = (saved_fuji_dr - d->fuji_dr) / 100;


            if (s) { /* DR-mode */

                th = l = d->maximum - b;
                m = 1 << s;
                tl = th / m;
                th += tl;
                m += 1;
                l *= m;

                for (i = 0 ; i < d->raw_height * d->raw_width; i++) {

                    /* Range check to avoid problems when value is below black. */
                    S = LIM(saved_raw_image[i], b, d->maximum) - b;
                    R = LIM(d->raw_image[i], b, d->maximum) - b;
                    /* Adding R to S pixels reduces noise a bit. */
                    S += R;
                    R *= m;

                    /* Fade from S to R in ~1.5 or 2.25 stops. */
                    /* Response of EXR sensors appears to be linear. */
                    if (S > tl) {
                        if (S < th) {
                            w = (S - tl) / (th - tl);
                            S = (1 - w) * S + w * R;
                        } else
                            S = R;
                    }

                    /* l can be larger than 0xffff. */
                    d->raw_image[i] = CLIP(S * 0xffff / l);
                }

                d->maximum = 0xffff;
                d->black = 0;

                for (i = 6 ; i < 10 ; i++)
                    d->cblack[i] = 0;

                //d->fuji_dr = saved_fuji_dr;

            } else { /* Low-noise-mode */

                for (i = 0 ; i < d->raw_height * d->raw_width ; i++)
                    d->raw_image[i] += saved_raw_image[i];

                d->maximum *= 2;
                d->black *= 2;

                for (i = 6 ; i < 10 ; i++)
                    d->cblack[i] *= 2;
            }
        }
    }

    int dcraw_load_raw(dcraw_data *h)
    {
        /* 'volatile' supresses clobbering warning */
        DCRaw * volatile d = (DCRaw *)h->dcraw;
        int c, i, j;
        double dmin;

start:
        g_free(d->messageBuffer);
        d->messageBuffer = NULL;
        d->lastStatus = DCRAW_SUCCESS;
        d->raw_image = 0;
        if (setjmp(d->failure)) {
            d->dcraw_message(DCRAW_ERROR, _("Fatal internal error\n"));
            h->message = d->messageBuffer;
            delete d;
            return DCRAW_ERROR;
        }
        h->raw.height = d->iheight = (h->height + h->shrink) >> h->shrink;
        h->raw.width = d->iwidth = (h->width + h->shrink) >> h->shrink;
        h->raw.colors = d->colors;
        h->fourColorFilters = d->filters;
        if (d->filters || d->colors == 1) {
            if (d->colors == 1 || d->filters == 1 || d->filters > 1000)
                d->raw_image = (ushort *) g_malloc((d->raw_height + 7) * d->raw_width * 2);
            else
                d->raw_image = (ushort *) g_malloc(sizeof(dcraw_image_type) * (d->raw_height + 7) * d->raw_width);
        } else {
            h->raw.image = d->image = g_new0(dcraw_image_type, d->iheight * d->iwidth
                                             + d->meta_length);
            d->meta_data = (char *)(d->image + d->iheight * d->iwidth);
        }
        d->dcraw_message(DCRAW_VERBOSE, _("Loading %s %s image from %s ...\n"),
                         d->make, d->model, d->ifname_display);
        fseek(d->ifp, 0, SEEK_END);
        d->ifpSize = ftell(d->ifp);
        fseek(d->ifp, d->data_offset, SEEK_SET);
        (d->*d->load_raw)();

        /* multishot support, for now Pentax only. */
        if (d->is_raw == 4 && !strncasecmp(d->make, "Pentax", 6)) {

            int row, col, i;
            int positions[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            static dcraw_image_type *tmp = NULL;

            if (!tmp)
                tmp = d->image = g_new0(dcraw_image_type, d->height * d->width + d->meta_length);

#ifdef _OPENMP
            #pragma omp parallel for private(col)
#endif
            for (row = 0 ; row < d->height ; row++)
                for (col = 0 ; col < d->width ; col++)
                    tmp[row * d->width + col][fcol_INDI(d->filters, row + positions[d->shot_select][0], col + positions[d->shot_select][1], d->top_margin, d->left_margin, d->xtrans)] = d->raw_image[(row + d->top_margin + positions[d->shot_select][0]) * d->raw_width + col + d->left_margin + positions[d->shot_select][1]];

            g_free(d->raw_image);
            d->raw_image = NULL;

            if (d->shot_select < 3) {
                d->shot_select++;
                fseek(d->ifp, 0, SEEK_SET);
                d->identify();
                goto start;
            }

            if (d->shot_select == 3) /* Just to keep the compiler happy. */
#ifdef _OPENMP
                #pragma omp parallel for
#endif
                for (i = 0 ; i < d->height * d->width ; i++)
                    tmp[i][1] = (tmp[i][1] + tmp[i][3]) / 2;

            d->image = tmp;
            d->shot_select = 0;
            d->is_raw = 0;
            d->filters = 0;
            d->shrink = 0;
            d->meta_data = (char *)(tmp + d->height * d->width);

            h->raw.image = tmp;
            h->filters = 0;
            h->shrink = 0;

            tmp = NULL;
        }

        /* Fuji Super CCD SR and EXR support */
        if (d->is_raw == 2 && !strncasecmp(d->make, "Fujifilm", 8)) {

            static int saved_fuji_dr;
            static float saved_cam_mul[4];
            static guint16 *saved_raw_image = NULL;

            if (!saved_raw_image) {

                saved_raw_image = d->raw_image;
                d->raw_image = NULL;
                saved_fuji_dr = d->fuji_dr;
                FORC4 saved_cam_mul[c] = d->cam_mul[c];

                d->shot_select++;
                fseek(d->ifp, 0, SEEK_SET);
                d->identify();
                goto start;
            }

            fuji_merge(d, saved_raw_image, saved_cam_mul, saved_fuji_dr);

            free(saved_raw_image);
            saved_raw_image = NULL;
            d->shot_select--;

            FORC4 h->cam_mul[c] = d->cam_mul[c];
            h->fuji_dr = d->fuji_dr;
            h->filters = d->filters;
            h->rgbMax = d->maximum;
            h->black = d->black;
        }

        h->raw.height = d->iheight = (h->height + h->shrink) >> h->shrink;
        h->raw.width = d->iwidth = (h->width + h->shrink) >> h->shrink;
        if (d->raw_image) {
            h->raw.image = d->image = g_new0(dcraw_image_type, d->iheight * d->iwidth
                                             + d->meta_length);
            d->meta_data = (char *)(d->image + d->iheight * d->iwidth);
            d->crop_masked_pixels();
            g_free(d->raw_image);

            if (d->filters > 1 && d->filters <= 1000)
                lin_interpolate_INDI(d->image, d->filters, d->width, d->height, d->colors, d, h);
        }
        if (!--d->data_error) d->lastStatus = DCRAW_ERROR;
        if (d->zero_is_bad) d->remove_zeroes();
        d->bad_pixels(NULL);
        if (d->is_foveon) {
            if (d->load_raw == &DCRaw::foveon_dp_load_raw) {
                d->meta_data = 0;
                d->sigma_true_ii_interpolate();
            } else d->foveon_interpolate();
            h->raw.width = h->width = d->width;
            h->raw.height = h->height = d->height;
        }
        fclose(d->ifp);
        h->ifp = NULL;
        // TODO: Go over the following settings to see if they change during
        // load_raw. If they change, document where. If not, move to dcraw_open().
        h->rgbMax = d->maximum;
        i = d->cblack[3];
        FORC3 if ((unsigned)i > d->cblack[c]) i = d->cblack[c];
        FORC4 d->cblack[c] -= i;
        d->black += i;
        i = d->cblack[6];
        FORC(d->cblack[4] * d->cblack[5])
        if (i > d->cblack[6 + c]) i = d->cblack[6 + c];
        FORC(d->cblack[4] * d->cblack[5])
        d->cblack[6 + c] -= i;
        d->black += i;
        h->black = d->black;
        d->dcraw_message(DCRAW_VERBOSE, _("Black: %d, Maximum: %d\n"),
                         d->black, d->maximum);
        dmin = DBL_MAX;
        for (i = 0; i < h->colors; i++) if (dmin > d->pre_mul[i]) dmin = d->pre_mul[i];
        for (i = 0; i < h->colors; i++) h->pre_mul[i] = d->pre_mul[i] / dmin;
        if (h->colors == 3) h->pre_mul[3] = 0;
        memcpy(h->rgb_cam, d->rgb_cam, sizeof d->rgb_cam);

        double rgb_cam_transpose[4][3];
        for (i = 0; i < 4; i++) for (j = 0; j < 3; j++)
                rgb_cam_transpose[i][j] = d->rgb_cam[j][i];
        d->pseudoinverse(rgb_cam_transpose, h->cam_rgb, d->colors);

        h->message = d->messageBuffer;
        return d->lastStatus;
    }

    int dcraw_load_thumb(dcraw_data *h, dcraw_image_data *thumb)
    {
        DCRaw *d = (DCRaw *)h->dcraw;

        g_free(d->messageBuffer);
        d->messageBuffer = NULL;
        d->lastStatus = DCRAW_SUCCESS;

        thumb->height = d->thumb_height;
        thumb->width = d->thumb_width;
        h->thumbOffset = d->thumb_offset;
        h->thumbBufferLength = d->thumb_length;
        if (d->thumb_offset == 0) {
            dcraw_message(d, DCRAW_ERROR, _("%s has no thumbnail."),
                          d->ifname_display);
        } else if (d->thumb_load_raw != NULL) {
            dcraw_message(d, DCRAW_ERROR,
                          _("Unsupported thumb format (load_raw) for %s"),
                          d->ifname_display);
        } else if (d->write_thumb == &DCRaw::jpeg_thumb) {
            h->thumbType = jpeg_thumb_type;
        } else if (d->write_thumb == &DCRaw::ppm_thumb) {
            h->thumbType = ppm_thumb_type;
            // Copied from dcraw's ppm_thumb()
            h->thumbBufferLength = thumb->width * thumb->height * 3;
        } else {
            dcraw_message(d, DCRAW_ERROR,
                          _("Unsupported thumb format for %s"), d->ifname_display);
        }
        h->message = d->messageBuffer;
        return d->lastStatus;
    }

    /* Grab a pixel from the raw data, doing dark frame removal on the fly.
     *
     * The most obvious algorithm for dark frame removal is to simply
     * subtract the dark frame from the image (rounding negative values to
     * zero).  However, this leaves holes in the resulting image that need
     * to be interpolated from the surrounding pixels.
     *
     * The processing works by subtracting the dark frame as usual for most
     * pixels.  For all pixels where the dark frame is brighter than a given
     * threshold, the result is instead calculated as the average of the
     * dark-adjusted values of the 4 surrounding pixels.  By this method,
     * only hot pixels (as determined by the threshold) are examined and
     * recalculated.
     */
    static int get_dark_pixel(const dcraw_data *h, const dcraw_data *dark,
                              int i, int cl)
    {
        return MAX(h->raw.image[i][cl] - dark->raw.image[i][cl], 0);
    }

    static int get_pixel(const dcraw_data *h, const dcraw_data *dark,
                         int i, int cl, int pixels)
    {
        int pixel = h->raw.image[i][cl];
        if (dark != 0) {
            int w = h->raw.width;
            pixel = (dark->raw.image[i][cl] <= dark->thresholds[cl])
                    ? MAX(pixel - dark->raw.image[i][cl], 0)
                    : (get_dark_pixel(h, dark, i + ((i >= 1) ? -1 : 1), cl) +
                       get_dark_pixel(h, dark, i + ((i < pixels - 1) ? 1 : -1), cl) +
                       get_dark_pixel(h, dark, i + ((i >= w) ? -w : w), cl) +
                       get_dark_pixel(h, dark, i + ((i < pixels - w) ? w : -w), cl))
                    / 4;
        }
        return pixel;
    }

    /*
     * fcol_INDI() optimizing wrapper.
     * fcol_sequence() cooks up the filter color sequence for a row knowing that
     * it doesn't have to store more than 16 values. The result can be indexed
     * by the column using fcol_color() and that part must of course be inlined
     * for maximum performance. The inner loop for image processing should
     * always try to index the column and not the row in order to reduce the
     * data cache footprint.
     */
    static unsigned fcol_sequence(int filters, int row, int top_margin,
                                  int left_margin, char xtrans[6][6])
    {
        unsigned sequence = 0;
        int c;

        for (c = 15; c >= 0; --c)
            sequence = (sequence << 2) | fcol_INDI(filters, row, c, top_margin, left_margin, xtrans);
        return sequence;
    }

    /*
     * Note: smart compilers will inline anyway in most cases: the "inline"
     * below is a comment reminding not to make it an external function.
     */
    static inline int fcol_color(unsigned sequence, int col)
    {
        return (sequence >> ((col << 1) & 0x1f)) & 3;
    }

    static inline void shrink_accumulate_row(unsigned *sum, int size,
            dcraw_image_type *base, int scale, int color)
    {
        int i, j;
        unsigned v;

        for (i = 0; i < size; ++i) {
            v = 0;
            for (j = 0; j < scale; ++j)
                v += base[i * scale + j][color];
            sum[i] += v;
        }
    }

    static inline void shrink_row(dcraw_image_type *obase, int osize,
                                  dcraw_image_type *ibase, int isize, int colors, int scale)
    {
        unsigned *sum;
        dcraw_image_type *iptr;
        int cl, i;

        sum = (unsigned*) g_malloc(osize * sizeof(unsigned));
        for (cl = 0; cl < colors; ++cl) {
            memset(sum, 0, osize * sizeof(unsigned));
            iptr = ibase;
            for (i = 0; i < scale; ++i) {
                shrink_accumulate_row(sum, osize, iptr, scale, cl);
                iptr += isize;
            }
            for (i = 0; i < osize; ++i)
                obase[i][cl] = sum[i] / (scale * scale);
        }
        g_free(sum);
    }

    static inline void shrink_pixel(dcraw_image_type pixp, int row, int col,
                                    dcraw_data *hh, unsigned *fseq, int scale)
    {
        unsigned sum[4], count[4];
        int ri, ci, cl;
        dcraw_image_type *ibase;

        memset(sum, 0, 4 * sizeof(unsigned));
        memset(count, 0, 4 * sizeof(unsigned));
        for (ri = 0; ri < scale; ++ri) {
            ibase = hh->raw.image + ((row * scale + ri) / 2) * hh->raw.width;
            for (ci = 0; ci < scale; ++ci) {
                cl = fcol_color(fseq[ri], col * scale + ci);
                sum[cl] += ibase[(col * scale + ci) / 2][cl];
                ++count[cl];
            }
        }
        for (cl = 0; cl < hh->raw.colors; ++cl)
            pixp[cl] = sum[cl] / count[cl];
    }

    int dcraw_finalize_shrink(dcraw_image_data *f, dcraw_data *hh,
                              int scale)
    {
        DCRaw *d = (DCRaw *)hh->dcraw;
        int h, w, fujiWidth, r, c, ri, recombine, f4;
        dcraw_image_type *ibase, *obase;
        unsigned *fseq;
        unsigned short *pixp;

        g_free(d->messageBuffer);
        d->messageBuffer = NULL;
        d->lastStatus = DCRAW_SUCCESS;

        recombine = (hh->colors == 3 && hh->raw.colors == 4);
        /* the last row/column will be skipped if input is incomplete */
        f->height = h = hh->height / scale;
        f->width = w = hh->width / scale;
        f->colors = hh->colors;

        /* hh->raw.image is shrunk in half if there are filters.
         * If scale is odd we need to "unshrink" it using the info in
         * hh->fourColorFilters before scaling it. */
        if ((hh->filters == 1 || hh->filters > 1000) && scale % 2 == 1) {
            fujiWidth = hh->fuji_width / scale;
            f->image = (dcraw_image_type *)
                       g_realloc(f->image, h * w * sizeof(dcraw_image_type));
            f4 = hh->fourColorFilters;

#ifdef _OPENMP
            #pragma omp parallel for schedule(static) private(r,ri,fseq,c,pixp)
#endif
            for (r = 0; r < h; ++r) {
                fseq = (unsigned*) g_malloc(scale * sizeof(unsigned));
                for (ri = 0; ri < scale; ++ri)
                    fseq[ri] = fcol_sequence(f4, r + ri, hh->top_margin, hh->left_margin, hh->xtrans);
                for (c = 0; c < w; ++c) {
                    pixp = f->image[r * w + c];
                    shrink_pixel(pixp, r, c, hh, fseq, scale);
                    if (recombine)
                        pixp[1] = (pixp[1] + pixp[3]) / 2;
                }
                g_free(fseq);
            }
        } else {
            if (hh->filters == 1 || hh->filters > 1000) scale /= 2;
            fujiWidth = ((hh->fuji_width + hh->shrink) >> hh->shrink) / scale;
            f->image = (dcraw_image_type *)g_realloc(
                           f->image, h * w * sizeof(dcraw_image_type));
#ifdef _OPENMP
            #pragma omp parallel for schedule(static) private(r,ibase,obase,c)
#endif
            for (r = 0; r < h; ++r) {
                ibase = hh->raw.image + r * hh->raw.width * scale;
                obase = f->image + r * w;
                if (scale == 1)
                    memcpy(obase, ibase, sizeof(dcraw_image_type) * w);
                else
                    shrink_row(obase, w, ibase, hh->raw.width, hh->raw.colors, scale);
                if (recombine) {
                    for (c = 0; c < w; c++)
                        obase[c][1] = (obase[c][1] + obase[c][3]) / 2;
                }
            }
        }
        fuji_rotate_INDI(&f->image, &f->height, &f->width, &fujiWidth,
                         f->colors, hh->fuji_step, d);

        hh->message = d->messageBuffer;
        return d->lastStatus;
    }

    int dcraw_image_resize(dcraw_image_data *image, int size)
    {
        int h, w, wid, r, ri, rii, c, ci, cii, cl, norm;
        guint64 riw, riiw, ciw, ciiw;
        guint64(*iBuf)[4];
        int mul = size, div = MAX(image->height, image->width);

        if (mul > div) return DCRAW_ERROR;
        if (mul == div) return DCRAW_SUCCESS;
        /* I'm skiping the last row/column if it is not a full row/column */
        h = image->height * mul / div;
        w = image->width * mul / div;
        wid = image->width;
        iBuf = (guint64(*)[4])g_new0(guint64, h * w * 4);
        norm = div * div;

        for (r = 0; r < image->height; r++) {
            /* r should be divided between ri and rii */
            ri = r * mul / div;
            rii = (r + 1) * mul / div;
            /* with weights riw and riiw (riw+riiw==mul) */
            riw = rii * div - r * mul;
            riiw = (r + 1) * mul - rii * div;
            if (rii >= h) {
                rii = h - 1;
                riiw = 0;
            }
            if (ri >= h) {
                ri = h - 1;
                riw = 0;
            }
            for (c = 0; c < image->width; c++) {
                ci = c * mul / div;
                cii = (c + 1) * mul / div;
                ciw = cii * div - c * mul;
                ciiw = (c + 1) * mul - cii * div;
                if (cii >= w) {
                    cii = w - 1;
                    ciiw = 0;
                }
                if (ci >= w) {
                    ci = w - 1;
                    ciw = 0;
                }
                for (cl = 0; cl < image->colors; cl++) {
                    iBuf[ri * w + ci ][cl] += image->image[r * wid + c][cl] * riw * ciw ;
                    iBuf[ri * w + cii][cl] += image->image[r * wid + c][cl] * riw * ciiw;
                    iBuf[rii * w + ci ][cl] += image->image[r * wid + c][cl] * riiw * ciw ;
                    iBuf[rii * w + cii][cl] += image->image[r * wid + c][cl] * riiw * ciiw;
                }
            }
        }
        for (c = 0; c < h * w; c++) for (cl = 0; cl < image->colors; cl++)
                image->image[c][cl] = iBuf[c][cl] / norm;
        g_free(iBuf);
        image->height = h;
        image->width = w;
        return DCRAW_SUCCESS;
    }

    /* Adapted from dcraw.c stretch() - NKBJ */
    int dcraw_image_stretch(dcraw_image_data *image, double pixel_aspect)
    {
        int newdim, row, col, c, colors = image->colors;
        double rc, frac;
        ushort *pix0, *pix1;
        dcraw_image_type *iBuf;

        if (pixel_aspect == 1) return DCRAW_SUCCESS;
        if (pixel_aspect < 1) {
            newdim = (int)(image->height / pixel_aspect + 0.5);
            iBuf = g_new(dcraw_image_type, image->width * newdim);
            for (rc = row = 0; row < newdim; row++, rc += pixel_aspect) {
                frac = rc - (c = (int)rc);
                pix0 = pix1 = image->image[c * image->width];
                if (c + 1 < image->height) pix1 += image->width * 4;
                for (col = 0; col < image->width; col++, pix0 += 4, pix1 += 4)
                    FORCC iBuf[row * image->width + col][c] =
                        (guint16)(pix0[c] * (1 - frac) + pix1[c] * frac + 0.5);
            }
            image->height = newdim;
        } else {
            newdim = (int)(image->width * pixel_aspect + 0.5);
            iBuf = g_new(dcraw_image_type, image->height * newdim);
            for (rc = col = 0; col < newdim; col++, rc += 1 / pixel_aspect) {
                frac = rc - (c = (int)rc);
                pix0 = pix1 = image->image[c];
                if (c + 1 < image->width) pix1 += 4;
                for (row = 0; row < image->height;
                        row++, pix0 += image->width * 4, pix1 += image->width * 4)
                    FORCC iBuf[row * newdim + col][c] =
                        (guint16)(pix0[c] * (1 - frac) + pix1[c] * frac + 0.5);
            }
            image->width = newdim;
        }
        g_free(image->image);
        image->image = iBuf;
        return DCRAW_SUCCESS;
    }

    int dcraw_flip_image(dcraw_image_data *image, int flip)
    {
        if (flip)
            flip_image_INDI(image->image, &image->height, &image->width, flip);
        return DCRAW_SUCCESS;
    }

    int dcraw_set_color_scale(dcraw_data *h, int useCameraWB)
    {
        DCRaw *d = (DCRaw *)h->dcraw;
        g_free(d->messageBuffer);
        d->messageBuffer = NULL;
        d->lastStatus = DCRAW_SUCCESS;
        memcpy(h->post_mul, h->pre_mul, sizeof h->post_mul);
        if (d->is_foveon) {
            // foveon_interpolate() applies the camera-wb already.
            for (int c = 0; c < 4; c++)
                h->post_mul[c] = 1.0;
        } else {
            scale_colors_INDI(h->rgbMax, h->black, useCameraWB, h->cam_mul,
                              h->colors, h->post_mul, h->filters, d->white,
                              d->ifname_display, d);
        }
        h->message = d->messageBuffer;
        return d->lastStatus;
    }

    void dcraw_wavelet_denoise(dcraw_data *h, float threshold)
    {
        if (threshold)
            wavelet_denoise_INDI(h->raw.image, h->black, h->raw.height,
                                 h->raw.width, h->height, h->width, h->colors, h->shrink,
                                 h->post_mul, threshold, h->fourColorFilters);
    }

    void dcraw_wavelet_denoise_shrinked(dcraw_image_data *f, float threshold)
    {
        if (threshold)
            wavelet_denoise_INDI(f->image, 0, f->height, f->width, 0, 0, 4, 0,
                                 NULL, threshold, 0);
    }

    /*
     * Do black level adjustment, dark frame subtraction and white balance
     * (plus normalization to use the full 16 bit pixel value range) in one
     * pass.
     *
     * TODO: recode and optimize dark frame path
     */
    void dcraw_finalize_raw(dcraw_data *h, dcraw_data *dark, int rgbWB[4])
    {
        const int pixels = h->raw.width * h->raw.height;
        const unsigned black = dark ? MAX(h->black - dark->black, 0) : h->black;
        if (h->colors == 3)
            rgbWB[3] = rgbWB[1];
        if (dark) {
#ifdef _OPENMP
            #pragma omp parallel for schedule(static) \
            shared(h,dark,rgbWB)
#endif
            for (int i = 0; i < pixels; i++) {
                int cc;
                for (cc = 0; cc < 4; cc++) {
                    gint32 p = (gint64)(get_pixel(h, dark, i, cc, pixels) - black) *
                               rgbWB[cc] / 0x10000;
                    h->raw.image[i][cc] = MIN(MAX(p, 0), 0xFFFF);
                }
            }
        } else {
#ifdef _OPENMP
            #pragma omp parallel for schedule(static) \
            shared(h,dark,rgbWB)
#endif
            for (int i = 0; i < pixels; i++) {
                int cc;
                for (cc = 0; cc < 4; cc++)
                    h->raw.image[i][cc] = MIN(MAX(
                                                  ((gint64)h->raw.image[i][cc] - black) *
                                                  rgbWB[cc] / 0x10000, 0), 0xFFFF);
            }
        }
    }

    int dcraw_finalize_interpolate(dcraw_image_data *f, dcraw_data *h,
                                   int interpolation, int smoothing)
    {
        DCRaw *d = (DCRaw *)h->dcraw;
        int fujiWidth, i, r, c, cl;
        unsigned ff, f4;

        g_free(d->messageBuffer);
        d->messageBuffer = NULL;
        d->lastStatus = DCRAW_SUCCESS;

        f->width = h->width;
        f->height = h->height;
        fujiWidth = h->fuji_width;
        f->colors = h->colors;
        f->image = (dcraw_image_type *)
                   g_realloc(f->image, f->height * f->width * sizeof(dcraw_image_type));
        memset(f->image, 0, f->height * f->width * sizeof(dcraw_image_type));

        if (h->filters == 0)
            return DCRAW_ERROR;

        cl = h->colors;
        if (interpolation == dcraw_four_color_interpolation || h->colors == 4) {
            ff = h->fourColorFilters;
            cl = 4;
            interpolation = dcraw_vng_interpolation;
        } else {
            ff = h->filters &= ~((h->filters & 0x55555555) << 1);
        }

        /* It might be better to report an error here: */
        /* (dcraw also forbids AHD for Fuji rotated images) */
        if (h->filters == 9 && interpolation != dcraw_bilinear_interpolation)
            interpolation = dcraw_xtrans_interpolation;
        if (interpolation == dcraw_ahd_interpolation && h->colors > 3)
            interpolation = dcraw_vng_interpolation;
        if (interpolation == dcraw_ppg_interpolation && h->colors > 3)
            interpolation = dcraw_vng_interpolation;
        f4 = h->fourColorFilters;
        if (h->filters == 1 || h->filters > 1000) {
            for (r = 0; r < h->height; r++)
                for (c = 0; c < h->width; c++) {
                    int cc = fcol_INDI(f4, r, c, h->top_margin, h->left_margin, h->xtrans);
                    f->image[r * f->width + c][fcol_INDI(ff, r, c, h->top_margin, h->left_margin, h->xtrans)] =
                        h->raw.image[r / 2 * h->raw.width + c / 2][cc];
                }
        } else
            memcpy(f->image, h->raw.image, h->height * h->width * sizeof(dcraw_image_type));
        int smoothPasses = 1;
        if (interpolation == dcraw_bilinear_interpolation && (h->filters == 1 || h->filters > 1000))
            lin_interpolate_INDI(f->image, ff, f->width, f->height, cl, d, h);
#ifdef ENABLE_INTERP_NONE
        else if (interpolation == dcraw_none_interpolation)
            smoothing = 0;
#endif
        else if (interpolation == dcraw_vng_interpolation || h->colors > 3)
            vng_interpolate_INDI(f->image, ff, f->width, f->height, cl, 0xFFFF, d, h);
        else if (interpolation == dcraw_ppg_interpolation && h->filters > 1000)
            ppg_interpolate_INDI(f->image, ff, f->width, f->height, cl, d, h);

        else if (interpolation == dcraw_xtrans_interpolation) {
            xtrans_interpolate_INDI(f->image, h->filters, f->width, f->height,
                                    h->colors, h->rgb_cam, d, h, 3);
            smoothPasses = 3;
        } else if (interpolation == dcraw_ahd_interpolation) {
            ahd_interpolate_INDI(f->image, ff, f->width, f->height, cl,
                                 h->rgb_cam, d, h);
            smoothPasses = 3;
        }
        if (smoothing)
            color_smooth(f->image, f->width, f->height, smoothPasses);

        if (cl == 4 && h->colors == 3) {
            for (i = 0; i < f->height * f->width; i++)
                f->image[i][1] = (f->image[i][1] + f->image[i][3]) / 2;
        }
        fuji_rotate_INDI(&f->image, &f->height, &f->width, &fujiWidth,
                         f->colors, h->fuji_step, d);

        h->message = d->messageBuffer;
        return d->lastStatus;
    }

    void dcraw_close(dcraw_data *h)
    {
        DCRaw *d = (DCRaw *)h->dcraw;
        g_free(h->raw.image);
        delete d;
    }

    char *ufraw_message(int code, char *message, ...);

    void DCRaw::dcraw_message(int code, const char *format, ...)
    {
        char *buf, *message;
        va_list ap;
        va_start(ap, format);
        message = g_strdup_vprintf(format, ap);
        va_end(ap);
#ifdef DEBUG
        fprintf(stderr, message);
#endif
        if (code == DCRAW_VERBOSE)
            ufraw_message(code, message);
        else {
            if (messageBuffer == NULL) messageBuffer = g_strdup(message);
            else {
                buf = g_strconcat(messageBuffer, message, NULL);
                g_free(messageBuffer);
                messageBuffer = buf;
            }
            lastStatus = code;
        }
        g_free(message);
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

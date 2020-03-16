/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * dcraw_api.h - API for DCRaw
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

#ifndef _DCRAW_API_H
#define _DCRAW_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef guint16 dcraw_image_type[4];

typedef struct {
    dcraw_image_type *image;
    int width, height, colors;
} dcraw_image_data;

typedef struct {
    void *dcraw;
    FILE *ifp;
    int width, height, colors, fourColorFilters, raw_color;
    unsigned filters;
    int top_margin, left_margin, flip, shrink;
    double pixel_aspect;
    dcraw_image_data raw;
    dcraw_image_type thresholds;
    float pre_mul[4], post_mul[4], cam_mul[4], rgb_cam[3][4];
    double cam_rgb[4][3];
    int rgbMax, black, fuji_width;
    double fuji_step;
    short fuji_dr;
    int toneCurveSize, toneCurveOffset;
    int toneModeSize, toneModeOffset;
    char *message, xtrans[6][6];
    float iso_speed, shutter, aperture, focal_len;
    time_t timestamp;
    char make[80], model[80];
    int thumbType, thumbOffset;
    size_t thumbBufferLength;
} dcraw_data;

enum { dcraw_ahd_interpolation,
       dcraw_vng_interpolation, dcraw_four_color_interpolation,
       dcraw_ppg_interpolation, dcraw_bilinear_interpolation,
       dcraw_xtrans_interpolation, dcraw_none_interpolation
     };
enum { unknown_thumb_type, jpeg_thumb_type, ppm_thumb_type };
int dcraw_open(dcraw_data *h, char *filename);
int dcraw_load_raw(dcraw_data *h);
int dcraw_load_thumb(dcraw_data *h, dcraw_image_data *thumb);
int dcraw_finalize_shrink(dcraw_image_data *f, dcraw_data *h,
                          int scale);
int dcraw_image_resize(dcraw_image_data *image, int size);
int dcraw_image_stretch(dcraw_image_data *image, double pixel_aspect);
int dcraw_flip_image(dcraw_image_data *image, int flip);
int dcraw_set_color_scale(dcraw_data *h, int useCameraWB);
void dcraw_wavelet_denoise(dcraw_data *h, float threshold);
void dcraw_wavelet_denoise_shrinked(dcraw_image_data *f, float threshold);
void dcraw_finalize_raw(dcraw_data *h, dcraw_data *dark, int rgbWB[4]);
int dcraw_finalize_interpolate(dcraw_image_data *f, dcraw_data *h,
                               int interpolation, int smoothing);
void dcraw_close(dcraw_data *h);
void dcraw_image_dimensions(dcraw_data *raw, int flip, int shrink,
                            int *height, int *width);

#define DCRAW_SUCCESS 0
#define DCRAW_ERROR 1
#define DCRAW_UNSUPPORTED 2
#define DCRAW_NO_CAMERA_WB 3
#define DCRAW_VERBOSE 4
#define DCRAW_WARNING 5
#define DCRAW_OPEN_ERROR 6

void dcraw_message(void *dcraw, int code, char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /*_DCRAW_API_H*/

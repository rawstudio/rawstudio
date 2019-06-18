/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * dcraw_indi.c - DCRaw functions made independent
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gi18n.h> /*For _(String) definition - NKBJ*/
#include "dcraw_api.h"
#include "uf_progress.h"

#ifdef _OPENMP
#include <omp.h>
#define uf_omp_get_thread_num() omp_get_thread_num()
#define uf_omp_get_num_threads() omp_get_num_threads()
#else
#define uf_omp_get_thread_num() 0
#define uf_omp_get_num_threads() 1
#endif

#if !defined(ushort)
#define ushort unsigned short
#endif

extern const double xyz_rgb[3][3];
extern const float d65_white[3];

#define CLASS

#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)

#define SQR(x) ((x)*(x))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define ULIM(x,y,z) ((y) < (z) ? LIM(x,y,z) : LIM(x,z,y))
#define CLIP(x) LIM((int)(x),0,65535)
#define SWAP(a,b) { a ^= b; a ^= (b ^= a); }

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2
 */
#define FC(row,col) \
    (int)(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

#define BAYER(row,col) \
    image[((row) >> shrink)*iwidth + ((col) >> shrink)][FC(row,col)]

int CLASS fcol_INDI(const unsigned filters, const int row, const int col,
                    const int top_margin, const int left_margin,
                    /*const*/ char xtrans[6][6])
{
    static const char filter[16][16] = {
        { 2, 1, 1, 3, 2, 3, 2, 0, 3, 2, 3, 0, 1, 2, 1, 0 },
        { 0, 3, 0, 2, 0, 1, 3, 1, 0, 1, 1, 2, 0, 3, 3, 2 },
        { 2, 3, 3, 2, 3, 1, 1, 3, 3, 1, 2, 1, 2, 0, 0, 3 },
        { 0, 1, 0, 1, 0, 2, 0, 2, 2, 0, 3, 0, 1, 3, 2, 1 },
        { 3, 1, 1, 2, 0, 1, 0, 2, 1, 3, 1, 3, 0, 1, 3, 0 },
        { 2, 0, 0, 3, 3, 2, 3, 1, 2, 0, 2, 0, 3, 2, 2, 1 },
        { 2, 3, 3, 1, 2, 1, 2, 1, 2, 1, 1, 2, 3, 0, 0, 1 },
        { 1, 0, 0, 2, 3, 0, 0, 3, 0, 3, 0, 3, 2, 1, 2, 3 },
        { 2, 3, 3, 1, 1, 2, 1, 0, 3, 2, 3, 0, 2, 3, 1, 3 },
        { 1, 0, 2, 0, 3, 0, 3, 2, 0, 1, 1, 2, 0, 1, 0, 2 },
        { 0, 1, 1, 3, 3, 2, 2, 1, 1, 3, 3, 0, 2, 1, 3, 2 },
        { 2, 3, 2, 0, 0, 1, 3, 0, 2, 0, 1, 2, 3, 0, 1, 0 },
        { 1, 3, 1, 2, 3, 2, 3, 2, 0, 2, 0, 1, 1, 0, 3, 0 },
        { 0, 2, 0, 3, 1, 0, 0, 1, 1, 3, 3, 2, 3, 2, 2, 1 },
        { 2, 1, 3, 2, 3, 1, 2, 1, 0, 3, 0, 2, 0, 2, 0, 2 },
        { 0, 3, 1, 0, 0, 2, 0, 3, 2, 1, 3, 1, 1, 3, 1, 3 }
    };

    if (filters == 1) return filter[(row + top_margin) & 15][(col + left_margin) & 15];
    if (filters == 9) return xtrans[(row + 6) % 6][(col + 6) % 6];
    return FC(row, col);
}

static void CLASS merror(void *ptr, char *where)
{
    if (ptr) return;
    g_error("Out of memory in %s\n", where);
}

static void CLASS hat_transform(float *temp, float *base, int st, int size, int sc)
{
    int i;
    for (i = 0; i < sc; i++)
        temp[i] = 2 * base[st * i] + base[st * (sc - i)] + base[st * (i + sc)];
    for (; i + sc < size; i++)
        temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (i + sc)];
    for (; i < size; i++)
        temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (2 * size - 2 - (i + sc))];
}

void CLASS wavelet_denoise_INDI(ushort(*image)[4], const int black,
                                const int iheight, const int iwidth,
                                const int height, const int width,
                                const int colors, const int shrink,
                                const float pre_mul[4], const float threshold,
                                const unsigned filters)
{
    float *fimg = 0, thold, mul[2], avg, diff;
    int size, lev, hpass, lpass, row, col, nc, c, i, wlast;
    ushort *window[4];
    static const float noise[] =
    { 0.8002, 0.2735, 0.1202, 0.0585, 0.0291, 0.0152, 0.0080, 0.0044 };

//  dcraw_message (dcraw, DCRAW_VERBOSE,_("Wavelet denoising...\n")); /*UF*/

    /* Scaling is done somewhere else - NKBJ*/
    size = iheight * iwidth;
    float temp[iheight + iwidth];
    if ((nc = colors) == 3 && filters) nc++;
    progress(PROGRESS_WAVELET_DENOISE, -nc * 5);
#ifdef _OPENMP
#if defined(__sun) && !defined(__GNUC__)	/* Fix bug #3205673 - NKBJ */
    #pragma omp parallel for				\
    shared(nc,image,size,noise)				\
    private(c,i,hpass,lev,lpass,row,col,thold,fimg,temp)
#else
    #pragma omp parallel for				\
    shared(nc,image,size)				\
    private(c,i,hpass,lev,lpass,row,col,thold,fimg,temp)
#endif
#endif
    FORC(nc) {			/* denoise R,G1,B,G3 individually */
        fimg = (float *) malloc(size * 3 * sizeof * fimg);
        for (i = 0; i < size; i++)
            fimg[i] = 256 * sqrt(image[i][c] /*<< scale*/);
        for (hpass = lev = 0; lev < 5; lev++) {
            progress(PROGRESS_WAVELET_DENOISE, 1);
            lpass = size * ((lev & 1) + 1);
            for (row = 0; row < iheight; row++) {
                hat_transform(temp, fimg + hpass + row * iwidth, 1, iwidth, 1 << lev);
                for (col = 0; col < iwidth; col++)
                    fimg[lpass + row * iwidth + col] = temp[col] * 0.25;
            }
            for (col = 0; col < iwidth; col++) {
                hat_transform(temp, fimg + lpass + col, iwidth, iheight, 1 << lev);
                for (row = 0; row < iheight; row++)
                    fimg[lpass + row * iwidth + col] = temp[row] * 0.25;
            }
            thold = threshold * noise[lev];
            for (i = 0; i < size; i++) {
                fimg[hpass + i] -= fimg[lpass + i];
                if	(fimg[hpass + i] < -thold) fimg[hpass + i] += thold;
                else if (fimg[hpass + i] >  thold) fimg[hpass + i] -= thold;
                else	 fimg[hpass + i] = 0;
                if (hpass) fimg[i] += fimg[hpass + i];
            }
            hpass = lpass;
        }
        for (i = 0; i < size; i++)
            image[i][c] = CLIP(SQR(fimg[i] + fimg[lpass + i]) / 0x10000);
        free(fimg);
    }
    if (filters && colors == 3) {  /* pull G1 and G3 closer together */
        for (row = 0; row < 2; row++)
            mul[row] = 0.125 * pre_mul[FC(row + 1, 0) | 1] / pre_mul[FC(row, 0) | 1];
        ushort window_mem[4][width];
        for (i = 0; i < 4; i++)
            window[i] = window_mem[i]; /*(ushort *) fimg + width*i;*/
        for (wlast = -1, row = 1; row < height - 1; row++) {
            while (wlast < row + 1) {
                for (wlast++, i = 0; i < 4; i++)
                    window[(i + 3) & 3] = window[i];
                for (col = FC(wlast, 1) & 1; col < width; col += 2)
                    window[2][col] = BAYER(wlast, col);
            }
            thold = threshold / 512;
            for (col = (FC(row, 0) & 1) + 1; col < width - 1; col += 2) {
                avg = (window[0][col - 1] + window[0][col + 1] +
                       window[2][col - 1] + window[2][col + 1] - black * 4)
                      * mul[row & 1] + (window[1][col] - black) * 0.5 + black;
                avg = avg < 0 ? 0 : sqrt(avg);
                diff = sqrt(BAYER(row, col)) - avg;
                if (diff < -thold) diff += thold;
                else if (diff >  thold) diff -= thold;
                else diff = 0;
                BAYER(row, col) = CLIP(SQR(avg + diff) + 0.5);
            }
        }
    }
}

void CLASS scale_colors_INDI(const int maximum, const int black,
                             const int use_camera_wb, const float cam_mul[4], const int colors,
                             float pre_mul[4], const unsigned filters, /*const*/ ushort white[8][8],
                             const char *ifname_display, void *dcraw)
{
    unsigned row, col, c, sum[8];
    int val;
    double dmin, dmax;

    if (use_camera_wb && cam_mul[0] != -1) {
        memset(sum, 0, sizeof sum);
        for (row = 0; row < 8; row++)
            for (col = 0; col < 8; col++) {
                c = FC(row, col);
                if ((val = white[row][col] - black) > 0)
                    sum[c] += val;
                sum[c + 4]++;
            }
        if (sum[0] && sum[1] && sum[2] && sum[3])
            FORC4 pre_mul[c] = (float) sum[c + 4] / sum[c];
        else if (cam_mul[0] && cam_mul[2])
            /* 'sizeof pre_mul' does not work because pre_mul is an argument (UF)*/
            memcpy(pre_mul, cam_mul, 4 * sizeof(float));
        else
            dcraw_message(dcraw, DCRAW_NO_CAMERA_WB,
                          _("%s: Cannot use camera white balance.\n"), ifname_display);
    } else {
        dcraw_message(dcraw, DCRAW_NO_CAMERA_WB,
                      _("%s: Cannot use camera white balance.\n"), ifname_display);
    }
    if (pre_mul[1] == 0) pre_mul[1] = 1;
    if (pre_mul[3] == 0) pre_mul[3] = colors < 4 ? pre_mul[1] : 1;
    for (dmin = DBL_MAX, dmax = c = 0; c < 4; c++) {
        if (dmin > pre_mul[c])
            dmin = pre_mul[c];
        if (dmax < pre_mul[c])
            dmax = pre_mul[c];
    }
    FORC4 pre_mul[c] /= dmax;
    dcraw_message(dcraw, DCRAW_VERBOSE,
                  _("Scaling with darkness %d, saturation %d, and\nmultipliers"),
                  black, maximum);
    FORC4 dcraw_message(dcraw, DCRAW_VERBOSE, " %f", pre_mul[c]);
    dcraw_message(dcraw, DCRAW_VERBOSE, "\n");

    /* The rest of the scaling is done somewhere else UF*/
}

void CLASS border_interpolate_INDI(const int height, const int width,
                                   ushort(*image)[4], const unsigned filters, int colors, int border, dcraw_data *h)
{
    int row, col, y, x, f, c, sum[8];

    for (row = 0; row < height; row++)
        for (col = 0; col < width; col++) {
            if (col == border && row >= border && row < height - border)
                col = width - border;
            memset(sum, 0, sizeof sum);
            for (y = row - 1; y != row + 2; y++)
                for (x = col - 1; x != col + 2; x++)
                    if (y >= 0 && y < height && x >= 0 && x < width) {
                        f = fcol_INDI(filters, y, x, h->top_margin, h->left_margin, h->xtrans);
                        sum[f] += image[y * width + x][f];
                        sum[f + 4]++;
                    }
            f = fcol_INDI(filters, row, col, h->top_margin, h->left_margin, h->xtrans);
            FORCC if (c != f && sum[c + 4])
                image[row * width + col][c] = sum[c] / sum[c + 4];
        }
}

void CLASS lin_interpolate_INDI(ushort(*image)[4], const unsigned filters,
                                const int width, const int height, const int colors, void *dcraw, dcraw_data *h) /*UF*/
{
    int code[16][16][32], size = 16, *ip, sum[4];
    int f, c, i, x, y, row, col, shift, color;
    ushort *pix;

    dcraw_message(dcraw, DCRAW_VERBOSE, _("Bilinear interpolation...\n")); /*UF*/
    if (filters == 9) size = 6;
    border_interpolate_INDI(height, width, image, filters, colors, 1, h);
    for (row = 0; row < size; row++) {
        for (col = 0; col < size; col++) {
            ip = code[row][col] + 1;
            f = fcol_INDI(filters, row, col, h->top_margin, h->left_margin, h->xtrans);
            memset(sum, 0, sizeof sum);
            for (y = -1; y <= 1; y++)
                for (x = -1; x <= 1; x++) {
                    shift = (y == 0) + (x == 0);
                    color = fcol_INDI(filters, row + y, col + x, h->top_margin, h->left_margin, h->xtrans);
                    if (color == f) continue;
                    *ip++ = (width * y + x) * 4 + color;
                    *ip++ = shift;
                    *ip++ = color;
                    sum[color] += 1 << shift;
                }
            code[row][col][0] = (ip - code[row][col]) / 3;
            FORCC
            if (c != f) {
                *ip++ = c;
                *ip++ = 256 / sum[c];
            }
        }
    }
#ifdef _OPENMP
    #pragma omp parallel for default(shared) private(row,col,pix,ip,sum,i)
#endif
    for (row = 1; row < height - 1; row++) {
        for (col = 1; col < width - 1; col++) {
            pix = image[row * width + col];
            ip = code[row % size][col % size];
            memset(sum, 0, sizeof sum);
            for (i = *ip++; i--; ip += 3)
                sum[ip[2]] += pix[ip[0]] << ip[1];
            for (i = colors; --i; ip += 2)
                pix[ip[0]] = sum[ip[0]] * ip[1] >> 8;
        }
    }
}

/*
   This algorithm is officially called:

   "Interpolation using a Threshold-based variable number of gradients"

   described in http://scien.stanford.edu/class/psych221/projects/99/tingchen/algodep/vargra.html

   I've extended the basic idea to work with non-Bayer filter arrays.
   Gradients are numbered clockwise from NW=0 to W=7.
 */
void CLASS vng_interpolate_INDI(ushort(*image)[4], const unsigned filters,
                                const int width, const int height, const int colors, void *dcraw, dcraw_data *h) /*UF*/
{
    static const signed char *cp, terms[] = {
        -2, -2, +0, -1, 0, 0x01, -2, -2, +0, +0, 1, 0x01, -2, -1, -1, +0, 0, 0x01,
        -2, -1, +0, -1, 0, 0x02, -2, -1, +0, +0, 0, 0x03, -2, -1, +0, +1, 1, 0x01,
        -2, +0, +0, -1, 0, 0x06, -2, +0, +0, +0, 1, 0x02, -2, +0, +0, +1, 0, 0x03,
        -2, +1, -1, +0, 0, 0x04, -2, +1, +0, -1, 1, 0x04, -2, +1, +0, +0, 0, 0x06,
        -2, +1, +0, +1, 0, 0x02, -2, +2, +0, +0, 1, 0x04, -2, +2, +0, +1, 0, 0x04,
        -1, -2, -1, +0, 0, 0x80, -1, -2, +0, -1, 0, 0x01, -1, -2, +1, -1, 0, 0x01,
        -1, -2, +1, +0, 1, 0x01, -1, -1, -1, +1, 0, 0x88, -1, -1, +1, -2, 0, 0x40,
        -1, -1, +1, -1, 0, 0x22, -1, -1, +1, +0, 0, 0x33, -1, -1, +1, +1, 1, 0x11,
        -1, +0, -1, +2, 0, 0x08, -1, +0, +0, -1, 0, 0x44, -1, +0, +0, +1, 0, 0x11,
        -1, +0, +1, -2, 1, 0x40, -1, +0, +1, -1, 0, 0x66, -1, +0, +1, +0, 1, 0x22,
        -1, +0, +1, +1, 0, 0x33, -1, +0, +1, +2, 1, 0x10, -1, +1, +1, -1, 1, 0x44,
        -1, +1, +1, +0, 0, 0x66, -1, +1, +1, +1, 0, 0x22, -1, +1, +1, +2, 0, 0x10,
        -1, +2, +0, +1, 0, 0x04, -1, +2, +1, +0, 1, 0x04, -1, +2, +1, +1, 0, 0x04,
        +0, -2, +0, +0, 1, 0x80, +0, -1, +0, +1, 1, 0x88, +0, -1, +1, -2, 0, 0x40,
        +0, -1, +1, +0, 0, 0x11, +0, -1, +2, -2, 0, 0x40, +0, -1, +2, -1, 0, 0x20,
        +0, -1, +2, +0, 0, 0x30, +0, -1, +2, +1, 1, 0x10, +0, +0, +0, +2, 1, 0x08,
        +0, +0, +2, -2, 1, 0x40, +0, +0, +2, -1, 0, 0x60, +0, +0, +2, +0, 1, 0x20,
        +0, +0, +2, +1, 0, 0x30, +0, +0, +2, +2, 1, 0x10, +0, +1, +1, +0, 0, 0x44,
        +0, +1, +1, +2, 0, 0x10, +0, +1, +2, -1, 1, 0x40, +0, +1, +2, +0, 0, 0x60,
        +0, +1, +2, +1, 0, 0x20, +0, +1, +2, +2, 0, 0x10, +1, -2, +1, +0, 0, 0x80,
        +1, -1, +1, +1, 0, 0x88, +1, +0, +1, +2, 0, 0x08, +1, +0, +2, -1, 0, 0x40,
        +1, +0, +2, +1, 0, 0x10
    }, chood[] = { -1, -1, -1, 0, -1, +1, 0, +1, +1, +1, +1, 0, +1, -1, 0, -1 };
    ushort(*brow[4])[4], *pix;
    int prow = 8, pcol = 2, *ip, *code[16][16], gval[8], gmin, gmax, sum[4];
    int row, col, x, y, x1, x2, y1, y2, t, weight, grads, color, diag;
    int g, diff, thold, num, c;
    ushort rowtmp[4][width * 4];

    lin_interpolate_INDI(image, filters, width, height, colors, dcraw, h); /*UF*/
    dcraw_message(dcraw, DCRAW_VERBOSE, _("VNG interpolation...\n")); /*UF*/

    if (filters == 1) prow = pcol = 16;
    if (filters == 9) prow = pcol =  6;
    int *ipalloc = ip = (int *) calloc(prow * pcol, 1280);
    merror(ip, "vng_interpolate()");
    for (row = 0; row < prow; row++)		/* Precalculate for VNG */
        for (col = 0; col < pcol; col++) {
            code[row][col] = ip;
            for (cp = terms, t = 0; t < 64; t++) {
                y1 = *cp++;
                x1 = *cp++;
                y2 = *cp++;
                x2 = *cp++;
                weight = *cp++;
                grads = *cp++;
                color = fcol_INDI(filters, row + y1, col + x1, h->top_margin, h->left_margin, h->xtrans);
                if (fcol_INDI(filters, row + y2, col + x2, h->top_margin, h->left_margin, h->xtrans) != color) continue;
                diag = (fcol_INDI(filters, row, col + 1, h->top_margin, h->left_margin, h->xtrans) == color && fcol_INDI(filters, row + 1, col, h->top_margin, h->left_margin, h->xtrans) == color) ? 2 : 1;
                if (abs(y1 - y2) == diag && abs(x1 - x2) == diag) continue;
                *ip++ = (y1 * width + x1) * 4 + color;
                *ip++ = (y2 * width + x2) * 4 + color;
                *ip++ = weight;
                for (g = 0; g < 8; g++)
                    if (grads & 1 << g) *ip++ = g;
                *ip++ = -1;
            }
            *ip++ = INT_MAX;
            for (cp = chood, g = 0; g < 8; g++) {
                y = *cp++;
                x = *cp++;
                *ip++ = (y * width + x) * 4;
                color = fcol_INDI(filters, row, col, h->top_margin, h->left_margin, h->xtrans);
                if (fcol_INDI(filters, row + y, col + x, h->top_margin, h->left_margin, h->xtrans) != color && fcol_INDI(filters, row + y * 2, col + x * 2, h->top_margin, h->left_margin, h->xtrans) == color)
                    *ip++ = (y * width + x) * 8 + color;
                else
                    *ip++ = 0;
            }
        }
    progress(PROGRESS_INTERPOLATE, -height);
#ifdef _OPENMP
    #pragma omp parallel				\
    shared(image,code,prow,pcol,h)			\
    private(row,col,g,brow,rowtmp,pix,ip,gval,diff,gmin,gmax,thold,sum,color,num,c,t)
#endif
    {
        int slice = (height - 4) / uf_omp_get_num_threads();
        int start_row = 2 + slice * uf_omp_get_thread_num();
        int end_row = MIN(start_row + slice, height - 2);
        for (row = start_row; row < end_row; row++) { /* Do VNG interpolation */
            progress(PROGRESS_INTERPOLATE, 1);
            for (g = 0; g < 4; g++)
                brow[g] = &rowtmp[(row + g - 2) % 4];
            for (col = 2; col < width - 2; col++) {
                pix = image[row * width + col];
                ip = code[row % prow][col % pcol];
                memset(gval, 0, sizeof gval);
                while ((g = ip[0]) != INT_MAX) { /* Calculate gradients */
                    diff = ABS(pix[g] - pix[ip[1]]) << ip[2];
                    gval[ip[3]] += diff;
                    ip += 5;
                    if ((g = ip[-1]) == -1) continue;
                    gval[g] += diff;
                    while ((g = *ip++) != -1)
                        gval[g] += diff;
                }
                ip++;
                gmin = gmax = gval[0]; /* Choose a threshold */
                for (g = 1; g < 8; g++) {
                    if (gmin > gval[g]) gmin = gval[g];
                    if (gmax < gval[g]) gmax = gval[g];
                }
                if (gmax == 0) {
                    memcpy(brow[2][col], pix, sizeof * image);
                    continue;
                }
                thold = gmin + (gmax >> 1);
                memset(sum, 0, sizeof sum);
                color = fcol_INDI(filters, row, col, h->top_margin, h->left_margin, h->xtrans);
                for (num = g = 0; g < 8; g++, ip += 2) { /* Average the neighbors */
                    if (gval[g] <= thold) {
                        FORCC
                        if (c == color && ip[1])
                            sum[c] += (pix[c] + pix[ip[1]]) >> 1;
                        else
                            sum[c] += pix[ip[0] + c];
                        num++;
                    }
                }
                FORCC {				/* Save to buffer */
                    t = pix[color];
                    if (c != color)
                        t += (sum[c] - sum[color]) / num;
                    brow[2][col][c] = CLIP(t);
                }
            }
            /* Write buffer to image */
            if ((row > start_row + 1) || (row == height - 2))
                memcpy(image[(row - 2)*width + 2], brow[0] + 2, (width - 4)*sizeof * image);
            if (row == height - 2) {
                memcpy(image[(row - 1)*width + 2], brow[1] + 2, (width - 4)*sizeof * image);
                break;
            }
        }
    }
    free(ipalloc);
}

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
void CLASS ppg_interpolate_INDI(ushort(*image)[4], const unsigned filters,
                                const int width, const int height,
                                const int colors, void *dcraw, dcraw_data *h)
{
    int dir[5] = { 1, width, -1, -width, 1 };
    int row, col, diff[2] = { 0, 0 }, guess[2], c, d, i;
    ushort(*pix)[4];

    border_interpolate_INDI(height, width, image, filters, colors, 3, h);
    dcraw_message(dcraw, DCRAW_VERBOSE, _("PPG interpolation...\n")); /*UF*/

#ifdef _OPENMP
    #pragma omp parallel				\
    shared(image,dir,diff)				\
    private(row,col,i,d,c,pix,guess)
#endif
    {
        /*  Fill in the green layer with gradients and pattern recognition: */
#ifdef _OPENMP
        #pragma omp for
#endif
        for (row = 3; row < height - 3; row++) {
            for (col = 3 + (FC(row, 3) & 1), c = FC(row, col); col < width - 3; col += 2) {
                pix = image + row * width + col;
                for (i = 0; (d = dir[i]) > 0; i++) {
                    guess[i] = (pix[-d][1] + pix[0][c] + pix[d][1]) * 2
                               - pix[-2 * d][c] - pix[2 * d][c];
                    diff[i] = (ABS(pix[-2 * d][c] - pix[ 0][c]) +
                               ABS(pix[ 2 * d][c] - pix[ 0][c]) +
                               ABS(pix[  -d][1] - pix[ d][1])) * 3 +
                              (ABS(pix[ 3 * d][1] - pix[ d][1]) +
                               ABS(pix[-3 * d][1] - pix[-d][1])) * 2;
                }
                d = dir[i = diff[0] > diff[1]];
                pix[0][1] = ULIM(guess[i] >> 2, pix[d][1], pix[-d][1]);
            }
        }
        /*  Calculate red and blue for each green pixel: */
#ifdef _OPENMP
        #pragma omp for
#endif
        for (row = 1; row < height - 1; row++) {
            for (col = 1 + (FC(row, 2) & 1), c = FC(row, col + 1); col < width - 1; col += 2) {
                pix = image + row * width + col;
                for (i = 0; (d = dir[i]) > 0; c = 2 - c, i++)
                    pix[0][c] = CLIP((pix[-d][c] + pix[d][c] + 2 * pix[0][1]
                                      - pix[-d][1] - pix[d][1]) >> 1);
            }
        }
        /*  Calculate blue for red pixels and vice versa: */
#ifdef _OPENMP
        #pragma omp for
#endif
        for (row = 1; row < height - 1; row++) {
            for (col = 1 + (FC(row, 1) & 1), c = 2 - FC(row, col); col < width - 1; col += 2) {
                pix = image + row * width + col;
                for (i = 0; (d = dir[i] + dir[i + 1]) > 0; i++) {
                    diff[i] = ABS(pix[-d][c] - pix[d][c]) +
                              ABS(pix[-d][1] - pix[0][1]) +
                              ABS(pix[ d][1] - pix[0][1]);
                    guess[i] = pix[-d][c] + pix[d][c] + 2 * pix[0][1]
                               - pix[-d][1] - pix[d][1];
                }
                if (diff[0] != diff[1])
                    pix[0][c] = CLIP(guess[diff[0] > diff[1]] >> 1);
                else
                    pix[0][c] = CLIP((guess[0] + guess[1]) >> 2);
            }
        }
    }
}

void CLASS cielab_INDI(ushort rgb[3], short lab[3], const int colors,
                       const float rgb_cam[3][4])
{
    int c, i, j, k;
    float r, xyz[3];
    static float cbrt[0x10000], xyz_cam[3][4];

    if (!rgb) {
        for (i = 0; i < 0x10000; i++) {
            r = i / 65535.0;
            cbrt[i] = r > 0.008856 ? pow(r, (float)(1 / 3.0)) : 7.787 * r + 16 / 116.0;
        }
        for (i = 0; i < 3; i++)
            for (j = 0; j < colors; j++)
                for (xyz_cam[i][j] = k = 0; k < 3; k++)
                    xyz_cam[i][j] += xyz_rgb[i][k] * rgb_cam[k][j] / d65_white[i];
        return;
    }
    xyz[0] = xyz[1] = xyz[2] = 0.5;
    FORCC {
        xyz[0] += xyz_cam[0][c] * rgb[c];
        xyz[1] += xyz_cam[1][c] * rgb[c];
        xyz[2] += xyz_cam[2][c] * rgb[c];
    }
    xyz[0] = cbrt[CLIP((int) xyz[0])];
    xyz[1] = cbrt[CLIP((int) xyz[1])];
    xyz[2] = cbrt[CLIP((int) xyz[2])];
    lab[0] = 64 * (116 * xyz[1] - 16);
    lab[1] = 64 * 500 * (xyz[0] - xyz[1]);
    lab[2] = 64 * 200 * (xyz[1] - xyz[2]);
}

#define TS 512		/* Tile Size */
/*
   Frank Markesteijn's algorithm for Fuji X-Trans sensors
 */
void CLASS xtrans_interpolate_INDI(ushort(*image)[4], const unsigned filters,
                                   const int width, const int height,
                                   const int colors, const float rgb_cam[3][4],
                                   void *dcraw, dcraw_data *hh, const int passes)
{
    int c, d, f, g, h, i, v, ng, row, col, top, left, mrow, mcol;
    int val, ndir, pass, hm[8], avg[4], color[3][8];
    static const short orth[12] = { 1, 0, 0, 1, -1, 0, 0, -1, 1, 0, 0, 1 },
    patt[2][16] = { { 0, 1, 0, -1, 2, 0, -1, 0, 1, 1, 1, -1, 0, 0, 0, 0 },
        { 0, 1, 0, -2, 1, 0, -2, 0, 1, 1, -2, -2, 1, -1, -1, 1 }
    },
    dir[4] = { 1, TS, TS + 1, TS - 1 };
    short allhex[3][3][2][8], *hex;
    ushort min, max, sgrow = 0, sgcol = 0;
    ushort(*rgb)[TS][TS][3], (*rix)[3], (*pix)[4];
    short(*lab)    [TS][3], (*lix)[3];
    float(*drv)[TS][TS], diff[6], tr;
    char(*homo)[TS][TS], *buffer;

    dcraw_message(dcraw, DCRAW_VERBOSE, _("%d-pass X-Trans interpolation...\n"), passes); /*NKBJ*/

    cielab_INDI(0, 0, colors, rgb_cam);
    ndir = 4 << (passes > 1);

    /* Map a green hexagon around each non-green pixel and vice versa:      */
    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++)
            for (ng = d = 0; d < 10; d += 2) {
                g = fcol_INDI(filters, row, col, hh->top_margin, hh->left_margin, hh->xtrans) == 1;
                if (fcol_INDI(filters, row + orth[d], col + orth[d + 2], hh->top_margin, hh->left_margin, hh->xtrans) == 1) ng = 0;
                else ng++;
                if (ng == 4) {
                    sgrow = row;
                    sgcol = col;
                }
                if (ng == g + 1) FORC(8) {
                    v = orth[d  ] * patt[g][c * 2] + orth[d + 1] * patt[g][c * 2 + 1];
                    h = orth[d + 2] * patt[g][c * 2] + orth[d + 3] * patt[g][c * 2 + 1];
                    allhex[row][col][0][c ^ (g * 2 & d)] = h + v * width;
                    allhex[row][col][1][c ^ (g * 2 & d)] = h + v * TS;
                }
            }

    /* Set green1 and green3 to the minimum and maximum allowed values:     */
    for (row = 2; row < height - 2; row++)
        for (min = ~(max = 0), col = 2; col < width - 2; col++) {
            if (fcol_INDI(filters, row, col, hh->top_margin, hh->left_margin, hh->xtrans) == 1 && (min = ~(max = 0))) continue;
            pix = image + row * width + col;
            hex = allhex[row % 3][col % 3][0];
            if (!max) FORC(6) {
                val = pix[hex[c]][1];
                if (min > val) min = val;
                if (max < val) max = val;
            }
            pix[0][1] = min;
            pix[0][3] = max;
            switch ((row - sgrow) % 3) {
                case 1:
                    if (row < height - 3) {
                        row++;
                        col--;
                    }
                    break;
                case 2:
                    if ((min = ~(max = 0)) && (col += 2) < width - 3 && row > 2) row--;
            }
        }


#ifdef _OPENMP
    #pragma omp parallel				\
    default(shared)					\
    private(top, left, row, col, pix, mrow, mcol, hex, color, c, pass, rix, val, d, f, g, h, i, diff, lix, tr, avg, v, buffer, rgb, lab, drv, homo, hm, max)
#endif
    {
        buffer = (char *) malloc(TS * TS * (ndir * 11 + 6));
        merror(buffer, "xtrans_interpolate()");
        rgb  = (ushort(*)[TS][TS][3]) buffer;
        lab  = (short(*)    [TS][3])(buffer + TS * TS * (ndir * 6));
        drv  = (float(*)[TS][TS])(buffer + TS * TS * (ndir * 6 + 6));
        homo = (char(*)[TS][TS])(buffer + TS * TS * (ndir * 10 + 6));

        progress(PROGRESS_INTERPOLATE, -height);

#ifdef _OPENMP
        #pragma omp for
#endif

        for (top = 3; top < height - 19; top += TS - 16) {
            progress(PROGRESS_INTERPOLATE, TS - 16);
            for (left = 3; left < width - 19; left += TS - 16) {
                mrow = MIN(top + TS, height - 3);
                mcol = MIN(left + TS, width - 3);
                for (row = top; row < mrow; row++)
                    for (col = left; col < mcol; col++)
                        memcpy(rgb[0][row - top][col - left], image[row * width + col], 6);
                FORC3 memcpy(rgb[c + 1], rgb[0], sizeof * rgb);

                /* Interpolate green horizontally, vertically, and along both diagonals: */
                for (row = top; row < mrow; row++)
                    for (col = left; col < mcol; col++) {
                        if ((f = fcol_INDI(filters, row, col, hh->top_margin, hh->left_margin, hh->xtrans)) == 1) continue;
                        pix = image + row * width + col;
                        hex = allhex[row % 3][col % 3][0];
                        color[1][0] = 174 * (pix[  hex[1]][1] + pix[  hex[0]][1]) -
                                      46 * (pix[2 * hex[1]][1] + pix[2 * hex[0]][1]);
                        color[1][1] = 223 *  pix[  hex[3]][1] + pix[  hex[2]][1] * 33 +
                                      92 * (pix[      0 ][f] - pix[ -hex[2]][f]);
                        FORC(2) color[1][2 + c] =
                            164 * pix[hex[4 + c]][1] + 92 * pix[-2 * hex[4 + c]][1] + 33 *
                            (2 * pix[0][f] - pix[3 * hex[4 + c]][f] - pix[-3 * hex[4 + c]][f]);
                        FORC4 rgb[c ^ !((row - sgrow) % 3)][row - top][col - left][1] =
                            LIM(color[1][c] >> 8, pix[0][1], pix[0][3]);
                    }

                for (pass = 0; pass < passes; pass++) {
                    if (pass == 1)
                        memcpy(rgb += 4, buffer, 4 * sizeof * rgb);

                    /* Recalculate green from interpolated values of closer pixels: */
                    if (pass) {
                        for (row = top + 2; row < mrow - 2; row++)
                            for (col = left + 2; col < mcol - 2; col++) {
                                if ((f = fcol_INDI(filters, row, col, hh->top_margin, hh->left_margin, hh->xtrans)) == 1) continue;
                                pix = image + row * width + col;
                                hex = allhex[row % 3][col % 3][1];
                                for (d = 3; d < 6; d++) {
                                    rix = &rgb[(d - 2) ^ !((row - sgrow) % 3)][row - top][col - left];
                                    val = rix[-2 * hex[d]][1] + 2 * rix[hex[d]][1]
                                          - rix[-2 * hex[d]][f] - 2 * rix[hex[d]][f] + 3 * rix[0][f];
                                    rix[0][1] = LIM(val / 3, pix[0][1], pix[0][3]);
                                }
                            }
                    }

                    /* Interpolate red and blue values for solitary green pixels:   */
                    for (row = (top - sgrow + 4) / 3 * 3 + sgrow; row < mrow - 2; row += 3)
                        for (col = (left - sgcol + 4) / 3 * 3 + sgcol; col < mcol - 2; col += 3) {
                            rix = &rgb[0][row - top][col - left];
                            h = fcol_INDI(filters, row, col + 1, hh->top_margin, hh->left_margin, hh->xtrans);
                            memset(diff, 0, sizeof diff);
                            for (i = 1, d = 0; d < 6; d++, i ^= TS ^ 1, h ^= 2) {
                                for (c = 0; c < 2; c++, h ^= 2) {
                                    g = 2 * rix[0][1] - rix[i << c][1] - rix[-i << c][1];
                                    color[h][d] = g + rix[i << c][h] + rix[-i << c][h];
                                    if (d > 1)
                                        diff[d] += SQR(rix[i << c][1] - rix[-i << c][1]
                                                       - rix[i << c][h] + rix[-i << c][h]) + SQR(g);
                                }
                                if (d > 1 && (d & 1))
                                    if (diff[d - 1] < diff[d])
                                        FORC(2) color[c * 2][d] = color[c * 2][d - 1];
                                if (d < 2 || (d & 1)) {
                                    FORC(2) rix[0][c * 2] = CLIP(color[c * 2][d] / 2);
                                    rix += TS * TS;
                                }
                            }
                        }

                    /* Interpolate red for blue pixels and vice versa:              */
                    for (row = top + 3; row < mrow - 3; row++)
                        for (col = left + 3; col < mcol - 3; col++) {
                            if ((f = 2 - fcol_INDI(filters, row, col, hh->top_margin, hh->left_margin, hh->xtrans)) == 1) continue;
                            rix = &rgb[0][row - top][col - left];
                            c = (row - sgrow) % 3 ? TS : 1;
                            h = 3 * (c ^ TS ^ 1);
                            for (d = 0; d < 4; d++, rix += TS * TS) {
                                i = d > 1 || ((d ^ c) & 1) ||
                                    ((ABS(rix[0][1] - rix[c][1]) + ABS(rix[0][1] - rix[-c][1])) <
                                     2 * (ABS(rix[0][1] - rix[h][1]) + ABS(rix[0][1] - rix[-h][1]))) ? c : h;
                                rix[0][f] = CLIP((rix[i][f] + rix[-i][f] +
                                                  2 * rix[0][1] - rix[i][1] - rix[-i][1]) / 2);
                            }
                        }

                    /* Fill in red and blue for 2x2 blocks of green:                */
                    for (row = top + 2; row < mrow - 2; row++) if ((row - sgrow) % 3)
                            for (col = left + 2; col < mcol - 2; col++) if ((col - sgcol) % 3) {
                                    rix = &rgb[0][row - top][col - left];
                                    hex = allhex[row % 3][col % 3][1];
                                    for (d = 0; d < ndir; d += 2, rix += TS * TS)
                                        if (hex[d] + hex[d + 1]) {
                                            g = 3 * rix[0][1] - 2 * rix[hex[d]][1] - rix[hex[d + 1]][1];
                                            for (c = 0; c < 4; c += 2) rix[0][c] =
                                                    CLIP((g + 2 * rix[hex[d]][c] + rix[hex[d + 1]][c]) / 3);
                                        } else {
                                            g = 2 * rix[0][1] - rix[hex[d]][1] - rix[hex[d + 1]][1];
                                            for (c = 0; c < 4; c += 2) rix[0][c] =
                                                    CLIP((g + rix[hex[d]][c] + rix[hex[d + 1]][c]) / 2);
                                        }
                                }
                }
                rgb = (ushort(*)[TS][TS][3]) buffer;
                mrow -= top;
                mcol -= left;

                /* Convert to CIELab and differentiate in all directions:       */
                for (d = 0; d < ndir; d++) {
                    for (row = 2; row < mrow - 2; row++)
                        for (col = 2; col < mcol - 2; col++)
                            cielab_INDI(rgb[d][row][col], lab[row][col], colors, rgb_cam);
                    for (f = dir[d & 3], row = 3; row < mrow - 3; row++)
                        for (col = 3; col < mcol - 3; col++) {
                            lix = &lab[row][col];
                            g = 2 * lix[0][0] - lix[f][0] - lix[-f][0];
                            drv[d][row][col] = SQR(g)
                                               + SQR((2 * lix[0][1] - lix[f][1] - lix[-f][1] + g * 500 / 232))
                                               + SQR((2 * lix[0][2] - lix[f][2] - lix[-f][2] - g * 500 / 580));
                        }
                }

                /* Build homogeneity maps from the derivatives:                 */
                memset(homo, 0, ndir * TS * TS);
                for (row = 4; row < mrow - 4; row++)
                    for (col = 4; col < mcol - 4; col++) {
                        for (tr = FLT_MAX, d = 0; d < ndir; d++)
                            if (tr > drv[d][row][col])
                                tr = drv[d][row][col];
                        tr *= 8;
                        for (d = 0; d < ndir; d++)
                            for (v = -1; v <= 1; v++)
                                for (h = -1; h <= 1; h++)
                                    if (drv[d][row + v][col + h] <= tr)
                                        homo[d][row][col]++;
                    }

                /* Average the most homogenous pixels for the final result:     */
                if (height - top < TS + 4) mrow = height - top + 2;
                if (width - left < TS + 4) mcol = width - left + 2;
                for (row = MIN(top, 8); row < mrow - 8; row++)
                    for (col = MIN(left, 8); col < mcol - 8; col++) {
                        for (d = 0; d < ndir; d++)
                            for (hm[d] = 0, v = -2; v <= 2; v++)
                                for (h = -2; h <= 2; h++)
                                    hm[d] += homo[d][row + v][col + h];
                        for (d = 0; d < ndir - 4; d++)
                            if (hm[d] < hm[d + 4]) hm[d  ] = 0;
                            else if (hm[d] > hm[d + 4]) hm[d + 4] = 0;
                        for (max = hm[0], d = 1; d < ndir; d++)
                            if (max < hm[d]) max = hm[d];
                        max -= max >> 3;
                        memset(avg, 0, sizeof avg);
                        for (d = 0; d < ndir; d++)
                            if (hm[d] >= max) {
                                FORC3 avg[c] += rgb[d][row][col][c];
                                avg[3]++;
                            }
                        FORC3 image[(row + top)*width + col + left][c] = avg[c] / avg[3];
                    }
            }
        }
        free(buffer);
    } /* _OPENMP */
    border_interpolate_INDI(height, width, image, filters, colors, 8, hh);
}

/*
   Adaptive Homogeneity-Directed interpolation is based on
   the work of Keigo Hirakawa, Thomas Parks, and Paul Lee.
 */
void CLASS ahd_interpolate_INDI(ushort(*image)[4], const unsigned filters,
                                const int width, const int height,
                                const int colors, const float rgb_cam[3][4],
                                void *dcraw, dcraw_data *h)
{
    int i, j, top, left, row, col, tr, tc, c, d, val, hm[2];
    static const int dir[4] = { -1, 1, -TS, TS };
    unsigned ldiff[2][4], abdiff[2][4], leps, abeps;
    ushort(*rgb)[TS][TS][3], (*rix)[3], (*pix)[4];
    short(*lab)[TS][TS][3], (*lix)[3];
    char(*homo)[TS][TS], *buffer;

    dcraw_message(dcraw, DCRAW_VERBOSE, _("AHD interpolation...\n")); /*UF*/

#ifdef _OPENMP
    #pragma omp parallel				\
    default(shared)					\
    private(top, left, row, col, pix, rix, lix, c, val, d, tc, tr, i, j, ldiff, abdiff, leps, abeps, hm, buffer, rgb, lab, homo)
#endif
    {
        cielab_INDI(0, 0, colors, rgb_cam);
        border_interpolate_INDI(height, width, image, filters, colors, 5, h);
        buffer = (char *) malloc(26 * TS * TS);
        merror(buffer, "ahd_interpolate()");
        rgb  = (ushort(*)[TS][TS][3]) buffer;
        lab  = (short(*)[TS][TS][3])(buffer + 12 * TS * TS);
        homo = (char(*)[TS][TS])(buffer + 24 * TS * TS);

        progress(PROGRESS_INTERPOLATE, -height);
#ifdef _OPENMP
        #pragma omp for
#endif
        for (top = 2; top < height - 5; top += TS - 6) {
            progress(PROGRESS_INTERPOLATE, TS - 6);
            for (left = 2; left < width - 5; left += TS - 6) {

                /*  Interpolate green horizontally and vertically: */
                for (row = top; row < top + TS && row < height - 2; row++) {
                    col = left + (FC(row, left) & 1);
                    for (c = FC(row, col); col < left + TS && col < width - 2; col += 2) {
                        pix = image + row * width + col;
                        val = ((pix[-1][1] + pix[0][c] + pix[1][1]) * 2
                               - pix[-2][c] - pix[2][c]) >> 2;
                        rgb[0][row - top][col - left][1] = ULIM(val, pix[-1][1], pix[1][1]);
                        val = ((pix[-width][1] + pix[0][c] + pix[width][1]) * 2
                               - pix[-2 * width][c] - pix[2 * width][c]) >> 2;
                        rgb[1][row - top][col - left][1] = ULIM(val, pix[-width][1], pix[width][1]);
                    }
                }
                /*  Interpolate red and blue, and convert to CIELab: */
                for (d = 0; d < 2; d++)
                    for (row = top + 1; row < top + TS - 1 && row < height - 3; row++)
                        for (col = left + 1; col < left + TS - 1 && col < width - 3; col++) {
                            pix = image + row * width + col;
                            rix = &rgb[d][row - top][col - left];
                            lix = &lab[d][row - top][col - left];
                            if ((c = 2 - FC(row, col)) == 1) {
                                c = FC(row + 1, col);
                                val = pix[0][1] + ((pix[-1][2 - c] + pix[1][2 - c]
                                                    - rix[-1][1] - rix[1][1]) >> 1);
                                rix[0][2 - c] = CLIP(val);
                                val = pix[0][1] + ((pix[-width][c] + pix[width][c]
                                                    - rix[-TS][1] - rix[TS][1]) >> 1);
                            } else
                                val = rix[0][1] + ((pix[-width - 1][c] + pix[-width + 1][c]
                                                    + pix[+width - 1][c] + pix[+width + 1][c]
                                                    - rix[-TS - 1][1] - rix[-TS + 1][1]
                                                    - rix[+TS - 1][1] - rix[+TS + 1][1] + 1) >> 2);
                            rix[0][c] = CLIP(val);
                            c = FC(row, col);
                            rix[0][c] = pix[0][c];
                            cielab_INDI(rix[0], lix[0], colors, rgb_cam);
                        }
                /*  Build homogeneity maps from the CIELab images: */
                memset(homo, 0, 2 * TS * TS);
                for (row = top + 2; row < top + TS - 2 && row < height - 4; row++) {
                    tr = row - top;
                    for (col = left + 2; col < left + TS - 2 && col < width - 4; col++) {
                        tc = col - left;
                        for (d = 0; d < 2; d++) {
                            lix = &lab[d][tr][tc];
                            for (i = 0; i < 4; i++) {
                                ldiff[d][i] = ABS(lix[0][0] - lix[dir[i]][0]);
                                abdiff[d][i] = SQR(lix[0][1] - lix[dir[i]][1])
                                               + SQR(lix[0][2] - lix[dir[i]][2]);
                            }
                        }
                        leps = MIN(MAX(ldiff[0][0], ldiff[0][1]),
                                   MAX(ldiff[1][2], ldiff[1][3]));
                        abeps = MIN(MAX(abdiff[0][0], abdiff[0][1]),
                                    MAX(abdiff[1][2], abdiff[1][3]));
                        for (d = 0; d < 2; d++)
                            for (i = 0; i < 4; i++)
                                if (ldiff[d][i] <= leps && abdiff[d][i] <= abeps)
                                    homo[d][tr][tc]++;
                    }
                }
                /*  Combine the most homogenous pixels for the final result: */
                for (row = top + 3; row < top + TS - 3 && row < height - 5; row++) {
                    tr = row - top;
                    for (col = left + 3; col < left + TS - 3 && col < width - 5; col++) {
                        tc = col - left;
                        for (d = 0; d < 2; d++)
                            for (hm[d] = 0, i = tr - 1; i <= tr + 1; i++)
                                for (j = tc - 1; j <= tc + 1; j++)
                                    hm[d] += homo[d][i][j];
                        if (hm[0] != hm[1])
                            FORC3 image[row * width + col][c] = rgb[hm[1] > hm[0]][tr][tc][c];
                        else
                            FORC3 image[row * width + col][c] =
                                (rgb[0][tr][tc][c] + rgb[1][tr][tc][c]) >> 1;
                    }
                }
            }
        }
        free(buffer);
    } /* _OPENMP */
}
#undef TS


#define DTOP(x) ((x>65535) ? (unsigned short)65535 : (x<0) ? (unsigned short)0 : (unsigned short) x)

/*
 * MG - This comment applies to the 3x3 optimized median function
 *
 * The following routines have been built from knowledge gathered
 * around the Web. I am not aware of any copyright problem with
 * them, so use it as you want.
 * N. Devillard - 1998
 */

#define PIX_SORT(a,b) { if ((a)>(b)) PIX_SWAP((a),(b)); }
#define PIX_SWAP(a,b) { int temp=(a);(a)=(b);(b)=temp; }

static inline int median9(int *p)
{
    PIX_SORT(p[1], p[2]) ;
    PIX_SORT(p[4], p[5]) ;
    PIX_SORT(p[7], p[8]) ;
    PIX_SORT(p[0], p[1]) ;
    PIX_SORT(p[3], p[4]) ;
    PIX_SORT(p[6], p[7]) ;
    PIX_SORT(p[1], p[2]) ;
    PIX_SORT(p[4], p[5]) ;
    PIX_SORT(p[7], p[8]) ;
    PIX_SORT(p[0], p[3]) ;
    PIX_SORT(p[5], p[8]) ;
    PIX_SORT(p[4], p[7]) ;
    PIX_SORT(p[3], p[6]) ;
    PIX_SORT(p[1], p[4]) ;
    PIX_SORT(p[2], p[5]) ;
    PIX_SORT(p[4], p[7]) ;
    PIX_SORT(p[4], p[2]) ;
    PIX_SORT(p[6], p[4]) ;
    PIX_SORT(p[4], p[2]) ;
    return (p[4]) ;
}

#undef PIX_SWAP
#undef PIX_SORT

// Just making this function inline speeds up ahd_interpolate_INDI() up to 15%
static inline ushort eahd_median(int row, int col, int color,
                                 ushort(*image)[4], const int width)
{
    //declare the pixel array
    int pArray[9];
    int result;

    //perform the median filter (this only works for red or blue)
    //  result = median(R-G)+G or median(B-G)+G
    //
    // to perform the filter on green, it needs to be the average
    //  results = (median(G-R)+median(G-B)+R+B)/2

    //no checks are done here to speed up the inlining
    pArray[0] = image[width * (row) + col + 1][color] - image[width * (row) + col + 1][1];
    pArray[1] = image[width * (row - 1) + col + 1][color] - image[width * (row - 1) + col + 1][1];
    pArray[2] = image[width * (row - 1) + col  ][color] - image[width * (row - 1) + col  ][1];
    pArray[3] = image[width * (row - 1) + col - 1][color] - image[width * (row - 1) + col - 1][1];
    pArray[4] = image[width * (row) + col - 1][color] - image[width * (row) + col - 1][1];
    pArray[5] = image[width * (row + 1) + col - 1][color] - image[width * (row + 1) + col - 1][1];
    pArray[6] = image[width * (row + 1) + col  ][color] - image[width * (row + 1) + col  ][1];
    pArray[7] = image[width * (row + 1) + col + 1][color] - image[width * (row + 1) + col + 1][1];
    pArray[8] = image[width * (row) + col  ][color] - image[width * (row) + col  ][1];

    median9(pArray);
    result = pArray[4] + image[width * (row) + col  ][1];
    return DTOP(result);

}

// Add the color smoothing from Kimmel as suggested in the AHD paper
// Algorithm updated by Michael Goertz
void CLASS color_smooth(ushort(*image)[4], const int width, const int height,
                        const int passes)
{
    int row, col;
    int row_start = 2;
    int row_stop  = height - 2;
    int col_start = 2;
    int col_stop  = width - 2;
    //interate through all the colors
    int count;

    ushort *mpix;

    for (count = 0; count < passes; count++) {
        //perform 3 iterations - seems to be a commonly settled upon number of iterations
#ifdef _OPENMP
        #pragma omp parallel for default(shared) private(row,col,mpix)
#endif
        for (row = row_start; row < row_stop; row++) {
            for (col = col_start; col < col_stop; col++) {
                //calculate the median only over the red and blue
                //calculating over green seems to offer very little additional quality
                mpix = image[row * width + col];
                mpix[0] = eahd_median(row, col, 0, image, width);
                mpix[2] = eahd_median(row, col, 2, image, width);
            }
        }
    }
}

void CLASS fuji_rotate_INDI(ushort(**image_p)[4], int *height_p,
                            int *width_p, int *fuji_width_p, const int colors,
                            const double step, void *dcraw)
{
    int height = *height_p, width = *width_p, fuji_width = *fuji_width_p; /*UF*/
    ushort(*image)[4] = *image_p;  /*UF*/
    int i, row, col;
    float r, c, fr, fc;
    int ur, uc;
    ushort wide, high, (*img)[4], (*pix)[4];

    if (!fuji_width) return;
    dcraw_message(dcraw, DCRAW_VERBOSE, _("Rotating image 45 degrees...\n"));
    fuji_width = (fuji_width - 1/* + shrink*/)/* >> shrink*/;
    wide = fuji_width / step;
    high = (height - fuji_width) / step;
    img = (ushort(*)[4]) calloc(wide * high, sizeof * img);
    merror(img, "fuji_rotate()");

#ifdef _OPENMP
    #pragma omp parallel for default(shared) private(row,col,ur,uc,r,c,fr,fc,pix,i)
#endif
    for (row = 0; row < high; row++) {
        for (col = 0; col < wide; col++) {
            ur = r = fuji_width + (row - col) * step;
            uc = c = (row + col) * step;
            if (ur > height - 2 || uc > width - 2) continue;
            fr = r - ur;
            fc = c - uc;
            pix = image + ur * width + uc;
            for (i = 0; i < colors; i++)
                img[row * wide + col][i] =
                    (pix[    0][i] * (1 - fc) + pix[      1][i] * fc) * (1 - fr) +
                    (pix[width][i] * (1 - fc) + pix[width + 1][i] * fc) * fr;
        }
    }
    free(image);
    width  = wide;
    height = high;
    image  = img;
    fuji_width = 0;
    *height_p = height; /* INDI - UF*/
    *width_p = width;
    *fuji_width_p = fuji_width;
    *image_p = image;
}

void CLASS flip_image_INDI(ushort(*image)[4], int *height_p, int *width_p,
                           /*const*/ int flip) /*UF*/
{
    unsigned *flag;
    int size, base, dest, next, row, col;
    gint64 *img, hold;
    int height = *height_p, width = *width_p;/* INDI - UF*/

//  Message is suppressed because error handling is not enabled here.
//  dcraw_message (dcraw, DCRAW_VERBOSE,_("Flipping image %c:%c:%c...\n"),
//      flip & 1 ? 'H':'0', flip & 2 ? 'V':'0', flip & 4 ? 'T':'0'); /*UF*/

    img = (gint64 *) image;
    size = height * width;
    flag = calloc((size + 31) >> 5, sizeof * flag);
    merror(flag, "flip_image()");
    for (base = 0; base < size; base++) {
        if (flag[base >> 5] & (1 << (base & 31)))
            continue;
        dest = base;
        hold = img[base];
        while (1) {
            if (flip & 4) {
                row = dest % height;
                col = dest / height;
            } else {
                row = dest / width;
                col = dest % width;
            }
            if (flip & 2)
                row = height - 1 - row;
            if (flip & 1)
                col = width - 1 - col;
            next = row * width + col;
            if (next == base) break;
            flag[next >> 5] |= 1 << (next & 31);
            img[dest] = img[next];
            dest = next;
        }
        img[dest] = hold;
    }
    free(flag);
    if (flip & 4) SWAP(height, width);
    *height_p = height; /* INDI - UF*/
    *width_p = width;
}

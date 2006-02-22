/*
   dcraw_api.c - an API for dcraw
   by udi Fuchs,

   based on dcraw by Dave Coffin
   http://www.cybercom.net/~dcoffin/

   UFRaw is licensed under the GNU General Public License.
   It uses "dcraw" code to do the actual raw decoding.
*/

#define _GNU_SOURCE /* for memmem() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <float.h>
#include <glib.h>
#include "dcraw_api.h"

extern FILE *ifp;
extern char *ifname, make[], model[];
extern float bright, red_scale, blue_scale;
extern int use_secondary, verbose, flip, height, width, fuji_width, rgb_max,
    iheight, iwidth, shrink, is_foveon, four_color_rgb;
extern unsigned filters;
//extern guint16 (*image)[4];
extern dcraw_image_type *image;
extern float pre_mul[4];
extern void (*load_raw)();
//void write_ppm16(FILE *);
//extern void (*write_fun)(FILE *);
extern jmp_buf failure;
extern int tone_curve_size, tone_curve_offset;
extern int black, colors, use_coeff, is_cmy, ymag, xmag;
extern float camera_red, camera_blue;
extern gushort white[8][8];
extern float coeff[3][4];
#define FC(filters,row,col) \
    (filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)
int identify();
void bad_pixels();
void foveon_interpolate_INDI(gushort (*image)[4],
    const int width, const int height);
void scale_colors_INDI(gushort (*image)[4], const int rgb_max, const int black,
    const int use_auto_wb, const int use_camera_wb,
    const float camera_red, const float camera_blue,
    const int height, const int width, const int colors,
    float pre_mul[4], const unsigned filters, /*const*/ gushort white[8][8],
    const char *ifname, const int use_coeff, /*const*/ float coeff[3][4],
    const float red_scale, const float blue_scale);
void vng_interpolate_INDI(gushort (*image)[4], const unsigned filters,
    const int width, const int height, const int colors,
    const int quick_interpolate, const int rgb_max);
void convert_to_rgb_INDI(gushort (*image)[4], const int document_mode,
    int *colors_p, const int trim, const int height, const int width,
    const unsigned filters, const int use_coeff,
    /*const*/ float coeff[3][4], const int is_cmy, const int rgb_max);
void flip_image_INDI(gushort (*image)[4], int *height_p, int *width_p,
    const int flip, int *ymag_p, int *xmag_p);
void fuji_rotate_INDI(gushort (**image_p)[4], int *height_p, int *width_p,
    int *fuji_width_p, const int shrink);

/* To prevent calculation of histogram */

char *messageBuffer = NULL;
int lastStatus = DCRAW_SUCCESS;

int dcraw_open(dcraw_data *h,char *filename)
{
    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    verbose = 1;
    ifname = filename;
//    use_secondary = 0; /* for Fuji Super CCD SR */
//    To prevent histogram calculation in convert_to_rgb():
//    write_fun = write_ppm16;
    if (setjmp(failure)) {
        dcraw_message(DCRAW_ERROR, "Fatal internal error\n");
        h->message = messageBuffer;
        return DCRAW_ERROR;
    }
    if (!(ifp = fopen (ifname, "rb"))) {
        dcraw_message(DCRAW_OPEN_ERROR, "Could not open %s: %s\n",
                filename, strerror(errno));
        h->message = messageBuffer;
        return DCRAW_OPEN_ERROR;
    }
    if (identify()) { /* dcraw already sent a dcraw_message() */
        fclose(ifp);
        h->message = messageBuffer;
        return lastStatus;
    }
    h->ifp = ifp;
    h->height = height;
    h->width = width;
    h->fuji_width = fuji_width;
    h->colors = colors;
    h->filters = h->originalFilters = filters;
    h->trim = h->filters!=0;
    /* copied from dcraw's flip_image() */
    switch ((flip+3600) % 360) {
        case 270: flip = 5; break;
        case 180: flip = 3; break;
        case  90: flip = 6;
    }
    h->flip = flip;
    h->toneCurveSize = tone_curve_size;
    h->toneCurveOffset = tone_curve_offset;
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_load_raw(dcraw_data *h, int half)
{
    int i;

    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    h->shrank = shrink = half && h->filters;
    h->width = iwidth = (h->width+shrink) >> shrink;
    h->height = iheight = (h->height+shrink) >> shrink;
    h->fuji_width = (h->fuji_width+shrink) >> shrink;
    h->rawImage = image = g_new0(dcraw_image_type, h->height * h->width);
    /* copied from the end of dcraw's identify() */
    if (shrink && colors == 3) {
        for (i=0; i < 32; i+=4) {
            if ((filters >> i & 15) == 9) filters |= 2 << i;
            if ((filters >> i & 15) == 6) filters |= 8 << i;
        }
        colors++;
        pre_mul[3] = pre_mul[1];
        if (use_coeff)
            for (i=0; i < 3; i++) coeff[i][3] = coeff[i][1] /= 2;
        h->filters = filters;
        h->colors = colors;
    }
    dcraw_message(DCRAW_VERBOSE, "Loading %s %s image from %s...\n",
                make, model, ifname);
    (*load_raw)();
    fclose(ifp);
    bad_pixels();
    /* Moved here from scale_colors() */
    h->rgbMax = rgb_max; // -= black;
    h->black = black;
    memcpy(h->pre_mul, pre_mul, sizeof pre_mul);
    h->use_coeff = use_coeff;
    memcpy(h->coeff, coeff, sizeof coeff);
    if (is_foveon) {
        dcraw_message(DCRAW_VERBOSE, "Foveon interpolation\n");
        foveon_interpolate_INDI(h->rawImage, h->width, h->height);
    }
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_copy_shrink(dcraw_data *h1, dcraw_data *h2, int scale)
{
    int h, w, r, c, ri, ci, cl, sum[4], count[4], norm, s, f, recombine;

    scale >>= h2->shrank;
    /* I'm skiping the last row/column if it is not a full row/column */
    h = h2->height/scale;
    w = h2->width/scale;
    if (h1!=h2) {
        *h1 = *h2;
        h1->rawImage = g_new0(dcraw_image_type, h * w);
    }
    norm = scale * scale;
    recombine = h1->colors==4 && !h1->use_coeff;
    if (h1->filters!=0 && !h1->shrank && scale%2==0) {
        f = h1->filters;
        for (cl=0; cl<h1->colors; cl++) count[cl] = 0;
        /* Filter pattern is eight rows by two columns (dcraw assumption) */
        for(r=0; r<8; r++)
            for(c=0; c<2; c++) count[FC(f,r,c)]++;
        for (cl=0; cl<h1->colors; cl++)
            if (count[cl]!=0) count[cl] = 16/count[cl];
        for(r=0; r<h; r++)
            for(c=0; c<w; c++) {
                for (cl=0; cl<h1->colors; cl++) sum[cl] = 0;
                for (ri=0; ri<scale; ri++)
                    for (ci=0; ci<scale; ci++)
                        sum[FC(f,r*scale+ri,ci)] += 
                            h2->rawImage[(r*scale+ri)*h2->width+c*scale+ci]
                           [FC(f,r*scale+ri,ci)];
                for (cl=0; cl<h1->colors; cl++)
                    h1->rawImage[r*w+c][cl] =
                        MAX(count[cl]*sum[cl]/norm - h1->black,0);
                if (recombine) h1->rawImage[r*w+c][1] =
                (h1->rawImage[r*w+c][1] + h1->rawImage[r*w+c][3])>>1;
            }
    } else if (h1->filters!=0 && !h1->shrank && scale%2==1) {
        f = h1->filters;
        for(r=0; r<h; r++)
            for(c=0; c<w; c++) {
                for (cl=0; cl<h1->colors; cl++) sum[cl] = count[cl] = 0;
                for (ri=0; ri<scale; ri++)
                    for (ci=0; ci<scale; ci++) {
                        sum[FC(f,r*scale+ri,c*scale+ci)] +=
                            h2->rawImage[(r*scale+ri)*h2->width+c*scale+ci]
                           [FC(f,r*scale+ri,c*scale+ci)];
                        count[FC(f,r*scale+ri,c*scale+ci)]++;
                    }
                for (cl=0; cl<h1->colors; cl++)
                    h1->rawImage[r*w+c][cl] =
                        MAX(sum[cl]/count[cl] - h1->black,0);
                if (recombine) h1->rawImage[r*w+c][1] =
                    (h1->rawImage[r*w+c][1] + h1->rawImage[r*w+c][3])>>1;
            }
    } else if (scale>1) {
        for(r=0; r<h; r++)
            for(c=0; c<w; c++) {
                for (cl=0;cl<colors; cl++) {
                    for (ri=0, s=0; ri<scale; ri++)
                        for (ci=0; ci<scale; ci++)
                            s += h2->rawImage
                                [(r*scale+ri)*h2->width+c*scale+ci][cl];
                    h1->rawImage[r*w+c][cl] = MAX(s/norm - h1->black,0);
                }
                if (recombine) h1->rawImage[r*w+c][1] =
                    (h1->rawImage[r*w+c][1] + h1->rawImage[r*w+c][3])>>1;
            }
    }
    if (recombine) h1->colors = 3;
    h1->height = h;
    h1->width = w;
    h1->fuji_width = h2->fuji_width / scale << h2->shrank;
    h1->filters = 0;
    h1->trim = 0;
    h1->rgbMax -= h1->black;
    h1->black = 0;
    return DCRAW_SUCCESS;
}

int dcraw_fuji_rotate(dcraw_data *h)
{
    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    fuji_rotate_INDI(&h->rawImage, &h->height, &h->width, &h->fuji_width, h->shrank);
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_flip_image(dcraw_data *h)
{
    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    if (h->flip) {
        dcraw_message(DCRAW_VERBOSE, "Flipping image %c:%c:%c...\n",
                h->flip & 1 ? 'H':'0', h->flip & 2 ? 'V':'0',
                h->flip & 4 ? 'T':'0');
        flip_image_INDI(h->rawImage, &h->height, &h->width,
                h->flip, &ymag, &xmag);
    }
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_set_color_scale(dcraw_data *h, int useAutoWB, int useCameraWB)
{
    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    memcpy(h->post_mul, h->pre_mul, sizeof h->post_mul);
    scale_colors_INDI(h->rawImage,
            h->rgbMax-h->black, h->black, useAutoWB, useCameraWB,
            camera_red, camera_blue, h->height, h->width, h->colors,
            h->post_mul, h->originalFilters, white, ifname,
            h->use_coeff, h->coeff, 1, 1);
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_scale_colors(dcraw_data *h, int rgbWB[4])
{
    int r, c, f, cl;

    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    if (h->filters!=0) {
        f = h->filters;
        for(r=0; r<h->height; r++)
            for(c=0; c<h->width; c++)
                h->rawImage[r*h->width+c][FC(f,r,c)] = MIN( MAX( (gint64)
                    (h->rawImage[r*h->width+c][FC(f,r,c)] - h->black) *
                    rgbWB[FC(f,r,c)]/(h->rgbMax-h->black), 0), 0xFFFF);
    } else {
        for(r=0; r<h->height; r++)
            for(c=0; c<h->width; c++) {
                for (cl=0; cl<h->colors; cl++)
                    h->rawImage[r*h->width+c][cl] = MIN( MAX(
                        (guint64)(h->rawImage[r*h->width+c][cl]-h->black)*
                        rgbWB[cl]/(h->rgbMax-h->black), 0), 0xFFFF);
                if (h->colors==4 && !h->use_coeff)
                    h->rawImage[r*h->width+c][1] =
                            (h->rawImage[r*h->width+c][1] +
                            h->rawImage[r*h->width+c][3])>>1;
            }
        if (!h->use_coeff) h->colors = 3;
    }
    h->rgbMax = 0xFFFF;
    h->black = 0;
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_interpolate(dcraw_data *h, int quick, int fourColors)
{
    int i, r, c;
    unsigned filters;

    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    if (!h->filters) return lastStatus;
    /* copied from the end of dcraw's identify() */
    if (fourColors && h->colors == 3) {
        filters = h->filters;
        for (i=0; i < 32; i+=4) {
            if ((filters >> i & 15) == 9) filters |= 2 << i;
            if ((filters >> i & 15) == 6) filters |= 8 << i;
        }
        h->colors++;
        h->pre_mul[3] = h->pre_mul[1];
        if (h->use_coeff)
            for (i=0; i < 3; i++)
                h->coeff[i][3] = h->coeff[i][1] /= 2;
        for(r=0; r<h->height; r++)
            for(c=0; c<h->width; c++)
               h->rawImage[r*h->width+c][FC(filters,r,c)] = 
                   h->rawImage[r*h->width+c][FC(h->filters,r,c)];
        h->filters = filters;
    }
    dcraw_message(DCRAW_VERBOSE, "%s interpolation...\n",
                quick ? "Bilinear":"VNG");
    vng_interpolate_INDI(h->rawImage, h->filters, h->width, h->height,
                h->colors, quick, h->rgbMax);
    if (fourColors && !h->use_coeff) {
        for (i=0; i<h->height*h->width; i++)
            h->rawImage[i][1] = (h->rawImage[i][1]+h->rawImage[i][3])/2;
        h->colors--;
    }
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_convert_to_rgb(dcraw_data *h)
{
    int i, r, g;
    float rgb[3];

    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    if (!h->use_coeff) return lastStatus;
    for (i=0; i<h->height*h->width; i++) {
        for (r=0; r<3; r++)
            for (rgb[r]=g=0; g<h->colors; g++)
                rgb[r] += h->rawImage[i][g] * h->coeff[r][g];
        for (r=0; r<3; r++)
            h->rawImage[i][r] = MIN(MAX((int)rgb[r],0),h->rgbMax);
    }    
    return lastStatus;
}

void dcraw_close(dcraw_data *h)
{
    if (h->rawImage!=NULL) g_free(h->rawImage);
}

void dcraw_message_handler(int code, char *message)
{
    char *buf;

#ifdef DEBUG
    fprintf(stderr, message);
#endif
    if (messageBuffer==NULL) messageBuffer = g_strdup(message);
    else {
        buf = g_strconcat(messageBuffer, message, NULL);
        g_free(messageBuffer);
        messageBuffer = buf;
    }
    lastStatus = code;
}

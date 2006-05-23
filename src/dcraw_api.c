/*
   dcraw_api.c - an API for DCRaw
   by Udi Fuchs,

   based on DCRaw by Dave Coffin
   http://www.cybercom.net/~dcoffin/

   UFRaw is licensed under the GNU General Public License.
   It uses DCRaw code to do the actual raw decoding.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h> /* for sqrt() */
#include <setjmp.h>
#include <errno.h>
#include <float.h>
#include <glib.h>
#include "dcraw_api.h"

extern FILE *ifp;
extern char *ifname, make[], model[];
extern int use_secondary, verbose, flip, height, width, fuji_width, maximum,
    iheight, iwidth, shrink, is_raw, is_foveon;
extern unsigned filters, data_offset;
//extern guint16 (*image)[4];
extern dcraw_image_type *image;
extern float pre_mul[4];
extern void (*load_raw)();
extern void kodak_ycbcr_load_raw(); 
//void write_ppm16(FILE *);
//extern void (*write_fun)(FILE *);
extern jmp_buf failure;
extern int tone_curve_size, tone_curve_offset;
extern int tone_mode_offset, tone_mode_size;
extern int black, colors, raw_color, /*xmag,*/ ymag;
extern float cam_mul[4];
extern gushort white[8][8];
extern float rgb_cam[3][4];
extern char *meta_data;
extern int meta_length;
extern float iso_speed, shutter, aperture, focal_len;
extern time_t timestamp;
#define FC(filters,row,col) \
    (filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)
extern void pseudoinverse(double (*in)[3], double (*out)[3], int size);
extern void identify();
extern void bad_pixels();
extern void foveon_interpolate();
void scale_colors_INDI(gushort (*image)[4], const int rgb_max, const int black,
    const int use_auto_wb, const int use_camera_wb, const float cam_mul[4],
    const int height, const int width, const int colors,
    float pre_mul[4], const unsigned filters, /*const*/ gushort white[8][8],
    const char *ifname);
void lin_interpolate_INDI(gushort (*image)[4], const unsigned filters,
    const int width, const int height, const int colors);
void vng_interpolate_INDI(gushort (*image)[4], const unsigned filters,
    const int width, const int height, const int colors, const int rgb_max);
void cam_to_cielab_INDI (gushort cam[4], float lab[3],
    const int colors, const int maximum, float rgb_cam[3][4]);
void ahd_interpolate_INDI(gushort (*image)[4], const unsigned filters,
    const int width, const int height, const int colors,
    const int maximum, float rgb_cam[3][4]);
void flip_image_INDI(gushort (*image)[4], int *height_p, int *width_p,
    const int flip);
void fuji_rotate_INDI(gushort (**image_p)[4], int *height_p, int *width_p,
    int *fuji_width_p, const int colors, const double step);

char *messageBuffer = NULL;
int lastStatus = DCRAW_SUCCESS;

int dcraw_open(dcraw_data *h,char *filename)
{
    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    verbose = 1;
    ifname = g_strdup(filename);
//    use_secondary = 0; /* for Fuji Super CCD SR */
    if (setjmp(failure)) {
        dcraw_message(DCRAW_ERROR, "Fatal internal error\n");
        h->message = messageBuffer;
        return DCRAW_ERROR;
    }
    if (!(ifp = fopen (ifname, "rb"))) {
        dcraw_message(DCRAW_OPEN_ERROR, "Could not open %s: %s\n",
                filename, strerror(errno));
        g_free(ifname);
        h->message = messageBuffer;
        return DCRAW_OPEN_ERROR;
    }
    identify();
    /* We first check if dcraw recognizes the file, this is equivalent
     * to 'dcraw -i' succeeding */
    if (!make[0]) {
	dcraw_message(DCRAW_OPEN_ERROR, "%s: unsupported file format.\n",
		ifname);
        fclose(ifp);
        g_free(ifname);
        h->message = messageBuffer;
        return lastStatus;
    }
    /* Next we check if dcraw can decode the file */
    if (!is_raw) {
	dcraw_message(DCRAW_OPEN_ERROR, "Cannot decode %s\n", ifname);
        fclose(ifp);
        g_free(ifname);
        h->message = messageBuffer;
        return lastStatus;
    }
    if (load_raw == kodak_ycbcr_load_raw) {
	height += height & 1;
	width += width & 1;
    }
    /* Pass global variables to the handler on two conditions:
     * 1. They are needed at this stage.
     * 2. They where set in identify() and won't change in load_raw() */
    h->ifp = ifp;
    h->height = height;
    h->width = width;
    h->fuji_width = fuji_width;
    h->fuji_step = sqrt(0.5);
    h->colors = colors;
    h->filters = filters;
    h->raw_color = raw_color;
    h->shrink = shrink = (h->filters!=0);
    h->ymag = ymag;
    /* copied from dcraw's main() */
    switch ((flip+3600) % 360) {
        case 270: flip = 5; break;
        case 180: flip = 3; break;
        case  90: flip = 6;
    }
    h->flip = flip;
    h->toneCurveSize = tone_curve_size;
    h->toneCurveOffset = tone_curve_offset;
    h->toneModeOffset = tone_mode_offset;
    h->toneModeSize = tone_mode_size;
    g_strlcpy(h->make, make, 80);
    g_strlcpy(h->model, model, 80);
    h->iso_speed = iso_speed;
    h->shutter = shutter;
    h->aperture = aperture;
    h->focal_len = focal_len;
    h->timestamp = timestamp;
    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_load_raw(dcraw_data *h)
{
    int i, j;
    double dmin;

    g_free(messageBuffer);
    messageBuffer = NULL;
    lastStatus = DCRAW_SUCCESS;
    h->raw.height = iheight = (h->height+h->shrink) >> h->shrink;
    h->raw.width = iwidth = (h->width+h->shrink) >> h->shrink;
    h->raw.image = image = g_new0(dcraw_image_type, iheight * iwidth
	    + meta_length);
    meta_data = (char *) (image + iheight*iwidth);
    /* copied from the end of dcraw's identify() */
    if (filters && colors == 3) {
        for (i=0; i < 32; i+=4) {
            if ((filters >> i & 15) == 9) filters |= 2 << i;
            if ((filters >> i & 15) == 6) filters |= 8 << i;
        }
        colors++;
    }
    h->raw.colors = colors;
    h->fourColorFilters = filters;
    dcraw_message(DCRAW_VERBOSE, "Loading %s %s image from %s...\n",
                make, model, ifname);
    fseek (ifp, data_offset, SEEK_SET);
    (*load_raw)();
    bad_pixels();
    if (is_foveon) {
        foveon_interpolate();
	h->raw.width = width;
	h->raw.height = height;
    }
    fclose(ifp);
    h->ifp = NULL;
    h->rgbMax = maximum;
    h->black = black;
    dcraw_message(DCRAW_VERBOSE, "Black: %d, Maximum: %d\n", black, maximum);
// RS
//    cam_to_cielab_INDI(NULL, NULL, h->colors, h->rgbMax, rgb_cam);
    dmin = DBL_MAX;
    for (i=0; i<h->colors; i++) if (dmin > pre_mul[i]) dmin = pre_mul[i];
    for (i=0; i<h->colors; i++) h->pre_mul[i] = pre_mul[i]/dmin;
    memcpy(h->cam_mul, cam_mul, sizeof cam_mul);
    memcpy(h->rgb_cam, rgb_cam, sizeof rgb_cam);

    double rgb_cam_transpose[4][3];
    for (i=0; i<4; i++) for (j=0; j<3; j++)
	rgb_cam_transpose[i][j] = rgb_cam[j][i];
    pseudoinverse (rgb_cam_transpose, h->cam_rgb, colors);

    h->message = messageBuffer;
    return lastStatus;
}

int dcraw_image_resize(dcraw_image_data *image, int size)
{
    int h, w, wid, r, ri, rii, c, ci, cii, cl, norm;
    guint64 riw, riiw, ciw, ciiw;
    guint64 (*iBuf)[4];
    int mul=size, div=MAX(image->height, image->width);

    if (mul > div) return DCRAW_ERROR;
    /* I'm skiping the last row/column if it is not a full row/column */
    h = image->height * mul / div;
    w = image->width * mul / div;
    wid = image->width;
    iBuf = (void*)g_new0(guint64, h * w * 4);
    norm = div * div;

    for(r=0; r<image->height; r++) {
        /* r should be divided between ri and rii */
        ri = r * mul / div;
        rii = (r+1) * mul / div;
        /* with weights riw and riiw (riw+riiw==mul) */
        riw = rii * div - r * mul;
        riiw = (r+1) * mul - rii * div;
        if (rii>=h) {rii=h-1; riiw=0;}
        if (ri>=h) {ri=h-1; riw=0;}
        for(c=0; c<image->width; c++) {
            ci = c * mul / div;
            cii = (c+1) * mul / div;
            ciw = cii * div - c * mul;
            ciiw = (c+1) * mul - cii * div;
            if (cii>=w) {cii=w-1; ciiw=0;}
            if (ci>=w) {ci=w-1; ciw=0;}
            for (cl=0; cl<image->colors; cl++) {
                iBuf[ri *w+ci ][cl] += image->image[r*wid+c][cl]*riw *ciw ;
                iBuf[ri *w+cii][cl] += image->image[r*wid+c][cl]*riw *ciiw;
                iBuf[rii*w+ci ][cl] += image->image[r*wid+c][cl]*riiw*ciw ;
                iBuf[rii*w+cii][cl] += image->image[r*wid+c][cl]*riiw*ciiw;
            }
        }
    }
    for (c=0; c<h*w; c++) for (cl=0; cl<image->colors; cl++)
        image->image[c][cl] = iBuf[c][cl]/norm;
    g_free(iBuf);
    image->height = h;
    image->width = w;
    return DCRAW_SUCCESS;
}

/* Stretch image by 'ymag'. It is needed only for Nikon D1X images.
 * We can ignore 'xmag' since we always stretch before we flip */
int dcraw_image_stretch(dcraw_image_data *image, int ymag)
{
    int r;
    if (ymag==1) return DCRAW_SUCCESS;
    if (ymag!=2) return DCRAW_ERROR;
    int w = image->width;
    dcraw_image_type *iBuf = g_new(dcraw_image_type,
	    image->height * 2  * w);
    for(r=0; r<image->height; r++) {
	memcpy(iBuf[(2*r)*w], image->image[r*w], w*4*2);
	memcpy(iBuf[(2*r+1)*w], image->image[r*w], w*4*2);
    }
    g_free(image->image);
    image->image = iBuf;
    image->height *= 2;
    return DCRAW_SUCCESS;
}

void dcraw_close(dcraw_data *h)
{
    g_free(ifname);
    g_free(h->raw.image);
}

char *ufraw_message(int code, char *message, ...);

void dcraw_message(int code, char *format, ...)
{
    char *buf, *message;
    va_list ap;
    va_start(ap, format);
    message = g_strdup_vprintf(format, ap);
    va_end(ap);
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
    g_free(message);
}

/*
   dcraw_api.h - an API for dcraw
   by udi Fuchs,

   based on dcraw by Dave Coffin
   http://www.cybercom.net/~dcoffin/

   UFRaw is licensed under the GNU General Public License.
   It uses "dcraw" code to do the actual raw decoding.
*/

#ifndef _DCRAW_API_H
#define _DCRAW_API_H

typedef guint16 dcraw_image_type[4];

typedef struct {
    FILE *ifp;
    int width, height, colors, originalFilters, filters, use_coeff, trim;
    int flip, shrank;
    dcraw_image_type *rawImage;
    float pre_mul[4], post_mul[4], coeff[3][4], post_coeff[3][4];
    int rgbMax, black, fuji_width;
    int toneCurveSize, toneCurveOffset;
    char *message;
} dcraw_data;

int dcraw_open(dcraw_data *h, char *filename);
int dcraw_load_raw(dcraw_data *h, int half);
int dcraw_copy_shrink(dcraw_data *h1, dcraw_data *h2, int scale);
int dcraw_flip_image(dcraw_data *h);
int dcraw_set_color_scale(dcraw_data *h, int useAutoWB, int useCameraWB);
int dcraw_scale_colors(dcraw_data *h, int rgbWB[3]);
int dcraw_interpolate(dcraw_data *h, int quick, int fourColor);
int dcraw_convert_to_rgb(dcraw_data *h);
int dcraw_fuji_rotate(dcraw_data *h);
void dcraw_close(dcraw_data *h);

#define DCRAW_SUCCESS 0
#define DCRAW_ERROR 1
#define DCRAW_UNSUPPORTED 2
#define DCRAW_NO_CAMERA_WB 3
#define DCRAW_VERBOSE 4
#define DCRAW_OPEN_ERROR 5
                                                                                
void dcraw_message_handler(int code, char *message);
#define dcraw_message(code, format, ...) {\
        char message[200];\
        snprintf(message, 200, format, ## __VA_ARGS__);\
        dcraw_message_handler(code, message);\
}

#endif /*_DCRAW_API_H*/

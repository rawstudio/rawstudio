
/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw.h - Common definitions for UFRaw.
 * Copyright 2004-2016 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UFRAW_H
#define _UFRAW_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "uf_glib.h"
#include "ufobject.h"

#include "nikon_curve.h"
#include "uf_progress.h"

#ifndef HAVE_STRCASECMP
#define strcasecmp stricmp
#endif

/* macro to clamp a number between two values */
#ifndef LIM
#define LIM(x,min,max) MAX(min,MIN(x,max))
#endif

#define MAXOUT 255 /* Max output sample */

#define max_curves 20
#define max_anchors 20
#define max_profiles 20
#define max_path 200
#define max_name 80
#define max_adjustments 3

/* An impossible value for conf float values */
#define NULLF -10000.0

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* Options, like auto-adjust buttons can be in 3 states. Enabled and disabled
 * are obvious. Apply means that the option was selected and some function
 * has to act accourdingly, before changing to one of the first two states */
enum {
    disabled_state, enabled_state, apply_state
};

extern const char uf_spot_wb[];
extern const char uf_manual_wb[];
extern const char uf_camera_wb[];
extern const char uf_auto_wb[];

/*
 * UFObject Definitions for ufraw_settings.cc
 */

extern UFName ufWB;
extern UFName ufPreset;
extern UFName ufWBFineTuning;
extern UFName ufTemperature;
extern UFName ufGreen;
extern UFName ufChannelMultipliers;
extern UFName ufLensfunAuto;
extern UFName ufLensfun;
extern UFName ufCameraModel;
extern UFName ufLensModel;
extern UFName ufFocalLength;
extern UFName ufAperture;
extern UFName ufDistance;
extern UFName ufTCA;
extern UFName ufVignetting;
extern UFName ufDistortion;
extern UFName ufModel;
extern UFName ufLensGeometry;
extern UFName ufTargetLensGeometry;
extern UFName ufRawImage;
extern UFName ufRawResources;
extern UFName ufCommandLine;

UFObject *ufraw_image_new();
#ifdef HAVE_LENSFUN
UFObject *ufraw_lensfun_new();
void ufraw_lensfun_init(UFObject *lensfun, UFBoolean reset);
struct lfDatabase *ufraw_lensfun_db(); /* mount/camera/lens database */
const struct lfCamera *ufraw_lensfun_camera(const UFObject *lensfun);
void ufraw_lensfun_set_camera(UFObject *lensfun, const struct lfCamera *camera);
const struct lfLens *ufraw_lensfun_interpolation_lens(const UFObject *lensfun);
void ufraw_lensfun_set_lens(UFObject *lensfun, const struct lfLens *lens);
#endif
struct ufraw_struct *ufraw_image_get_data(UFObject *obj);
void ufraw_image_set_data(UFObject *obj, struct ufraw_struct *uf);
UFObject *ufraw_resources_new();
UFObject *ufraw_command_line_new();

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

enum { rgb_histogram, r_g_b_histogram, luminosity_histogram, value_histogram,
       saturation_histogram
     };
enum { linear_histogram, log_histogram };
/* The following enum should match the dcraw_interpolation enum
 * in dcraw_api.h. */
enum { ahd_interpolation, vng_interpolation, four_color_interpolation,
       ppg_interpolation, bilinear_interpolation, xtrans_interpolation,
       none_interpolation, half_interpolation, obsolete_eahd_interpolation,
       num_interpolations
     };
enum { no_id, also_id, only_id, send_id };
enum { manual_curve, linear_curve, custom_curve, camera_curve };
enum { in_profile, out_profile, display_profile, profile_types};
enum { raw_expander, live_expander, expander_count };
enum { ppm_type, ppm16_deprecated_type, tiff_type, tiff16_deprecated_type,
       jpeg_type, png_type, png16_deprecated_type,
       embedded_jpeg_type, embedded_png_type, fits_type, num_types
     };
enum { clip_details, restore_lch_details, restore_hsv_details,
       restore_types
     };
enum { digital_highlights, film_highlights, highlights_types };

/* ufraw_standalone        : Normal stand-alone
 * ufraw_gimp_plugin       : Gimp plug-in
 * ufraw_standalone_output : Stand-alone with --output option
 */
enum { ufraw_standalone, ufraw_gimp_plugin, ufraw_standalone_output };

typedef enum { display_developer, file_developer, auto_developer }
DeveloperMode;
typedef enum { perceptual_intent, relative_intent, saturation_intent,
               absolute_intent, disable_intent
             } Intent;
typedef enum { ufraw_raw_phase, ufraw_first_phase, ufraw_transform_phase,
               ufraw_develop_phase, ufraw_display_phase, ufraw_phases_num
             } UFRawPhase;
typedef enum { grayscale_none, grayscale_lightness,
               grayscale_luminance, grayscale_value,
               grayscale_mixer, grayscale_invalid
             } GrayscaleMode;

typedef struct {
    const char *make;
    const char *model;
    const char *name;
    int tuning;
    double channel[4];
} wb_data;

typedef struct {
    double adjustment;
    double hue;
    double hueWidth;
} lightness_adjustment;

typedef struct {
    DeveloperMode mode;
    unsigned rgbMax, max, exposure, colors, useMatrix;
    int restoreDetails, clipHighlights;
    int rgbWB[4], colorMatrix[3][4];
    double gamma, linear;
    char profileFile[profile_types][max_path];
    void *profile[profile_types];
    Intent intent[profile_types];
    gboolean updateTransform;
    void *colorTransform;
    void *working2displayTransform;
    void *rgbtolabTransform;
    double saturation;
#ifdef UFRAW_CONTRAST
    double contrast;
#endif
    CurveData baseCurveData, luminosityCurveData;
    guint16 gammaCurve[0x10000];
    void *luminosityProfile;
    void *TransferFunction[3];
    void *saturationProfile;
    void *adjustmentProfile;
    GrayscaleMode grayscaleMode;
    double grayscaleMixer[3];
    lightness_adjustment lightnessAdjustment[max_adjustments];
} developer_data;

typedef guint16 ufraw_image_type[4];

typedef struct {
    char name[max_name];
    char file[max_path];
    char productName[max_name];
    double gamma, linear;
    int BitDepth;
} profile_data;

typedef struct {
    gint x;
    gint y;
    gint width;
    gint height;
} UFRectangle;

/* conf_data holds the configuration data of UFRaw.
 * The data can be split into three groups:
 * IMAGE manipulation, SAVE options and GUI settings.
 * The sources for this information are:
 * DEF: UFRaw's defaults from conf_defaults.
 * RC: users defaults from ~/.ufrawrc. These options are set from the last
 *     interactive session.
 *     If saveConfiguration==disabled_state, IMAGE options are not saved.
 * ID: UFRaw ID files used on their original image.
 * CONF: same ID files used as configuration for other raw images.
 * CMD: command line options.
 * UI: interactive input.
 * The options are set in the above order, therefore the last sources will
 * override the first ones with some subtelties:
 * * ID|CONF contains only data which is different from DEF, still it is
 *   assumed that IMAGE and SAVE options are included. Therefore missing
 *   options are set to DEF overwriting RC.
 * * if both CONF and ID are specified, only in/out filenames are taken from ID.
 * * in batch mode SAVE options from RC are ignored.
 * Some fields need special treatment:
 * RC|CONF: auto[Exposure|Black]==enable_state it is switched to apply_state.
 * RC|CONF: if !spot_wb reset chanMul[] to -1.0.
 * CONF|ID: curve/profile are added to the list from RC.
 * CONF: inputFilename, outputFilename are ignored.
 * outputPath can only be specified in CMD or guessed in interactive mode.
 * ID: createID==only_id is switched to no_id in case of ufraw-batch.
 * ID: chanMul[] override wb, green, temperature.
 */
typedef struct {
    /* Internal data */
    int version;

    // Eventually ufobject should replace conf_data.
    UFObject *ufobject;

    /* IMAGE manipulation settings */
    double threshold;
    double hotpixel;
#ifdef UFRAW_CONTRAST
    double contrast;
#endif
    double exposure, saturation, black; /* black is only used in CMD */
    int ExposureNorm;
    int restoreDetails, clipHighlights;
    int autoExposure, autoBlack, fullCrop, autoCrop;
    int BaseCurveIndex, BaseCurveCount;
    CurveData BaseCurve[max_curves];
    int curveIndex, curveCount;
    CurveData curve[max_curves];
    int profileIndex[profile_types], profileCount[profile_types];
    profile_data profile[profile_types][max_profiles];
    Intent intent[profile_types];
    int interpolation;
    int smoothing;
    char darkframeFile[max_path];
    struct ufraw_struct *darkframe;
    int CropX1, CropY1, CropX2, CropY2;
    double aspectRatio;
    int orientation;
    double rotationAngle;
    int lightnessAdjustmentCount;
    lightness_adjustment lightnessAdjustment[max_adjustments];
    int grayscaleMode;
    double grayscaleMixer[3];
    int grayscaleMixerDefined;
    double despeckleWindow[4];
    double despeckleDecay[4];
    double despecklePasses[4];

    /* SAVE options */
    char inputFilename[max_path], outputFilename[max_path],
         outputPath[max_path];
    char inputURI[max_path], inputModTime[max_name];
    int type, compression, createID, embedExif, progressiveJPEG;
    int shrink, size;
    gboolean overwrite, losslessCompress, embeddedImage, noExit;
    gboolean rotate;

    /* GUI settings */
    double Zoom;
    gboolean LockAspect; /* True if aspect ratio is locked */
    int saveConfiguration;
    int histogram, liveHistogramScale;
    int rawHistogramScale;
    int expander[expander_count];
    gboolean overExp, underExp, blinkOverUnder;
    gboolean RememberOutputPath;
    gboolean WindowMaximized;
    int drawLines;
    char curvePath[max_path];
    char profilePath[max_path];
    gboolean silent;
    char remoteGimpCommand[max_path];

    /* EXIF data */
    int CameraOrientation;
    float iso_speed, shutter, aperture, focal_len, subject_distance;
    char exifSource[max_name], isoText[max_name], shutterText[max_name],
         apertureText[max_name], focalLenText[max_name],
         focalLen35Text[max_name], lensText[max_name],
         flashText[max_name], whiteBalanceText[max_name];
    char timestampText[max_name], make[max_name], model[max_name];
    time_t timestamp;
    /* Unfortunately dcraw strips make and model, but we need originals too */
    char real_make[max_name], real_model[max_name];
} conf_data;

typedef struct {
    guint8 *buffer;
    int height, width, depth, rowstride;
    /* This bit field marks valid pieces of the image with 1's.
       The variable contains a fixed 4x8 matrix of bits, every bit containing
       the validity of the respective subarea of the whole image. The subarea
       sizes are determined by dividing the width by 4 and height by 8.
       This field must always contain at least 32 bits. */
    guint32 valid;
    gboolean rgbg;
    gboolean invalidate_event;
} ufraw_image_data;

typedef struct ufraw_struct {
    int status;
    char *message;
    char filename[max_path];
    int initialHeight, initialWidth, rgbMax, colors, raw_color, useMatrix;
    int rotatedHeight, rotatedWidth;
    int autoCropHeight, autoCropWidth;
    gboolean LoadingID; /* Indication that we are loading an ID file */
    gboolean WBDirty;
    float rgb_cam[3][4];
    ufraw_image_data Images[ufraw_phases_num];
    ufraw_image_data thumb;
    void *raw;
    gboolean HaveFilters;
    gboolean IsXTrans;
    void *unzippedBuf;
    gsize unzippedBufLen;
    developer_data *developer;
    developer_data *AutoDeveloper;
    guint8 *displayProfile;
    gint displayProfileSize;
    conf_data *conf;
    guchar *inputExifBuf;
    guint inputExifBufLen;
    guchar *outputExifBuf;
    guint outputExifBufLen;
    int gimpImage;
    int *RawHistogram;
    int RawChanMul[4];
    int RawCount;
#ifdef HAVE_LENSFUN
    int modFlags; /* postprocessing operations (LF_MODIFY_XXX) */
    struct lfModifier *TCAmodifier;
    struct lfModifier *modifier;
#endif /* HAVE_LENSFUN */
    int hotpixels;
    gboolean mark_hotpixels;
    unsigned raw_multiplier;
    gboolean wb_presets_make_model_match;
} ufraw_data;

extern const conf_data conf_default;
extern const wb_data wb_preset[];
extern const int wb_preset_count;
extern const char raw_ext[];
extern const char *file_type[];

/* ufraw_binary contains the name of the binary file for error messages.
 * It should be set in every UFRaw main() */
extern char *ufraw_binary;

#ifdef __cplusplus
extern "C" {
#endif

/* prototypes for functions in ufraw_ufraw.c */
ufraw_data *ufraw_open(char *filename);
int ufraw_config(ufraw_data *uf, conf_data *rc, conf_data *conf, conf_data *cmd);
int ufraw_load_raw(ufraw_data *uf);
int ufraw_load_darkframe(ufraw_data *uf);
void ufraw_developer_prepare(ufraw_data *uf, DeveloperMode mode);
int ufraw_convert_image(ufraw_data *uf);
ufraw_image_data *ufraw_get_image(ufraw_data *uf, UFRawPhase phase,
                                  gboolean bufferok);
ufraw_image_data *ufraw_convert_image_area(ufraw_data *uf, unsigned saidx,
        UFRawPhase phase);
void ufraw_close_darkframe(conf_data *uf);
void ufraw_close(ufraw_data *uf);
void ufraw_flip_orientation(ufraw_data *uf, int flip);
void ufraw_flip_image(ufraw_data *uf, int flip);
void ufraw_invalidate_layer(ufraw_data *uf, UFRawPhase phase);
void ufraw_invalidate_tca_layer(ufraw_data *uf);
void ufraw_invalidate_hotpixel_layer(ufraw_data *uf);
void ufraw_invalidate_denoise_layer(ufraw_data *uf);
void ufraw_invalidate_darkframe_layer(ufraw_data *uf);
void ufraw_invalidate_despeckle_layer(ufraw_data *uf);
void ufraw_invalidate_whitebalance_layer(ufraw_data *uf);
void ufraw_invalidate_smoothing_layer(ufraw_data *uf);
int ufraw_set_wb(ufraw_data *uf, gboolean interactive);
void ufraw_auto_expose(ufraw_data *uf);
void ufraw_auto_black(ufraw_data *uf);
void ufraw_auto_curve(ufraw_data *uf);
void ufraw_normalize_rotation(ufraw_data *uf);
void ufraw_unnormalize_rotation(ufraw_data *uf);
void ufraw_get_image_dimensions(ufraw_data *uf);
/* Get scaled crop coordinates in final image coordinates */
void ufraw_get_scaled_crop(ufraw_data *uf, UFRectangle *crop);

UFRectangle ufraw_image_get_subarea_rectangle(ufraw_image_data *img,
        unsigned saidx);
unsigned ufraw_img_get_subarea_idx(ufraw_image_data *img, int x, int y);

/* prototypes for functions in ufraw_message.c */
char *ufraw_get_message(ufraw_data *uf);
/* The following functions should only be used internally */
void ufraw_message_init(ufraw_data *uf);
void ufraw_message_reset(ufraw_data *uf);
void ufraw_set_error(ufraw_data *uf, const char *format, ...);
void ufraw_set_warning(ufraw_data *uf, const char *format, ...);
void ufraw_set_info(ufraw_data *uf, const char *format, ...);
int ufraw_get_status(ufraw_data *uf);
int ufraw_is_error(ufraw_data *uf);
// Old error handling, should be removed after being fully implemented.
char *ufraw_message(int code, const char *format, ...);
void ufraw_batch_messenger(char *message);

/* prototypes for functions in ufraw_preview.c */
int ufraw_preview(ufraw_data *uf, conf_data *rc, int plugin,
                  long(*save_func)());
void ufraw_focus(void *window, gboolean focus);
void ufraw_messenger(char *message, void *parentWindow);

/* prototypes for functions in ufraw_routines.c */
const char *uf_get_home_dir();
void uf_init_locale(const char *exename);
char *uf_file_set_type(const char *filename, const char *type);
char *uf_file_set_absolute(const char *filename);
/* Set locale of LC_NUMERIC to "C" to make sure that printf behaves correctly.*/
char *uf_set_locale_C();
void uf_reset_locale(char *locale);
char *uf_markup_buf(char *buffer, const char *format, ...);
double profile_default_linear(profile_data *p);
double profile_default_gamma(profile_data *p);
void Temperature_to_RGB(double T, double RGB[3]);
void RGB_to_Temperature(double RGB[3], double *T, double *Green);
int curve_load(CurveData *cp, char *filename);
int curve_save(CurveData *cp, char *filename);
char *curve_buffer(CurveData *cp);
/* Useful functions for handling the underappreciated Glib ptr arrays */
int ptr_array_insert_sorted(GPtrArray *array, const void *item, GCompareFunc compare);
int ptr_array_find_sorted(const GPtrArray *array, const void *item, GCompareFunc compare);
void ptr_array_insert_index(GPtrArray *array, const void *item, int index);

/* prototypes for functions in ufraw_conf.c */
int conf_load(conf_data *c, const char *confFilename);
void conf_file_load(conf_data *conf, char *confFilename);
int conf_save(conf_data *c, char *confFilename, char **confBuffer);
/* copy default config to given instance and initialize non-const fields */
void conf_init(conf_data *c);
/* Copy the image manipulation options from *src to *dst */
void conf_copy_image(conf_data *dst, const conf_data *src);
/* Copy the transformation options from *src to *dst */
void conf_copy_transform(conf_data *dst, const conf_data *src);
/* Copy the 'save options' from *src to *dst */
void conf_copy_save(conf_data *dst, const conf_data *src);
int conf_set_cmd(conf_data *conf, const conf_data *cmd);
int ufraw_process_args(int *argc, char ***argv, conf_data *cmd, conf_data *rc);

/* prototype for functions in ufraw_developer.c */
// Convert linear RGB to CIE-LCh
void uf_rgb_to_cielch(gint64 rgb[3], float lch[3]);
// Convert CIE-LCh to linear RGB
void uf_cielch_to_rgb(float lch[3], gint64 rgb[3]);
void uf_raw_to_cielch(const developer_data *d,
                      const guint16 raw[4], float lch[3]);
developer_data *developer_init();
void developer_destroy(developer_data *d);
void developer_profile(developer_data *d, int type, profile_data *p);
void developer_display_profile(developer_data *d,
                               unsigned char *profile, int size, char productName[]);
void developer_prepare(developer_data *d, conf_data *conf,
                       int rgbMax, float rgb_cam[3][4], int colors, int useMatrix,
                       DeveloperMode mode);
void develop(void *po, guint16 pix[4], developer_data *d, int mode, int count);
void develop_display(void *pout, void *pin, developer_data *d, int count);
void develop_linear(guint16 in[4], guint16 out[3], developer_data *d);

/* prototype for functions in ufraw_saver.c */
long ufraw_save_now(ufraw_data *uf, void *widget);
long ufraw_send_to_gimp(ufraw_data *uf);

/* prototype for functions in ufraw_writer.c */
int ufraw_write_image(ufraw_data *uf);
void ufraw_write_image_data(
    ufraw_data *uf, void * volatile out,
    const UFRectangle *Crop, int bitDepth, int grayscaleMode,
    int (*row_writer)(ufraw_data *, void * volatile, void *, int, int, int, int, int));

/* prototype for functions in ufraw_delete.c */
long ufraw_delete(void *widget, ufraw_data *uf);

/* prototype for functions in ufraw_embedded.c */
int ufraw_read_embedded(ufraw_data *uf);
int ufraw_convert_embedded(ufraw_data *uf);
int ufraw_write_embedded(ufraw_data *uf);

/* prototype for functions in ufraw_chooser.c */
void ufraw_chooser(conf_data *conf, conf_data *rc, conf_data *cmd,
                   const char *defPath);

/* prototype for functions in ufraw_icons.c */
void ufraw_icons_init();

/* prototype for functions in ufraw_exiv2.cc */
int ufraw_exif_read_input(ufraw_data *uf);
int ufraw_exif_prepare_output(ufraw_data *uf);
int ufraw_exif_write(ufraw_data *uf);

#ifdef __cplusplus
} // extern "C"
#endif

/* status numbers from DCRaw and UFRaw */
#define UFRAW_SUCCESS 0
//#define UFRAW_DCRAW_ERROR 1 /* General dcraw unrecoverable error */
//#define UFRAW_DCRAW_UNSUPPORTED 2
//#define UFRAW_DCRAW_NO_CAMERA_WB 3
//#define UFRAW_DCRAW_VERBOSE 4
//#define UFRAW_DCRAW_OPEN_ERROR 5
#define UFRAW_DCRAW_SET_LOG 4 /* DCRAW_VERBOSE */
#define UFRAW_ERROR 100
#define UFRAW_CANCEL 101
#define UFRAW_RC_VERSION 103 /* Mismatch in version from .ufrawrc */
#define UFRAW_WARNING 104
#define UFRAW_MESSAGE 105
#define UFRAW_SET_ERROR 200
#define UFRAW_SET_WARNING 201
#define UFRAW_SET_LOG 202
#define UFRAW_GET_ERROR 203 /* Return the warning buffer if an error occured */
#define UFRAW_GET_WARNING 204 /* Return the warning buffer */
#define UFRAW_GET_LOG 205 /* Return the log buffer */
#define UFRAW_BATCH_MESSAGE 206
#define UFRAW_INTERACTIVE_MESSAGE 207
#define UFRAW_REPORT 208 /* Report previous messages */
#define UFRAW_CLEAN 209 /* Clean all buffers */
#define UFRAW_RESET 210 /* Reset warnings and errors */
#define UFRAW_SET_PARENT 211 /* Set parent window for message dialog */

#endif /*_UFRAW_H*/

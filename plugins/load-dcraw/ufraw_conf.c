/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_conf.c - handle configuration issues
 * Copyright 2004-2015 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufraw.h"
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h> /* needed for fstat() */
#include <getopt.h>
#include <glib/gi18n.h>

#ifdef HAVE_EXIV2
#include <exiv2/exv_conf.h>
#endif

const conf_data conf_default = {
    /* Internal data */
    7, /* configuration version */

    NULL, /* ufobject */
    /* Image manipulation settings */
    0.0, /* wavelet denoising threshold */
    0.0, /* hotpixel sensitivity */
#ifdef UFRAW_CONTRAST
    1.0, /* global contrast */
#endif
    0.0, 1.0, 0.0, /* exposure, saturation, black */
    0, /* ExposureNorm */
    restore_lch_details, /* restoreDetails */
    digital_highlights, /* clipHighlights */
    disabled_state, /* autoExposure */
    disabled_state, /* autoBlack */
    enabled_state, /* fullCrop */
    disabled_state, /* autoCrop */
    camera_curve, camera_curve + 1, /* BaseCurveIndex, BaseCurveCount */
    /* BaseCurve data defaults */
    { {
            N_("Manual curve"), TONE_CURVE, 0.0, 1.0, 0.0, 1.0, 1.0,
            2 , { { 0.0, 0.0 }, { 1.0, 1.0 } }
        },
        {
            N_("Linear curve"), TONE_CURVE, 0.0, 1.0, 0.0, 1.0, 1.0,
            2 , { { 0.0, 0.0 }, { 1.0, 1.0 } }
        },
        {
            N_("Custom curve"), TONE_CURVE, 0.0, 1.0, 0.0, 1.0, 1.0,
            2 , { { 0.0, 0.0 }, { 1.0, 1.0 } }
        },
        {
            N_("Camera curve"), TONE_CURVE, 0.0, 1.0, 0.0, 1.0, 1.0,
            2 , { { 0.0, 0.0 }, { 1.0, 1.0 } }
        }
    },
    linear_curve, linear_curve + 1, /* curveIndex, curveCount */
    /* Curve data defaults */
    { {
            N_("Manual curve"), TONE_CURVE, 0.0, 1.0, 0.0, 1.0, 1.0,
            2 , { { 0.0, 0.0 }, { 1.0, 1.0 } }
        },
        {
            N_("Linear curve"), TONE_CURVE, 0.0, 1.0, 0.0, 1.0, 1.0,
            2 , { { 0.0, 0.0 }, { 1.0, 1.0 } }
        }
    },
    { 1, 0, 0 } , { 2, 2, 2 }, /* profileIndex[], profileCount[] */
    /* Profile data defaults */
    {   {   { N_("No profile"), "", "", 0.45, 0.1, 0 },
            { N_("Color matrix"), "", "", 0.45, 0.1, 0 },
            { "Some ICC Profile", "", "", 0.45, 0.0, 0 }
        },
        {   { N_("sRGB"), "", "", 0.0, 0.0, 8 },
            { N_("sRGB (embedded)"), "", "", 0.0, 0.0, 8 },
            { "Some ICC Profile", "", "", 0.0, 0.0, 8 }
        },
        {   { N_("System default"), "", "", 0.0, 0.0, 0 },
            { N_("sRGB"), "", "", 0.0, 0.0, 0 },
            { "Some ICC Profile", "", "", 0.0, 0.0, 0 }
        }
    },
    { 0, 0, 0 }, /* intent */
    ahd_interpolation, 0, /* interpolation, smoothing */
    "", NULL, /* darkframeFile, darkframe */
    -1, -1, -1, -1, /* Crop X1,Y1,X2,Y2 */
    0.0, /* aspectRatio */
    -1, /* orientation */
    0, /* rotationAngle */
    0, /* lightness adjustment count */
    {
        { 1, 0, 0 },
        { 1, 0, 0 },
        { 1, 0, 0 },
    }, /* lightness adjustments */
    grayscale_none, /* grayscale mode */
    { 1.0, 1.0, 1.0 }, /* grayscale mixer */
    1, /* grayscale mixer defined */
    { 0.0, 0.0, 0.0, 0.0 }, /* despeckle window */
    { 0.0, 0.0, 0.0, 0.0 }, /* despeckle color decay */
    { 1.0, 1.0, 1.0, 1.0 }, /* despeckle passes */
    /* Save options */
    "", "", "", /* inputFilename, outputFilename, outputPath */
    "", "", /* inputURI, inputModTime */
    ppm_type, 85, no_id, /* type, compression, createID */
    TRUE, /* embedExif */
    FALSE, /* progressiveJPEG */
    1, 0, /* shrink, size */
    FALSE, /* overwrite existing files without asking */
    FALSE, /* losslessCompress */
    FALSE, /* load embedded preview image */
    TRUE, /* rotate to camera's setting */

    /* GUI settings */
    25.0, TRUE, /* Zoom, LockAspect */
    enabled_state, /* saveConfiguration */
    rgb_histogram, /* histogram */
    linear_histogram, /* liveHistogramScale */
    linear_histogram, /* rawHistogramScale */
    { TRUE, TRUE }, /* expander[] */
    FALSE, /* overExp indicator */
    FALSE, /* underExp indicator */
    TRUE, /* blinkOverUnder indicators */
    FALSE, /* RememberOutputPath */
    FALSE, /* WindowMaximized */
    0, /* number of helper lines to draw */
    "", "", /* curvePath, profilePath */
    FALSE, /* silent */
#ifdef _WIN32
    "gimp-win-remote gimp-2.8.exe", /* remoteGimpCommand */
#elif HAVE_GIMP_2_4
    "gimp", /* remoteGimpCommand */
#else
    "gimp-remote", /* remoteGimpCommand */
#endif

    /* EXIF data */
    0, /* CameraOrientation */
    0.0, 0.0, 0.0, 0.0, 1.0, /* iso_speed, shutter, aperture, focal_len, subject_dist */
    "", "", "", "", /* exifSource, isoText, shutterText, apertureText */
    "", "", "", "", /* focalLenText, focalLen35Text, lensText, flashText */
    "", /* whiteBalanceText */
    "", "", "", /* timestamp, make, model */
    0, /* timestamp */
    "", "", /* real_make, real_model */
};

static const char *interpolationNames[] = {
    "ahd", "vng", "four-color", "ppg", "bilinear", "xtrans", "none", "half",
    "eahd", NULL
};
static const char *restoreDetailsNames[] =
{ "clip", "lch", "hsv", NULL };
static const char *clipHighlightsNames[] =
{ "digital", "film", NULL };
static const char *intentNames[] =
{ "perceptual", "relative", "saturation", "absolute", "disable", NULL };
static const char *grayscaleModeNames[] =
{ "none", "lightness", "luminance", "value", "mixer", NULL };

void conf_init(conf_data *c)
{
    *c = conf_default;
}

static int conf_find_name(const char name[], const char *namesList[], int notFound)
{
    int i;
    for (i = 0; namesList[i] != NULL; i++) {
        if (!strcmp(name, namesList[i])) return i;
    }
    return notFound;
}

static const char *conf_get_name(const char *namesList[], int index)
{
    int i;
    for (i = 0; namesList[i] != NULL; i++)
        if (i == index) return namesList[i];
    return "Error";
}

typedef struct {
    conf_data *conf;
    UFObject *group;
    GQuark ufrawQuark;
} parse_data;

static void conf_parse_start(GMarkupParseContext *context,
                             const gchar *element, const gchar **names, const gchar **values,
                             gpointer user, GError **error)
{
    parse_data *data = (parse_data *)user;
    conf_data *c = data->conf;
    int int_value;
    (void)context;

    int i;
    for (i = 0; names[i] != NULL; i++) {
        if (strcmp(names[i], "Index") == 0) {
            if (!ufgroup_has(data->group, element)) {
                ufraw_message(UFRAW_WARNING,
                              "UFGroup '%s' does not contain UFArray '%s'",
                              ufobject_name(data->group), element);
                return;
            }
            data->group = ufgroup_element(data->group, element);
            if (!ufobject_set_string(data->group, values[i])) {
                ufraw_message(UFRAW_WARNING,
                              "UFArray set '%s' to string value '%s' failed",
                              ufobject_name(data->group), values[i]);
            }
            return;
        } else if (strcmp(names[i], "Label") == 0) {
            // We assume that only UFArray elements have a label.
            if (!ufgroup_has(data->group, values[i])) {
                ufraw_message(UFRAW_WARNING,
                              "UFArray '%s' does not contain UFObject '%s'",
                              ufobject_name(data->group), element);
                return;
            }
            data->group = ufgroup_element(data->group, values[i]);
            if (strcmp(ufobject_name(data->group), element) != 0)
                g_set_error(error, data->ufrawQuark, UFRAW_ERROR,
                            "Expecting '%s' XML element and not '%s' XML element",
                            ufobject_name(data->group), element);
            return;
        }
    }
    if (ufgroup_has(data->group, element)) {
        data->group = ufgroup_element(data->group, element);
        return;
    }
    while (*names != NULL) {
        if (!strcasecmp(*values, "yes"))
            int_value = 1;
        if (!strcasecmp(*values, "no"))
            int_value = 0;
        else
            sscanf(*values, "%d", &int_value);
        if (!strcmp(element, "UFRaw") && !strcmp(*names, "Version")) {
            if (int_value == 3) {
                ufraw_message(UFRAW_WARNING,
                              _("Trying to convert .ufrawrc from UFRaw-0.4 or earlier"));
                c->version = int_value;
            }
            /* In version 7 temperature calculation has changed */
            if (int_value == 5) {
                ufraw_message(UFRAW_WARNING,
                              _("Trying to convert .ufrawrc from UFRaw-0.6 or earlier"));
                c->version = int_value;
            }
            if (int_value != c->version)
                g_set_error(error, data->ufrawQuark, UFRAW_RC_VERSION,
                            _("UFRaw version in .ufrawrc is not supported"));
        }
        if (!strcmp(*names, "Current") && int_value != 0) {
            if (!strcmp("BaseManualCurve", element))
                c->BaseCurveIndex = manual_curve;
            if (!strcmp("BaseLinearCurve", element))
                c->BaseCurveIndex = linear_curve;
            if (!strcmp("BaseCustomCurve", element))
                c->BaseCurveIndex = custom_curve;
            if (!strcmp("BaseCameraCurve", element))
                c->BaseCurveIndex = camera_curve;
            if (!strcmp("BaseCurve", element))
                c->BaseCurveIndex = c->BaseCurveCount;
            if (!strcmp("ManualCurve", element))
                c->curveIndex = manual_curve;
            if (!strcmp("LinearCurve", element))
                c->curveIndex = linear_curve;
            if (!strcmp("Curve", element))
                c->curveIndex = c->curveCount;
            // For compatibility with ufraw-0.13 or older
            if (!strcmp("sRGBInputProfile", element))
                c->profileIndex[in_profile] = 0;
            if (!strcmp("NoInputProfile", element))
                c->profileIndex[in_profile] = 0;
            if (!strcmp("MatrixInputProfile", element))
                c->profileIndex[in_profile] = 1;
            if (!strcmp("sRGBOutputProfile", element))
                c->profileIndex[out_profile] = 0;
            if (!strcmp("sRGBEmbeddedOutputProfile", element))
                c->profileIndex[out_profile] = 1;
            if (!strcmp("SystemDisplayProfile", element))
                c->profileIndex[display_profile] = 0;
            if (!strcmp("sRGBDisplayProfile", element))
                c->profileIndex[display_profile] = 1;
            if (!strcmp("InputProfile", element))
                c->profileIndex[in_profile] = c->profileCount[in_profile];
            if (!strcmp("OutputProfile", element))
                c->profileIndex[out_profile] = c->profileCount[out_profile];
            if (!strcmp("DisplayProfile", element))
                c->profileIndex[display_profile] =
                    c->profileCount[display_profile];
        }
        names++;
        values++;
    }
    /* The default curve/profile count is always larger than 0,
     * threfore we can treat 0 as a negative number.
     * m_numAnchors==0 marks that no anchors where read from the XML. */
    if (!strcmp("BaseManualCurve", element)) {
        c->BaseCurveCount = - manual_curve;
        c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 0;
    }
    if (!strcmp("BaseLinearCurve", element)) {
        c->BaseCurveCount = - linear_curve;
        c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 0;
    }
    if (!strcmp("BaseCustomCurve", element)) {
        c->BaseCurveCount = - custom_curve;
        c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 0;
    }
    if (!strcmp("BaseCameraCurve", element)) {
        c->BaseCurveCount = - camera_curve;
        c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 0;
    }
    if (!strcmp("ManualCurve", element)) {
        c->curveCount = - manual_curve;
        c->curve[-c->curveCount].m_numAnchors = 0;
    }
    if (!strcmp("LinearCurve", element)) {
        c->curveCount = - linear_curve;
        c->curve[-c->curveCount].m_numAnchors = 0;
    }
    if (!strcmp("NoInputProfile", element))
        c->profileCount[in_profile] = 0;
    if (!strcmp("MatrixInputProfile", element))
        c->profileCount[in_profile] = -1;
    // For compatibility with ufraw-0.13 or older
    if (!strcmp("sRGBInputProfile", element))
        c->profileCount[in_profile] = -1;
    if (!strcmp("sRGBOutputProfile", element))
        c->profileCount[out_profile] = 0;
    if (!strcmp("sRGBEmbeddedOutputProfile", element))
        c->profileCount[out_profile] = -1;
    if (!strcmp("SystemDisplayProfile", element))
        c->profileCount[display_profile] = 0;
    if (!strcmp("sRGBDisplayProfile", element))
        c->profileCount[display_profile] = -1;
}

static void conf_parse_end(GMarkupParseContext *context, const gchar *element,
                           gpointer user, GError **error)
{
    parse_data *data = (parse_data *)user;
    conf_data *c = data->conf;

    (void)context;
    (void)error;

    if (strcmp(ufobject_name(data->group), element) == 0) {
        data->group = ufobject_parent(data->group);
        return;
    }
    if (c->BaseCurveCount <= 0 &&
            (!strcmp("BaseManualCurve", element) ||
             !strcmp("BaseLinearCurve", element) ||
             !strcmp("BaseCustomCurve", element) ||
             !strcmp("BaseCameraCurve", element))) {
        if (c->BaseCurve[-c->BaseCurveCount].m_numAnchors == 0)
            c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 2;
        c->BaseCurveCount = camera_curve + 1;
    }
    if (c->BaseCurveCount <= 0 && !strcmp("BaseCurve", element)) {
        if (c->BaseCurve[-c->BaseCurveCount].m_numAnchors == 0)
            c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 2;
        c->BaseCurveCount = - c->BaseCurveCount + 1;
    }
    if (c->curveCount <= 0 &&
            (!strcmp("ManualCurve", element) ||
             !strcmp("LinearCurve", element))) {
        if (c->curve[-c->curveCount].m_numAnchors == 0)
            c->curve[-c->curveCount].m_numAnchors = 2;
        c->curveCount = linear_curve + 1;
    }
    if (c->curveCount <= 0 && !strcmp("Curve", element)) {
        if (c->curve[-c->curveCount].m_numAnchors == 0)
            c->curve[-c->curveCount].m_numAnchors = 2;
        c->curveCount = - c->curveCount + 1;
    }
    // For compatibility with ufraw-0.13 or older
    if (!strcmp("sRGBInputProfile", element))
        c->profileCount[in_profile] =
            conf_default.profileCount[in_profile];
    if (!strcmp("NoInputProfile", element))
        c->profileCount[in_profile] =
            conf_default.profileCount[in_profile];
    if (!strcmp("MatrixInputProfile", element))
        c->profileCount[in_profile] =
            conf_default.profileCount[in_profile];
    if (!strcmp("sRGBOutputProfile", element))
        c->profileCount[out_profile] =
            conf_default.profileCount[out_profile];
    if (!strcmp("sRGBEmbeddedOutputProfile", element))
        c->profileCount[out_profile] =
            conf_default.profileCount[out_profile];
    if (!strcmp("SystemDisplayProfile", element))
        c->profileCount[display_profile] =
            conf_default.profileCount[display_profile];
    if (!strcmp("sRGBDisplayProfile", element))
        c->profileCount[display_profile] =
            conf_default.profileCount[display_profile];
    if (c->profileCount[in_profile] <= 0 && strcmp("InputProfile", element) == 0)
        c->profileCount[in_profile] = -c->profileCount[in_profile] + 1;
    if (c->profileCount[out_profile] <= 0 &&
            strcmp("OutputProfile", element) == 0)
        c->profileCount[out_profile] = - c->profileCount[out_profile] + 1;
    if (c->profileCount[display_profile] <= 0 &&
            strcmp("DisplayProfile", element) == 0)
        c->profileCount[display_profile] = -c->profileCount[display_profile] + 1;
}

static void conf_parse_text(GMarkupParseContext *context, const gchar *text,
                            gsize len, gpointer user, GError **error)
{
    parse_data *data = (parse_data *)user;
    conf_data *c = data->conf;

    const gchar *element = g_markup_parse_context_get_element(context);
    char temp[max_path];
    int i;
    error = error;
    for (; len > 0 && g_ascii_isspace(*text); len--, text++);
    for (; len > 0 && g_ascii_isspace(text[len - 1]); len--);
    if (len == 0) return;
    if (len > max_path - 1) len = max_path - 1;
    strncpy(temp, text, len);
    temp[len] = '\0';

    if (strcmp(ufobject_name(data->group), element) == 0) {
        if (!ufobject_set_string(data->group, text))
            ufraw_message(UFRAW_WARNING,
                          "UFObject set '%s' to string value '%s' failed",
                          ufobject_name(data->group), text);
        return;
    }
    if (c->curveCount <= 0) {
        i = - c->curveCount;
        /* for backward compatibility with ufraw-0.4 */
        if (!strcmp("File", element)) {
            /* Ignore this curve if curve_load() fails */
            if (curve_load(&c->curve[i], temp) != UFRAW_SUCCESS)
                c->curveCount = - c->curveCount;
        }
        if (!strcmp("MinXY", element)) {
            sscanf(temp, "%lf %lf", &c->curve[i].m_min_x, &c->curve[i].m_min_y);
            c->curve[i].m_min_x = LIM(c->curve[i].m_min_x, 0, 1);
            c->curve[i].m_min_y = LIM(c->curve[i].m_min_y, 0, 1);
        }
        if (!strcmp("MaxXY", element)) {
            sscanf(temp, "%lf %lf", &c->curve[i].m_max_x, &c->curve[i].m_max_y);
            c->curve[i].m_max_x = LIM(c->curve[i].m_max_x, 0, 1);
            c->curve[i].m_max_y = LIM(c->curve[i].m_max_y, 0, 1);
        }
        if (!strcmp("AnchorXY", element)) {
            if (c->curve[i].m_numAnchors == max_anchors) {
                ufraw_message(UFRAW_WARNING,
                              _("Too many anchors for curve '%s'"), c->curve[i].name);
                /* We try to keep the last anchor point */
                c->curve[i].m_numAnchors--;
            }
            /* If one anchor is supplied then all anchors should be supplied */
            sscanf(temp, "%lf %lf",
                   &c->curve[i].m_anchors[c->curve[i].m_numAnchors].x,
                   &c->curve[i].m_anchors[c->curve[i].m_numAnchors].y);
            c->curve[i].m_anchors[c->curve[i].m_numAnchors].x =
                LIM(c->curve[i].m_anchors[c->curve[i].m_numAnchors].x, 0, 1);
            c->curve[i].m_anchors[c->curve[i].m_numAnchors].y =
                LIM(c->curve[i].m_anchors[c->curve[i].m_numAnchors].y, 0, 1);
            c->curve[i].m_numAnchors++;
        }
        return;
    }

    if (c->BaseCurveCount <= 0) {
        i = - c->BaseCurveCount;
        if (!strcmp("MinXY", element)) {
            sscanf(temp, "%lf %lf", &c->BaseCurve[i].m_min_x, &c->BaseCurve[i].m_min_y);
            c->BaseCurve[i].m_min_x = LIM(c->BaseCurve[i].m_min_x, 0, 1);
            c->BaseCurve[i].m_min_y = LIM(c->BaseCurve[i].m_min_y, 0, 1);
        }
        if (!strcmp("MaxXY", element)) {
            sscanf(temp, "%lf %lf", &c->BaseCurve[i].m_max_x, &c->BaseCurve[i].m_max_y);
            c->BaseCurve[i].m_max_x = LIM(c->BaseCurve[i].m_max_x, 0, 1);
            c->BaseCurve[i].m_max_y = LIM(c->BaseCurve[i].m_max_y, 0, 1);
        }
        if (!strcmp("AnchorXY", element)) {
            /* If one anchor is supplied then all anchors should be supplied */
            sscanf(temp, "%lf %lf",
                   &c->BaseCurve[i].m_anchors[c->BaseCurve[i].m_numAnchors].x,
                   &c->BaseCurve[i].m_anchors[c->BaseCurve[i].m_numAnchors].y);
            c->BaseCurve[i].m_anchors[c->BaseCurve[i].m_numAnchors].x =
                LIM(c->BaseCurve[i].m_anchors[c->BaseCurve[i].m_numAnchors].x, 0, 1);
            c->BaseCurve[i].m_anchors[c->BaseCurve[i].m_numAnchors].y =
                LIM(c->BaseCurve[i].m_anchors[c->BaseCurve[i].m_numAnchors].y, 0, 1);
            c->BaseCurve[i].m_numAnchors++;
        }
        return;
    }

    if (c->profileCount[in_profile] <= 0) {
        i = - c->profileCount[in_profile];
        if (!strcmp("File", element))
            g_strlcpy(c->profile[in_profile][i].file, temp, max_path);
        if (!strcmp("ProductName", element))
            g_strlcpy(c->profile[in_profile][i].productName, temp, max_name);
        if (!strcmp("Gamma", element))
            sscanf(temp, "%lf", &c->profile[in_profile][i].gamma);
        if (!strcmp("Linearity", element))
            sscanf(temp, "%lf", &c->profile[in_profile][i].linear);
        if (!strcmp("UseColorMatrix", element)) {
            gboolean useMatrix;
            sscanf(temp, "%d", &useMatrix);
            // TODO: choose between 'No profile' and 'Color matrix'
        }
        return;
    }
    if (c->profileCount[out_profile] <= 0) {
        i = - c->profileCount[out_profile];
        if (!strcmp("File", element)) {
            char *utf8 = g_filename_display_name(temp);
            g_strlcpy(c->profile[out_profile][i].file, utf8, max_path);
            g_free(utf8);
        }
        if (!strcmp("ProductName", element))
            g_strlcpy(c->profile[out_profile][i].productName, temp, max_name);
        if (!strcmp("BitDepth", element))
            sscanf(temp, "%d", &c->profile[out_profile][i].BitDepth);
        return;
    }
    if (c->profileCount[display_profile] <= 0) {
        i = - c->profileCount[display_profile];
        if (!strcmp("File", element)) {
            char *utf8 = g_filename_display_name(temp);
            g_strlcpy(c->profile[display_profile][i].file, utf8, max_path);
            g_free(utf8);
        }
        if (!strcmp("ProductName", element))
            g_strlcpy(c->profile[display_profile][i].productName, temp,
                      max_name);
        return;
    }
    if (!strcmp("BaseCurve", element)) {
        c->BaseCurveCount = - c->BaseCurveCount;
        i = - c->BaseCurveCount;
        c->BaseCurve[i] = conf_default.BaseCurve[0];
        g_strlcpy(c->BaseCurve[i].name, temp, max_name);
        /* m_numAnchors==0 marks that no anchors where read from the XML */
        c->BaseCurve[-c->BaseCurveCount].m_numAnchors = 0;
    }
    if (!strcmp("Curve", element)) {
        c->curveCount = - c->curveCount;
        i = - c->curveCount;
        c->curve[i] = conf_default.curve[0];
        g_strlcpy(c->curve[i].name, temp, max_name);
        /* m_numAnchors==0 marks that no anchors where read from the XML */
        c->curve[-c->curveCount].m_numAnchors = 0;
    }
    if (!strcmp("InputProfile", element)) {
        c->profileCount[in_profile] = - c->profileCount[in_profile];
        i = - c->profileCount[in_profile];
        c->profile[in_profile][i] = conf_default.profile[in_profile][1];
        g_strlcpy(c->profile[in_profile][i].name, temp, max_name);
    }
    if (!strcmp("OutputProfile", element)) {
        c->profileCount[out_profile] = - c->profileCount[out_profile];
        i = - c->profileCount[out_profile];
        c->profile[out_profile][i] = conf_default.profile[out_profile][0];
        g_strlcpy(c->profile[out_profile][i].name, temp, max_name);
    }
    if (!strcmp("DisplayProfile", element)) {
        c->profileCount[display_profile] = - c->profileCount[display_profile];
        i = - c->profileCount[display_profile];
        c->profile[display_profile][i] =
            conf_default.profile[display_profile][0];
        g_strlcpy(c->profile[display_profile][i].name, temp, max_name);
    }
    if (!strcmp("InputFilename", element)) {
        char *utf8 = g_filename_from_utf8(temp, -1, NULL, NULL, NULL);
        if (utf8 != NULL)
            g_strlcpy(c->inputFilename, utf8, max_path);
        g_free(utf8);
    }
    if (!strcmp("OutputFilename", element)) {
        char *utf8 = g_filename_from_utf8(temp, -1, NULL, NULL, NULL);
        if (utf8 != NULL)
            g_strlcpy(c->outputFilename, utf8, max_path);
        g_free(utf8);
    }
    if (!strcmp("OutputPath", element)) {
        char *utf8 = g_filename_from_utf8(temp, -1, NULL, NULL, NULL);
        if (utf8 != NULL)
            g_strlcpy(c->outputPath, utf8, max_path);
        g_free(utf8);
    }
    if (!strcmp("SaveConfiguration", element))
        sscanf(temp, "%d", &c->saveConfiguration);
    if (!strcmp("Interpolation", element)) {
        /* Keep compatibility with old numbers from ufraw-0.5 */
        if (sscanf(temp, "%d", &c->interpolation) == 1) {
            switch (c->interpolation) {
                case 0:
                    c->interpolation = vng_interpolation;
                    break;
                case 1:
                    c->interpolation = four_color_interpolation;
                    break;
                case 2:
                    c->interpolation = bilinear_interpolation;
                    break;
                case 3:
                    c->interpolation = half_interpolation;
                    break;
                default:
                    c->interpolation = ahd_interpolation;
            }
        } else {
            c->interpolation = conf_find_name(temp, interpolationNames,
                                              conf_default.interpolation);
            if (c->interpolation == obsolete_eahd_interpolation) {
                c->interpolation = ahd_interpolation;
                c->smoothing = 3;
            }
        }
    }
    if (!strcmp("ColorSmoothing", element))
        sscanf(temp, "%d", &c->smoothing);
    if (!strcmp("RawExpander", element))
        sscanf(temp, "%d", &c->expander[raw_expander]);
    if (!strcmp("LiveExpander", element))
        sscanf(temp, "%d", &c->expander[live_expander]);
    if (!strcmp("Histogram", element)) sscanf(temp, "%d", &c->histogram);
    if (!strcmp("LiveHistogramScale", element))
        sscanf(temp, "%d", &c->liveHistogramScale);
    if (!strcmp("RawHistogramScale", element))
        sscanf(temp, "%d", &c->rawHistogramScale);
    if (!strcmp("RemoteGimpCommand", element))
        g_strlcpy(c->remoteGimpCommand, temp, max_path);
    if (!strcmp("LockAspectRatio", element)) sscanf(temp, "%d", &c->LockAspect);
    if (!strcmp("OverExposure", element)) sscanf(temp, "%d", &c->overExp);
    if (!strcmp("UnderExposure", element)) sscanf(temp, "%d", &c->underExp);
    if (!strcmp("BlinkOverUnder", element))
        sscanf(temp, "%d", &c->blinkOverUnder);
    if (!strcmp("DrawLines", element))
        sscanf(temp, "%d", &c->drawLines);
    if (!strcmp("RememberOutputPath", element))
        sscanf(temp, "%d", &c->RememberOutputPath);
    if (!strcmp("WindowMaximized", element))
        sscanf(temp, "%d", &c->WindowMaximized);
    if (!strcmp("WaveletDenoisingThreshold", element))
        sscanf(temp, "%lf", &c->threshold);
    if (!strcmp("HotpixelSensitivity", element))
        sscanf(temp, "%lf", &c->hotpixel);
#ifdef UFRAW_CONTRAST
    if (!strcmp("Contrast", element))
        sscanf(temp, "%lf", &c->contrast);
#endif
    if (!strcmp("Exposure", element)) sscanf(temp, "%lf", &c->exposure);
    if (!strcmp("ExposureNorm", element)) sscanf(temp, "%d", &c->ExposureNorm);
    if (!strcmp("Saturation", element)) sscanf(temp, "%lf", &c->saturation);
    if (!strcmp("RestoreDetails", element))
        c->restoreDetails = conf_find_name(temp, restoreDetailsNames,
                                           conf_default.restoreDetails);
    if (!strcmp("ClipHighlights", element))
        c->clipHighlights = conf_find_name(temp, clipHighlightsNames,
                                           conf_default.clipHighlights);
    /* For compatibility with UFRaw-0.10 and earlier. */
    if (!strcmp("Unclip", element)) {
        int unclip;
        sscanf(temp, "%d", &unclip);
        if (unclip) c->restoreDetails = restore_lch_details;
        else c->restoreDetails = clip_details;
    }
    if (!strcmp("AutoExposure", element)) sscanf(temp, "%d", &c->autoExposure);
    if (!strcmp("AutoBlack", element)) sscanf(temp, "%d", &c->autoBlack);
    if (!strcmp("AutoCrop", element)) sscanf(temp, "%d", &c->autoCrop);
    if (!strcmp("CurvePath", element)) {
        char *utf8 = g_filename_from_utf8(temp, -1, NULL, NULL, NULL);
        if (utf8 != NULL)
            g_strlcpy(c->curvePath, utf8, max_path);
        g_free(utf8);
    }
    if (strcmp("Intent", element) == 0) {
        /* Keep compatibility with numbers from ufraw-0.11 */
        if (sscanf(temp, "%d", &i) == 1) c->intent[out_profile] = i;
        else c->intent[out_profile] = conf_find_name(temp, intentNames,
                                          conf_default.intent[out_profile]);
    }
    if (strcmp("LightnessAdjustment", element) == 0) {
        if (c->lightnessAdjustmentCount < max_adjustments) {
            lightness_adjustment *a = &c->lightnessAdjustment[c->lightnessAdjustmentCount];
            sscanf(temp, "%lf %lf %lf",
                   &a->adjustment, &a->hue, &a->hueWidth);
            c->lightnessAdjustmentCount++;
        } else {
            ufraw_message(UFRAW_SET_ERROR,
                          _("Too many lightness adjustments in the ID file, ignored\n"));
        }
    }
    if (strcmp("GrayscaleMode", element) == 0) {
        c->grayscaleMode = (sscanf(temp, "%d", &i) == 1)
                           ? i
                           : conf_find_name(temp, grayscaleModeNames,
                                            conf_default.grayscaleMode);
    }
    c->grayscaleMixerDefined = 0;
    if (strcmp("GrayscaleMixer", element) == 0) {
        sscanf(temp, "%lf %lf %lf", &c->grayscaleMixer[0],
               &c->grayscaleMixer[1], &c->grayscaleMixer[2]);
        c->grayscaleMixerDefined = 1;
    }
    if (strcmp("DespeckleWindow", element) == 0) {
        sscanf(temp, "%lf %lf %lf", &c->despeckleWindow[0],
               &c->despeckleWindow[1], &c->despeckleWindow[2]);
    }
    if (strcmp("DespeckleDecay", element) == 0) {
        sscanf(temp, "%lf %lf %lf", &c->despeckleDecay[0],
               &c->despeckleDecay[1], &c->despeckleDecay[2]);
    }
    if (strcmp("DespecklePasses", element) == 0) {
        sscanf(temp, "%lf %lf %lf", &c->despecklePasses[0],
               &c->despecklePasses[1], &c->despecklePasses[2]);
    }
    /* OutputIntent replaces Intent starting from ufraw-0.12. */
    if (strcmp("OutputIntent", element) == 0)
        c->intent[out_profile] = conf_find_name(temp, intentNames,
                                                conf_default.intent[out_profile]);
    if (strcmp("DisplayIntent", element) == 0)
        c->intent[display_profile] = conf_find_name(temp, intentNames,
                                     conf_default.intent[display_profile]);
    if (!strcmp("Make", element)) g_strlcpy(c->make, temp, max_name);
    if (!strcmp("Model", element)) g_strlcpy(c->model, temp, max_name);
    if (!strcmp("Lens", element)) g_strlcpy(c->lensText, temp, max_name);
    if (!strcmp("DarkframeFile", element))
        g_strlcpy(c->darkframeFile, temp, max_path);
    if (!strcmp("ProfilePath", element)) {
        char *utf8 = g_filename_from_utf8(temp, -1, NULL, NULL, NULL);
        if (utf8 != NULL)
            g_strlcpy(c->profilePath, utf8, max_path);
        g_free(utf8);
    }
    if (!strcmp("Orientation", element)) sscanf(temp, "%d", &c->orientation);
    if (!strcmp("Crop", element)) sscanf(temp, "%d %d %d %d",
                                             &c->CropX1, &c->CropY1, &c->CropX2, &c->CropY2);
    if (!strcmp("AspectRatio", element)) sscanf(temp, "%lf", &c->aspectRatio);
    if (!strcmp("Rotation", element)) sscanf(temp, "%lf", &c->rotationAngle);
    if (!strcmp("Shrink", element)) sscanf(temp, "%d", &c->shrink);
    if (!strcmp("Size", element)) sscanf(temp, "%d", &c->size);
    if (!strcmp("OutputType", element)) sscanf(temp, "%d", &c->type);
    if (!strcmp("CreateID", element)) sscanf(temp, "%d", &c->createID);
    if (!strcmp("EmbedExif", element)) sscanf(temp, "%d", &c->embedExif);
    if (!strcmp("ProgressiveJPEG", element)) sscanf(temp, "%d", &c->progressiveJPEG);
    if (!strcmp("Compression", element)) sscanf(temp, "%d", &c->compression);
    if (!strcmp("Overwrite", element)) sscanf(temp, "%d", &c->overwrite);
    if (!strcmp("LosslessCompression", element))
        sscanf(temp, "%d", &c->losslessCompress);
}

int conf_load(conf_data *c, const char *IDFilename)
{
    char *confFilename, line[max_path], *locale;
    const char *xdgconf;
    FILE *in;
    GMarkupParser parser = {
        &conf_parse_start, &conf_parse_end,
        &conf_parse_text, NULL, NULL
    };
    GMarkupParseContext *context;
    GError *err = NULL;
    int i;

    conf_init(c);
    if (IDFilename == NULL)
        c->ufobject = ufraw_resources_new();
    else
        c->ufobject = ufraw_image_new();
    if (IDFilename == NULL) {
        confFilename = g_build_filename(uf_get_home_dir(), ".ufrawrc", NULL);
        if (!g_file_test(confFilename, G_FILE_TEST_IS_REGULAR))
            if ((xdgconf = g_get_user_config_dir()))
                confFilename = g_build_filename(xdgconf, "ufrawrc", NULL);

        in = g_fopen(confFilename, "r");
        /* We don't mind if confFilename does not exist. */
        if (in == NULL) {
            g_free(confFilename);
            return UFRAW_SUCCESS;
        }
    } else {
        if (!g_file_test(IDFilename, G_FILE_TEST_IS_REGULAR)) {
            ufraw_message(UFRAW_SET_ERROR,
                          _("ID file %s does not appear to be a regular file\n%s\n"),
                          IDFilename, strerror(errno));
            return UFRAW_ERROR;
        }
        if ((in = g_fopen(IDFilename, "r")) == NULL) {
            ufraw_message(UFRAW_SET_ERROR,
                          _("Can't open ID file %s for reading\n%s\n"),
                          IDFilename, strerror(errno));
            return UFRAW_ERROR;
        }
        confFilename = g_strdup(IDFilename);
    }
    g_snprintf(c->inputURI, max_path, "file://%s", confFilename);
    struct stat s;
    fstat(fileno(in), &s);
    g_snprintf(c->inputModTime, max_name, "%d", (int)s.st_mtime);

    locale = uf_set_locale_C();
    parse_data user_data;
    user_data.conf = c;
    if (ufobject_name(c->ufobject) == ufRawImage)
        user_data.group = c->ufobject;
    else
        user_data.group = ufgroup_element(c->ufobject, ufRawImage);
    user_data.ufrawQuark = g_quark_from_static_string("UFRaw");
    context = g_markup_parse_context_new(&parser, 0, &user_data, NULL);
    line[max_path - 1] = '\0';
    if (fgets(line, max_path - 1, in) == NULL && !feof(in)) {
        ufraw_message(UFRAW_ERROR, _("Error reading from file '%s'."),
                      confFilename);
        uf_reset_locale(locale);
        g_free(confFilename);
        fclose(in);
        return UFRAW_ERROR;
    }
    while (!feof(in)) {
        if (!g_markup_parse_context_parse(context, line, strlen(line), &err)) {
            ufraw_message(UFRAW_ERROR, _("Error parsing '%s'\n%s"),
                          confFilename, err->message);
            g_markup_parse_context_free(context);
            uf_reset_locale(locale);
            g_free(confFilename);
            fclose(in);
            // We could if needed check explicitly for a version mismatch error
            //GQuark ufrawQuark = g_quark_from_static_string("UFRaw");
            //if (g_error_matches(err, data->ufrawQuark, UFRAW_RC_VERSION))
            g_error_free(err);
            return UFRAW_ERROR;
        }
        if (fgets(line, max_path, in) == NULL && !feof(in)) {
            ufraw_message(UFRAW_ERROR, _("Error reading from file '%s'."),
                          confFilename);
            uf_reset_locale(locale);
            g_free(confFilename);
            fclose(in);
            return UFRAW_ERROR;
        }
    }
    g_markup_parse_context_end_parse(context, NULL);
    g_markup_parse_context_free(context);
    uf_reset_locale(locale);
    g_free(confFilename);
    fclose(in);

    if (c->version == 3) {
        c->version = conf_default.version;
        /* Don't add linear part to existing profile curves (except sRGB) */
        for (i = 2; i < c->profileCount[in_profile]; i++)
            c->profile[in_profile][i].linear = 0.0;
    }
    if (c->version == 5) {
        c->version = conf_default.version;
    }
    // Display profile should not be read from ID files.
    if (IDFilename != NULL)
        c->profileIndex[display_profile] =
            conf_default.profileIndex[display_profile];

    // Support OutputType's deprecated in UFRaw-0.14
    if (c->type == ppm16_deprecated_type) {
        c->type = ppm_type;
        c->profile[out_profile][c->profileIndex[out_profile]].BitDepth = 16;
    }
    if (c->type == tiff16_deprecated_type) {
        c->type = tiff_type;
        c->profile[out_profile][c->profileIndex[out_profile]].BitDepth = 16;
    }
    if (c->type == png16_deprecated_type) {
        c->type = png_type;
        c->profile[out_profile][c->profileIndex[out_profile]].BitDepth = 16;
    }
    /* a few consistency settings */
    if (c->curveIndex >= c->curveCount) c->curveIndex = conf_default.curveIndex;
    return UFRAW_SUCCESS;
}

void conf_file_load(conf_data *conf, char *confFilename)
{
    /* Load the --conf file. version==0 means ignore conf. */
    conf->version = 0;
    if (strlen(confFilename) > 0) {
        int status = conf_load(conf, confFilename);
        if (status == UFRAW_SUCCESS) {
            strcpy(conf->inputFilename, "");
            strcpy(conf->outputFilename, "");
            strcpy(conf->outputPath, "");
        } else {
            ufraw_message(UFRAW_REPORT, NULL);
            conf->version = 0;
        }
    }
}

int conf_save(conf_data *c, char *IDFilename, char **confBuffer)
{
    char *buf = NULL;
    int i, j;

    char *locale = uf_set_locale_C();

    buf = uf_markup_buf(buf, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    buf = uf_markup_buf(buf, "<UFRaw Version='%d'>\n", c->version);
    if (strlen(c->inputFilename) > 0 && IDFilename != NULL) {
        char *utf8 = g_filename_display_name(c->inputFilename);
        buf = uf_markup_buf(buf, "<InputFilename>%s</InputFilename>\n", utf8);
        g_free(utf8);
    }
    if (strlen(c->outputFilename) > 0 && IDFilename != NULL) {
        char *utf8 = g_filename_display_name(c->outputFilename);
        buf = uf_markup_buf(buf, "<OutputFilename>%s</OutputFilename>\n", utf8);
        g_free(utf8);
    }
    if (strlen(c->outputPath) > 0) {
        char *utf8 = g_filename_display_name(c->outputPath);
        buf = uf_markup_buf(buf, "<OutputPath>%s</OutputPath>\n", utf8);
        g_free(utf8);
    }

    /* GUI settings are only saved to .ufrawrc */
    if (IDFilename == NULL) {
        if (c->saveConfiguration != conf_default.saveConfiguration)
            buf = uf_markup_buf(buf,
                                "<SaveConfiguration>%d</SaveConfiguration>\n",
                                c->saveConfiguration);
        if (c->expander[raw_expander] != conf_default.expander[raw_expander])
            buf = uf_markup_buf(buf, "<RawExpander>%d</RawExpander>\n",
                                c->expander[raw_expander]);
        if (c->expander[live_expander] != conf_default.expander[live_expander])
            buf = uf_markup_buf(buf, "<LiveExpander>%d</LiveExpander>\n",
                                c->expander[live_expander]);
        if (c->histogram != conf_default.histogram)
            buf = uf_markup_buf(buf,
                                "<Histogram>%d</Histogram>\n", c->histogram);
        if (c->liveHistogramScale != conf_default.liveHistogramScale)
            buf = uf_markup_buf(buf,
                                "<LiveHistogramScale>%d</LiveHistogramScale>\n",
                                c->liveHistogramScale);
        if (c->rawHistogramScale != conf_default.rawHistogramScale)
            buf = uf_markup_buf(buf,
                                "<RawHistogramScale>%d</RawHistogramScale>\n",
                                c->rawHistogramScale);
        if (c->LockAspect != conf_default.LockAspect)
            buf = uf_markup_buf(buf,
                                "<LockAspectRatio>%d</LockAspectRatio>\n", c->LockAspect);
        if (c->overExp != conf_default.overExp)
            buf = uf_markup_buf(buf,
                                "<OverExposure>%d</OverExposure>\n", c->overExp);
        if (c->underExp != conf_default.underExp)
            buf = uf_markup_buf(buf,
                                "<UnderExposure>%d</UnderExposure>\n", c->underExp);
        if (c->blinkOverUnder != conf_default.blinkOverUnder)
            buf = uf_markup_buf(buf,
                                "<BlinkOverUnder>%d</BlinkOverUnder>\n", c->blinkOverUnder);
        if (c->drawLines != conf_default.drawLines)
            buf = uf_markup_buf(buf,
                                "<DrawLines>%d</DrawLines>\n", c->drawLines);
        if (c->RememberOutputPath != conf_default.RememberOutputPath)
            buf = uf_markup_buf(buf,
                                "<RememberOutputPath>%d</RememberOutputPath>\n",
                                c->RememberOutputPath);
        if (c->WindowMaximized != conf_default.WindowMaximized)
            buf = uf_markup_buf(buf,
                                "<WindowMaximized>%d</WindowMaximized>\n",
                                c->WindowMaximized);
        if (strcmp(c->remoteGimpCommand, conf_default.remoteGimpCommand) != 0)
            buf = uf_markup_buf(buf,
                                "<RemoteGimpCommand>%s</RemoteGimpCommand>\n",
                                c->remoteGimpCommand);
        if (strlen(c->curvePath) > 0) {
            char *utf8 = g_filename_display_name(c->curvePath);
            buf = uf_markup_buf(buf, "<CurvePath>%s</CurvePath>\n", utf8);
            g_free(utf8);
        }
        if (strlen(c->profilePath) > 0) {
            char *utf8 = g_filename_display_name(c->profilePath);
            buf = uf_markup_buf(buf, "<ProfilePath>%s</ProfilePath>\n", utf8);
            g_free(utf8);
        }
    }
    if (c->interpolation != conf_default.interpolation)
        buf = uf_markup_buf(buf, "<Interpolation>%s</Interpolation>\n",
                            conf_get_name(interpolationNames, c->interpolation));
    // 'smoothing' is boolean at the moment, but we keep the option
    // to upgrade it to the number of color smoothing passes.
    if (c->smoothing != 0) {
        if (c->interpolation == ahd_interpolation)
            c->smoothing = 3;
        else
            c->smoothing = 1;
    }
    if (c->smoothing != conf_default.smoothing)
        buf = uf_markup_buf(buf, "<ColorSmoothing>%d</ColorSmoothing>\n",
                            c->smoothing);
    UFObject *image;
    if (ufobject_name(c->ufobject) == ufRawImage)
        image = c->ufobject;
    else
        image = ufgroup_element(c->ufobject, ufRawImage);
    {
        char *xml = ufobject_xml(image, "");
        char *newbuf = g_strconcat(buf, xml, NULL);
        g_free(xml);
        g_free(buf);
        buf = newbuf;
    }
    if (c->threshold != conf_default.threshold)
        buf = uf_markup_buf(buf,
                            "<WaveletDenoisingThreshold>%d</WaveletDenoisingThreshold>\n",
                            (int)floor(c->threshold));
    if (c->hotpixel != conf_default.hotpixel)
        buf = uf_markup_buf(buf,
                            "<HotpixelSensitivity>%f</HotpixelSensitivity>\n",
                            c->hotpixel);
#ifdef UFRAW_CONTRAST
    if (c->contrast != conf_default.contrast)
        buf = uf_markup_buf(buf,
                            "<Contrast>%f</Contrast>\n", c->contrast);
#endif
    if (c->exposure != conf_default.exposure)
        buf = uf_markup_buf(buf, "<Exposure>%lf</Exposure>\n", c->exposure);
    if (c->ExposureNorm != conf_default.ExposureNorm)
        buf = uf_markup_buf(buf, "<ExposureNorm>%d</ExposureNorm>\n",
                            c->ExposureNorm);
    if (c->restoreDetails != conf_default.restoreDetails)
        buf = uf_markup_buf(buf, "<RestoreDetails>%s</RestoreDetails>\n",
                            conf_get_name(restoreDetailsNames, c->restoreDetails));
    if (c->clipHighlights != conf_default.clipHighlights)
        buf = uf_markup_buf(buf, "<ClipHighlights>%s</ClipHighlights>\n",
                            conf_get_name(clipHighlightsNames, c->clipHighlights));
    if (c->autoExposure != conf_default.autoExposure)
        buf = uf_markup_buf(buf,
                            "<AutoExposure>%d</AutoExposure>\n", c->autoExposure);
    if (c->autoBlack != conf_default.autoBlack)
        buf = uf_markup_buf(buf, "<AutoBlack>%d</AutoBlack>\n", c->autoBlack);
    if (c->autoCrop != conf_default.autoCrop)
        buf = uf_markup_buf(buf, "<AutoCrop>%d</AutoCrop>\n", c->autoCrop);
    if (c->saturation != conf_default.saturation)
        buf = uf_markup_buf(buf,
                            "<Saturation>%lf</Saturation>\n", c->saturation);
    for (i = 0; i < max_adjustments; ++i) {
        lightness_adjustment *a = &c->lightnessAdjustment[i];
        if (fabs(a->adjustment - conf_default.lightnessAdjustment[i].adjustment) > 0.01) {
            buf = uf_markup_buf(buf, "<LightnessAdjustment>%f %f %f</LightnessAdjustment>\n",
                                a->adjustment, a->hue, a->hueWidth);
        }
    }
    if (c->grayscaleMode != grayscale_invalid &&
            c->grayscaleMode != conf_default.grayscaleMode)
        buf = uf_markup_buf(buf,
                            "<GrayscaleMode>%s</GrayscaleMode>\n",
                            grayscaleModeNames[c->grayscaleMode]);
    if (c->grayscaleMode == grayscale_mixer) {
        buf = uf_markup_buf(buf,
                            "<GrayscaleMixer>%f %f %f</GrayscaleMixer>\n",
                            c->grayscaleMixer[0],
                            c->grayscaleMixer[1],
                            c->grayscaleMixer[2]);
    }
    if (c->despeckleWindow[0] != conf_default.despeckleWindow[0] ||
            c->despeckleWindow[1] != conf_default.despeckleWindow[1] ||
            c->despeckleWindow[2] != conf_default.despeckleWindow[2]) {
        buf = uf_markup_buf(buf, "<DespeckleWindow>%f %f %f</DespeckleWindow>\n",
                            c->despeckleWindow[0], c->despeckleWindow[1], c->despeckleWindow[2]);
    }
    if (c->despeckleDecay[0] != conf_default.despeckleDecay[0] ||
            c->despeckleDecay[1] != conf_default.despeckleDecay[1] ||
            c->despeckleDecay[2] != conf_default.despeckleDecay[2]) {
        buf = uf_markup_buf(buf, "<DespeckleDecay>%f %f %f</DespeckleDecay>\n",
                            c->despeckleDecay[0], c->despeckleDecay[1], c->despeckleDecay[2]);
    }
    if (c->despecklePasses[0] != conf_default.despecklePasses[0] ||
            c->despecklePasses[1] != conf_default.despecklePasses[1] ||
            c->despecklePasses[2] != conf_default.despecklePasses[2]) {
        buf = uf_markup_buf(buf, "<DespecklePasses>%f %f %f</DespecklePasses>\n",
                            c->despecklePasses[0], c->despecklePasses[1], c->despecklePasses[2]);
    }
    if (c->size != conf_default.size)
        buf = uf_markup_buf(buf, "<Size>%d</Size>\n", c->size);
    if (c->shrink != conf_default.shrink)
        buf = uf_markup_buf(buf, "<Shrink>%d</Shrink>\n", c->shrink);
    if (c->type != conf_default.type)
        buf = uf_markup_buf(buf, "<OutputType>%d</OutputType>\n", c->type);
    if (c->createID != conf_default.createID)
        buf = uf_markup_buf(buf, "<CreateID>%d</CreateID>\n", c->createID);
    if (c->embedExif != conf_default.embedExif)
        buf = uf_markup_buf(buf, "<EmbedExif>%d</EmbedExif>\n", c->embedExif);
    if (c->progressiveJPEG != conf_default.progressiveJPEG)
        buf = uf_markup_buf(buf, "<ProgressiveJPEG>%d</ProgressiveJPEG>\n", c->progressiveJPEG);
    if (c->compression != conf_default.compression)
        buf = uf_markup_buf(buf,
                            "<Compression>%d</Compression>\n", c->compression);
    if (c->overwrite != conf_default.overwrite)
        buf = uf_markup_buf(buf, "<Overwrite>%d</Overwrite>\n", c->overwrite);
    if (c->losslessCompress != conf_default.losslessCompress)
        buf = uf_markup_buf(buf,
                            "<LosslessCompression>%d</LosslessCompression>\n",
                            c->losslessCompress);
    for (i = 0; i < c->BaseCurveCount; i++) {
        char *curveBuf = curve_buffer(&c->BaseCurve[i]);
        /* Write curve if it is non-default and we are not writing to .ufraw */
        /* But ALWAYS write the current curve */
        if (c->BaseCurveIndex == i || (curveBuf != NULL && IDFilename == NULL)) {
            if (curveBuf == NULL) curveBuf = g_strdup("");
            char *current = i == c->BaseCurveIndex ? "yes" : "no";
            switch (i) {
                case manual_curve:
                    buf = uf_markup_buf(buf,
                                        "<BaseManualCurve Current='%s'>\n", current);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</BaseManualCurve>\n");
                    break;
                case linear_curve:
                    buf = uf_markup_buf(buf,
                                        "<BaseLinearCurve Current='%s'>\n", current);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</BaseLinearCurve>\n");
                    break;
                case custom_curve:
                    buf = uf_markup_buf(buf,
                                        "<BaseCustomCurve Current='%s'>\n", current);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</BaseCustomCurve>\n");
                    break;
                case camera_curve:
                    buf = uf_markup_buf(buf,
                                        "<BaseCameraCurve Current='%s'>\n", current);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</BaseCameraCurve>\n");
                    break;
                default:
                    buf = uf_markup_buf(buf,
                                        "<BaseCurve Current='%s'>%s\n", current,
                                        c->BaseCurve[i].name);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</BaseCurve>\n");
            }
        }
        g_free(curveBuf);
    }
    for (i = 0; i < c->curveCount; i++) {
        char *curveBuf = curve_buffer(&c->curve[i]);
        /* Write curve if it is non-default and we are not writing to .ufraw */
        /* But ALWAYS write the current curve */
        if (c->curveIndex == i || (curveBuf != NULL && IDFilename == NULL)) {
            if (curveBuf == NULL) curveBuf = g_strdup("");
            char *current = i == c->curveIndex ? "yes" : "no";
            switch (i) {
                case manual_curve:
                    buf = uf_markup_buf(buf,
                                        "<ManualCurve Current='%s'>\n", current);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</ManualCurve>\n");
                    break;
                case linear_curve:
                    buf = uf_markup_buf(buf,
                                        "<LinearCurve Current='%s'>\n", current);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</LinearCurve>\n");
                    break;
                default:
                    buf = uf_markup_buf(buf,
                                        "<Curve Current='%s'>%s\n", current,
                                        c->curve[i].name);
                    buf = uf_markup_buf(buf, curveBuf);
                    buf = uf_markup_buf(buf, "</Curve>\n");
            }
        }
        g_free(curveBuf);
    }
    for (j = 0; j < profile_types; j++) {
        // Display profile does not belong in ID files.
        if (IDFilename != NULL && j == display_profile)
            continue;
        char *type = j == in_profile ? "InputProfile" :
                     j == out_profile ? "OutputProfile" :
                     j == display_profile ? "DisplayProfile" : "Error";

        for (i = 0; i < c->profileCount[j]; i++) {

            gboolean current = i == c->profileIndex[j];
            /* In ID files we only save the current profiles */
            if (IDFilename != NULL && !current)
                continue;
            /* For the default profiles, if it is not the current profile
             * and nothing change, do not write it. */
            if (i < conf_default.profileCount[j] && !current &&
                    (c->profile[j][i].gamma == conf_default.profile[j][i].gamma &&
                     c->profile[j][i].linear == conf_default.profile[j][i].linear &&
                     c->profile[j][i].BitDepth ==
                     conf_default.profile[j][i].BitDepth))
                continue;
            char *profile = "";
            if (j == in_profile && i == 0) profile = "No";
            if (j == in_profile && i == 1) profile = "Matrix";
            if (j == out_profile && i == 0) profile = "sRGB";
            if (j == out_profile && i == 1) profile = "sRGBEmbedded";
            if (j == display_profile && i == 0) profile = "System";
            if (j == display_profile && i == 1) profile = "sRGB";
            buf = uf_markup_buf(buf, "<%s%s Current='%s'>%s\n",
                                profile, type, current ? "yes" : "no", c->profile[j][i].name);
            if (i >= conf_default.profileCount[j]) {
                char *utf8 = g_filename_display_name(c->profile[j][i].file);
                buf = uf_markup_buf(buf, "\t<File>%s</File>\n", utf8);
                g_free(utf8);
                buf = uf_markup_buf(buf, "\t<ProductName>%s</ProductName>\n",
                                    c->profile[j][i].productName);
            }
            if (c->profile[j][i].gamma != conf_default.profile[j][1].gamma)
                buf = uf_markup_buf(buf,
                                    "\t<Gamma>%lf</Gamma>\n", c->profile[j][i].gamma);
            if (c->profile[j][i].linear != conf_default.profile[j][1].linear)
                buf = uf_markup_buf(buf, "\t<Linearity>%lf</Linearity>\n",
                                    c->profile[j][i].linear);
            if (c->profile[j][i].BitDepth != conf_default.profile[j][1].BitDepth)
                buf = uf_markup_buf(buf, "\t<BitDepth>%d</BitDepth>\n",
                                    c->profile[j][i].BitDepth);
            buf = uf_markup_buf(buf, "</%s%s>\n", profile, type);
        }
    }
    if (c->intent[out_profile] != conf_default.intent[out_profile])
        buf = uf_markup_buf(buf, "<OutputIntent>%s</OutputIntent>\n",
                            conf_get_name(intentNames, c->intent[out_profile]));
    if (c->intent[display_profile] != conf_default.intent[display_profile])
        buf = uf_markup_buf(buf, "<DisplayIntent>%s</DisplayIntent>\n",
                            conf_get_name(intentNames, c->intent[display_profile]));
    /* We always write the Make and Mode information
     * to know if the WB setting is relevant */
    buf = uf_markup_buf(buf, "<Make>%s</Make>\n", c->make);
    buf = uf_markup_buf(buf, "<Model>%s</Model>\n", c->model);
    if (IDFilename != NULL) {
        if (strcmp(c->darkframeFile, conf_default.darkframeFile) != 0)
            buf = uf_markup_buf(buf,
                                "<DarkframeFile>%s</DarkframeFile>\n", c->darkframeFile);
        buf = uf_markup_buf(buf, "<Timestamp>%s</Timestamp>\n",
                            c->timestampText);
        buf = uf_markup_buf(buf, "<Orientation>%d</Orientation>\n",
                            c->orientation);
        buf = uf_markup_buf(buf, "<ISOSpeed>%s</ISOSpeed>\n", c->isoText);
        buf = uf_markup_buf(buf, "<Shutter>%s</Shutter>\n", c->shutterText);
        buf = uf_markup_buf(buf, "<Aperture>%s</Aperture>\n", c->apertureText);
        buf = uf_markup_buf(buf, "<FocalLength>%s</FocalLength>\n",
                            c->focalLenText);
        buf = uf_markup_buf(buf, "<FocalLength35>%s</FocalLength35>\n",
                            c->focalLen35Text);
        if (strlen(c->lensText) > 0)
            buf = uf_markup_buf(buf, "<Lens>%s</Lens>\n", c->lensText);
        buf = uf_markup_buf(buf, "<EXIFSource>%s</EXIFSource>\n",
                            c->exifSource);
        buf = uf_markup_buf(buf, "<Crop>%d %d %d %d</Crop>\n",
                            c->CropX1, c->CropY1, c->CropX2, c->CropY2);
        if (c->aspectRatio != 0.0)
            buf = uf_markup_buf(buf, "<AspectRatio>%lf</AspectRatio>\n", c->aspectRatio);
        buf = uf_markup_buf(buf, "<Rotation>%lf</Rotation>\n", c->rotationAngle);
        char *log = ufraw_message(UFRAW_GET_LOG, NULL);
        if (log != NULL) {
            char *utf8 = g_filename_display_name(log);
            buf = uf_markup_buf(buf, "<Log>\n%s</Log>\n", utf8);
            g_free(utf8);
        }
        /* As long as darkframe is not in the GUI we save it only to ID files.*/
    }
    buf = uf_markup_buf(buf, "</UFRaw>\n");
    uf_reset_locale(locale);
    if (confBuffer == NULL) {
        char *confFilename;
        const char *xdgconf;
        FILE *out;
        if (IDFilename == NULL) {
            confFilename = g_build_filename(uf_get_home_dir(), ".ufrawrc", NULL);
            if (!g_file_test(confFilename, G_FILE_TEST_IS_REGULAR))
                if ((xdgconf = g_get_user_config_dir()))
                    confFilename = g_build_filename(xdgconf, "ufrawrc", NULL);
        } else
            confFilename = g_strdup(IDFilename);
        if ((out = g_fopen(confFilename, "w")) == NULL) {
            ufraw_message(UFRAW_ERROR,
                          _("Can't open file %s for writing\n%s\n"),
                          confFilename, strerror(errno));
            g_free(confFilename);
            g_free(buf);
            return UFRAW_ERROR;
        }
        fputs(buf, out);
        fclose(out);
        g_free(confFilename);
        g_free(buf);
    } else {
        *confBuffer = buf;
    }
    return UFRAW_SUCCESS;
}

/* Copy the image manipulation options from *src to *dst */
void conf_copy_image(conf_data *dst, const conf_data *src)
{
    int i, j;

    UFObject *dstImage;
    if (ufobject_name(dst->ufobject) == ufRawImage)
        dstImage = dst->ufobject;
    else
        dstImage = ufgroup_element(dst->ufobject, ufRawImage);
    ufobject_copy(dstImage, src->ufobject);

    dst->interpolation = src->interpolation;
    dst->smoothing = src->smoothing;
    /* make and model are 'part of' ChanMul,
     * since on different make and model ChanMul are meaningless */
    g_strlcpy(dst->make, src->make, max_name);
    g_strlcpy(dst->model, src->model, max_name);
    dst->threshold = src->threshold;
    dst->exposure = src->exposure;
    dst->hotpixel = src->hotpixel;
#ifdef UFRAW_CONTRAST
    dst->contrast = src->contrast;
#endif
    dst->ExposureNorm = src->ExposureNorm;
    dst->saturation = src->saturation;
    dst->black = src->black;
    dst->autoExposure = src->autoExposure;
    dst->autoBlack = src->autoBlack;
    dst->autoCrop = src->autoCrop;
    if (src->autoCrop == enabled_state)
        dst->fullCrop = disabled_state;
    dst->restoreDetails = src->restoreDetails;
    dst->clipHighlights = src->clipHighlights;
    memcpy(dst->lightnessAdjustment, src->lightnessAdjustment,
           sizeof dst->lightnessAdjustment);
    dst->lightnessAdjustmentCount = src->lightnessAdjustmentCount;
    dst->grayscaleMode = src->grayscaleMode;
    dst->grayscaleMixerDefined = src->grayscaleMixerDefined;
    memcpy(dst->grayscaleMixer, src->grayscaleMixer,
           sizeof dst->grayscaleMixer);
    memcpy(dst->despeckleWindow, src->despeckleWindow, sizeof(dst->despeckleWindow));
    memcpy(dst->despeckleDecay, src->despeckleDecay, sizeof(dst->despeckleDecay));
    memcpy(dst->despecklePasses, src->despecklePasses, sizeof(dst->despecklePasses));
    g_strlcpy(dst->darkframeFile, src->darkframeFile, max_path);
    /* We only copy the current BaseCurve */
    if (src->BaseCurveIndex <= camera_curve) {
        dst->BaseCurveIndex = src->BaseCurveIndex;
        if (src->BaseCurveIndex == manual_curve)
            dst->BaseCurve[manual_curve] = src->BaseCurve[manual_curve];
    } else {
        /* For non-standard curves we look for a curve with the same name
         * and override it, assuming it is the same curve. */
        for (i = camera_curve + 1; i < dst->BaseCurveCount; i++) {
            if (!strcmp(dst->BaseCurve[i].name,
                        src->BaseCurve[src->BaseCurveIndex].name)) {
                dst->BaseCurve[i] = src->BaseCurve[src->BaseCurveIndex];
                dst->BaseCurveIndex = i;
                break;
            }
        }
        /* If the curve was not found we add it. */
        if (i == dst->BaseCurveCount) {
            /* If there is no more room we throw away the last curve. */
            if (dst->BaseCurveCount == max_curves) dst->BaseCurveCount--;
            dst->BaseCurve[dst->BaseCurveCount] =
                src->BaseCurve[src->BaseCurveIndex];
            dst->BaseCurveIndex = dst->BaseCurveCount;
            dst->BaseCurveCount++;
        }
    }
    /* We only copy the current curve */
    if (src->curveIndex <= linear_curve) {
        dst->curveIndex = src->curveIndex;
        if (src->curveIndex == manual_curve)
            dst->curve[manual_curve] = src->curve[manual_curve];
    } else {
        /* For non-standard curves we look for a curve with the same name
         * and override it, assuming it is the same curve. */
        for (i = camera_curve + 1; i < dst->curveCount; i++) {
            if (!strcmp(dst->curve[i].name, src->curve[src->curveIndex].name)) {
                dst->curve[i] = src->curve[src->curveIndex];
                dst->curveIndex = i;
                break;
            }
        }
        /* If the curve was not found we add it. */
        if (i == dst->curveCount) {
            /* If there is no more room we throw away the last curve. */
            if (dst->curveCount == max_curves) dst->curveCount--;
            dst->curve[dst->curveCount] = src->curve[src->curveIndex];
            dst->curveIndex = dst->curveCount;
            dst->curveCount++;
        }
    }
    /* We only copy the current input/output profile */
    for (j = 0; j < profile_types; j++) {
        // Ignore the display profile
        if (j == display_profile)
            continue;
        if (src->profileIndex[j] == 0) {
            dst->profileIndex[j] = src->profileIndex[j];
            dst->profile[j][0] = src->profile[j][0];
        } else {
            /* For non-standard profiles we look for a profile with the same
             * name and override it, assuming it is the same profile. */
            for (i = 1; i < dst->profileCount[j]; i++) {
                if (!strcmp(dst->profile[j][i].name,
                            src->profile[j][src->profileIndex[j]].name)) {
                    dst->profile[j][i] = src->profile[j][src->profileIndex[j]];
                    dst->profileIndex[j] = i;
                    break;
                }
            }
            /* If the profile was not found we add it. */
            if (i == dst->profileCount[j]) {
                /* If there is no more room we throw away the last profile. */
                if (dst->profileCount[j] == max_profiles) dst->profileCount[j]--;
                dst->profile[j][dst->profileCount[j]] =
                    src->profile[j][src->profileIndex[j]];
                dst->profileIndex[j] = dst->profileCount[j];
                dst->profileCount[j]++;
            }
        }
    }
    dst->intent[out_profile] = src->intent[out_profile];
    dst->intent[display_profile] = src->intent[display_profile];
}

/* Copy the transformation information from *src to *dst. */
void conf_copy_transform(conf_data *dst, const conf_data *src)
{
    dst->orientation = src->orientation;
    dst->CropX1 = src->CropX1;
    dst->CropY1 = src->CropY1;
    dst->CropX2 = src->CropX2;
    dst->CropY2 = src->CropY2;
    dst->aspectRatio = src->aspectRatio;
    dst->rotationAngle = src->rotationAngle;

    if (src->CropX1 != -1 || src->CropX2 != -1 ||
            src->CropY1 != -1 || src->CropY2 != -1) {
        dst->fullCrop = disabled_state;
        dst->autoCrop = disabled_state;
    }
}

/* Copy the 'save options' from *src to *dst */
void conf_copy_save(conf_data *dst, const conf_data *src)
{
    /* Filenames get special treatment and are not simply copied
    g_strlcpy(dst->inputFilename, src->inputFilename, max_path);
    g_strlcpy(dst->outputFilename, src->outputFilename, max_path);
    g_strlcpy(dst->outputPath, src->outputPath, max_path);
    */
    g_strlcpy(dst->inputURI, src->inputURI, max_path);
    g_strlcpy(dst->inputModTime, src->inputModTime, max_name);
    dst->type = src->type;
    dst->compression = src->compression;
    dst->createID = src->createID;
    dst->embedExif = src->embedExif;
    dst->shrink = src->shrink;
    dst->size = src->size;
    dst->overwrite = src->overwrite;
    dst->RememberOutputPath = src->RememberOutputPath;
    dst->progressiveJPEG = src->progressiveJPEG;
    dst->losslessCompress = src->losslessCompress;
    dst->embeddedImage = src->embeddedImage;
}

int conf_set_cmd(conf_data *conf, const conf_data *cmd)
{
    UFObject *cmdImage = ufgroup_element(cmd->ufobject, ufRawImage);
    ufobject_copy(conf->ufobject, cmdImage);

    if (cmd->overwrite != -1) conf->overwrite = cmd->overwrite;
    if (cmd->WindowMaximized != -1) conf->WindowMaximized = cmd->WindowMaximized;
    if (cmd->restoreDetails != -1)
        conf->restoreDetails = cmd->restoreDetails;
    if (cmd->clipHighlights != -1)
        conf->clipHighlights = cmd->clipHighlights;
    if (cmd->losslessCompress != -1)
        conf->losslessCompress = cmd->losslessCompress;
    if (cmd->embedExif != -1) conf->embedExif = cmd->embedExif;
    if (cmd->embeddedImage != -1) conf->embeddedImage = cmd->embeddedImage;
    if (cmd->rotate != -1) conf->rotate = cmd->rotate;
    if (cmd->rotationAngle != NULLF) conf->rotationAngle = cmd->rotationAngle;
    if (cmd->autoCrop != -1)
        if ((conf->autoCrop = cmd->autoCrop) == enabled_state)
            conf->fullCrop = disabled_state;
    if (cmd->CropX1 != -1 || cmd->CropX2 != -1 ||
            cmd->CropY1 != -1 || cmd->CropY2 != -1) {
        conf->fullCrop = disabled_state;
        conf->autoCrop = disabled_state;
    }
    if (cmd->CropX1 != -1) conf->CropX1 = cmd->CropX1;
    if (cmd->CropY1 != -1) conf->CropY1 = cmd->CropY1;
    if (cmd->CropX2 != -1) conf->CropX2 = cmd->CropX2;
    if (cmd->CropY2 != -1) conf->CropY2 = cmd->CropY2;
    if (cmd->aspectRatio != 0.0) conf->aspectRatio = cmd->aspectRatio;
    if (cmd->silent != -1) conf->silent = cmd->silent;
    if (cmd->compression != NULLF) conf->compression = cmd->compression;
    if (cmd->autoExposure) {
        conf->autoExposure = cmd->autoExposure;
    }
    if (cmd->threshold != NULLF) conf->threshold = cmd->threshold;
    if (cmd->hotpixel != NULLF) conf->hotpixel = cmd->hotpixel;
#ifdef UFRAW_CONTRAST
    if (cmd->contrast != NULLF) conf->contrast = cmd->contrast;
#endif
    if (cmd->exposure != NULLF) {
        conf->exposure = cmd->exposure;
        conf->autoExposure = disabled_state;
    }
    if (cmd->profile[0][0].gamma != NULLF)
        conf->profile[0][conf->profileIndex[0]].gamma =
            cmd->profile[0][0].gamma;
    if (cmd->profile[0][0].linear != NULLF)
        conf->profile[0][conf->profileIndex[0]].linear =
            cmd->profile[0][0].linear;
    if (cmd->profile[1][0].BitDepth != -1)
        conf->profile[1][conf->profileIndex[1]].BitDepth =
            cmd->profile[1][0].BitDepth;
    if (cmd->saturation != NULLF)
        conf->saturation = cmd->saturation;
    if (cmd->grayscaleMode != -1) {
        conf->grayscaleMode = cmd->grayscaleMode;
        if (cmd->grayscaleMode == grayscale_mixer &&
                cmd->grayscaleMixerDefined == 1) {
            conf->grayscaleMixerDefined = 1;
            conf->grayscaleMixer[0] = cmd->grayscaleMixer[0];
            conf->grayscaleMixer[1] = cmd->grayscaleMixer[1];
            conf->grayscaleMixer[2] = cmd->grayscaleMixer[2];
        }
    }
    if (cmd->BaseCurveIndex >= 0) conf->BaseCurveIndex = cmd->BaseCurveIndex;
    if (cmd->curveIndex >= 0) conf->curveIndex = cmd->curveIndex;
    if (cmd->autoBlack) {
        conf->autoBlack = cmd->autoBlack;
    }
    if (cmd->black != NULLF) {
        CurveDataSetPoint(&conf->curve[conf->curveIndex],
                          0, cmd->black, 0);
        conf->autoBlack = disabled_state;
    }
    if (cmd->smoothing != -1) conf->smoothing = cmd->smoothing;
    if (cmd->interpolation >= 0) conf->interpolation = cmd->interpolation;
    if (cmd->interpolation == obsolete_eahd_interpolation) {
        conf->interpolation = ahd_interpolation;
        conf->smoothing = 3;
    }
    if (cmd->shrink != NULLF) {
        conf->shrink = cmd->shrink;
        conf->size = 0;
        if (conf->interpolation == half_interpolation)
            conf->interpolation = ahd_interpolation;
    }
    if (cmd->size != NULLF) {
        conf->size = cmd->size;
        conf->shrink = 1;
        if (conf->interpolation == half_interpolation)
            conf->interpolation = ahd_interpolation;
    }
    if (cmd->type >= 0) conf->type = cmd->type;
    if (cmd->createID >= 0) conf->createID = cmd->createID;
    if (strlen(cmd->darkframeFile) > 0)
        g_strlcpy(conf->darkframeFile, cmd->darkframeFile, max_path);
    if (cmd->darkframe != NULL)
        conf->darkframe = cmd->darkframe;
    if (strlen(cmd->outputPath) > 0)
        g_strlcpy(conf->outputPath, cmd->outputPath, max_path);
    if (strlen(cmd->outputFilename) > 0) {
        if (conf->createID != no_id && !strcmp(cmd->outputFilename, "-") &&
                !cmd->embeddedImage) {
            ufraw_message(UFRAW_ERROR, _("cannot --create-id with stdout"));
            return UFRAW_ERROR;
        }
        g_strlcpy(conf->outputFilename, cmd->outputFilename, max_path);
    }
    return UFRAW_SUCCESS;
}

/* Following are global strings and functions used by both 'ufraw' and
 * 'ufraw-batch'. ufraw_conf.c is a good home for them since they are
 * closely related to the other configuration functions. */
char *helpText[] = {
    N_("UFRaw "), VERSION, N_(" - Unidentified Flying Raw converter for digital camera images.\n"),
    "\n",
    N_("Usage: ufraw [ options ... ] [ raw-image-files ... ]\n"),
    N_("       ufraw-batch [ options ... ] [ raw-image-files ... ]\n"),
    N_("       ufraw [ options ... ] [ default-directory ]\n"),
    "\n",
    N_("By default 'ufraw' displays a preview window for each raw image allowing\n"
    "the user to tweak the image parameters before saving. If no raw images\n"
    "are given at the command line, UFRaw will display a file chooser dialog.\n"
    "To process the images with no questions asked (and no preview) use\n"
    "'ufraw-batch'.\n"),
    "\n",
    N_("The input files can be either raw images or ufraw's ID files. ID files\n"
    "contain a raw image filename and the parameters for handling the image.\n"
    "One can also use an ID file with the option:\n"),
    "\n",
    N_("--conf=ID-file        Apply the parameters in ID-file to other raw images.\n"),
    "\n",
    N_("The rest of the options are separated into two groups.\n"),
    N_("The options which are related to the image manipulation are:\n"),
    "\n",
    N_("--wb=camera|auto      White balance setting.\n"),
    N_("--temperature=TEMP    Color temperature in Kelvin.\n"),
    N_("--green=GREEN         Green color normalization.\n"),
    N_("--base-curve=manual|linear|camera|custom|CURVE\n"
    "                      Type of base tone curve to use. CURVE can be any\n"
    "                      curve that was previously loaded in the GUI.\n"
    "                      (default camera if such exists, linear otherwise).\n"),
    N_("--base-curve-file=file\n"
    "                      Use base tone curve included in specified file.\n"
    "                      Overrides --base-curve option.\n"),
    N_("--curve=manual|linear|CURVE\n"
    "                      Type of luminosity curve to use. CURVE can be any\n"
    "                      curve that was previously loaded in the GUI.\n"
    "                      (default linear).\n"),
    N_("--curve-file=file     Use luminosity curve included in specified file.\n"
    "                      Overrides --curve option.\n"),
    N_("--restore=clip|lch|hsv\n"
    "                      Restore details for negative EV.\n"
    "                      'clip' restores nothing - safe from artifacts.\n"
    "                      'lch' restores in LCH space - giving soft details.\n"
    "                      'hsv' restores in HSV space - giving sharp details.\n"
    "                      (default lch).\n"),
    N_("--clip=digital|film   Clip highlights for positive EV.\n"
    "                      'digital' linear digital sensor response.\n"
    "                      'film' emulate soft film response. (default digital).\n"),
    N_("--gamma=GAMMA         Gamma adjustment of the base curve (default 0.45).\n"),
    N_("--linearity=LINEARITY Linearity of the base curve (default 0.10).\n"),
#ifdef UFRAW_CONTRAST
    N_("--contrast=CONT       Contrast adjustment (default 1.0).\n"),
#endif
    N_("--saturation=SAT      Saturation adjustment (default 1.0, 0 for B&W output).\n"),
    N_("--wavelet-denoising-threshold=THRESHOLD\n"
    "                      Wavelet denoising threshold (default 0.0).\n"),
    N_("--hotpixel-sensitivity=VALUE\n"
    "                      Sensitivity for detecting and shaving hot pixels (default 0.0).\n"),
    N_("--exposure=auto|EXPOSURE\n"
    "                      Auto exposure or exposure correction in EV (default 0).\n"),
    N_("--black-point=auto|BLACK\n"
    "                      Auto black-point or black-point value (default 0).\n"),
    N_("--interpolation=ahd|vng|four-color|ppg|bilinear\n"
    "                      Interpolation algorithm to use (default ahd).\n"),
    N_("--color-smoothing     Apply color smoothing.\n"),
    N_("--grayscale=none|lightness|luminance|value|mixer\n"
    "                      Grayscale conversion algorithm to use (default none).\n"),
    N_("--grayscale-mixer=RED,GREEN,BLUE\n"
    "                      Grayscale mixer values to use (default 1,1,1).\n"),
    "\n",
    N_("The options which are related to the final output are:\n"),
    "\n",
    N_("--shrink=FACTOR       Shrink the image by FACTOR (default 1).\n"),
    N_("--size=SIZE           Downsize max(height,width) to SIZE.\n"),
    N_("--out-type=ppm|tiff|tif|png|jpeg|jpg|fits\n"
    "                      Output file format (default ppm).\n"),
    N_("--out-depth=8|16      Output bit depth per channel (default 8).\n"),
    N_("--create-id=no|also|only\n"
    "                      Create no|also|only ID file (default no).\n"),
    N_("--compression=VALUE   JPEG compression (0-100, default 85).\n"),
    N_("--[no]exif            Embed EXIF in output (default embed EXIF).\n"),
    N_("--[no]zip             Enable [disable] TIFF zip compression (default nozip).\n"),
    N_("--embedded-image      Extract the preview image embedded in the raw file\n"
    "                      instead of converting the raw image. This option\n"
    "                      is only valid with 'ufraw-batch'.\n"),
    N_("--rotate=camera|ANGLE|no\n"
    "                      Rotate image to camera's setting, by ANGLE degrees\n"
    "                      clockwise, or do not rotate the image (default camera).\n"),
    N_("--crop-(left|right|top|bottom)=PIXELS\n"
    "                      Crop the output to the given pixel range, relative to the\n"
    "                      raw image after rotation but before any scaling.\n"),
    N_("--auto-crop           Crop the output automatically.\n"),
    N_("--aspect-ratio X:Y    Set crop area aspect ratio.\n"),
#ifdef HAVE_LENSFUN
    N_("--lensfun=none|auto   Do not apply lens correction or try to apply\n"
    "                      correction by auto-detecting the lens (default auto).\n"),
#endif
    N_("--out-path=PATH       PATH for output file (default use input file's path).\n"),
    N_("--output=FILE         Output file name, use '-' to output to stdout.\n"),
    N_("--darkframe=FILE      Use FILE for raw darkframe subtraction.\n"),
    N_("--overwrite           Overwrite existing files without asking (default no).\n"),
    N_("--maximize-window     Force window to be maximized.\n"),
    N_("--silent              Do not display any messages during conversion. This\n"
    "                      option is only valid with 'ufraw-batch'.\n"),
    "\n",
    N_("UFRaw first reads the setting from the resource file $HOME/.ufrawrc.\n"
    "Then, if an ID file is specified, its setting are read. Next, the setting from\n"
    "the --conf option are taken, ignoring input/output filenames in the ID file.\n"
    "Lastly, the options from the command line are set. In batch mode, the second\n"
    "group of options is NOT read from the resource file.\n"),
    "\n",
    N_("Last, but not least, --version displays the version number and compilation\n"
    "options for ufraw and --help displays this help message and exits.\n"),
    "END"
};

char versionText[] =
    "%s " VERSION "\n"

    "EXIV2 "
#ifdef HAVE_EXIV2
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "JPEG "
#ifdef HAVE_LIBJPEG
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "JPEG2000 (libjasper) "
#ifdef HAVE_LIBJASPER
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "TIFF "
#ifdef HAVE_LIBTIFF
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "PNG "
#ifdef HAVE_LIBPNG
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "FITS "
#ifdef HAVE_LIBCFITSIO
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "ZIP "
#ifdef HAVE_LIBZ
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "BZIP2 "
#ifdef HAVE_LIBBZ2
    "enabled.\n"
#else
    "disabled.\n"
#endif

    "LENSFUN "
#ifdef HAVE_LENSFUN
    "enabled.\n"
#else
    "disabled.\n"
#endif
    "";

/* ufraw_process_args returns values:
 * -1     : an error occurred.
 * 0      : --help or --version text were printed.
 * optint : the index in argv of the first argv-element that is not an option.
 */
int ufraw_process_args(int *argc, char ***argv, conf_data *cmd, conf_data *rc)
{
    cmd->ufobject = ufraw_command_line_new();
    UFObject *cmdImage = ufgroup_element(cmd->ufobject, ufRawImage);
    int index = 0, c, i;
    char *base, *locale;
    char *baseCurveName = NULL, *baseCurveFile = NULL,
          *curveName = NULL, *curveFile = NULL, *outTypeName = NULL, *rotateName = NULL,
           *createIDName = NULL, *outPath = NULL, *output = NULL, *conf = NULL,
            *interpolationName = NULL, *darkframeFile = NULL,
             *restoreName = NULL, *clipName = NULL, *grayscaleName = NULL,
              *grayscaleMixer = NULL;
    static const struct option options[] = {
        { "wb", 1, 0, 'w'},
        { "temperature", 1, 0, 't'},
        { "green", 1, 0, 'g'},
#ifdef HAVE_LENSFUN
        { "lensfun", 1, 0, 'A'},
#endif
        { "base-curve", 1, 0, 'B'},
        { "base-curve-file", 1, 0, 'S'},
        { "curve", 1, 0, 'c'},
        { "curve-file", 1, 0, 'f'},
        { "gamma", 1, 0, 'G'},
        { "linearity", 1, 0, 'L'},
        { "saturation", 1, 0, 's'},
        { "hotpixel-sensitivity", 1, 0, 'H'},
#ifdef UFRAW_CONTRAST
        { "contrast", 1, 0, 'y'},
#endif
        { "wavelet-denoising-threshold", 1, 0, 'n'},
        { "exposure", 1, 0, 'e'},
        { "black-point", 1, 0, 'k'},
        { "interpolation", 1, 0, 'i'},
        { "grayscale", 1, 0, 'Y'},
        { "grayscale-mixer", 1, 0, 'a'},
        { "shrink", 1, 0, 'x'},
        { "size", 1, 0, 'X'},
        { "compression", 1, 0, 'j'},
        { "out-type", 1, 0, 'T'},
        { "out-depth", 1, 0, 'd'},
        { "rotate", 1, 0, 'R'},
        { "create-id", 1, 0, 'I'},
        { "out-path", 1, 0, 'p'},
        { "output", 1, 0, 'o'},
        { "darkframe", 1, 0, 'D'},
        { "restore", 1, 0, 'r'},
        { "clip", 1, 0, 'u'},
        { "conf", 1, 0, 'C'},
        { "crop-left", 1, 0, '1'},
        { "crop-top", 1, 0, '2'},
        { "crop-right", 1, 0, '3'},
        { "crop-bottom", 1, 0, '4'},
        { "aspect-ratio", 1, 0, 'P'},
        /* Binary flags that don't have a value are here at the end */
        { "zip", 0, 0, 'z'},
        { "nozip", 0, 0, 'Z'},
        { "overwrite", 0, 0, 'O'},
        { "color-smoothing", 0, 0, 'M' },
        { "maximize-window", 0, 0, 'W'},
        { "exif", 0, 0, 'E'},
        { "noexif", 0, 0, 'F'},
        { "embedded-image", 0, 0, 'm'},
        { "silent", 0, 0, 'q'},
        { "help", 0, 0, 'h'},
        { "version", 0, 0, 'v'},
        { "batch", 0, 0, 'b'},
        { "auto-crop", 0, 0, '0'},
        { 0, 0, 0, 0}
    };
    UFObject *tmpImage = ufraw_image_new();
    void *optPointer[] = {
        ufgroup_element(tmpImage, ufWB),
        ufgroup_element(tmpImage, ufTemperature),
        ufgroup_element(tmpImage, ufGreen),
#ifdef HAVE_LENSFUN
        ufgroup_element(tmpImage, ufLensfunAuto),
#endif
        &baseCurveName, &baseCurveFile, &curveName, &curveFile,
        &cmd->profile[0][0].gamma, &cmd->profile[0][0].linear,
        &cmd->saturation,
        &cmd->hotpixel,
#ifdef UFRAW_CONTRAST
        &cmd->contrast,
#endif
        &cmd->threshold,
        &cmd->exposure, &cmd->black, &interpolationName, &grayscaleName,
        &grayscaleMixer,
        &cmd->shrink, &cmd->size, &cmd->compression,
        &outTypeName, &cmd->profile[1][0].BitDepth, &rotateName,
        &createIDName, &outPath, &output, &darkframeFile,
        &restoreName, &clipName, &conf,
        &cmd->CropX1, &cmd->CropY1, &cmd->CropX2, &cmd->CropY2,
        &cmd->aspectRatio
    };
    cmd->autoExposure = disabled_state;
    cmd->autoBlack = disabled_state;
    cmd->losslessCompress = -1;
    cmd->overwrite = -1;
    cmd->WindowMaximized = -1;
    cmd->embedExif = -1;
    cmd->profile[1][0].BitDepth = -1;
    cmd->embeddedImage = FALSE;
    cmd->silent = FALSE;
    cmd->profile[0][0].gamma = NULLF;
    cmd->profile[0][0].linear = NULLF;
    cmd->hotpixel = NULLF;
#ifdef UFRAW_CONTRAST
    cmd->contrast = NULLF;
#endif
    cmd->saturation = NULLF;
    cmd->black = NULLF;
    cmd->threshold = NULLF;
    cmd->exposure = NULLF;
    cmd->shrink = NULLF;
    cmd->size = NULLF;
    cmd->compression = NULLF;
    cmd->rotationAngle = NULLF;
    cmd->CropX1 = -1;
    cmd->CropY1 = -1;
    cmd->CropX2 = -1;
    cmd->CropY2 = -1;
    cmd->fullCrop = -1;
    cmd->autoCrop = -1;
    cmd->aspectRatio = 0.0;
    cmd->rotate = -1;
    cmd->smoothing = -1;

    while (1) {
        c = getopt_long(*argc, *argv, "h", options, &index);
        if (c == -1) break;
        switch (c) {
            case 'w': // --wb
            case 't': // --temperature
            case 'g': // --green
            case 'A': // --lensfun
                locale = uf_set_locale_C();
                if (!ufobject_set_string(optPointer[index], optarg)) {
                    ufraw_message(UFRAW_ERROR,
                                  _("'%s' is not a valid value for the --%s option."),
                                  optarg, options[index].name);
                    uf_reset_locale(locale);
                    return -1;
                }
                uf_reset_locale(locale);
                if (!ufgroup_add(cmdImage, optPointer[index]))
                    return -1;
                break;
            case 'e':
                if (!strcmp(optarg, "auto")) {
                    cmd->autoExposure = apply_state;
                    break;
                }
            case 'k':
                if (!strcmp(optarg, "auto")) {
                    cmd->autoBlack = apply_state;
                    break;
                }
            case 'G':
            case 'L':
            case 's':
            case 'H':
#ifdef UFRAW_CONTRAST
            case 'y':
#endif
            case 'n':
                locale = uf_set_locale_C();
                if (sscanf(optarg, "%lf", (double *)optPointer[index]) == 0) {
                    ufraw_message(UFRAW_ERROR,
                                  _("'%s' is not a valid value for the --%s option."),
                                  optarg, options[index].name);
                    uf_reset_locale(locale);
                    return -1;
                }
                uf_reset_locale(locale);
                break;
            case 'x':
            case 'X':
            case 'j':
            case 'd':
            case '1':
            case '2':
            case '3':
            case '4':
                locale = uf_set_locale_C();
                if (sscanf(optarg, "%d", (int *)optPointer[index]) == 0) {
                    ufraw_message(UFRAW_ERROR,
                                  _("'%s' is not a valid value for the --%s option."),
                                  optarg, options[index].name);
                    uf_reset_locale(locale);
                    return -1;
                }
                uf_reset_locale(locale);
                break;
            case 'B':
            case 'S':
            case 'c':
            case 'f':
            case 'i':
            case 'T':
            case 'R':
            case 'I':
            case 'p':
            case 'o':
            case 'D':
            case 'C':
            case 'r':
            case 'u':
            case 'Y':
            case 'a':
                *(char **)optPointer[index] = optarg;
                break;
            case 'O':
                cmd->overwrite = TRUE;
                break;
            case 'W':
                cmd->WindowMaximized = TRUE;
                break;
            case 'm':
                cmd->embeddedImage = TRUE;
                break;
            case 'M':
                cmd->smoothing = TRUE;
                break;
            case 'q':
                cmd->silent = TRUE;
                break;
            case 'z':
#ifdef HAVE_LIBZ
                cmd->losslessCompress = TRUE;
                break;
#else
                ufraw_message(UFRAW_ERROR,
                              _("ufraw was build without ZIP support."));
                return -1;
#endif
            case 'Z':
                cmd->losslessCompress = FALSE;
                break;
            case 'E':
                cmd->embedExif = TRUE;
                break;
            case 'F':
                cmd->embedExif = FALSE;
                break;
            case 'h':
                for (i = 0; strcmp(helpText[i], "END") != 0; i++)
                    ufraw_message(UFRAW_SET_WARNING, _(helpText[i]));
                ufraw_message(UFRAW_WARNING,
                              ufraw_message(UFRAW_GET_WARNING, NULL));
                return 0;
            case 'v':
                base = g_path_get_basename(*argv[0]);
                ufraw_message(UFRAW_WARNING, versionText, base);
                g_free(base);
                return 0;
            case 'b':
                ufraw_message(UFRAW_ERROR,
                              _("--batch is obsolete. Use 'ufraw-batch' instead."));
                return -1;
            case '0':
                cmd->autoCrop = enabled_state;
                break;
            case 'P': {
                double num = 0.0, denom = 1.0;
                locale = uf_set_locale_C();
                if (sscanf(optarg, "%lf:%lf", &num, &denom) < 2 &&
                        sscanf(optarg, "%lf/%lf", &num, &denom) < 2 &&
                        sscanf(optarg, "%lf", &num) == 0) {
                    ufraw_message(UFRAW_ERROR,
                                  _("'%s' is not a valid value for the --%s option."),
                                  optarg, options[index].name);
                    uf_reset_locale(locale);
                    return -1;
                }
                *(double *)optPointer[index] = num / denom;
                uf_reset_locale(locale);
            }
            break;
            case '?': /* invalid option. Warning printed by getopt() */
                return -1;
            default:
                ufraw_message(UFRAW_ERROR, _("getopt returned "
                                             "character code 0%o ??"), c);
                return -1;
        }
    }
    cmd->BaseCurveIndex = -1;
    if (baseCurveFile != NULL) {
        baseCurveFile = uf_win32_locale_to_utf8(baseCurveFile);
        if (cmd->BaseCurveCount == max_curves) {
            ufraw_message(UFRAW_ERROR, _("failed to load curve from %s, "
                                         "too many configured base curves"), baseCurveFile);
            uf_win32_locale_free(baseCurveFile);
            return -1;
        }
        cmd->BaseCurveIndex = cmd->BaseCurveCount;
        if (curve_load(&(rc->BaseCurve[cmd->BaseCurveIndex]), baseCurveFile)
                == UFRAW_ERROR) {
            ufraw_message(UFRAW_ERROR,
                          _("failed to load curve from %s"), baseCurveFile);
            uf_win32_locale_free(baseCurveFile);
            return -1;
        }
        uf_win32_locale_free(baseCurveFile);
        cmd->BaseCurveCount++;
    } else if (baseCurveName != NULL) {
        if (!strcmp(baseCurveName, "manual"))
            cmd->BaseCurveIndex = manual_curve;
        else if (!strcmp(baseCurveName, "linear"))
            cmd->BaseCurveIndex = linear_curve;
        else if (!strcmp(baseCurveName, "custom"))
            cmd->BaseCurveIndex = custom_curve;
        else if (!strcmp(baseCurveName, "camera"))
            cmd->BaseCurveIndex = camera_curve;
        else {
            cmd->BaseCurveIndex = -1;
            for (i = camera_curve + 1; i < rc->BaseCurveCount; i++) {
                if (!strcmp(baseCurveName, rc->BaseCurve[i].name)) {
                    cmd->BaseCurveIndex = i;
                    break;
                }
            }
            if (cmd->BaseCurveIndex == -1) {
                ufraw_message(UFRAW_ERROR,
                              _("'%s' is not a valid base curve name."), baseCurveName);
                return -1;
            }
        }
    }
    cmd->curveIndex = -1;
    if (curveFile != NULL) {
        curveFile = uf_win32_locale_to_utf8(curveFile);
        if (cmd->curveCount == max_curves) {
            ufraw_message(UFRAW_ERROR, _("failed to load curve from %s, "
                                         "too many configured curves"), curveFile);
            uf_win32_locale_free(curveFile);
            return -1;
        }
        cmd->curveIndex = cmd->curveCount;
        if (curve_load(&(rc->curve[cmd->curveIndex]), curveFile) == UFRAW_ERROR) {
            ufraw_message(UFRAW_ERROR,
                          _("failed to load curve from %s"), curveFile);
            uf_win32_locale_free(curveFile);
            return -1;
        }
        uf_win32_locale_free(curveFile);
        cmd->curveCount++;
    } else if (curveName != NULL) {
        if (!strcmp(curveName, "manual")) cmd->curveIndex = manual_curve;
        else if (!strcmp(curveName, "linear")) cmd->curveIndex = linear_curve;
        else {
            cmd->curveIndex = -1;
            for (i = linear_curve + 1; i < rc->curveCount; i++) {
                if (!strcmp(curveName, rc->curve[i].name)) {
                    cmd->curveIndex = i;
                    break;
                }
            }
            if (cmd->curveIndex == -1) {
                ufraw_message(UFRAW_ERROR,
                              _("'%s' is not a valid curve name."), curveName);
                return -1;
            }
        }
    }
    cmd->interpolation = -1;
    if (interpolationName != NULL) {
        /* Keep compatebility with old numbers from ufraw-0.5 */
        /*if (!strcmp(interpolationName, "full"))
            cmd->interpolation = vng_interpolation;
        else if (!strcmp(interpolationName, "quick"))
            cmd->interpolation = bilinear_interpolation;
        else */
        cmd->interpolation = conf_find_name(interpolationName,
                                            interpolationNames, -1);
        // "eahd" is being silently supported since ufraw-0.13.
        if (cmd->interpolation < 0 || cmd->interpolation == half_interpolation) {
            ufraw_message(UFRAW_ERROR,
                          _("'%s' is not a valid interpolation option."),
                          interpolationName);
            return -1;
        }
    }
    if (cmd->smoothing != -1) {
        if (cmd->interpolation == ahd_interpolation)
            cmd->smoothing = 3;
        else
            cmd->smoothing = 1;
    }
    cmd->grayscaleMode = -1;
    cmd->grayscaleMixerDefined = 0;
    if (grayscaleName != NULL) {
        cmd->grayscaleMode = conf_find_name(grayscaleName, grayscaleModeNames,
                                            grayscale_invalid);
        if (cmd->grayscaleMode == grayscale_invalid) {
            ufraw_message(UFRAW_ERROR,
                          _("'%s' is not a valid grayscale option."),
                          grayscaleName);
            return -1;
        }
        if (cmd->grayscaleMode == grayscale_mixer) {
            if (grayscaleMixer != NULL) {
                double	red, green, blue;
                char *locale = uf_set_locale_C();
                if (sscanf(grayscaleMixer, "%lf,%lf,%lf", &red, &green, &blue) != 3) {
                    ufraw_message(UFRAW_ERROR,
                                  _("'%s' is not a valid grayscale-mixer option."),
                                  grayscaleMixer);
                    uf_reset_locale(locale);
                    return -1;
                }
                cmd->grayscaleMixerDefined = 1;
                cmd->grayscaleMixer[0] = red;
                cmd->grayscaleMixer[1] = green;
                cmd->grayscaleMixer[2] = blue;
                uf_reset_locale(locale);
            }
        }
    }
    cmd->restoreDetails = -1;
    if (restoreName != NULL) {
        cmd->restoreDetails = conf_find_name(restoreName,
                                             restoreDetailsNames, -1);
        if (cmd->restoreDetails < 0) {
            ufraw_message(UFRAW_ERROR, _("'%s' is not a valid restore option."),
                          restoreName);
            return -1;
        }
    }
    cmd->clipHighlights = -1;
    if (clipName != NULL) {
        cmd->clipHighlights = conf_find_name(clipName,
                                             clipHighlightsNames, -1);
        if (cmd->clipHighlights < 0) {
            ufraw_message(UFRAW_ERROR, _("'%s' is not a valid clip option."),
                          clipName);
            return -1;
        }
    }
    if (cmd->shrink != NULLF && cmd->size != NULLF) {
        ufraw_message(UFRAW_ERROR,
                      _("you can not specify both --shrink and --size"));
        return -1;
    }
    if (cmd->profile[1][0].BitDepth != -1) {
        if (cmd->profile[1][0].BitDepth != 8 &&
                cmd->profile[1][0].BitDepth != 16) {
            ufraw_message(UFRAW_ERROR,
                          _("'%d' is not a valid bit depth."),
                          cmd->profile[1][0].BitDepth);
            return -1;
        }
    }
    cmd->type = -1;
    if (outTypeName != NULL && !cmd->embeddedImage) {
        if (strcmp(outTypeName, "ppm") == 0) {
            cmd->type = ppm_type;
        } else if (strcmp(outTypeName, "ppm8") == 0) {
            cmd->type = ppm_type;
            cmd->profile[1][0].BitDepth = 8;
            ufraw_message(UFRAW_WARNING,
                          _("Output type '%s' is deprecated"), outTypeName);
        } else if (strcmp(outTypeName, "ppm16") == 0) {
            cmd->type = ppm_type;
            cmd->profile[1][0].BitDepth = 16;
            ufraw_message(UFRAW_WARNING,
                          _("Output type '%s' is deprecated"), outTypeName);
        }
#ifdef HAVE_LIBCFITSIO
        else if (strcmp(outTypeName, "fits") == 0) {
            cmd->type = fits_type;
        }
#endif

        else if (!strcmp(outTypeName, "tiff") || !strcmp(outTypeName, "tif"))
#ifdef HAVE_LIBTIFF
        {
            cmd->type = tiff_type;
        }
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without TIFF support."));
            return -1;
        }
#endif
        else if (!strcmp(outTypeName, "tiff8"))
#ifdef HAVE_LIBTIFF
        {
            cmd->type = tiff_type;
            cmd->profile[1][0].BitDepth = 8;
            ufraw_message(UFRAW_WARNING,
                          _("Output type '%s' is deprecated"), outTypeName);
        }
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without TIFF support."));
            return -1;
        }
#endif
        else if (!strcmp(outTypeName, "tiff16"))
#ifdef HAVE_LIBTIFF
        {
            cmd->type = tiff_type;
            cmd->profile[1][0].BitDepth = 16;
            ufraw_message(UFRAW_WARNING,
                          _("Output type '%s' is deprecated"), outTypeName);
        }
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without TIFF support."));
            return -1;
        }
#endif
        else if (!strcmp(outTypeName, "jpeg") || !strcmp(outTypeName, "jpg"))
#ifdef HAVE_LIBJPEG
            cmd->type = jpeg_type;
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without JPEG support."));
            return -1;
        }
#endif
        else if (!strcmp(outTypeName, "png"))
#ifdef HAVE_LIBPNG
        {
            cmd->type = png_type;
        }
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without PNG support."));
            return -1;
        }
#endif
        else if (!strcmp(outTypeName, "png8"))
#ifdef HAVE_LIBPNG
        {
            cmd->type = png_type;
            cmd->profile[1][0].BitDepth = 8;
            ufraw_message(UFRAW_WARNING,
                          _("Output type '%s' is deprecated"), outTypeName);
        }
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without PNG support."));
            return -1;
        }
#endif
        else if (!strcmp(outTypeName, "png16"))
#ifdef HAVE_LIBPNG
        {
            cmd->type = png_type;
            cmd->profile[1][0].BitDepth = 16;
            ufraw_message(UFRAW_WARNING,
                          _("Output type '%s' is deprecated"), outTypeName);
        }
#else
        {
            ufraw_message(UFRAW_ERROR,
                          _("ufraw was build without PNG support."));
            return -1;
        }
#endif
        else {
            ufraw_message(UFRAW_ERROR,
                          _("'%s' is not a valid output type."), outTypeName);
            return -1;
        }
    }
    if (cmd->embeddedImage) {
#ifndef HAVE_LIBJPEG
        ufraw_message(UFRAW_ERROR, _("ufraw was build without JPEG support."));
        return -1;
#endif
        if (outTypeName == NULL || !strcmp(outTypeName, "jpeg") ||
                !strcmp(outTypeName, "jpg"))
            cmd->type = embedded_jpeg_type;
#ifdef HAVE_LIBPNG
        else if (strcmp(outTypeName, "png") == 0)
            cmd->type = embedded_png_type;
#endif

        else {
            ufraw_message(UFRAW_ERROR,
                          _("'%s' is not a valid output type for embedded image."),
                          outTypeName);
            return -1;
        }
        if (cmd->profile[1][0].BitDepth != -1 &&
                cmd->profile[1][0].BitDepth != 8) {
            ufraw_message(UFRAW_ERROR,
                          _("'%d' is not a valid bit depth for embedded image."),
                          cmd->profile[1][0].BitDepth);
            return -1;
        }
    }
    if (rotateName != NULL) {
        if (strcmp(rotateName, "camera") == 0)
            cmd->rotate = TRUE;
        else if (strcmp(rotateName, "no") == 0)
            cmd->rotate = FALSE;
        else if (sscanf(rotateName, "%lf", &cmd->rotationAngle) == 1) {
            cmd->rotate = TRUE;
        } else {
            ufraw_message(UFRAW_ERROR,
                          _("'%s' is not a valid rotate option."), rotateName);
            return -1;
        }
    }
    cmd->createID = -1;
    if (createIDName != NULL) {
        if (!strcmp(createIDName, "no"))
            cmd->createID = no_id;
        else if (!strcmp(createIDName, "also"))
            cmd->createID = also_id;
        else if (!strcmp(createIDName, "only"))
            cmd->createID = only_id;
        else {
            ufraw_message(UFRAW_ERROR,
                          _("'%s' is not a valid create-id option."), createIDName);
            return -1;
        }
    }
    g_strlcpy(cmd->outputPath, "", max_path);
    if (outPath != NULL) {
        outPath = uf_win32_locale_to_utf8(outPath);
        if (g_file_test(outPath, G_FILE_TEST_IS_DIR)) {
            g_strlcpy(cmd->outputPath, outPath, max_path);
            uf_win32_locale_free(outPath);
        } else {
            ufraw_message(UFRAW_ERROR, _("'%s' is not a valid path."), outPath);
            uf_win32_locale_free(outPath);
            return -1;
        }
    }
    g_strlcpy(cmd->outputFilename, "", max_path);
    if (output != NULL) {
        if (*argc - optind > 1) {
            ufraw_message(UFRAW_ERROR,
                          _("cannot output more than one file to the same output"));
            return -1;
        }
        output = uf_win32_locale_to_utf8(output);
        g_strlcpy(cmd->outputFilename, output, max_path);
        uf_win32_locale_free(output);
    }
    g_strlcpy(cmd->darkframeFile, "", max_path);
    cmd->darkframe = NULL;
    if (darkframeFile != NULL) {
        darkframeFile = uf_win32_locale_to_utf8(darkframeFile);
        char *df = uf_file_set_absolute(darkframeFile);
        uf_win32_locale_free(darkframeFile);
        g_strlcpy(cmd->darkframeFile, df, max_path);
        g_free(df);
    }
    /* cmd->inputFilename is used to store the conf file */
    g_strlcpy(cmd->inputFilename, "", max_path);
    if (conf != NULL)
        g_strlcpy(cmd->inputFilename, conf, max_path);

    ufobject_delete(tmpImage);
    return optind;
}

/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_developer.c - functions for developing images or more exactly pixels.
 * Copyright 2004-2016 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufraw.h"
#ifdef _OPENMP
#include <omp.h>
#endif
#include <math.h>
#include <string.h>
#include <lcms2.h>
#include <lcms2_plugin.h>
#include "ufraw_colorspaces.h"

static void lcms_message(cmsContext ContextID,
                         cmsUInt32Number ErrorCode,
                         const char *ErrorText)
{
    (void) ContextID;
    /* Possible ErrorCode: see cmsERROR_* in <lcms2.h>. */
    (void) ErrorCode;
    ufraw_message(UFRAW_ERROR, "%s", ErrorText);
}

developer_data *developer_init()
{
    int i;
    developer_data *d = g_new(developer_data, 1);
    d->mode = -1;
    d->gamma = -1;
    d->linear = -1;
    d->saturation = -1;
#ifdef UFRAW_CONTRAST
    d->contrast = -1;
#endif
    for (i = 0; i < profile_types; i++) {
        d->profile[i] = NULL;
        strcpy(d->profileFile[i], "no such file");
    }
    memset(&d->baseCurveData, 0, sizeof(d->baseCurveData));
    d->baseCurveData.m_gamma = -1.0;
    memset(&d->luminosityCurveData, 0, sizeof(d->luminosityCurveData));
    d->luminosityCurveData.m_gamma = -1.0;
    d->luminosityProfile = NULL;
    cmsToneCurve **TransferFunction = (cmsToneCurve **)d->TransferFunction;
    TransferFunction[0] = cmsBuildGamma(NULL, 1.0);
    TransferFunction[1] = TransferFunction[2] = cmsBuildGamma(NULL, 1.0);
    d->saturationProfile = NULL;
    d->adjustmentProfile = NULL;
    d->intent[out_profile] = -1;
    d->intent[display_profile] = -1;
    d->updateTransform = TRUE;
    d->colorTransform = NULL;
    d->working2displayTransform = NULL;
    d->rgbtolabTransform = NULL;
    d->grayscaleMode = -1;
    d->grayscaleMixer[0] = d->grayscaleMixer[1] = d->grayscaleMixer[2] = -1;
    for (i = 0; i < max_adjustments; i++) { /* Suppress valgrind error. */
        d->lightnessAdjustment[i].adjustment = 0.0;
        d->lightnessAdjustment[i].hue = 0.0;
        d->lightnessAdjustment[i].hueWidth = 0.0;
    }
    cmsSetLogErrorHandler(lcms_message);
    return d;
}

void developer_destroy(developer_data *d)
{
    int i;
    if (d == NULL) return;
    for (i = 0; i < profile_types; i++)
        if (d->profile[i] != NULL) cmsCloseProfile(d->profile[i]);
    cmsCloseProfile(d->luminosityProfile);
    cmsFreeToneCurve(d->TransferFunction[0]);
    cmsFreeToneCurve(d->TransferFunction[1]);
    cmsCloseProfile(d->saturationProfile);
    cmsCloseProfile(d->adjustmentProfile);
    if (d->colorTransform != NULL)
        cmsDeleteTransform(d->colorTransform);
    if (d->working2displayTransform != NULL)
        cmsDeleteTransform(d->working2displayTransform);
    if (d->rgbtolabTransform != NULL)
        cmsDeleteTransform(d->rgbtolabTransform);
    g_free(d);
}

static const char *embedded_display_profile = "embedded display profile";

/*
 * Emulates cmsTakeProductName() from lcms 1.x.
 *
 * This is tailored for use with statically allocated strings and not
 * thread-safe.
 */
const char *cmsTakeProductName(cmsHPROFILE profile)
{
    static char name[max_name * 2 + 4];
    char manufacturer[max_name], model[max_name];

    name[0] = manufacturer[0] = model[0] = '\0';

    cmsGetProfileInfoASCII(profile, cmsInfoManufacturer,
                           "en", "US", manufacturer, max_name);
    cmsGetProfileInfoASCII(profile, cmsInfoModel,
                           "en", "US", model, max_name);

    if (!manufacturer[0] && !model[0]) {
        cmsGetProfileInfoASCII(profile, cmsInfoDescription,
                               "en", "US", name, max_name * 2 + 4);
    } else {
        if (!manufacturer[0] || (strncmp(model, manufacturer, 8) == 0) ||
                strlen(model) > 30)
            strcpy(name, model);
        else
            sprintf(name, "%s - %s", model, manufacturer);
    }

    return name;
}

/* Update the profile in the developer
 * and init values in the profile if needed */
void developer_profile(developer_data *d, int type, profile_data *p)
{
    // embedded_display_profile were handled by developer_display_profile()
    if (strcmp(d->profileFile[type], embedded_display_profile) == 0)
        return;
    if (strcmp(p->file, d->profileFile[type])) {
        g_strlcpy(d->profileFile[type], p->file, max_path);
        if (d->profile[type] != NULL) cmsCloseProfile(d->profile[type]);
        if (!strcmp(d->profileFile[type], ""))
            d->profile[type] = uf_colorspaces_create_srgb_profile();
        else {
            char *filename =
                uf_win32_locale_filename_from_utf8(d->profileFile[type]);
            d->profile[type] = cmsOpenProfileFromFile(filename, "r");
            uf_win32_locale_filename_free(filename);
            if (d->profile[type] == NULL)
                d->profile[type] = uf_colorspaces_create_srgb_profile();
        }
        d->updateTransform = TRUE;
    }
    if (d->updateTransform) {
        if (d->profile[type] != NULL)
            g_strlcpy(p->productName, cmsTakeProductName(d->profile[type]),
                      max_name);
        else
            strcpy(p->productName, "");
    }
}

void developer_display_profile(developer_data *d,
                               unsigned char *profile, int size, char productName[])
{
    int type = display_profile;
    if (profile != NULL) {
        if (d->profile[type] != NULL) cmsCloseProfile(d->profile[type]);
        d->profile[type] = cmsOpenProfileFromMem(profile, size);
        // If embedded profile is invalid fall-back to sRGB
        if (d->profile[type] == NULL)
            d->profile[type] = uf_colorspaces_create_srgb_profile();
        if (strcmp(d->profileFile[type], embedded_display_profile) != 0) {
            // start using embedded profile
            g_strlcpy(d->profileFile[type], embedded_display_profile, max_path);
            d->updateTransform = TRUE;
        }
    } else {
        if (strcmp(d->profileFile[type], embedded_display_profile) == 0) {
            // embedded profile is no longer used
            if (d->profile[type] != NULL) cmsCloseProfile(d->profile[type]);
            d->profile[type] = uf_colorspaces_create_srgb_profile();
            strcpy(d->profileFile[type], "");
            d->updateTransform = TRUE;
        }
    }
    if (d->updateTransform) {
        if (d->profile[type] != NULL)
            g_strlcpy(productName, cmsTakeProductName(d->profile[type]),
                      max_name);
        else
            strcpy(productName, "");
    }
}

static double clamp(double in, double min, double max)
{
    return (in < min) ? min : (in > max) ? max : in;
}

struct contrast_saturation {
    double contrast;
    double saturation;
};

/* Scale in along a curve from min to max by scale */
static double scale_curve(double in, double min, double max, double scale)
{
    double halfrange = (max - min) / 2.0;
    /* Normalize in to [ -1, 1 ] */
    double value = clamp((in - min) / halfrange - 1.0, -1.0, 1.0);
    /* Linear scaling makes more visual sense for low contrast values. */
    if (scale > 1.0) {
        double n = fabs(value);
        if (n > 1.0)
            n = 1.0;
        scale = n <= 0.0 ? 0.0 : (1.0 - pow(1.0 - n, scale)) / n;
    }
    return clamp((value * scale + 1.0) * halfrange + min, min, max);
}

static const double max_luminance = 100.0;
static const double max_colorfulness = 181.019336; /* sqrt(128*128+128*128) */

static cmsInt32Number contrast_saturation_sampler(const cmsUInt16Number In[],
        cmsUInt16Number Out[],
        void *Cargo)
{
    cmsCIELab Lab;
    cmsCIELCh LCh;
    const struct contrast_saturation* cs = Cargo;

    cmsLabEncoded2Float(&Lab, In);
    cmsLab2LCh(&LCh, &Lab);
    LCh.L = scale_curve(LCh.L, 0.0, max_luminance, cs->contrast);
    LCh.C = scale_curve(LCh.C, -max_colorfulness, max_colorfulness,
                        cs->saturation);
    cmsLCh2Lab(&Lab, &LCh);
    cmsFloat2LabEncoded(Out, &Lab);

    return TRUE;
}

/* Based on lcms' cmsCreateBCHSWabstractProfile() */
static cmsHPROFILE create_contrast_saturation_profile(double contrast,
        double saturation)
{
    cmsHPROFILE hICC;
    struct contrast_saturation cs = { contrast, saturation };

    cmsPipeline* Pipeline = NULL;
    cmsStage* CLUT = NULL;

    hICC = cmsCreateProfilePlaceholder(NULL);
    if (hICC == NULL) return NULL; // can't allocate

    cmsSetDeviceClass(hICC, cmsSigAbstractClass);
    cmsSetColorSpace(hICC, cmsSigLabData);
    cmsSetPCS(hICC, cmsSigLabData);
    cmsSetHeaderRenderingIntent(hICC, INTENT_PERCEPTUAL);

    // Creates a pipeline with 3D grid only
    Pipeline = cmsPipelineAlloc(NULL, 3, 3);
    if (!Pipeline) goto error_out;

    if (!(CLUT = cmsStageAllocCLut16bit(NULL, 11, 3, 3, NULL)))
        goto error_out;
    if (!cmsStageSampleCLut16bit(CLUT, contrast_saturation_sampler, &cs, 0))
        goto error_out;

#if LCMS_VERSION >= 2050
    if (!cmsPipelineInsertStage(Pipeline, cmsAT_END, CLUT))
        goto error_out;
#else
    cmsPipelineInsertStage(Pipeline, cmsAT_END, CLUT);
#endif

    // Create tags
    cmsWriteTag(hICC, cmsSigMediaWhitePointTag, cmsD50_XYZ());
    cmsWriteTag(hICC, cmsSigAToB0Tag, Pipeline);

    // Pipeline is already on virtual profile
    cmsPipelineFree(Pipeline);

    return hICC;

error_out:
    if (CLUT) cmsStageFree(CLUT);
    if (Pipeline) cmsPipelineFree(Pipeline);
    if (hICC) cmsCloseProfile(hICC);
    return NULL;
}

static cmsInt32Number luminance_adjustment_sampler(const cmsUInt16Number In[],
        cmsUInt16Number Out[],
        void *Cargo)
{
    cmsCIELab Lab;
    cmsCIELCh LCh;
    const developer_data *d = Cargo;
    const lightness_adjustment *a;

    cmsLabEncoded2Float(&Lab, In);
    cmsLab2LCh(&LCh, &Lab);

    double adj = 0.0;
    int i;
    for (i = 0, a = d->lightnessAdjustment; i < max_adjustments; i++, a++) {
        double deltaHue = fabs(LCh.h - a->hue);
        double hueWidth = MAX(a->hueWidth, 360.0 / 33.0);
        if (deltaHue > 180.0)
            deltaHue = 360.0 - deltaHue;
        if (deltaHue > hueWidth)
            continue;
        /* This assigns the scales on a nice curve. */
        double scale = cos(deltaHue / hueWidth * (M_PI / 2));
        adj += (a->adjustment - 1) * (scale * scale);
    }
    /* The adjustment is scaled based on the colorfulness of the point,
     * since uncolored pixels should not be adjusted.  However, few
     * (s)RGB colors have a colorfulness value larger than 1/2 of
     * max_colorfulness, so use that as an actual maximum colorfulness. */
    adj = adj * MIN(LCh.C / (max_colorfulness / 2), 1.0) + 1;
    LCh.L *= adj;

    cmsLCh2Lab(&Lab, &LCh);
    cmsFloat2LabEncoded(Out, &Lab);

    return TRUE;
}

/* Based on lcms' cmsCreateBCHSWabstractProfile() */
static cmsHPROFILE create_adjustment_profile(const developer_data *d)
{
    cmsHPROFILE hICC;

    cmsPipeline* Pipeline = NULL;
    cmsStage* CLUT = NULL;

    hICC = cmsCreateProfilePlaceholder(NULL);
    if (hICC == NULL) return NULL; // can't allocate

    cmsSetDeviceClass(hICC, cmsSigAbstractClass);
    cmsSetColorSpace(hICC, cmsSigLabData);
    cmsSetPCS(hICC, cmsSigLabData);
    cmsSetHeaderRenderingIntent(hICC, INTENT_PERCEPTUAL);

    // Creates a pipeline with 3D grid only
    Pipeline = cmsPipelineAlloc(NULL, 3, 3);
    if (!Pipeline) goto error_out;

    if (!(CLUT = cmsStageAllocCLut16bit(NULL, 11, 3, 3, NULL)))
        goto error_out;

    if (!cmsStageSampleCLut16bit(CLUT, luminance_adjustment_sampler,
                                 (void*)d, 0))
        goto error_out;

#if LCMS_VERSION >= 2050
    if (!cmsPipelineInsertStage(Pipeline, cmsAT_END, CLUT))
        goto error_out;
#else
    cmsPipelineInsertStage(Pipeline, cmsAT_END, CLUT);
#endif

    // Create tags
    cmsWriteTag(hICC, cmsSigMediaWhitePointTag, cmsD50_XYZ());
    cmsWriteTag(hICC, cmsSigAToB0Tag, Pipeline);

    // Pipeline is already on virtual profile
    cmsPipelineFree(Pipeline);

    return hICC;

error_out:
    if (CLUT) cmsStageFree(CLUT);
    if (Pipeline) cmsPipelineFree(Pipeline);
    if (hICC) cmsCloseProfile(hICC);
    return NULL;
}

/* Find a for which (1-exp(-a x)/(1-exp(-a)) has derivative b at x=0 */
/* In other words, solve a/(1-exp(-a))==b */
static double findExpCoeff(double b)
{
    double a, bg;
    int try;
    if (b <= 1) return 0;
    if (b < 2) a = (b - 1) / 2;
    else a = b;
    bg = a / (1 - exp(-a));
    /* The limit on try is just to be sure there is no infinite loop. */
    for (try = 0; abs(bg - b) > 0.001 || try < 100; try++) {
                    a = a + (b - bg);
                    bg = a / (1 - exp(-a));
                }
    return a;
}

static void developer_create_transform(developer_data *d, DeveloperMode mode)
{
    if (!d->updateTransform)
        return;
    d->updateTransform = FALSE;
    /* Create transformations according to mode:
     * auto_developer|output_developer:
     *	    colorTransformation from in to out
     *	    working2displayTransform is null
     * display_developer:
     *	    with softproofing:
     *	        colorTransformation from in to out
     *	        working2displayTransform from out to display
     *	    without softproofing:
     *	        colorTransformation from in to display
     *	        working2displayTransform is null
     */
    int targetProfile;
    if (mode == display_developer
            && d->intent[display_profile] == disable_intent) {
        targetProfile = display_profile;
    } else {
        targetProfile = out_profile;
    }
    if (d->colorTransform != NULL)
        cmsDeleteTransform(d->colorTransform);
    if (strcmp(d->profileFile[in_profile], "") == 0 &&
            strcmp(d->profileFile[targetProfile], "") == 0 &&
            d->luminosityProfile == NULL &&
            d->adjustmentProfile == NULL &&
            d->saturationProfile == NULL) {
        /* No transformation at all. */
        d->colorTransform = NULL;
    } else {
        cmsHPROFILE prof[5];
        int i = 0;
        prof[i++] = d->profile[in_profile];
        if (d->luminosityProfile != NULL)
            prof[i++] = d->luminosityProfile;
        if (d->adjustmentProfile != NULL)
            prof[i++] = d->adjustmentProfile;
        if (d->saturationProfile != NULL)
            prof[i++] = d->saturationProfile;
        prof[i++] = d->profile[targetProfile];
        d->colorTransform = cmsCreateMultiprofileTransform(prof, i,
                            TYPE_RGB_16, TYPE_RGB_16, d->intent[out_profile], 0);
    }

    if (d->working2displayTransform != NULL)
        cmsDeleteTransform(d->working2displayTransform);
    if (mode == display_developer
            && d->intent[display_profile] != disable_intent
            && strcmp(d->profileFile[out_profile],
                      d->profileFile[display_profile]) != 0) {
        // TODO: We should use TYPE_RGB_'bit_depth' for working profile.
        d->working2displayTransform = cmsCreateTransform(
                                          d->profile[out_profile], TYPE_RGB_8,
                                          d->profile[display_profile], TYPE_RGB_8,
                                          d->intent[display_profile], 0);
    } else {
        d->working2displayTransform = NULL;
    }

    if (d->rgbtolabTransform == NULL) {
        cmsHPROFILE labProfile = cmsCreateLab2Profile(cmsD50_xyY());
        d->rgbtolabTransform = cmsCreateTransform(d->profile[in_profile],
                               TYPE_RGB_16, labProfile,
                               TYPE_Lab_16, INTENT_ABSOLUTE_COLORIMETRIC, 0);
        cmsCloseProfile(labProfile);
    }
}

static gboolean test_adjustments(const lightness_adjustment values[max_adjustments],
                                 gdouble reference, gdouble threshold)
{
    int i;
    for (i = 0; i < max_adjustments; ++i)
        if (fabs(values[i].adjustment - reference) >= threshold)
            return TRUE;
    return FALSE;
}

void developer_prepare(developer_data *d, conf_data *conf,
                       int rgbMax, float rgb_cam[3][4], int colors, int useMatrix,
                       DeveloperMode mode)
{
    unsigned c, i;
    profile_data *in, *out, *display;
    CurveData *baseCurve, *curve;
    double total;

    if (mode != d->mode) {
        d->mode = mode;
        d->updateTransform = TRUE;
    }
    in = &conf->profile[in_profile][conf->profileIndex[in_profile]];
    /* For auto-tools we create an sRGB output. */
    if (mode == auto_developer)
        out = &conf->profile[out_profile][0];
    else
        out = &conf->profile[out_profile][conf->profileIndex[out_profile]];
    display = &conf->profile[display_profile]
              [conf->profileIndex[display_profile]];
    baseCurve = &conf->BaseCurve[conf->BaseCurveIndex];
    curve = &conf->curve[conf->curveIndex];

    d->rgbMax = rgbMax;
    d->colors = colors;
    d->useMatrix = useMatrix;

    double max = 0;
    UFObject *chanMul = ufgroup_element(conf->ufobject, ufChannelMultipliers);
    /* We assume that min(chanMul)==1.0 */
    for (c = 0; c < d->colors; c++)
        max = MAX(max, ufnumber_array_value(chanMul, c));
    d->max = 0x10000 / max;
    /* rgbWB is used in dcraw_finalized_interpolation() before the color filter
     * array interpolation. It is normalized to guarantee that values do not
     * exceed 0xFFFF */
    for (c = 0; c < d->colors; c++)
        d->rgbWB[c] = ufnumber_array_value(chanMul, c) *  d->max *
                      0xFFFF / d->rgbMax;

    if (d->useMatrix) {
        if (d->colors == 1)
            for (i = 0; i < 3; i++)
                d->colorMatrix[i][0] = rgb_cam[0][0] * 0x10000;
        else
            for (i = 0; i < 3; i++)
                for (c = 0; c < d->colors; c++)
                    d->colorMatrix[i][c] = rgb_cam[i][c] * 0x10000;
    }

    switch (conf->grayscaleMode) {

        case grayscale_mixer:
            d->grayscaleMode = grayscale_mixer;
            for (c = 0, total = 0.0; c < 3; ++c)
                total += fabs(conf->grayscaleMixer[c]);
            total = total == 0.0 ? 1.0 : total;
            for (c = 0; c < 3; ++c)
                d->grayscaleMixer[c] = conf->grayscaleMixer[c] / total;
            break;

        case grayscale_lightness:
        case grayscale_value:
            d->grayscaleMode = conf->grayscaleMode;
            break;

        default:
            d->grayscaleMode = grayscale_none;
    }

    d->restoreDetails = conf->restoreDetails;
    int clipHighlights = conf->clipHighlights;
    unsigned exposure = pow(2, conf->exposure) * 0x10000;
    /* Handle the exposure normalization for Canon EOS cameras. */
    if (conf->ExposureNorm > 0)
        exposure = (guint64)exposure * d->rgbMax / conf->ExposureNorm;
    if (exposure >= 0x10000) d->restoreDetails = clip_details;
    if (exposure <= 0x10000) clipHighlights = digital_highlights;
    /* Check if gamma curve data has changed. */
    if (in->gamma != d->gamma || in->linear != d->linear ||
            exposure != d->exposure || clipHighlights != d->clipHighlights ||
            memcmp(baseCurve, &d->baseCurveData, sizeof(CurveData)) != 0) {
        d->baseCurveData = *baseCurve;
        guint16 BaseCurve[0x10000];
        CurveSample *cs = CurveSampleInit(0x10000, 0x10000);
        ufraw_message(UFRAW_RESET, NULL);
        if (CurveDataSample(baseCurve, cs) != UFRAW_SUCCESS) {
            ufraw_message(UFRAW_REPORT, NULL);
            for (i = 0; i < 0x10000; i++) cs->m_Samples[i] = i;
        }
        for (i = 0; i < 0x10000; i++) BaseCurve[i] = cs->m_Samples[i];
        CurveSampleFree(cs);

        d->gamma = in->gamma;
        d->linear = in->linear;
        d->exposure = exposure;
        d->clipHighlights = clipHighlights;
        guint16 FilmCurve[0x10000];
        if (d->clipHighlights == film_highlights) {
            /* Exposure is set by FilmCurve[].
             * Set initial slope to d->exposuse/0x10000 */
            double a = findExpCoeff((double)d->exposure / 0x10000);
            for (i = 0; i < 0x10000; i++) FilmCurve[i] =
                    (1 - exp(-a * i / 0x10000)) / (1 - exp(-a)) * 0xFFFF;
        } else { /* digital highlights */
            for (i = 0; i < 0x10000; i++) FilmCurve[i] = i;
        }
        double a, b, c, g;
        /* The parameters of the linearized gamma curve are set in a way that
         * keeps the curve continuous and smooth at the connecting point.
         * d->linear also changes the real gamma used for the curve (g) in
         * a way that keeps the derivative at i=0x10000 constant.
         * This way changing the linearity changes the curve behaviour in
         * the shadows, but has a minimal effect on the rest of the range. */
        if (d->linear < 1.0) {
            g = d->gamma * (1.0 - d->linear) / (1.0 - d->gamma * d->linear);
            a = 1.0 / (1.0 + d->linear * (g - 1));
            b = d->linear * (g - 1) * a;
            c = pow(a * d->linear + b, g) / d->linear;
        } else {
            a = b = g = 0.0;
            c = 1.0;
        }
        for (i = 0; i < 0x10000; i++)
            if (BaseCurve[FilmCurve[i]] < 0x10000 * d->linear)
                d->gammaCurve[i] = MIN(c * BaseCurve[FilmCurve[i]], 0xFFFF);
            else
                d->gammaCurve[i] = MIN(pow(a * BaseCurve[FilmCurve[i]] / 0x10000 + b,
                                           g) * 0x10000, 0xFFFF);
    }
    developer_profile(d, in_profile, in);
    developer_profile(d, out_profile, out);
    if (conf->intent[out_profile] != d->intent[out_profile]) {
        d->intent[out_profile] = conf->intent[out_profile];
        d->updateTransform = TRUE;
    }
    /* For auto-tools we ignore all the output settings:
     * luminosity, saturation, output profile and proofing. */
    if (mode == auto_developer) {
        developer_create_transform(d, mode);
        return;
    }
    developer_profile(d, display_profile, display);
    if (conf->intent[display_profile] != d->intent[display_profile]) {
        d->intent[display_profile] = conf->intent[display_profile];
        d->updateTransform = TRUE;
    }
    /* Check if curve data has changed. */
    if (memcmp(curve, &d->luminosityCurveData, sizeof(CurveData))) {
        d->luminosityCurveData = *curve;
        /* Trivial curve does not require a profile */
        if (CurveDataIsTrivial(curve)) {
            d->luminosityProfile = NULL;
        } else {
            cmsCloseProfile(d->luminosityProfile);
            CurveSample *cs = CurveSampleInit(0x100, 0x10000);
            ufraw_message(UFRAW_RESET, NULL);
            if (CurveDataSample(curve, cs) != UFRAW_SUCCESS) {
                ufraw_message(UFRAW_REPORT, NULL);
                d->luminosityProfile = NULL;
            } else {
                cmsToneCurve **TransferFunction =
                    (cmsToneCurve **)d->TransferFunction;
                cmsFloat32Number values[0x100];
                cmsFreeToneCurve(TransferFunction[0]);
                for (i = 0; i < 0x100; i++)
                    values[i] = (cmsFloat32Number) cs->m_Samples[i] / 0x10000;
                TransferFunction[0] =
                    cmsBuildTabulatedToneCurveFloat(NULL, 0x100, values);
                d->luminosityProfile = cmsCreateLinearizationDeviceLink(
                                           cmsSigLabData, TransferFunction);
                cmsSetDeviceClass(d->luminosityProfile, cmsSigAbstractClass);
            }
            CurveSampleFree(cs);
        }
        d->updateTransform = TRUE;
    }
    if (memcmp(d->lightnessAdjustment, conf->lightnessAdjustment,
               sizeof d->lightnessAdjustment) != 0) {
        /* Adjustments have changed, need to update them. */
        d->updateTransform = TRUE;
        memcpy(d->lightnessAdjustment, conf->lightnessAdjustment,
               sizeof d->lightnessAdjustment);
        cmsCloseProfile(d->adjustmentProfile);
        d->adjustmentProfile = test_adjustments(d->lightnessAdjustment, 1.0, 0.01)
                               ? create_adjustment_profile(d)
                               : NULL;
    }

    if (conf->saturation != d->saturation
#ifdef UFRAW_CONTRAST
            || conf->contrast != d->contrast
#endif
            || conf->grayscaleMode == grayscale_luminance) {
#ifdef UFRAW_CONTRAST
        d->contrast = conf->contrast;
#endif
        d->saturation = (conf->grayscaleMode == grayscale_luminance)
                        ? 0 : conf->saturation;
        cmsCloseProfile(d->saturationProfile);
        if (d->saturation == 1.0
#ifdef UFRAW_CONTRAST
                && d->contrast == 1.0
#endif
           )
            d->saturationProfile = NULL;
        else
            d->saturationProfile = create_contrast_saturation_profile(
#ifdef UFRAW_CONTRAST
                                       d->contrast,
#else
                                       1.0,
#endif
                                       d->saturation);
        d->updateTransform = TRUE;
    }
    developer_create_transform(d, mode);
}

static void apply_matrix(const developer_data *d,
                         const gint64 in[4],
                         gint64 out[3])
{
    gint64 tmp[3];
    unsigned c, cc;
    for (cc = 0; cc < 3; cc++) {
        tmp[cc] = 0;
        for (c = 0; c < d->colors; c++)
            tmp[cc] += in[c] * d->colorMatrix[cc][c];
    }
    for (cc = 0; cc < 3; cc++)
        out[cc] = MAX(tmp[cc] / 0x10000, 0);
}

static void cond_apply_matrix(const developer_data *d,
                              const gint64 in[4],
                              gint64 out[3])
{
    if (d->useMatrix)
        apply_matrix(d, in, out);
    else
        memcpy(out, in, 3 * sizeof out[0]);
}

extern const double xyz_rgb[3][3];
static const double rgb_xyz[3][3] = {			/* RGB from XYZ */
    { 3.24048, -1.53715, -0.498536 },
    { -0.969255, 1.87599, 0.0415559 },
    { 0.0556466, -0.204041, 1.05731 }
};

// Convert linear RGB to CIE-LCh
void uf_rgb_to_cielch(gint64 rgb[3], float lch[3])
{
    int c, cc, i;
    float r, xyz[3], lab[3];
    // The use of static varibles here should be thread safe.
    // In the worst case cbrt[] will be calculated more than once.
    static gboolean firstRun = TRUE;
    static float cbrt[0x10000];

    if (firstRun) {
        for (i = 0; i < 0x10000; i++) {
            r = i / 65535.0;
            cbrt[i] = r > 0.008856 ? pow(r, 1 / 3.0) : 7.787 * r + 16 / 116.0;
        }
        firstRun = FALSE;
    }
    xyz[0] = xyz[1] = xyz[2] = 0.5;
    for (c = 0; c < 3; c++)
        for (cc = 0; cc < 3; cc++)
            xyz[cc] += xyz_rgb[cc][c] * rgb[c];
    for (c = 0; c < 3; c++)
        xyz[c] = cbrt[MAX(MIN((int)xyz[c], 0xFFFF), 0)];
    lab[0] = 116 * xyz[1] - 16;
    lab[1] = 500 * (xyz[0] - xyz[1]);
    lab[2] = 200 * (xyz[1] - xyz[2]);

    lch[0] = lab[0];
    lch[1] = sqrt(lab[1] * lab[1] + lab[2] * lab[2]);
    lch[2] = atan2(lab[2], lab[1]);
}

// Convert CIE-LCh to linear RGB
void uf_cielch_to_rgb(float lch[3], gint64 rgb[3])
{
    int c, cc;
    float xyz[3], fx, fy, fz, xr, yr, zr, kappa, epsilon, tmpf, lab[3];
    epsilon = 0.008856;
    kappa = 903.3;
    lab[0] = lch[0];
    lab[1] = lch[1] * cos(lch[2]);
    lab[2] = lch[1] * sin(lch[2]);
    yr = (lab[0] <= kappa * epsilon) ?
         (lab[0] / kappa) : (pow((lab[0] + 16.0) / 116.0, 3.0));
    fy = (yr <= epsilon) ? ((kappa * yr + 16.0) / 116.0) : ((lab[0] + 16.0) / 116.0);
    fz = fy - lab[2] / 200.0;
    fx = lab[1] / 500.0 + fy;
    zr = (pow(fz, 3.0) <= epsilon) ? ((116.0 * fz - 16.0) / kappa) : (pow(fz, 3.0));
    xr = (pow(fx, 3.0) <= epsilon) ? ((116.0 * fx - 16.0) / kappa) : (pow(fx, 3.0));

    xyz[0] = xr * 65535.0 - 0.5;
    xyz[1] = yr * 65535.0 - 0.5;
    xyz[2] = zr * 65535.0 - 0.5;

    for (c = 0; c < 3; c++) {
        tmpf = 0;
        for (cc = 0; cc < 3; cc++)
            tmpf += rgb_xyz[c][cc] * xyz[cc];
        rgb[c] = MAX(tmpf, 0);
    }
}

void uf_raw_to_cielch(const developer_data *d,
                      const guint16 raw[4],
                      float lch[3])
{
    gint64 tmp[4];
    guint16 rgbpixel[3];
    guint16 labpixel[3];
    cmsCIELab Lab;
    cmsCIELCh LCh;
    unsigned int c;

    for (c = 0; c < d->colors; ++c) {
        tmp[c] = raw[c];
        tmp[c] *= d->rgbWB[c];
        tmp[c] /= 0x10000;
    }
    cond_apply_matrix(d, tmp, tmp);
    for (c = 0; c < 3; ++c)
        rgbpixel[c] = tmp[c];

    cmsDoTransform(d->rgbtolabTransform, rgbpixel, labpixel, 1);

    cmsLabEncoded2Float(&Lab, labpixel);
    cmsLab2LCh(&LCh, &Lab);
    lch[0] = LCh.L;
    lch[1] = LCh.C;
    lch[2] = LCh.h;
}

static void MaxMidMin(const gint64 p[3], int *maxc, int *midc, int *minc)
{
    gint64 a = p[0];
    gint64 b = p[1];
    gint64 c = p[2];
    int max = 0;
    int mid = 1;
    int min = 2;

    if (a < b) {
        gint64 tmp = b;
        b = a;
        a = tmp;
        max = 1;
        mid = 0;
    }
    if (b < c) {
        b = c;
        min = mid;
        mid = 2;
        if (a < b) {
            int tmp = max;
            max = mid;
            mid = tmp;
        }
    }

    *maxc = max;
    *midc = mid;
    *minc = min;
}

void develop(void *po, guint16 pix[4], developer_data *d, int mode, int count)
{
    guint16 c, tmppix[3], *buf;
    int i;
    if (mode == 16) buf = po;
    else buf = g_alloca(count * 6);

#ifdef _OPENMP
    #pragma omp parallel				\
    if (count > 16)				\
        default(none)				\
        shared(d, buf, count, pix)			\
        private(i, tmppix, c)
    {
        int chunk = count / omp_get_num_threads() + 1;
        int offset = chunk * omp_get_thread_num();
        int width = (chunk > count - offset) ? count - offset : chunk;
        for (i = offset; i < offset + width; i++) {
            develop_linear(pix + i * 4, tmppix, d);
            for (c = 0; c < 3; c++)
                buf[i * 3 + c] = d->gammaCurve[tmppix[c]];
        }
        if (d->colorTransform != NULL)
            cmsDoTransform(d->colorTransform,
                           buf + offset * 3, buf + offset * 3, width);
    }
#else
    for (i = 0; i < count; i++) {
        develop_linear(pix + i * 4, tmppix, d);
        for (c = 0; c < 3; c++)
            buf[i * 3 + c] = d->gammaCurve[tmppix[c]];
    }
    if (d->colorTransform != NULL)
        cmsDoTransform(d->colorTransform, buf, buf, count);
#endif

    if (mode != 16) {
        guint8 *p8 = po;
        for (i = 0; i < 3 * count; i++) p8[i] = buf[i] >> 8;
    }
}

void develop_display(void *pout, void *pin, developer_data *d, int count)
{
    if (d->working2displayTransform == NULL)
        g_error("develop_display: working2displayTransform == NULL");

    cmsDoTransform(d->working2displayTransform, pin, pout, count);
}

static void develop_grayscale(guint16 *pixel, const developer_data *d)
{
    gint32 spot;
    guint16 min;
    guint16 max;

    switch (d->grayscaleMode) {

        case grayscale_mixer:
            spot = pixel[0] * d->grayscaleMixer[0]
                   + pixel[1] * d->grayscaleMixer[1]
                   + pixel[2] * d->grayscaleMixer[2];
            if (spot > 65535) spot = 65535;
            else if (spot < 0) spot = 0;
            break;

        case grayscale_lightness:
            min = max = pixel[0];
            if (pixel[1] > max) max = pixel[1];
            if (pixel[2] > max) max = pixel[2];
            if (pixel[1] < min) min = pixel[1];
            if (pixel[2] < min) min = pixel[2];
            spot = ((int)min + (int)max) / 2;
            break;

        case grayscale_value:
            max = pixel[0];
            if (pixel[1] > max) max = pixel[1];
            if (pixel[2] > max) max = pixel[2];
            spot = max;
            break;

        default:
            return;
    }
    pixel[0] = pixel[1] = pixel[2] = spot;
}

void develop_linear(guint16 in[4], guint16 out[3], developer_data *d)
{
    unsigned c;
    gint64 tmppix[4];
    gboolean clipped = FALSE;
    for (c = 0; c < d->colors; c++) {
        /* Set WB, normalizing tmppix[c]<0x10000 */
        tmppix[c] = in[c];
        tmppix[c] *= d->rgbWB[c];
        tmppix[c] /= 0x10000;
        if (d->restoreDetails != clip_details &&
                tmppix[c] > d->max) {
            clipped = TRUE;
        } else {
            tmppix[c] = MIN(tmppix[c], d->max);
        }
        /* We are counting on the fact that film_highlights
         * and !clip_highlights cannot be set simultaneously. */
        if (d->clipHighlights == film_highlights)
            tmppix[c] = tmppix[c] * 0x10000 / d->max;
        else
            tmppix[c] = tmppix[c] * d->exposure / d->max;
    }
    if (d->colors == 1)
        tmppix[1] = tmppix[2] = tmppix[0];
    if (clipped) {
        /* At this point a value of d->exposure in tmppix[c] corresponds
         * to "1.0" (full exposure). Still the maximal value can be
         * d->exposure * 0x10000 / d->max */
        gint64 unclippedPix[3], clippedPix[3];
        cond_apply_matrix(d, tmppix, unclippedPix);
        for (c = 0; c < 3; c++) tmppix[c] = MIN(tmppix[c], d->exposure);
        cond_apply_matrix(d, tmppix, clippedPix);
        if (d->restoreDetails == restore_lch_details) {
            float lch[3], clippedLch[3], unclippedLch[3];
            uf_rgb_to_cielch(unclippedPix, unclippedLch);
            uf_rgb_to_cielch(clippedPix, clippedLch);
            //lch[0] = clippedLch[0] + (unclippedLch[0]-clippedLch[0]) * x;
            lch[0] = unclippedLch[0];
            lch[1] = clippedLch[1];
            lch[2] = clippedLch[2];
            uf_cielch_to_rgb(lch, tmppix);
        } else { /* restore_hsv_details */
            int maxc, midc, minc;
            MaxMidMin(unclippedPix, &maxc, &midc, &minc);
            gint64 unclippedLum = unclippedPix[maxc];
            gint64 clippedLum = clippedPix[maxc];
            /*gint64 unclippedSat;
            if ( unclippedPix[maxc]==0 )
                unclippedSat = 0;
            else
                unclippedSat = 0x10000 -
            unclippedPix[minc] * 0x10000 / unclippedPix[maxc];*/
            gint64 clippedSat;
            if (clippedPix[maxc] < clippedPix[minc] || clippedPix[maxc] == 0)
                clippedSat = 0;
            else
                clippedSat = 0x10000 -
                             clippedPix[minc] * 0x10000 / clippedPix[maxc];
            gint64 clippedHue;
            if (clippedPix[maxc] == clippedPix[minc]) clippedHue = 0;
            else clippedHue =
                    (clippedPix[midc] - clippedPix[minc]) * 0x10000 /
                    (clippedPix[maxc] - clippedPix[minc]);
            gint64 unclippedHue;
            if (unclippedPix[maxc] == unclippedPix[minc])
                unclippedHue = clippedHue;
            else
                unclippedHue =
                    (unclippedPix[midc] - unclippedPix[minc]) * 0x10000 /
                    (unclippedPix[maxc] - unclippedPix[minc]);
            /* Here we decide how to mix the clipped and unclipped values.
             * The general equation is clipped + (unclipped - clipped) * x,
             * where x is between 0 and 1. */
            /* For lum we set x=1/2. Thus hightlights are not too bright. */
            gint64 lum = clippedLum + (unclippedLum - clippedLum) * 1 / 2;
            /* For sat we should set x=0 to prevent color artifacts. */
            //gint64 sat = clippedSat + (unclippedSat - clippedSat) * 0/1 ;
            gint64 sat = clippedSat;
            /* For hue we set x=1. This doesn't seem to have much effect. */
            gint64 hue = unclippedHue;

            tmppix[maxc] = lum;
            tmppix[minc] = lum * (0x10000 - sat) / 0x10000;
            tmppix[midc] = lum * (0x10000 - sat + sat * hue / 0x10000) / 0x10000;
        }
    } else { /* !clipped */
        if (d->useMatrix)
            apply_matrix(d, tmppix, tmppix);
        gint64 max = tmppix[0];
        for (c = 1; c < 3; c++) max = MAX(tmppix[c], max);
        if (max > 0xFFFF) {
            gint64 unclippedLum = max;
            gint64 clippedLum = 0xFFFF;
            gint64 lum = clippedLum + (unclippedLum - clippedLum) * 1 / 4;
            for (c = 0; c < 3; c++) tmppix[c] = tmppix[c] * lum / max;
        }
    }
    for (c = 0; c < 3; c++)
        out[c] = MIN(MAX(tmppix[c], 0), 0xFFFF);
    develop_grayscale(out, d);
}

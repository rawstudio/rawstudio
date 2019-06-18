/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_ufraw.c - program interface to all the components
 * Copyright 2004-2016 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufraw.h"
#include "dcraw_api.h"
#ifdef HAVE_LENSFUN
#include <lensfun.h>
#endif
#include <glib/gi18n.h>
#include <string.h>
#include <sys/stat.h> /* for fstat() */
#include <math.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef HAVE_LIBBZ2
#include <bzlib.h>
#endif

void (*ufraw_progress)(int what, int ticks) = NULL;

#ifdef HAVE_LENSFUN
#define UF_LF_TRANSFORM ( \
	LF_MODIFY_DISTORTION | LF_MODIFY_GEOMETRY | LF_MODIFY_SCALE)
static void ufraw_convert_image_vignetting(ufraw_data *uf,
        ufraw_image_data *img, UFRectangle *area);
static void ufraw_convert_image_tca(ufraw_data *uf, ufraw_image_data *img,
                                    ufraw_image_data *outimg,
                                    UFRectangle *area);
void ufraw_prepare_tca(ufraw_data *uf);
#endif
static void ufraw_image_format(int *colors, int *bytes, ufraw_image_data *img,
                               const char *formats, const char *caller);
static void ufraw_convert_image_raw(ufraw_data *uf, UFRawPhase phase);
static void ufraw_convert_image_first(ufraw_data *uf, UFRawPhase phase);
static void ufraw_convert_image_transform(ufraw_data *uf, ufraw_image_data *img,
        ufraw_image_data *outimg, UFRectangle *area);
static void ufraw_convert_prepare_first_buffer(ufraw_data *uf,
        ufraw_image_data *img);
static void ufraw_convert_prepare_transform_buffer(ufraw_data *uf,
        ufraw_image_data *img, int width, int height);
static void ufraw_convert_reverse_wb(ufraw_data *uf, UFRawPhase phase);
static void ufraw_convert_import_buffer(ufraw_data *uf, UFRawPhase phase,
                                        dcraw_image_data *dcimg);

static int make_temporary(char *basefilename, char **tmpfilename)
{
    int fd;
    char *basename = g_path_get_basename(basefilename);
    char *template = g_strconcat(basename, ".tmp.XXXXXX", NULL);
    fd = g_file_open_tmp(template, tmpfilename, NULL);
    g_free(template);
    g_free(basename);
    return fd;
}

static ssize_t writeall(int fd, const char *buf, ssize_t size)
{
    ssize_t written;
    ssize_t wr;
    for (written = 0; size > 0; size -= wr, written += wr, buf += wr)
        if ((wr = write(fd, buf, size)) < 0)
            break;
    return written;
}

static char *decompress_gz(char *origfilename)
{
#ifdef HAVE_LIBZ
    char *tempfilename;
    int tmpfd;
    gzFile gzfile;
    char buf[8192];
    ssize_t size;
    if ((tmpfd = make_temporary(origfilename, &tempfilename)) == -1)
        return NULL;
    char *filename = uf_win32_locale_filename_from_utf8(origfilename);
    gzfile = gzopen(filename, "rb");
    uf_win32_locale_filename_free(filename);
    if (gzfile != NULL) {
        while ((size = gzread(gzfile, buf, sizeof buf)) > 0) {
            if (writeall(tmpfd, buf, size) != size)
                break;
        }
        gzclose(gzfile);
        if (size == 0)
            if (close(tmpfd) == 0)
                return tempfilename;
    }
    close(tmpfd);
    g_unlink(tempfilename);
    g_free(tempfilename);
    return NULL;
#else
    (void)origfilename;
    ufraw_message(UFRAW_SET_ERROR,
                  "Cannot open gzip compressed images.\n");
    return NULL;
#endif
}

static char *decompress_bz2(char *origfilename)
{
#ifdef HAVE_LIBBZ2
    char *tempfilename;
    int tmpfd;
    FILE *compfile;
    BZFILE *bzfile;
    int bzerror;
    char buf[8192];
    ssize_t size;

    if ((tmpfd = make_temporary(origfilename, &tempfilename)) == -1)
        return NULL;
    compfile = g_fopen(origfilename, "rb");
    if (compfile != NULL) {
        if ((bzfile = BZ2_bzReadOpen(&bzerror, compfile, 0, 0, 0, 0)) != 0) {
            while ((size = BZ2_bzRead(&bzerror, bzfile, buf, sizeof buf)) > 0)
                if (writeall(tmpfd, buf, size) != size)
                    break;
            BZ2_bzReadClose(&bzerror, bzfile);
            fclose(compfile);
            if (size == 0) {
                close(tmpfd);
                return tempfilename;
            }
        }
    }
    close(tmpfd);
    g_unlink(tempfilename);
    g_free(tempfilename);
    return NULL;
#else
    (void)origfilename;
    ufraw_message(UFRAW_SET_ERROR,
                  "Cannot open bzip2 compressed images.\n");
    return NULL;
#endif
}

ufraw_data *ufraw_open(char *filename)
{
    int status;
    ufraw_data *uf;
    dcraw_data *raw;
    ufraw_message(UFRAW_CLEAN, NULL);
    conf_data *conf = NULL;
    char *fname, *hostname;
    char *origfilename;
    gchar *unzippedBuf = NULL;
    gsize unzippedBufLen = 0;

    fname = g_filename_from_uri(filename, &hostname, NULL);
    if (fname != NULL) {
        if (hostname != NULL) {
            ufraw_message(UFRAW_SET_ERROR, _("Remote URI is not supported"));
            g_free(hostname);
            g_free(fname);
            return NULL;
        }
        g_strlcpy(filename, fname, max_path);
        g_free(fname);
    }
    /* First handle ufraw ID files. */
    if (strcasecmp(filename + strlen(filename) - 6, ".ufraw") == 0) {
        conf = g_new(conf_data, 1);
        status = conf_load(conf, filename);
        if (status != UFRAW_SUCCESS) {
            g_free(conf);
            return NULL;
        }

        /* If inputFilename and outputFilename have the same path,
         * then inputFilename is searched for in the path of the ID file.
         * This allows moving raw and ID files together between folders. */
        char *inPath = g_path_get_dirname(conf->inputFilename);
        char *outPath = g_path_get_dirname(conf->outputFilename);
        if (strcmp(inPath, outPath) == 0) {
            char *path = g_path_get_dirname(filename);
            char *inName = g_path_get_basename(conf->inputFilename);
            char *inFile = g_build_filename(path, inName , NULL);
            if (g_file_test(inFile, G_FILE_TEST_EXISTS)) {
                g_strlcpy(conf->inputFilename, inFile, max_path);
            }
            g_free(path);
            g_free(inName);
            g_free(inFile);
        }
        g_free(inPath);
        g_free(outPath);

        /* Output image should be created in the path of the ID file */
        char *path = g_path_get_dirname(filename);
        g_strlcpy(conf->outputPath, path, max_path);
        g_free(path);

        filename = conf->inputFilename;
    }
    origfilename = filename;
    if (!strcasecmp(filename + strlen(filename) - 3, ".gz"))
        filename = decompress_gz(filename);
    else if (!strcasecmp(filename + strlen(filename) - 4, ".bz2"))
        filename = decompress_bz2(filename);
    if (filename == 0) {
        ufraw_message(UFRAW_SET_ERROR,
                      "Error creating temporary file for compressed data.");
        return NULL;
    }
    raw = g_new(dcraw_data, 1);
    status = dcraw_open(raw, filename);
    if (filename != origfilename) {
        g_file_get_contents(filename, &unzippedBuf, &unzippedBufLen, NULL);
        g_unlink(filename);
        g_free(filename);
        filename = origfilename;
    }
    if (status != DCRAW_SUCCESS) {
        /* Hold the message without displaying it */
        ufraw_message(UFRAW_SET_WARNING, raw->message);
        if (status != DCRAW_WARNING) {
            g_free(raw);
            g_free(unzippedBuf);
            return NULL;
        }
    }
    uf = g_new0(ufraw_data, 1);
    ufraw_message_init(uf);
    uf->rgbMax = 0; // This indicates that the raw file was not loaded yet.
    uf->unzippedBuf = unzippedBuf;
    uf->unzippedBufLen = unzippedBufLen;
    uf->conf = conf;
    g_strlcpy(uf->filename, filename, max_path);
    int i;
    for (i = ufraw_raw_phase; i < ufraw_phases_num; i++) {
        uf->Images[i].buffer = NULL;
        uf->Images[i].width = 0;
        uf->Images[i].height = 0;
        uf->Images[i].valid = 0;
        uf->Images[i].invalidate_event = TRUE;
    }
    uf->thumb.buffer = NULL;
    uf->raw = raw;
    uf->colors = raw->colors;
    uf->raw_color = raw->raw_color;
    uf->developer = NULL;
    uf->AutoDeveloper = NULL;
    uf->displayProfile = NULL;
    uf->displayProfileSize = 0;
    uf->RawHistogram = NULL;
    uf->HaveFilters = raw->filters != 0;
    uf->IsXTrans = raw->filters == 9;
#ifdef HAVE_LENSFUN
    uf->modFlags = 0;
    uf->TCAmodifier = NULL;
    uf->modifier = NULL;
#endif
    uf->inputExifBuf = NULL;
    uf->outputExifBuf = NULL;
    ufraw_message(UFRAW_SET_LOG, "ufraw_open: w:%d h:%d curvesize:%d\n",
                  raw->width, raw->height, raw->toneCurveSize);

    return uf;
}

int ufraw_load_darkframe(ufraw_data *uf)
{
    if (strlen(uf->conf->darkframeFile) == 0)
        return UFRAW_SUCCESS;
    if (uf->conf->darkframe != NULL) {
        // If the same file was already openned, there is nothing to do.
        if (strcmp(uf->conf->darkframeFile, uf->conf->darkframe->filename) == 0)
            return UFRAW_SUCCESS;
        // Otherwise we need to close the previous darkframe
        ufraw_close_darkframe(uf->conf);
    }
    ufraw_data *dark = uf->conf->darkframe =
                           ufraw_open(uf->conf->darkframeFile);
    if (dark == NULL) {
        ufraw_message(UFRAW_ERROR, _("darkframe error: %s is not a raw file\n"),
                      uf->conf->darkframeFile);
        uf->conf->darkframeFile[0] = '\0';
        return UFRAW_ERROR;
    }
    dark->conf = g_new(conf_data, 1);
    conf_init(dark->conf);
    /* initialize ufobject member */
    dark->conf->ufobject = ufraw_image_new();
    /* disable all auto settings on darkframe */
    dark->conf->autoExposure = disabled_state;
    dark->conf->autoBlack = disabled_state;
    if (ufraw_load_raw(dark) != UFRAW_SUCCESS) {
        ufraw_message(UFRAW_ERROR, _("error loading darkframe '%s'\n"),
                      uf->conf->darkframeFile);
        ufraw_close(dark);
        g_free(dark);
        uf->conf->darkframe = NULL;
        uf->conf->darkframeFile[0] = '\0';
        return UFRAW_ERROR;
    }
    // Make sure the darkframe matches the main data
    dcraw_data *raw = uf->raw;
    dcraw_data *darkRaw = dark->raw;
    if (raw->width != darkRaw->width ||
            raw->height != darkRaw->height ||
            raw->colors != darkRaw->colors) {
        ufraw_message(UFRAW_WARNING,
                      _("Darkframe '%s' is incompatible with main image"),
                      uf->conf->darkframeFile);
        ufraw_close(dark);
        g_free(dark);
        uf->conf->darkframe = NULL;
        uf->conf->darkframeFile[0] = '\0';
        return UFRAW_ERROR;
    }
    ufraw_message(UFRAW_BATCH_MESSAGE, _("using darkframe '%s'\n"),
                  uf->conf->darkframeFile);
    /* Calculate dark frame hot pixel thresholds as the 99.99th percentile
     * value.  That is, the value at which 99.99% of the pixels are darker.
     * Pixels below this threshold are considered to be bias noise, and
     * those above are "hot". */
    int color;
    int i;
    long frequency[65536];
    long sum;
    long point = darkRaw->raw.width * darkRaw->raw.height / 10000;

    for (color = 0; color < darkRaw->raw.colors; ++color) {
        memset(frequency, 0, sizeof frequency);
        for (i = 0; i < darkRaw->raw.width * darkRaw->raw.height; ++i)
            frequency[darkRaw->raw.image[i][color]]++;
        for (sum = 0, i = 65535; i > 1; --i) {
            sum += frequency[i];
            if (sum >= point)
                break;
        }
        darkRaw->thresholds[color] = i + 1;
    }
    return UFRAW_SUCCESS;
}

// Get the dimensions of the unshrunk, rotated image.autoCrop
// The crop coordinates are calculated based on these dimensions.
void ufraw_get_image_dimensions(ufraw_data *uf)
{
    dcraw_image_dimensions(uf->raw, uf->conf->orientation, 1,
                           &uf->initialHeight, &uf->initialWidth);

    ufraw_get_image(uf, ufraw_transform_phase, FALSE);

    if (uf->conf->fullCrop || uf->conf->CropX1 < 0) uf->conf->CropX1 = 0;
    if (uf->conf->fullCrop || uf->conf->CropY1 < 0) uf->conf->CropY1 = 0;
    if (uf->conf->fullCrop || uf->conf->CropX2 < 0) uf->conf->CropX2 = uf->rotatedWidth;
    if (uf->conf->fullCrop || uf->conf->CropY2 < 0) uf->conf->CropY2 = uf->rotatedHeight;

    if (uf->conf->fullCrop)
        uf->conf->aspectRatio = (double)uf->rotatedWidth / uf->rotatedHeight;
    else if (uf->conf->aspectRatio <= 0) {
        if (uf->conf->autoCrop)
            /* preserve the initial aspect ratio - this should be consistent
               with ufraw_convert_prepare_transform */
            uf->conf->aspectRatio = ((double)uf->initialWidth) / uf->initialHeight;
        else
            /* full rotated image / manually entered crop */
            uf->conf->aspectRatio = ((double)uf->conf->CropX2 - uf->conf->CropX1)
                                    / (uf->conf->CropY2 - uf->conf->CropY1);
    } else {
        /* given aspectRatio */
        int cropWidth = uf->conf->CropX2 - uf->conf->CropX1;
        int cropHeight = uf->conf->CropY2 - uf->conf->CropY1;

        if (cropWidth != (int)floor(cropHeight * uf->conf->aspectRatio + 0.5)) {
            /* aspectRatio does not match the crop area - shrink the area */

            if ((double)cropWidth / cropHeight > uf->conf->aspectRatio) {
                cropWidth = floor(cropHeight * uf->conf->aspectRatio + 0.5);
                uf->conf->CropX1 = (uf->conf->CropX1 + uf->conf->CropX2 - cropWidth) / 2;
                uf->conf->CropX2 = uf->conf->CropX1 + cropWidth;
            } else {
                cropHeight = floor(cropWidth / uf->conf->aspectRatio + 0.5);
                uf->conf->CropY1 = (uf->conf->CropY1 + uf->conf->CropY2 - cropHeight) / 2;
                uf->conf->CropY2 = uf->conf->CropY1 + cropHeight;
            }
        }
    }
}

/* Get scaled crop coordinates in final image coordinates */
void ufraw_get_scaled_crop(ufraw_data *uf, UFRectangle *crop)
{
    ufraw_image_data *img = ufraw_get_image(uf, ufraw_transform_phase, FALSE);

    float scale_x = ((float)img->width) / uf->rotatedWidth;
    float scale_y = ((float)img->height) / uf->rotatedHeight;
    crop->x = MAX(floor(uf->conf->CropX1 * scale_x), 0);
    int x2 = MIN(ceil(uf->conf->CropX2 * scale_x), img->width);
    crop->width = x2 - crop->x;
    crop->y = MAX(floor(uf->conf->CropY1 * scale_y), 0);
    int y2 = MIN(ceil(uf->conf->CropY2 * scale_y), img->height);
    crop->height = y2 - crop->y;
}

int ufraw_config(ufraw_data *uf, conf_data *rc, conf_data *conf, conf_data *cmd)
{
    int status;

    if (rc->autoExposure == enabled_state) rc->autoExposure = apply_state;
    if (rc->autoBlack == enabled_state) rc->autoBlack = apply_state;

    g_assert(uf != NULL);

    /* Check if we are loading an ID file */
    if (uf->conf != NULL) {
        /* ID file configuration is put "on top" of the rc data */
        uf->LoadingID = TRUE;
        conf_data tmp = *rc;
        tmp.ufobject = uf->conf->ufobject;
        conf_copy_image(&tmp, uf->conf);
        conf_copy_transform(&tmp, uf->conf);
        conf_copy_save(&tmp, uf->conf);
        g_strlcpy(tmp.outputFilename, uf->conf->outputFilename, max_path);
        g_strlcpy(tmp.outputPath, uf->conf->outputPath, max_path);
        *uf->conf = tmp;
    } else {
        uf->LoadingID = FALSE;
        uf->conf = g_new(conf_data, 1);
        *uf->conf = *rc;
        uf->conf->ufobject = ufraw_image_new();
        ufobject_copy(uf->conf->ufobject,
                      ufgroup_element(rc->ufobject, ufRawImage));
    }
    if (conf != NULL && conf->version != 0) {
        conf_copy_image(uf->conf, conf);
        conf_copy_save(uf->conf, conf);
        if (uf->conf->autoExposure == enabled_state)
            uf->conf->autoExposure = apply_state;
        if (uf->conf->autoBlack == enabled_state)
            uf->conf->autoBlack = apply_state;
    }
    if (cmd != NULL) {
        status = conf_set_cmd(uf->conf, cmd);
        if (status != UFRAW_SUCCESS) return status;
    }
    dcraw_data *raw = uf->raw;
    if (ufobject_name(uf->conf->ufobject) != ufRawImage)
        g_warning("uf->conf->ufobject is not a ufRawImage");
    /*Reset EXIF data text fields to avoid spill over between images.*/
    strcpy(uf->conf->isoText, "");
    strcpy(uf->conf->shutterText, "");
    strcpy(uf->conf->apertureText, "");
    strcpy(uf->conf->focalLenText, "");
    strcpy(uf->conf->focalLen35Text, "");
    strcpy(uf->conf->lensText, "");
    strcpy(uf->conf->flashText, "");
    // lensText is used in ufraw_lensfun_init()
    if (!uf->conf->embeddedImage) {
        if (ufraw_exif_read_input(uf) != UFRAW_SUCCESS) {
            ufraw_message(UFRAW_SET_LOG, "Error reading EXIF data from %s\n",
                          uf->filename);
            // If exiv2 fails to read the EXIF data, use the EXIF tags read
            // by dcraw.
            g_strlcpy(uf->conf->exifSource, "DCRaw", max_name);
            uf->conf->iso_speed = raw->iso_speed;
            g_snprintf(uf->conf->isoText, max_name, "%d",
                       (int)uf->conf->iso_speed);
            uf->conf->shutter = raw->shutter;
            if (uf->conf->shutter > 0 && uf->conf->shutter < 1)
                g_snprintf(uf->conf->shutterText, max_name, "1/%0.1f s",
                           1 / uf->conf->shutter);
            else
                g_snprintf(uf->conf->shutterText, max_name, "%0.1f s",
                           uf->conf->shutter);
            uf->conf->aperture = raw->aperture;
            g_snprintf(uf->conf->apertureText, max_name, "F/%0.1f",
                       uf->conf->aperture);
            uf->conf->focal_len = raw->focal_len;
            g_snprintf(uf->conf->focalLenText, max_name, "%0.1f mm",
                       uf->conf->focal_len);
        }
    }
    ufraw_image_set_data(uf->conf->ufobject, uf);
#ifdef HAVE_LENSFUN
    // Do not reset lensfun settings while loading ID.
    UFBoolean reset = !uf->LoadingID;
    if (conf != NULL && conf->version > 0 && conf->ufobject != NULL) {
        UFObject *conf_lensfun_auto = ufgroup_element(conf->ufobject,
                                      ufLensfunAuto);
        // Do not reset lensfun settings from conf file.
        if (ufstring_is_equal(conf_lensfun_auto, "no"))
            reset = FALSE;
    }
    ufraw_lensfun_init(ufgroup_element(uf->conf->ufobject, ufLensfun), reset);
#endif

    char *absname = uf_file_set_absolute(uf->filename);
    g_strlcpy(uf->conf->inputFilename, absname, max_path);
    g_free(absname);
    if (!uf->LoadingID) {
        g_snprintf(uf->conf->inputURI, max_path, "file://%s",
                   uf->conf->inputFilename);
        struct stat s;
        fstat(fileno(raw->ifp), &s);
        g_snprintf(uf->conf->inputModTime, max_name, "%d", (int)s.st_mtime);
    }
    if (strlen(uf->conf->outputFilename) == 0) {
        /* If output filename wasn't specified use input filename */
        char *filename = uf_file_set_type(uf->filename,
                                          file_type[uf->conf->type]);
        if (strlen(uf->conf->outputPath) > 0) {
            char *cp = g_path_get_basename(filename);
            g_free(filename);
            filename = g_build_filename(uf->conf->outputPath, cp , NULL);
            g_free(cp);
        }
        g_strlcpy(uf->conf->outputFilename, filename, max_path);
        g_free(filename);
    }
    g_free(uf->unzippedBuf);
    uf->unzippedBuf = NULL;
    /* Set the EXIF data */
#ifdef __MINGW32__
    /* MinG32 does not have ctime_r(). */
    g_strlcpy(uf->conf->timestampText, ctime(&raw->timestamp), max_name);
#elif defined(__sun) && !defined(_POSIX_PTHREAD_SEMANTICS) /* Solaris */
    /*
     * Some versions of Solaris followed a draft POSIX.1c standard
     * where ctime_r took a third length argument.
     */
    ctime_r(&raw->timestamp, uf->conf->timestampText,
            sizeof(uf->conf->timestampText));
#else
    /* POSIX.1c version of ctime_r() */
    ctime_r(&raw->timestamp, uf->conf->timestampText);
#endif
    if (uf->conf->timestampText[strlen(uf->conf->timestampText) - 1] == '\n')
        uf->conf->timestampText[strlen(uf->conf->timestampText) - 1] = '\0';

    uf->conf->timestamp = raw->timestamp;

    uf->conf->CameraOrientation = raw->flip;

    if (!uf->conf->rotate) {
        uf->conf->orientation = 0;
        uf->conf->rotationAngle = 0;
    } else {
        if (!uf->LoadingID || uf->conf->orientation < 0)
            uf->conf->orientation = uf->conf->CameraOrientation;
        // Normalise rotations to a flip, then rotation of 0 < a < 90 degrees.
        ufraw_normalize_rotation(uf);
    }
    /* If there is an embeded curve we "turn on" the custom/camera curve
     * mechanism */
    if (raw->toneCurveSize != 0) {
        CurveData nc;
        long pos = ftell(raw->ifp);
        if (RipNikonNEFCurve(raw->ifp, raw->toneCurveOffset, &nc, NULL)
                != UFRAW_SUCCESS) {
            ufraw_message(UFRAW_ERROR, _("Error reading NEF curve"));
            return UFRAW_WARNING;
        }
        fseek(raw->ifp, pos, SEEK_SET);
        if (nc.m_numAnchors < 2) nc = conf_default.BaseCurve[0];

        g_strlcpy(nc.name, uf->conf->BaseCurve[custom_curve].name, max_name);
        uf->conf->BaseCurve[custom_curve] = nc;

        int use_custom_curve = 0;
        if (raw->toneModeSize) {
            // "AUTO    " "HIGH    " "CS      " "MID.L   " "MID.H   "NORMAL  " "LOW     "
            long pos = ftell(raw->ifp);
            char buf[9];
            fseek(raw->ifp, raw->toneModeOffset, SEEK_SET);
            // read it in.
            size_t num = fread(&buf, 9, 1, raw->ifp);
            if (num != 1)
                // Maybe this should be a UFRAW_WARNING
                ufraw_message(UFRAW_SET_LOG,
                              "Warning: tone mode fread %d != %d\n", num, 1);
            fseek(raw->ifp, pos, SEEK_SET);

            if (!strncmp(buf, "CS      ", sizeof(buf)))  use_custom_curve = 1;

            // down the line, we need to translate the other values into
            // tone curves!
        }

        if (use_custom_curve) {
            uf->conf->BaseCurve[camera_curve] =
                uf->conf->BaseCurve[custom_curve];
            g_strlcpy(uf->conf->BaseCurve[camera_curve].name,
                      conf_default.BaseCurve[camera_curve].name, max_name);
        } else {
            uf->conf->BaseCurve[camera_curve] =
                conf_default.BaseCurve[camera_curve];
        }
    } else {
        /* If there is no embeded curve we "turn off" the custom/camera curve
         * mechanism */
        uf->conf->BaseCurve[camera_curve].m_numAnchors = 0;
        uf->conf->BaseCurve[custom_curve].m_numAnchors = 0;
        if (uf->conf->BaseCurveIndex == custom_curve ||
                uf->conf->BaseCurveIndex == camera_curve)
            uf->conf->BaseCurveIndex = linear_curve;
    }
    ufraw_load_darkframe(uf);

    ufraw_get_image_dimensions(uf);

    return UFRAW_SUCCESS;
}

/* Scale pixel values: occupy 16 bits to get more precision. In addition
 * this normalizes the pixel values which is good for non-linear algorithms
 * which forget to check rgbMax or assume a particular value. */
static unsigned ufraw_scale_raw(dcraw_data *raw)
{
    guint16 *p, *end;
    int scale;

    scale = 0;
    while ((raw->rgbMax << 1) <= 0xffff) {
        raw->rgbMax <<= 1;
        ++scale;
    }
    if (scale) {
        end = (guint16 *)(raw->raw.image + raw->raw.width * raw->raw.height);
        /* OpenMP overhead appears to be too large in this case */
        int max = 0x10000 >> scale;
        for (p = (guint16 *)raw->raw.image; p < end; ++p)
            if (*p < max)
                *p <<= scale;
            else
                *p = 0xffff;
        raw->black <<= scale;
    }
    return 1 << scale;
}

int ufraw_load_raw(ufraw_data *uf)
{
    int status;
    dcraw_data *raw = uf->raw;

    if (uf->conf->embeddedImage) {
        dcraw_image_data thumb;
        if ((status = dcraw_load_thumb(raw, &thumb)) != DCRAW_SUCCESS) {
            ufraw_message(status, raw->message);
            return status;
        }
        uf->thumb.height = thumb.height;
        uf->thumb.width = thumb.width;
        return ufraw_read_embedded(uf);
    }
    if ((status = dcraw_load_raw(raw)) != DCRAW_SUCCESS) {
        ufraw_message(UFRAW_SET_LOG, raw->message);
        ufraw_message(status, raw->message);
        if (status != DCRAW_WARNING) return status;
    }
    uf->HaveFilters = raw->filters != 0;
    uf->raw_multiplier = ufraw_scale_raw(raw);
    /* Canon EOS cameras require special exposure normalization */
    if (strcasecmp(uf->conf->make, "Canon") == 0 &&
            strncmp(uf->conf->model, "EOS", 3) == 0) {
        int c, max = raw->cam_mul[0];
        for (c = 1; c < raw->colors; c++) max = MAX(raw->cam_mul[c], max);
        /* Camera multipliers in DNG file are normalized to 1.
         * Therefore, they can not be used to normalize exposure.
         * Also, for some Canon DSLR cameras dcraw cannot read the
         * camera multipliers (1D for example). */
        if (max < 100) {
            uf->conf->ExposureNorm = 0;
            ufraw_message(UFRAW_SET_LOG, "Failed to normalizing exposure\n");
        } else {
            /* Convert exposure value from old ID files from before
             * ExposureNorm */
            if (uf->LoadingID && uf->conf->ExposureNorm == 0)
                uf->conf->exposure -= log(1.0 * raw->rgbMax / max) / log(2);
            uf->conf->ExposureNorm = max * raw->rgbMax / 4095;
            ufraw_message(UFRAW_SET_LOG,
                          "Exposure Normalization set to %d (%.2f EV)\n",
                          uf->conf->ExposureNorm,
                          log(1.0 * raw->rgbMax / uf->conf->ExposureNorm) / log(2));
        }
        /* FUJIFILM cameras have a special tag for exposure normalization */
    } else if (strcasecmp(uf->conf->make, "FUJIFILM") == 0) {
        if (raw->fuji_dr == 0) {
            uf->conf->ExposureNorm = 0;
        } else {
            int c, max = raw->cam_mul[0];
            for (c = 1; c < raw->colors; c++) max = MAX(raw->cam_mul[c], max);
            if (uf->LoadingID && uf->conf->ExposureNorm == 0)
                uf->conf->exposure -= log(1.0 * raw->rgbMax / max) / log(2);
            uf->conf->ExposureNorm = (int)(1.0 * raw->rgbMax * pow(2, (double)raw->fuji_dr / 100));
            ufraw_message(UFRAW_SET_LOG,
                          "Exposure Normalization set to %d (%.2f EV)\n",
                          uf->conf->ExposureNorm, -(float)raw->fuji_dr / 100);
        }
    } else {
        uf->conf->ExposureNorm = 0;
    }
    uf->rgbMax = raw->rgbMax - raw->black;
    memcpy(uf->rgb_cam, raw->rgb_cam, sizeof uf->rgb_cam);

    /* Foveon image dimensions are knows only after load_raw()*/
    ufraw_get_image_dimensions(uf);
    if (uf->conf->CropX2 > uf->rotatedWidth)
        uf->conf->CropX2 = uf->rotatedWidth;
    if (uf->conf->CropY2 > uf->rotatedHeight)
        uf->conf->CropY2 = uf->rotatedHeight;

    // Now we can finally calculate the channel multipliers.
    if (uf->WBDirty) {
        UFObject *wb = ufgroup_element(uf->conf->ufobject, ufWB);
        char *oldWB = g_strdup(ufobject_string_value(wb));
        UFObject *wbTuning = ufgroup_element(uf->conf->ufobject,
                                             ufWBFineTuning);
        double oldTuning = ufnumber_value(wbTuning);
        ufraw_set_wb(uf, FALSE);
        /* Here ufobject's automation goes against us. A change in
         * ChannelMultipliers might change ufWB to uf_manual_wb.
         * So we need to change it back. */
        if (ufarray_is_equal(wb, uf_manual_wb))
            ufobject_set_string(wb, oldWB);
        ufnumber_set(wbTuning, oldTuning);
        g_free(oldWB);
    }
    ufraw_auto_expose(uf);
    ufraw_auto_black(uf);
    return UFRAW_SUCCESS;
}

/* Free any darkframe associated with conf */
void ufraw_close_darkframe(conf_data *conf)
{
    if (conf && conf->darkframe != NULL) {
        ufraw_close(conf->darkframe);
        g_free(conf->darkframe);
        conf->darkframe = NULL;
        conf->darkframeFile[0] = '\0';
    }
}

void ufraw_close(ufraw_data *uf)
{
    dcraw_close(uf->raw);
    g_free(uf->unzippedBuf);
    g_free(uf->raw);
    g_free(uf->inputExifBuf);
    g_free(uf->outputExifBuf);
    int i;
    for (i = ufraw_raw_phase; i < ufraw_phases_num; i++)
        g_free(uf->Images[i].buffer);
    g_free(uf->thumb.buffer);
    developer_destroy(uf->developer);
    developer_destroy(uf->AutoDeveloper);
    g_free(uf->displayProfile);
    g_free(uf->RawHistogram);
#ifdef HAVE_LENSFUN
    if (uf->TCAmodifier != NULL)
        lf_modifier_destroy(uf->TCAmodifier);
    if (uf->modifier != NULL)
        lf_modifier_destroy(uf->modifier);
#endif
    ufobject_delete(uf->conf->ufobject);
    g_free(uf->conf);
    ufraw_message_reset(uf);
    ufraw_message(UFRAW_CLEAN, NULL);
}

/* Return the coordinates and the size of given image subarea.
 * There are always 32 subareas, numbered 0 to 31, ordered in a 4x8 matrix.
 */
UFRectangle ufraw_image_get_subarea_rectangle(ufraw_image_data *img,
        unsigned saidx)
{
    int saw = (img->width + 3) / 4;
    int sah = (img->height + 7) / 8;
    int sax = saidx % 4;
    int say = saidx / 4;
    UFRectangle area;
    area.x = saw * sax;
    area.y = sah * say;
    area.width = (sax < 3) ? saw : (img->width - saw * 3);
    area.height = (say < 7) ? sah : (img->height - sah * 7);
    return area;
}

/* Return the subarea index given some X,Y image coordinates.
 */
unsigned ufraw_img_get_subarea_idx(ufraw_image_data *img, int x, int y)
{
    int saw = (img->width + 3) / 4;
    int sah = (img->height + 7) / 8;
    return (x / saw) + (y / sah) * 4;
}

void ufraw_developer_prepare(ufraw_data *uf, DeveloperMode mode)
{
    int useMatrix = uf->conf->profileIndex[0] == 1 || uf->colors == 4;

    if (mode == auto_developer) {
        if (uf->AutoDeveloper == NULL)
            uf->AutoDeveloper = developer_init();
        developer_prepare(uf->AutoDeveloper, uf->conf,
                          uf->rgbMax, uf->rgb_cam, uf->colors, useMatrix, mode);
    } else {
        if (uf->developer == NULL)
            uf->developer = developer_init();
        if (mode == display_developer) {
            if (uf->conf->profileIndex[display_profile] != 0) {
                g_free(uf->displayProfile);
                uf->displayProfile = NULL;
            }
            developer_display_profile(uf->developer, uf->displayProfile,
                                      uf->displayProfileSize,
                                      uf->conf->profile[display_profile]
                                      [uf->conf->profileIndex[display_profile]].productName);
        }
        developer_prepare(uf->developer, uf->conf,
                          uf->rgbMax, uf->rgb_cam, uf->colors, useMatrix, mode);
    }
}

int ufraw_convert_image(ufraw_data *uf)
{
    uf->mark_hotpixels = FALSE;
    ufraw_developer_prepare(uf, file_developer);
    ufraw_convert_image_raw(uf, ufraw_raw_phase);

    ufraw_image_data *img = &uf->Images[ufraw_first_phase];
    ufraw_convert_prepare_first_buffer(uf, img);
    ufraw_convert_image_first(uf, ufraw_first_phase);

    UFRectangle area = { 0, 0, img->width, img->height };
    // prepare_transform has to be called before applying vignetting
    ufraw_image_data *img2 = &uf->Images[ufraw_transform_phase];
    ufraw_convert_prepare_transform_buffer(uf, img2, img->width, img->height);
#ifdef HAVE_LENSFUN
    if (uf->modifier != NULL) {
        ufraw_convert_image_vignetting(uf, img, &area);
    }
#endif
    if (img2->buffer != NULL) {
        area.width = img2->width;
        area.height = img2->height;
        /* Apply distortion, geometry and rotation */
        ufraw_convert_image_transform(uf, img, img2, &area);
        g_free(img->buffer);
        *img = *img2;
        img2->buffer = NULL;
    }
    if (uf->conf->autoCrop && !uf->LoadingID) {
        ufraw_get_image_dimensions(uf);
        uf->conf->CropX1 = (uf->rotatedWidth - uf->autoCropWidth) / 2;
        uf->conf->CropX2 = uf->conf->CropX1 + uf->autoCropWidth;
        uf->conf->CropY1 = (uf->rotatedHeight - uf->autoCropHeight) / 2;
        uf->conf->CropY2 = uf->conf->CropY1 + uf->autoCropHeight;
    }
    return UFRAW_SUCCESS;
}

#ifdef HAVE_LENSFUN
static void ufraw_convert_image_vignetting(ufraw_data *uf,
        ufraw_image_data *img, UFRectangle *area)
{
    /* Apply vignetting correction first, before distorting the image */
    if (uf->modFlags & LF_MODIFY_VIGNETTING)
        lf_modifier_apply_color_modification(
            uf->modifier, img->buffer,
            area->x, area->y, area->width, area->height,
            LF_CR_4(RED, GREEN, BLUE, UNKNOWN), img->rowstride);
}
#endif

/*
	ufraw_interpolate_pixel_linearly()
	Interpolate a new pixel value, for one or all colors, from a 2x2 pixel
	patch around coordinates x and y in the image, and write it to dst.
*/
/*
	Because integer arithmetic is faster than floating point operations,
	on popular CISC architectures, we cast floats to 32 bit integers,
	scaling them first will maintain sufficient precision.
*/
#define SCALAR 256

static inline void ufraw_interpolate_pixel_linearly(ufraw_image_data *image, float x, float y, ufraw_image_type *dst, int color)
{

    int i, j, c, cmax, xx, yy;
    unsigned int dx, dy, v, weights[2][2];
    ufraw_image_type *src;

    /*
    	When casting a float to an integer it will be rounded toward zero,
    	that will cause problems when x or y is negative (along the top and
    	left border) so, we add 2 and subtract that later, using floor()
    	and round() is much slower.
    */
    x += 2;
    y += 2;

    xx = x;
    yy = y;

    /*
    	Calculate weights for every pixel in the patch using the fractional
    	part of the coordinates.
    */
    dx = (int)(x * SCALAR + 0.5) - (xx * SCALAR);
    dy = (int)(y * SCALAR + 0.5) - (yy * SCALAR);

    weights[0][0] = (SCALAR - dy) * (SCALAR - dx);
    weights[0][1] = (SCALAR - dy) *           dx;
    weights[1][0] =           dy  * (SCALAR - dx);
    weights[1][1] =           dy  *           dx;

    xx -= 2;
    yy -= 2;

    src = (ufraw_image_type *)image->buffer + yy * image->width + xx;

    /* If an existing color number is given, then only that color will be interpolated, else all will be. */
    if (color < 0 || color >= (3 + (image->rgbg == TRUE)))
        c = 0, cmax = 2 + (image->rgbg == TRUE);
    else
        c = cmax = color;

    /* Check if the source pixels are near a border, if they aren't we can use faster code. */
    if (xx >= 0 && yy >= 0 && xx + 1 < image->width && yy + 1 < image->height) {

        for (; c <= cmax ; c++) {

            v = 0;

            for (i = 0 ; i < 2 ; i++)
                for (j = 0 ; j < 2 ; j++)
                    v += weights[i][j] * src[i * image->width + j][c];

            dst[0][c] =  v / (SCALAR * SCALAR);
        }

    } else { /* Near a border. */

        for (; c <= cmax ; c++) {

            v = 0;

            for (i = 0 ; i < 2 ; i++)
                for (j = 0 ; j < 2 ; j++)
                    /* Check if the source pixel lies inside the image */
                    if (xx + j >= 0 && yy + i >= 0 && xx + j < image->width && yy + i < image->height)
                        v += weights[i][j] * src[i * image->width + j][c];

            dst[0][c] =  v / (SCALAR * SCALAR);
        }
    }
}

#undef SCALAR


/* Apply distortion, geometry and rotation in a single pass */
static void ufraw_convert_image_transform(ufraw_data *uf, ufraw_image_data *img,
        ufraw_image_data *outimg, UFRectangle *area)
{
    float sine = sin(uf->conf->rotationAngle * 2 * M_PI / 360);
    float cosine = cos(uf->conf->rotationAngle * 2 * M_PI / 360);

    // If we rotate around the center:
    // srcX = (X-outimg->width/2)*cosine + (Y-outimg->height/2)*sine;
    // srcY = -(X-outimg->width/2)*sine + (Y-outimg->height/2)*cosine;
    // Then the base offset is:
    // baseX = img->width/2;
    // baseY = img->height/2;
    // Since we rotate around the top-left corner, the base offset is:
    float baseX = img->width / 2 - outimg->width / 2 * cosine - outimg->height / 2 * sine;
    float baseY = img->height / 2 + outimg->width / 2 * sine - outimg->height / 2 * cosine;
#ifdef HAVE_LENSFUN
    gboolean applyLF = uf->modifier != NULL && (uf->modFlags & UF_LF_TRANSFORM);
#endif
    int x, y;
    for (y = area->y; y < area->y + area->height; y++) {
        guint8 *cur0 = outimg->buffer + y * outimg->rowstride;
        float srcX0 = y * sine + baseX;
        float srcY0 = y * cosine + baseY;
        for (x = area->x; x < area->x + area->width; x++) {
            guint16 *cur = (guint16 *)(cur0 + x * outimg->depth);
            float srcX = srcX0 + x * cosine;
            float srcY = srcY0 - x * sine;
#ifdef HAVE_LENSFUN
            if (applyLF) {
                float buff[2];
                lf_modifier_apply_geometry_distortion(uf->modifier,
                                                      srcX, srcY, 1, 1, buff);
                srcX = buff[0];
                srcY = buff[1];
            }
#endif
            ufraw_interpolate_pixel_linearly(img, srcX, srcY, (ufraw_image_type *)cur, -1);
        }
    }
}

/*
 * A pixel with a significantly larger value than all of its four direct
 * neighbours is considered "hot". It will be replaced by the maximum value
 * of its neighbours. For simplicity border pixels are not considered.
 *
 * Reasonable values for uf->conf->hotpixel are in the range 0.5-10.
 *
 * Note that the algorithm uses pixel values from previous (processed) and
 * next (unprocessed) row and whether or not pixels are marked may make a
 * difference for the hot pixel count.
 *
 * Cleanup issue:
 * -	change prototype to void x(ufraw_data *uf, UFRawPhase phase)
 * -	use ufraw_image_format()
 * -	use uf->rgbMax (check, must be about 64k)
 */
static void ufraw_shave_hotpixels(ufraw_data *uf, dcraw_image_type *img,
                                  int width, int height, int colors,
                                  unsigned rgbMax)
{
    int w, h, c, i, count;
    unsigned delta, t, v, hi;
    dcraw_image_type *p;

    uf->hotpixels = 0;
    if (uf->conf->hotpixel <= 0.0)
        return;
    delta = rgbMax / (uf->conf->hotpixel + 1.0);
    count = 0;
#ifdef _OPENMP
    #pragma omp parallel for schedule(static) \
    shared(uf,img,width,height,colors,rgbMax,delta) \
reduction(+:count) \
    private(h,p,w,c,t,v,hi,i)
#endif
    for (h = 1; h < height - 1; ++h) {
        p = img + 1 + h * width;
        for (w = 1; w < width - 1; ++w, ++p) {
            for (c = 0; c < colors; ++c) {
                t = p[0][c];
                if (t <= delta)
                    continue;
                t -= delta;
                v = p[-1][c];
                if (v > t)
                    continue;
                hi = v;
                v = p[1][c];
                if (v > t)
                    continue;
                if (v > hi)
                    hi = v;
                v = p[-width][c];
                if (v > t)
                    continue;
                if (v > hi)
                    hi = v;
                v = p[width][c];
                if (v > t)
                    continue;
                if (v > hi)
                    hi = v;
                /* mark the pixel using the original hot value */
                if (uf->mark_hotpixels) {
                    for (i = -10; i >= -20 && w + i >= 0; --i)
                        memcpy(p[i], p[0], sizeof(p[i]));
                    for (i = 10; i <= 20 && w + i < width; ++i)
                        memcpy(p[i], p[0], sizeof(p[i]));
                }
                p[0][c] = hi;
                ++count;
            }
        }
    }
    uf->hotpixels = count;
}

static void ufraw_despeckle_line(guint16 *base, int step, int size, int window,
                                 double decay, int colors, int c)
{
    unsigned lum[size];
    int i, j, start, end, next, v, cold, hot, coldj, hotj, fix;
    guint16 *p;

    if (colors == 4) {
        for (i = 0; i < size; ++i) {
            p = base + i * step;
            lum[i] = (p[0] + p[1] + p[2] + p[3] - p[c]) / 3;
        }
    } else {
        for (i = 0; i < size; ++i) {
            p = base + i * step;
            lum[i] = (p[0] + p[1] + p[2] - p[c]) / 2;
        }
    }
    p = base + c;
    for (i = 1 - window; i < size; i = next) {
        start = i;
        end = i + window;
        if (start < 0)
            start = 0;
        if (end > size)
            end = size;
        cold = hot = p[start * step] - lum[start];
        coldj = hotj = start;
        for (j = start + 1; j < end; ++j) {
            v = p[j * step] - lum[j];
            if (v < cold) {
                cold = v;
                coldj = j;
            } else if (v > hot) {
                hot = v;
                hotj = j;
            }
        }
        if (cold < 0 && hot > 0) {
            fix = -cold;
            if (fix > hot)
                fix = hot;
            p[coldj * step] += fix;
            p[hotj * step] -= fix;
            hot -= fix;
        }
        if (hot > 0 && decay)
            p[hotj * step] -= hot * decay;
        next = coldj < hotj ? coldj : hotj;
        if (next == start)
            ++next;
    }
}

void ufraw_despeckle(ufraw_data *uf, UFRawPhase phase)
{
    ufraw_image_data *img = &uf->Images[phase];
    const int depth = img->depth / 2, rowstride = img->rowstride / 2;
    int passes[4], pass, maxpass;
    int win[4], i, c, colors;
    guint16 *base;
    double decay[4];

    ufraw_image_format(&colors, NULL, img, "68", G_STRFUNC);
    maxpass = 0;
    for (c = 0; c < colors; ++c) {
        win[c] = uf->conf->despeckleWindow[c < 3 ? c : 1] + 0.01;
        decay[c] = uf->conf->despeckleDecay[c < 3 ? c : 1];
        passes[c] = uf->conf->despecklePasses[c < 3 ? c : 1] + 0.01;
        if (!win[c])
            passes[c] = 0;
        if (passes[c] > maxpass)
            maxpass = passes[c];
    }
    progress(PROGRESS_DESPECKLE, -maxpass * colors);
    for (pass = maxpass - 1; pass >= 0; --pass) {
        for (c = 0; c < colors; ++c) {
            progress(PROGRESS_DESPECKLE, 1);
            if (pass >= passes[c])
                continue;
#ifdef _OPENMP
            #pragma omp parallel for default(shared) private(i,base)
#endif
            for (i = 0; i < img->height; ++i) {
                base = (guint16 *)img->buffer + i * rowstride;
                ufraw_despeckle_line(base, depth, img->width, win[c],
                                     decay[c], colors, c);
            }
#ifdef _OPENMP
            #pragma omp parallel for default(shared) private(i,base)
#endif
            for (i = 0; i < img->width; ++i) {
                base = (guint16 *)img->buffer + i * depth;
                ufraw_despeckle_line(base, rowstride, img->height, win[c],
                                     decay[c], colors, c);
            }
        }
    }
}

static gboolean ufraw_despeckle_active(ufraw_data *uf)
{
    int i;
    gboolean active = FALSE;

    for (i = 0; i < 3; ++i) {
        if (uf->conf->despeckleWindow[i] && uf->conf->despecklePasses[i])
            active = TRUE;
    }
    return active;
}

static int ufraw_calculate_scale(ufraw_data *uf)
{
    /* In the first call to ufraw_calculate_scale() the crop coordinates
     * are not set. They cannot be set, since uf->rotatedHeight/Width are
     * only calculated later in ufraw_convert_prepare_transform_buffer().
     * Therefore, if size > 0, scale = 1 will be returned.
     * Since the first call is from ufraw_convert_prepare_first_buffer(),
     * this is not a real issue. There should always be a second call to
     * this function before the actual buffer allocation. */
    dcraw_data *raw = uf->raw;
    int scale = 1;

    /* We can do a simple interpolation in the following cases:
     * We shrink by an integer value.
     * If pixel_aspect<1 (e.g. NIKON D1X) shrink must be at least 4. */
    if (uf->conf->size == 0 && uf->conf->shrink > 1) {
        scale = uf->conf->shrink * MIN(raw->pixel_aspect, 1 / raw->pixel_aspect);
    } else if (uf->conf->interpolation == half_interpolation) {
        scale = 2;
        /* Wanted size is smaller than raw size (size is after a raw->shrink)
         * (assuming there are filters). */
    } else if (uf->conf->size > 0 && uf->HaveFilters && !uf->IsXTrans) {
        int cropHeight = uf->conf->CropY2 - uf->conf->CropY1;
        int cropWidth = uf->conf->CropX2 - uf->conf->CropX1;
        int cropSize = MAX(cropHeight, cropWidth);
        if (cropSize / uf->conf->size >= 2)
            scale = cropSize / uf->conf->size;
    }
    return scale;
}

// Any change to ufraw_convertshrink() that might change the final image
// dimensions should also be applied to ufraw_convert_prepare_first_buffer().
static void ufraw_convertshrink(ufraw_data *uf, dcraw_image_data *final)
{
    dcraw_data *raw = uf->raw;
    int scale = ufraw_calculate_scale(uf);

    if (uf->HaveFilters && scale == 1)
        dcraw_finalize_interpolate(final, raw, uf->conf->interpolation,
                                   uf->conf->smoothing);
    else
        dcraw_finalize_shrink(final, raw, scale);

    dcraw_image_stretch(final, raw->pixel_aspect);
    if (uf->conf->size == 0 && uf->conf->shrink > 1) {
        dcraw_image_resize(final,
                           scale * MAX(final->height, final->width) / uf->conf->shrink);
    }
    if (uf->conf->size > 0) {
        int finalSize = scale * MAX(final->height, final->width);
        int cropSize;
        if (uf->conf->CropX1 == -1) {
            cropSize = finalSize;
        } else {
            int cropHeight = uf->conf->CropY2 - uf->conf->CropY1;
            int cropWidth = uf->conf->CropX2 - uf->conf->CropX1;
            cropSize = MAX(cropHeight, cropWidth);
        }
        // cropSize needs to be a integer multiplier of scale
        cropSize = cropSize / scale * scale;
        if (uf->conf->size > cropSize) {
            ufraw_message(UFRAW_ERROR, _("Can not downsize from %d to %d."),
                          cropSize, uf->conf->size);
        } else {
            /* uf->conf->size holds the size of the cropped image.
             * We need to calculate from it the desired size of
             * the uncropped image. */
            dcraw_image_resize(final, uf->conf->size * finalSize / cropSize);
        }
    }
}

/*
 * Interface of ufraw_shave_hotpixels(), dcraw_finalize_raw() and preferably
 * dcraw_wavelet_denoise() too should change to accept a phase argument and
 * no longer require type casts.
 */
static void ufraw_convert_image_raw(ufraw_data *uf, UFRawPhase phase)
{
    ufraw_image_data *img = &uf->Images[phase];
    dcraw_data *dark = uf->conf->darkframe ? uf->conf->darkframe->raw : NULL;
    dcraw_data *raw = uf->raw;
    dcraw_image_type *rawimage;

    ufraw_convert_import_buffer(uf, phase, &raw->raw);
    img->rgbg = raw->raw.colors == 4;
    ufraw_shave_hotpixels(uf, (dcraw_image_type *)(img->buffer), img->width,
                          img->height, raw->raw.colors, raw->rgbMax);
    rawimage = raw->raw.image;
    raw->raw.image = (dcraw_image_type *)img->buffer;
    /* The threshold is scaled for compatibility */
    if (!uf->IsXTrans) dcraw_wavelet_denoise(raw, uf->conf->threshold * sqrt(uf->raw_multiplier));
    dcraw_finalize_raw(raw, dark, uf->developer->rgbWB);
    raw->raw.image = rawimage;
    ufraw_despeckle(uf, phase);
#ifdef HAVE_LENSFUN
    ufraw_prepare_tca(uf);
    if (uf->TCAmodifier != NULL) {
        ufraw_image_data inImg = *img;
        img->buffer = g_malloc(img->height * img->rowstride);
        UFRectangle area = {0, 0, img->width, img->height };
        ufraw_convert_image_tca(uf, &inImg, img, &area);
        g_free(inImg.buffer);
    }
#endif
}

/*
 * Interface of ufraw_convertshrink() and dcraw_flip_image() should change
 * to accept a phase argument and no longer require type casts.
 */
static void ufraw_convert_image_first(ufraw_data *uf, UFRawPhase phase)
{
    ufraw_image_data *in = &uf->Images[phase - 1];
    ufraw_image_data *out = &uf->Images[phase];
    dcraw_data *raw = uf->raw;

    dcraw_image_data final;
    final.image = (ufraw_image_type *)out->buffer;

    dcraw_image_type *rawimage = raw->raw.image;
    raw->raw.image = (dcraw_image_type *)in->buffer;
    ufraw_convertshrink(uf, &final);
    raw->raw.image = rawimage;
    dcraw_flip_image(&final, uf->conf->orientation);
    /* The threshold is scaled for compatibility */
    if (uf->IsXTrans) dcraw_wavelet_denoise_shrinked(&final, uf->conf->threshold * sqrt(uf->raw_multiplier));

    // The 'out' image contains the predicted image dimensions.
    // We want to be sure that our predictions were correct.
    if (out->height != final.height) {
        g_warning("ufraw_convert_image_first: height mismatch %d!=%d",
                  out->height, final.height);
        out->height = final.height;
    }
    if (out->width != final.width) {
        g_warning("ufraw_convert_image_first: width mismatch %d!=%d",
                  out->width, final.width);
        out->width = final.width;
    }
    out->depth = sizeof(dcraw_image_type);
    out->rowstride = out->width * out->depth;
    out->buffer = (guint8 *)final.image;

    ufraw_convert_reverse_wb(uf, phase);
}

static void ufraw_convert_reverse_wb(ufraw_data *uf, UFRawPhase phase)
{
    ufraw_image_data *img = &uf->Images[phase];
    guint32 mul[4], px;
    guint16 *p16;
    int i, size, c;

    ufraw_image_format(NULL, NULL, img, "6", G_STRFUNC);
    /* The speedup trick is to keep the non-constant (or ugly constant)
     * divider out of the pixel iteration. If you really have to then
     * use double division (can be much faster, apparently). */
    for (i = 0; i < uf->colors; ++i)
        mul[i] = (guint64)0x10000 * 0x10000 / uf->developer->rgbWB[i];
    size = img->height * img->width;
#ifdef _OPENMP
    #pragma omp parallel for schedule(static) \
    shared(uf,phase,img,mul,size) \
    private(i,p16,c,px)
#endif
    for (i = 0; i < size; ++i) {
        p16 = (guint16 *)&img->buffer[i * img->depth];
        for (c = 0; c < uf->colors; ++c) {
            px = p16[c] * (guint64)mul[c] / 0x10000;
            if (px > 0xffff)
                px = 0xffff;
            p16[c] = px;
        }
    }
}

#ifdef HAVE_LENSFUN
/* Apply TCA */
static void ufraw_convert_image_tca(ufraw_data *uf, ufraw_image_data *img,
                                    ufraw_image_data *outimg,
                                    UFRectangle *area)
{
    if (uf->TCAmodifier == NULL)
        return;
    int y;
#ifdef _OPENMP
    #pragma omp parallel for schedule(static) \
    shared(uf,img,outimg,area)
#endif
    for (y = area->y; y < area->y + area->height; y++) {
        guint16 *dst = (guint16*)(outimg->buffer + y * outimg->rowstride +
                                  area->x * outimg->depth);
        ufraw_image_type *src = (ufraw_image_type *)(img->buffer +
                                y * img->rowstride + area->x * img->depth);
        ufraw_image_type *srcEnd = (ufraw_image_type *)(img->buffer +
                                   y * img->rowstride + (area->x + area->width) * img->depth);
        float buff[3 * 2 * area->width];
        lf_modifier_apply_subpixel_distortion(uf->TCAmodifier,
                                              area->x, y, area->width, 1, buff);
        float *modcoord = buff;
        for (; src < srcEnd; src++, dst += outimg->depth / 2) {
            int c;
            // Only red and blue channels get corrected
            for (c = 0; c <= 2; c += 2, modcoord += 4)
                ufraw_interpolate_pixel_linearly(img, modcoord[0], modcoord[1], (ufraw_image_type *)dst, c);

            modcoord -= 2;
            // Green channels are intact
            for (c = 1; c <= 3; c += 2)
                dst[c] = src[0][c];
        }
    }
}
#endif // HAVE_LENSFUN

static void ufraw_convert_import_buffer(ufraw_data *uf, UFRawPhase phase,
                                        dcraw_image_data *dcimg)
{
    ufraw_image_data *img = &uf->Images[phase];

    img->height = dcimg->height;
    img->width = dcimg->width;
    img->depth = sizeof(dcraw_image_type);
    img->rowstride = img->width * img->depth;
    g_free(img->buffer);
    img->buffer = g_memdup(dcimg->image, img->height * img->rowstride);
}

static void ufraw_image_init(ufraw_image_data *img,
                             int width, int height, int bitdepth)
{
    if (img->height == height && img->width == width &&
            img->depth == bitdepth && img->buffer != NULL)
        return;

    img->valid = 0;
    img->height = height;
    img->width = width;
    img->depth = bitdepth;
    img->rowstride = img->width * img->depth;
    img->buffer = g_realloc(img->buffer, img->height * img->rowstride);
}

static void ufraw_convert_prepare_first_buffer(ufraw_data *uf,
        ufraw_image_data *img)
{
    // The actual buffer allocation is done in ufraw_convertshrink().
    int scale = ufraw_calculate_scale(uf);
    dcraw_image_dimensions(uf->raw, uf->conf->orientation, scale,
                           &img->height, &img->width);
    // The final resizing in ufraw_convertshrink() is calculate here:
    if (uf->conf->size == 0 && uf->conf->shrink > 1) {
        // This is the effect of first call to dcraw_image_resize().
        // It only relevant when raw->pixel_aspect != 1.
        img->width = img->width * scale / uf->conf->shrink;
        img->height = img->height * scale / uf->conf->shrink;
    }
    if (uf->conf->size > 0) {
        int finalSize = scale * MAX(img->height, img->width);
        int cropSize;
        if (uf->conf->CropX1 == -1) {
            cropSize = finalSize;
        } else {
            int cropHeight = uf->conf->CropY2 - uf->conf->CropY1;
            int cropWidth = uf->conf->CropX2 - uf->conf->CropX1;
            cropSize = MAX(cropHeight, cropWidth);
        }
        // cropSize needs to be a integer multiplier of scale
        cropSize = cropSize / scale * scale;
        if (uf->conf->size > cropSize) {
            ufraw_message(UFRAW_ERROR, _("Can not downsize from %d to %d."),
                          cropSize, uf->conf->size);
        } else {
            /* uf->conf->size holds the size of the cropped image.
             * We need to calculate from it the desired size of
             * the uncropped image. */
            int mul = uf->conf->size * finalSize / cropSize;
            int div = MAX(img->height, img->width);
            img->height = img->height * mul / div;
            img->width = img->width * mul / div;
        }
    }
}

#ifdef HAVE_LENSFUN
void ufraw_convert_prepare_transform(ufraw_data *uf,
                                     int width, int height, gboolean reverse,
                                     float scale);
#endif

static void ufraw_convert_prepare_transform_buffer(ufraw_data *uf,
        ufraw_image_data *img, int width, int height)
{
    const int iWidth = uf->initialWidth;
    const int iHeight = uf->initialHeight;

    double aspectRatio = uf->conf->aspectRatio;

    if (aspectRatio == 0)
        aspectRatio = ((double)iWidth) / iHeight;

#ifdef HAVE_LENSFUN
    ufraw_convert_prepare_transform(uf, iWidth, iHeight, TRUE, 1.0);
    if (uf->conf->rotationAngle == 0 &&
            (uf->modifier == NULL || !(uf->modFlags & UF_LF_TRANSFORM))) {
#else
    if (uf->conf->rotationAngle == 0) {
#endif
        g_free(img->buffer);
        img->buffer = NULL;
        img->width = width;
        img->height = height;
        // We still need the transform for vignetting
#ifdef HAVE_LENSFUN
        ufraw_convert_prepare_transform(uf, width, height, FALSE, 1.0);
#endif
        uf->rotatedWidth = iWidth;
        uf->rotatedHeight = iHeight;
        uf->autoCropWidth = iWidth;
        uf->autoCropHeight = iHeight;
        if ((double)uf->autoCropWidth / uf->autoCropHeight > aspectRatio)
            uf->autoCropWidth = floor(uf->autoCropHeight * aspectRatio + 0.5);
        else
            uf->autoCropHeight = floor(uf->autoCropWidth / aspectRatio + 0.5);

        return;
    }
    const double sine = sin(uf->conf->rotationAngle * 2 * M_PI / 360);
    const double cosine = cos(uf->conf->rotationAngle * 2 * M_PI / 360);

    const float midX = iWidth / 2.0 - 0.5;
    const float midY = iHeight / 2.0 - 0.5;
#ifdef HAVE_LENSFUN
    gboolean applyLF = uf->modifier != NULL && (uf->modFlags & UF_LF_TRANSFORM);
#endif
    float maxX = 0, maxY = 0;
    float minX = 999999, minY = 999999;
    double lastX = 0, lastY = 0, area = 0;
    int i;
    for (i = 0; i < iWidth + iHeight - 1; i++) {
        int x, y;
        if (i < iWidth) { // Trace the left border of the image
            x = i;
            y = 0;
        } else { // Trace the bottom border of the image
            x = iWidth - 1;
            y = i - iWidth + 1;
        }
        float buff[2];
#ifdef HAVE_LENSFUN
        if (applyLF) {
            lf_modifier_apply_geometry_distortion(uf->modifier,
                                                  x, y, 1, 1, buff);
        } else {
            buff[0] = x;
            buff[1] = y;
        }
#else
        buff[0] = x;
        buff[1] = y;
#endif
        double srcX = (buff[0] - midX) * cosine - (buff[1] - midY) * sine;
        double srcY = (buff[0] - midX) * sine + (buff[1] - midY) * cosine;
        // A digital planimeter:
        area += srcY * lastX - srcX * lastY;
        lastX = srcX;
        lastY = srcY;
        maxX = MAX(maxX, fabs(srcX));
        maxY = MAX(maxY, fabs(srcY));
        if (fabs(srcX / srcY) > aspectRatio)
            minX = MIN(minX, fabs(srcX));
        else
            minY = MIN(minY, fabs(srcY));
    }
    float scale = sqrt((iWidth - 1) * (iHeight - 1) / area);
    // Do not allow increasing canvas size by more than a factor of 2
    uf->rotatedWidth = MIN(ceil(2 * maxX + 1.0) * scale, 2 * iWidth);
    uf->rotatedHeight = MIN(ceil(2 * maxY + 1.0) * scale, 2 * iHeight);

    uf->autoCropWidth = MIN(floor(2 * minX) * scale, 2 * iWidth);
    uf->autoCropHeight = MIN(floor(2 * minY) * scale, 2 * iHeight);

    if ((double)uf->autoCropWidth / uf->autoCropHeight > aspectRatio)
        uf->autoCropWidth = floor(uf->autoCropHeight * aspectRatio + 0.5);
    else
        uf->autoCropHeight = floor(uf->autoCropWidth / aspectRatio + 0.5);

    int newWidth = uf->rotatedWidth * width / iWidth;
    int newHeight = uf->rotatedHeight * height / iHeight;
    ufraw_image_init(img, newWidth, newHeight, 8);
#ifdef HAVE_LENSFUN
    ufraw_convert_prepare_transform(uf, width, height, FALSE, scale);
#endif
}

/*
 * This function does not set img->invalidate_event because the
 * invalidation here is a secondary effect of the need to resize
 * buffers. The invalidate events should all have been set already.
 */
static void ufraw_convert_prepare_buffers(ufraw_data *uf, UFRawPhase phase)
{
    ufraw_image_data *img = &uf->Images[phase];
    if (!img->invalidate_event)
        return;
    img->invalidate_event = FALSE;
    int width = 0, height = 0;
    if (phase > ufraw_first_phase) {
        ufraw_convert_prepare_buffers(uf, phase - 1);
        width = uf->Images[phase - 1].width;
        height = uf->Images[phase - 1].height;
    }
    switch (phase) {
        case ufraw_raw_phase:
            return;
        case ufraw_first_phase:
            ufraw_convert_prepare_first_buffer(uf, img);
            return;
        case ufraw_transform_phase:
            ufraw_convert_prepare_transform_buffer(uf, img, width, height);
            return;
        case ufraw_develop_phase:
            ufraw_image_init(img, width, height, 3);
            return;
        case ufraw_display_phase:
            if (uf->developer->working2displayTransform == NULL) {
                g_free(img->buffer);
                img->buffer = NULL;
                img->width = width;
                img->height = height;
            } else {
                ufraw_image_init(img, width, height, 3);
            }
            return;
        default:
            g_warning("ufraw_convert_prepare_buffers: unsupported phase %d", phase);
    }
}

/*
 * This function is very permissive in accepting NULL pointers but it does
 * so to make it easy to call this function: consider it documentation with
 * a free consistency check. It is not necessarily good to change existing
 * algorithms all over the place to accept more image formats: replacing
 * constants by variables may turn off some compiler optimizations.
 */
static void ufraw_image_format(int *colors, int *bytes, ufraw_image_data *img,
                               const char *formats, const char *caller)
{
    int b, c;

    switch (img->depth) {
        case 3:
            c = 3;
            b = 1;
            break;
        case 4:
            c = img->rgbg ? 4 : 3;
            b = 1;
            break;
        case 6:
            c = 3;
            b = 2;
            break;
        case 8:
            c = img->rgbg ? 4 : 3;
            b = 2;
            break;
        default:
            g_error("%s -> %s: unsupported depth %d\n", caller, G_STRFUNC, img->depth);
    }
    if (!strchr(formats, '0' + c * b))
        g_error("%s: unsupported depth %d (rgbg=%d)\n", caller, img->depth, img->rgbg);
    if (colors)
        *colors = c;
    if (bytes)
        *bytes = b;
}

ufraw_image_data *ufraw_get_image(ufraw_data *uf, UFRawPhase phase,
                                  gboolean bufferok)
{
    ufraw_convert_prepare_buffers(uf, phase);
    // Find the closest phase that is actually rendered:
    while (phase > ufraw_raw_phase && uf->Images[phase].buffer == NULL)
        phase--;

    if (bufferok) {
        /* It should never be necessary to actually finish the conversion
         * because it can break render_preview_image() which uses the
         * final image "valid" mask for deciding what to update in the
         * pixbuf. That can be fixed but is suboptimal anyway. The best
         * we can do is print a warning in case we need to finish the
         * conversion and finish it here. */
        if (uf->Images[phase].valid != 0xffffffff) {
            g_warning("%s: fixing unfinished conversion for phase %d.\n",
                      G_STRFUNC, phase);
            int i;
            for (i = 0; i < 32; ++i)
                ufraw_convert_image_area(uf, i, phase);
        }
    }
    return &uf->Images[phase];
}

ufraw_image_data *ufraw_convert_image_area(ufraw_data *uf, unsigned saidx,
        UFRawPhase phase)
{
    int yy;
    ufraw_image_data *out = &uf->Images[phase];

    if (out->valid & (1 << saidx))
        return out; // the subarea has been already computed

    /* Get the subarea image for previous phase */
    ufraw_image_data *in = NULL;
    if (phase > ufraw_raw_phase) {
        in = ufraw_convert_image_area(uf, saidx, phase - 1);
    }
    // ufraw_convert_prepare_buffers() may set out->buffer to NULL.
    ufraw_convert_prepare_buffers(uf, phase);
    if (phase > ufraw_first_phase && out->buffer == NULL)
        return in; // skip phase

    /* Get subarea coordinates */
    UFRectangle area = ufraw_image_get_subarea_rectangle(out, saidx);
    guint8 *dest = out->buffer + area.y * out->rowstride + area.x * out->depth;
    guint8 *src = NULL;
    if (in != NULL)
        src = in->buffer + area.y * in->rowstride + area.x * in->depth;

    switch (phase) {
        case ufraw_raw_phase:
            ufraw_convert_image_raw(uf, phase);
            out->valid = 0xffffffff;
            return out;

        case ufraw_first_phase:
            ufraw_convert_image_first(uf, phase);
            out->valid = 0xffffffff;
#ifdef HAVE_LENSFUN
            UFRectangle allArea = { 0, 0, out->width, out->height };
            ufraw_convert_image_vignetting(uf, out, &allArea);
#endif /* HAVE_LENSFUN */
            return out;

        case ufraw_transform_phase: {
            /* Area calculation is not needed at the moment since
             * ufraw_first_phase is not tiled yet. */
            /*
                        int yy;
                        float *buff = g_new (float, (w < 8) ? 8 * 2 * 3 : w * 2 * 3);

                        // Compute the previous stage subareas, if needed
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x, y, 1, 1, buff);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x + w/2, y, 1, 1, buff + 2 * 3);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x + w-1, y, 1, 1, buff + 4 * 3);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x, y + h/2, 1, 1, buff + 6 * 3);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x + w-1, y + h/2, 1, 1, buff + 8 * 3);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x, y + h-1, 1, 1, buff + 10 * 3);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x + w/2, y + h-1, 1, 1, buff + 12 * 3);
                        lf_modifier_apply_subpixel_geometry_distortion (
                            uf->modifier, x + w-1, y + h-1, 1, 1, buff + 14 * 3);

                        for (yy = 0; yy < 8 * 2 * 3; yy += 2)
                        {
                            int idx = ufraw_img_get_subarea_idx (in, buff [yy], buff [yy + 1]);
                            if (idx <= 31)
                                ufraw_convert_image_area (uf, idx, phase - 1);
                        }
            */
            ufraw_convert_image_transform(uf, in, out, &area);
        }
        break;

        case ufraw_develop_phase:
            for (yy = 0; yy < area.height; yy++, dest += out->rowstride,
                    src += in->rowstride) {
                develop(dest, (void *)src, uf->developer, 8, area.width);
            }
            break;

        case ufraw_display_phase:
            for (yy = 0; yy < area.height; yy++, dest += out->rowstride,
                    src += in->rowstride) {
                develop_display(dest, src, uf->developer, area.width);
            }
            break;

        default:
            g_warning("%s: invalid phase %d\n", G_STRFUNC, phase);
            return in;
    }

#ifdef _OPENMP
    #pragma omp critical
#endif
    // Mark the subarea as valid
    out->valid |= (1 << saidx);

    return out;
}

static void ufraw_flip_image_buffer(ufraw_image_data *img, int flip)
{
    if (img->buffer == NULL)
        return;
    /* Following code was copied from dcraw's flip_image()
     * and modified to work with any pixel depth. */
    int base, dest, next, row, col;
    guint8 *image = img->buffer;
    int height = img->height;
    int width = img->width;
    int depth = img->depth;
    int size = height * width;
    guint8 hold[8];
    unsigned *flag = g_new0(unsigned, (size + 31) >> 5);
    for (base = 0; base < size; base++) {
        if (flag[base >> 5] & (1 << (base & 31)))
            continue;
        dest = base;
        memcpy(hold, image + base * depth, depth);
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
            memcpy(image + dest * depth, image + next * depth, depth);
            dest = next;
        }
        memcpy(image + dest * depth, hold, depth);
    }
    g_free(flag);
    if (flip & 4) {
        img->height = width;
        img->width = height;
        img->rowstride = height * depth;
    }
}

void ufraw_flip_orientation(ufraw_data *uf, int flip)
{
    const char flipMatrix[8][8] = {
        { 0, 1, 2, 3, 4, 5, 6, 7 }, /* No flip */
        { 1, 0, 3, 2, 5, 4, 7, 6 }, /* Flip horizontal */
        { 2, 3, 0, 1, 6, 7, 4, 5 }, /* Flip vertical */
        { 3, 2, 1, 0, 7, 6, 5, 4 }, /* Rotate 180 */
        { 4, 6, 5, 7, 0, 2, 1, 3 }, /* Flip over diagonal "\" */
        { 5, 7, 4, 6, 1, 3, 0, 2 }, /* Rotate 270 */
        { 6, 4, 7, 5, 2, 0, 3, 1 }, /* Rotate 90 */
        { 7, 5, 6, 4, 3, 1, 2, 0 }  /* Flip over diagonal "/" */
    };
    uf->conf->orientation = flipMatrix[uf->conf->orientation][flip];
}

/*
 * Normalize arbitrary rotations into a 0..90 degree range.
 */
void ufraw_normalize_rotation(ufraw_data *uf)
{
    int angle, flip = 0;

    uf->conf->rotationAngle = fmod(uf->conf->rotationAngle, 360.0);
    if (uf->conf->rotationAngle < 0.0)
        uf->conf->rotationAngle += 360.0;
    angle = floor(uf->conf->rotationAngle / 90) * 90;
    switch (angle) {
        case  90:
            flip = 6;
            break;
        case 180:
            flip = 3;
            break;
        case 270:
            flip = 5;
            break;
    }
    ufraw_flip_orientation(uf, flip);
    uf->conf->rotationAngle -= angle;
}

/*
 * Unnormalize a normalized rotaion into a -180..180 degree range,
 * while orientation can be either 0 (normal) or 1 (flipped).
 * All image processing code assumes normalized rotation, therefore
 * each call to ufraw_unnormalize_rotation() must be followed by a call
 * to ufraw_normalize_rotation().
 */
void ufraw_unnormalize_rotation(ufraw_data *uf)
{
    switch (uf->conf->orientation) {
        case 5: /* Rotate 270 */
            uf->conf->rotationAngle += 90;
        case 3: /* Rotate 180 */
            uf->conf->rotationAngle += 90;
        case 6: /* Rotate 90 */
            uf->conf->rotationAngle += 90;
            uf->conf->orientation = 0;
        case 0: /* No flip */
            break;
        case 4: /* Flip over diagonal "\" */
            uf->conf->rotationAngle += 90;
        case 2: /* Flip vertical */
            uf->conf->rotationAngle += 90;
        case 7: /* Flip over diagonal "/" */
            uf->conf->rotationAngle += 90;
            uf->conf->orientation = 1;
        case 1: /* Flip horizontal */
            break;
        default:
            g_error("ufraw_unnormalized_roation(): orientation=%d out of range",
                    uf->conf->orientation);
    }
    uf->conf->rotationAngle = remainder(uf->conf->rotationAngle, 360.0);
}

void ufraw_flip_image(ufraw_data *uf, int flip)
{
    if (flip == 0)
        return;
    ufraw_flip_orientation(uf, flip);
    /* Usually orientation is applied before rotationAngle.
     * Here we are flipping after rotationAngle was applied.
     * We need to correct rotationAngle for this since these
     * operations do no commute.
     */
    if (flip == 1 || flip == 2 || flip == 4 || flip == 7) {
        uf->conf->rotationAngle = -uf->conf->rotationAngle;
        ufraw_normalize_rotation(uf);
    }
    UFRawPhase phase;
    for (phase = ufraw_first_phase; phase < ufraw_phases_num; phase++)
        ufraw_flip_image_buffer(&uf->Images[phase], flip);
}

void ufraw_invalidate_layer(ufraw_data *uf, UFRawPhase phase)
{
    for (; phase < ufraw_phases_num; phase++) {
        uf->Images[phase].valid = 0;
        uf->Images[phase].invalidate_event = TRUE;
    }
}

void ufraw_invalidate_tca_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_raw_phase);
}

void ufraw_invalidate_hotpixel_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_raw_phase);
}

void ufraw_invalidate_denoise_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_raw_phase);
}

void ufraw_invalidate_darkframe_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_raw_phase);
}

void ufraw_invalidate_despeckle_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_raw_phase);
}

/*
 * This one is special. The raw layer applies WB in preparation for optimal
 * interpolation but the first layer undoes it for develop() et.al. So, the
 * first layer stays valid but all the others must be invalidated upon WB
 * adjustments.
 */
void ufraw_invalidate_whitebalance_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_develop_phase);
    uf->Images[ufraw_raw_phase].valid = 0;
    uf->Images[ufraw_raw_phase].invalidate_event = TRUE;

    /* Despeckling is sensitive for WB changes because it is nonlinear. */
    if (ufraw_despeckle_active(uf))
        ufraw_invalidate_despeckle_layer(uf);
}

/*
 * This should be a no-op in case we don't interpolate but we don't care: the
 * delay will at least give the illusion that it matters. Color smoothing
 * implementation is a bit too simplistic.
 */
void ufraw_invalidate_smoothing_layer(ufraw_data *uf)
{
    ufraw_invalidate_layer(uf, ufraw_first_phase);
}

int ufraw_set_wb(ufraw_data *uf, gboolean interactive)
{
    dcraw_data *raw = uf->raw;
    double rgbWB[3];
    int c, cc, i;
    UFObject *temperature = ufgroup_element(uf->conf->ufobject, ufTemperature);
    UFObject *green = ufgroup_element(uf->conf->ufobject, ufGreen);
    UFObject *chanMul = ufgroup_element(uf->conf->ufobject,
                                        ufChannelMultipliers);
    UFObject *wb = ufgroup_element(uf->conf->ufobject, ufWB);
    UFObject *wbTuning = ufgroup_element(uf->conf->ufobject, ufWBFineTuning);

    ufraw_invalidate_whitebalance_layer(uf);

    /* For uf_manual_wb we calculate chanMul from the temperature/green. */
    /* For all other it is the other way around. */
    if (ufarray_is_equal(wb, uf_manual_wb)) {
        if (interactive) {
            double chanMulArray[4] = {1, 1, 1, 1 };
            Temperature_to_RGB(ufnumber_value(temperature), rgbWB);
            rgbWB[1] = rgbWB[1] / ufnumber_value(green);
            /* Suppose we shot a white card at some temperature:
             * rgbWB[3] = rgb_cam[3][4] * preMul[4] * camWhite[4]
             * Now we want to make it white (1,1,1), so we replace preMul
             * with chanMul, which is defined as:
             * chanMul[4][4] = cam_rgb[4][3] * (1/rgbWB[3][3]) * rgb_cam[3][4]
             *          * preMul[4][4]
             * We "upgraded" preMul, chanMul and rgbWB to diagonal matrices.
             * This allows for the manipulation:
             * (1/chanMul)[4][4] = (1/preMul)[4][4] * cam_rgb[4][3] * rgbWB[3][3]
             *          * rgb_cam[3][4]
             * We use the fact that rgb_cam[3][4] * (1,1,1,1) = (1,1,1) and get:
             * (1/chanMul)[4] = (1/preMul)[4][4] * cam_rgb[4][3] * rgbWB[3]
             */
            if (uf->raw_color) {
                /* If there is no color matrix it is simple */
                if (uf->colors > 1)
                    for (c = 0; c < 3; c++)
                        chanMulArray[c] = raw->pre_mul[c] / rgbWB[c];
                ufnumber_array_set(chanMul, chanMulArray);
            } else {
                for (c = 0; c < uf->colors; c++) {
                    double chanMulInv = 0;
                    for (cc = 0; cc < 3; cc++)
                        chanMulInv += 1 / raw->pre_mul[c] * raw->cam_rgb[c][cc]
                                      * rgbWB[cc];
                    chanMulArray[c] = 1 / chanMulInv;
                }
                ufnumber_array_set(chanMul, chanMulArray);
            }
        }
        ufnumber_set(wbTuning, 0);
        return UFRAW_SUCCESS;
    }
    if (ufarray_is_equal(wb, uf_spot_wb)) {
        /* do nothing */
        ufnumber_set(wbTuning, 0);
    } else if (ufarray_is_equal(wb, uf_auto_wb)) {
        int p;
        /* Build a raw channel histogram */
        ufraw_image_type *histogram = g_new0(ufraw_image_type, uf->rgbMax + 1);
        for (i = 0; i < raw->raw.height * raw->raw.width; i++) {
            gboolean countPixel = TRUE;
            /* The -25 bound was copied from dcraw */
            for (c = 0; c < raw->raw.colors; c++)
                if (raw->raw.image[i][c] > uf->rgbMax + raw->black - 25)
                    countPixel = FALSE;
            if (countPixel) {
                for (c = 0; c < raw->raw.colors; c++) {
                    p = MIN(MAX(raw->raw.image[i][c] - raw->black, 0), uf->rgbMax);
                    histogram[p][c]++;
                }
            }
        }
        double chanMulArray[4] = {1.0, 1.0, 1.0, 1.0 };
        double min = 1.0;
        for (c = 0; c < uf->colors; c++) {
            gint64 sum = 0;
            for (i = 0; i < uf->rgbMax + 1; i++)
                sum += (gint64)i * histogram[i][c];
            if (sum == 0) chanMulArray[c] = 1.0;
            else chanMulArray[c] = 1.0 / sum;
            if (chanMulArray[c] < min)
                min = chanMulArray[c];
        }
        for (c = 0; c < uf->colors; c++)
            chanMulArray[c] /= min;
        g_free(histogram);
        ufnumber_array_set(chanMul, chanMulArray);
        ufnumber_set(wbTuning, 0);
    } else if (ufarray_is_equal(wb, uf_camera_wb)) {
        double chanMulArray[4] = { 1.0, 1.0, 1.0, 1.0 };
        for (c = 0; c < uf->colors; c++)
            chanMulArray[c] = raw->post_mul[c];
        ufnumber_array_set(chanMul, chanMulArray);
        ufnumber_set(wbTuning, 0);
    } else {
        int lastTuning = -1;
        char model[max_name];
        if (strcasecmp(uf->conf->make, "Minolta") == 0 &&
                (strncmp(uf->conf->model, "ALPHA", 5) == 0 ||
                 strncmp(uf->conf->model, "MAXXUM", 6) == 0)) {
            /* Canonize Minolta model names (copied from dcraw) */
            g_snprintf(model, max_name, "DYNAX %s",
                       uf->conf->model + 6 + (uf->conf->model[0] == 'M'));
        } else {
            g_strlcpy(model, uf->conf->model, max_name);
        }
        for (i = 0; i < wb_preset_count; i++) {
            if (ufarray_is_equal(wb, wb_preset[i].name) &&
                    !strcasecmp(uf->conf->make, wb_preset[i].make) &&
                    !strcasecmp(model, wb_preset[i].model)) {
                if (ufnumber_value(wbTuning) == wb_preset[i].tuning) {
                    double chanMulArray[4] = {1, 1, 1, 1 };
                    for (c = 0; c < uf->colors; c++)
                        chanMulArray[c] = wb_preset[i].channel[c];
                    ufnumber_array_set(chanMul, chanMulArray);
                    break;
                } else if (ufnumber_value(wbTuning) < wb_preset[i].tuning) {
                    if (lastTuning == -1) {
                        /* wbTuning was set to a value smaller than possible */
                        ufnumber_set(wbTuning, wb_preset[i].tuning);
                        double chanMulArray[4] = {1, 1, 1, 1 };
                        for (c = 0; c < uf->colors; c++)
                            chanMulArray[c] = wb_preset[i].channel[c];
                        ufnumber_array_set(chanMul, chanMulArray);
                        break;
                    } else {
                        /* Extrapolate WB tuning values:
                         * f(x) = f(a) + (x-a)*(f(b)-f(a))/(b-a) */
                        double chanMulArray[4] = {1, 1, 1, 1 };
                        for (c = 0; c < uf->colors; c++)
                            chanMulArray[c] = wb_preset[i].channel[c] +
                                              (ufnumber_value(wbTuning) - wb_preset[i].tuning) *
                                              (wb_preset[lastTuning].channel[c] -
                                               wb_preset[i].channel[c]) /
                                              (wb_preset[lastTuning].tuning -
                                               wb_preset[i].tuning);
                        ufnumber_array_set(chanMul, chanMulArray);
                        break;
                    }
                } else if (ufnumber_value(wbTuning) > wb_preset[i].tuning) {
                    lastTuning = i;
                }
            } else if (lastTuning != -1) {
                /* wbTuning was set to a value larger than possible */
                ufnumber_set(wbTuning, wb_preset[lastTuning].tuning);
                double chanMulArray[4] = {1, 1, 1, 1 };
                for (c = 0; c < uf->colors; c++)
                    chanMulArray[c] = wb_preset[lastTuning].channel[c];
                ufnumber_array_set(chanMul, chanMulArray);
                break;
            }
        }
        if (i == wb_preset_count) {
            if (lastTuning != -1) {
                /* wbTuning was set to a value larger than possible */
                ufnumber_set(wbTuning, wb_preset[lastTuning].tuning);
                ufnumber_array_set(chanMul, wb_preset[lastTuning].channel);
            } else {
                ufobject_set_string(wb, uf_manual_wb);
                ufraw_set_wb(uf, interactive);
                return UFRAW_WARNING;
            }
        }
    }
    /* (1/chanMul)[4] = (1/preMul)[4][4] * cam_rgb[4][3] * rgbWB[3]
     * Therefore:
     * rgbWB[3] = rgb_cam[3][4] * preMul[4][4] * (1/chanMul)[4]
     */
    if (uf->raw_color) {
        /* If there is no color matrix it is simple */
        for (c = 0; c < 3; c++) {
            rgbWB[c] = raw->pre_mul[c] / ufnumber_array_value(chanMul, c);
        }
    } else {
        for (c = 0; c < 3; c++) {
            rgbWB[c] = 0;
            for (cc = 0; cc < uf->colors; cc++)
                rgbWB[c] += raw->rgb_cam[c][cc] * raw->pre_mul[cc]
                            / ufnumber_array_value(chanMul, cc);
        }
    }
    /* From these values we calculate temperature, green values */
    double temperatureValue, greenValue;
    RGB_to_Temperature(rgbWB, &temperatureValue, &greenValue);
    ufnumber_set(temperature, temperatureValue);
    ufnumber_set(green, greenValue);
    return UFRAW_SUCCESS;
}

static void ufraw_build_raw_histogram(ufraw_data *uf)
{
    int i, c;
    dcraw_data *raw = uf->raw;
    gboolean updateHistogram = FALSE;

    if (uf->RawHistogram == NULL) {
        uf->RawHistogram = g_new(int, uf->rgbMax + 1);
        updateHistogram = TRUE;
    }
    double maxChan = 0;
    UFObject *chanMul = ufgroup_element(uf->conf->ufobject,
                                        ufChannelMultipliers);
    for (c = 0; c < uf->colors; c++)
        maxChan = MAX(ufnumber_array_value(chanMul, c), maxChan);
    for (c = 0; c < uf->colors; c++) {
        int tmp = floor(ufnumber_array_value(chanMul, c) / maxChan * 0x10000);
        if (uf->RawChanMul[c] != tmp) {
            updateHistogram = TRUE;
            uf->RawChanMul[c] = tmp;
        }
    }
    if (!updateHistogram) return;

    if (uf->colors == 3) uf->RawChanMul[3] = uf->RawChanMul[1];
    memset(uf->RawHistogram, 0, (uf->rgbMax + 1)*sizeof(int));
    int count = raw->raw.height * raw->raw.width;
    for (i = 0; i < count; i++)
        for (c = 0; c < raw->raw.colors; c++)
            uf->RawHistogram[MIN(
                                 (gint64)MAX(raw->raw.image[i][c] - raw->black, 0) *
                                 uf->RawChanMul[c] / 0x10000, uf->rgbMax)]++;

    uf->RawCount = count * raw->raw.colors;
}

void ufraw_auto_expose(ufraw_data *uf)
{
    int sum, stop, wp, c, pMax, pMin, p;
    ufraw_image_type pix;
    guint16 p16[3];

    if (uf->conf->autoExposure != apply_state) return;

    /* Reset the exposure and luminosityCurve */
    uf->conf->exposure = 0;
    /* If we normalize the exposure then 0 EV also gets normalized */
    if (uf->conf->ExposureNorm > 0)
        uf->conf->exposure = -log(1.0 * uf->rgbMax / uf->conf->ExposureNorm) / log(2);
    ufraw_developer_prepare(uf, auto_developer);
    /* Find the grey value that gives 99% luminosity */
    double maxChan = 0;
    UFObject *chanMul = ufgroup_element(uf->conf->ufobject,
                                        ufChannelMultipliers);
    for (c = 0; c < uf->colors; c++)
        maxChan = MAX(ufnumber_array_value(chanMul, c), maxChan);
    for (pMax = uf->rgbMax, pMin = 0, p = (pMax + pMin) / 2; pMin < pMax - 1; p = (pMax + pMin) / 2) {
        for (c = 0; c < uf->colors; c++)
            pix[c] = MIN(p * maxChan / ufnumber_array_value(chanMul, c),
                         uf->rgbMax);
        develop(p16, pix, uf->AutoDeveloper, 16, 1);
        for (c = 0, wp = 0; c < 3; c++) wp = MAX(wp, p16[c]);
        if (wp < 0x10000 * 99 / 100) pMin = p;
        else pMax = p;
    }
    /* set cutoff at 99% of the histogram */
    ufraw_build_raw_histogram(uf);
    stop = uf->RawCount * 1 / 100;
    /* Calculate the white point */
    for (wp = uf->rgbMax, sum = 0; wp > 1 && sum < stop; wp--)
        sum += uf->RawHistogram[wp];
    /* Set 99% of the luminosity values with luminosity below 99% */
    uf->conf->exposure = log((double)p / wp) / log(2);
    /* If we are going to normalize the exposure later,
     * we need to cancel its effect here. */
    if (uf->conf->ExposureNorm > 0)
        uf->conf->exposure -=
            log(1.0 * uf->rgbMax / uf->conf->ExposureNorm) / log(2);
    uf->conf->autoExposure = enabled_state;
//    ufraw_message(UFRAW_SET_LOG, "ufraw_auto_expose: "
//	    "Exposure %f (white point %d/%d)\n", uf->conf->exposure, wp, p);
}

void ufraw_auto_black(ufraw_data *uf)
{
    int sum, stop, bp, c;
    ufraw_image_type pix;
    guint16 p16[3];

    if (uf->conf->autoBlack == disabled_state) return;

    /* Reset the luminosityCurve */
    ufraw_developer_prepare(uf, auto_developer);
    /* Calculate the black point */
    ufraw_build_raw_histogram(uf);
    stop = uf->RawCount / 256 / 4;
    for (bp = 0, sum = 0; bp < uf->rgbMax && sum < stop; bp++)
        sum += uf->RawHistogram[bp];
    double maxChan = 0;
    UFObject *chanMul = ufgroup_element(uf->conf->ufobject,
                                        ufChannelMultipliers);
    for (c = 0; c < uf->colors; c++)
        maxChan = MAX(ufnumber_array_value(chanMul, c), maxChan);
    for (c = 0; c < uf->colors; c++)
        pix[c] = MIN(bp * maxChan / ufnumber_array_value(chanMul, c), uf->rgbMax);
    develop(p16, pix, uf->AutoDeveloper, 16, 1);
    for (c = 0, bp = 0; c < 3; c++) bp = MAX(bp, p16[c]);

    CurveDataSetPoint(&uf->conf->curve[uf->conf->curveIndex],
                      0, (double)bp / 0x10000, 0);

    uf->conf->autoBlack = enabled_state;
//    ufraw_message(UFRAW_SET_LOG, "ufraw_auto_black: "
//	    "Black %f (black point %d)\n",
//	    uf->conf->curve[uf->conf->curveIndex].m_anchors[0].x, bp);
}

/* ufraw_auto_curve sets the black-point and then distribute the (step-1)
 * parts of the histogram with the weights: w_i = pow(decay,i). */
void ufraw_auto_curve(ufraw_data *uf)
{
    int sum, stop, steps = 8, bp, p, i, j, c;
    ufraw_image_type pix;
    guint16 p16[3];
    CurveData *curve = &uf->conf->curve[uf->conf->curveIndex];
    double decay = 0.90;
    double norm = (1 - pow(decay, steps)) / (1 - decay);

    CurveDataReset(curve);
    ufraw_developer_prepare(uf, auto_developer);
    /* Calculate curve points */
    ufraw_build_raw_histogram(uf);
    stop = uf->RawCount / 256 / 4;
    double maxChan = 0;
    UFObject *chanMul = ufgroup_element(uf->conf->ufobject,
                                        ufChannelMultipliers);
    for (c = 0; c < uf->colors; c++)
        maxChan = MAX(ufnumber_array_value(chanMul, c), maxChan);
    for (bp = 0, sum = 0, p = 0, i = j = 0; i < steps && bp < uf->rgbMax && p < 0xFFFF; i++) {
        for (; bp < uf->rgbMax && sum < stop; bp++)
            sum += uf->RawHistogram[bp];
        for (c = 0; c < uf->colors; c++)
            pix[c] = MIN(bp * maxChan / ufnumber_array_value(chanMul, c),
                         uf->rgbMax);
        develop(p16, pix, uf->AutoDeveloper, 16, 1);
        for (c = 0, p = 0; c < 3; c++) p = MAX(p, p16[c]);
        stop += uf->RawCount * pow(decay, i) / norm;
        /* Skip adding point if slope is too big (more than 4) */
        if (j > 0 && p - curve->m_anchors[j - 1].x * 0x10000 < (i + 1 - j) * 0x04000 / steps)
            continue;
        curve->m_anchors[j].x = (double)p / 0x10000;
        curve->m_anchors[j].y = (double)i / steps;
        j++;
    }
    if (bp == 0x10000) {
        curve->m_numAnchors = j;
    } else {
        curve->m_anchors[j].x = 1.0;
        /* The last point can be up to twice the height of a linear
         * interpolation of the last two points */
        if (j > 1) {
            curve->m_anchors[j].y = curve->m_anchors[j - 1].y +
                                    2 * (1.0 - curve->m_anchors[j - 1].x) *
                                    (curve->m_anchors[j - 1].y - curve->m_anchors[j - 2].y) /
                                    (curve->m_anchors[j - 1].x - curve->m_anchors[j - 2].x);
            if (curve->m_anchors[j].y > 1.0) curve->m_anchors[j].y = 1.0;
        } else {
            curve->m_anchors[j].y = 1.0;
        }
        curve->m_numAnchors = j + 1;
    }
}

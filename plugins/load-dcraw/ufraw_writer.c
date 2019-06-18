/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_writer.c - functions to output image files in different formats.
 * Copyright 2004-2016 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufraw.h"
#include <glib/gi18n.h>
#include <errno.h>	/* for errno */
#include <string.h>
#include <lcms2.h>
#include "ufraw_colorspaces.h"
#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif
#ifdef HAVE_LIBJPEG
#include <jerror.h>
#include "iccjpeg.h"
#endif
#ifdef HAVE_LIBPNG
#include <png.h>
#ifdef HAVE_LIBZ
#include <zlib.h>	/* for libpng 1.5.x */
#endif
#if PNG_LIBPNG_VER_MAJOR == 1 && (PNG_LIBPNG_VER_MINOR < 5 || \
    (PNG_LIBPNG_VER_MINOR == 5 && PNG_LIBPNG_VER_RELEASE < 1))
#define png_const_bytep png_charp
#endif
#endif

#ifdef _OPENMP
#include <omp.h>
#define uf_omp_get_thread_num() omp_get_thread_num()
#else
#define uf_omp_get_thread_num() 0
#endif

#ifdef HAVE_LIBCFITSIO
#include <fitsio.h>
#endif

#define DEVELOP_BATCH 64

static void grayscale_buffer(void *graybuf, int width, int bitDepth)
{
    int i;
    if (bitDepth > 8) {
        guint16 *pixbuf16 = graybuf;
        guint16 *graybuf16 = graybuf;
        for (i = 0; i < width; ++i, ++graybuf16, pixbuf16 += 3)
            * graybuf16 = pixbuf16[1];
    } else {
        guint8 *pixbuf8 = graybuf;
        guint8 *graybuf8 = graybuf;
        for (i = 0; i < width; ++i, ++graybuf8, pixbuf8 += 3)
            * graybuf8 = pixbuf8[1];
    }
}

static int ppm_row_writer(ufraw_data *uf, void *volatile out, void *pixbuf,
                          int row, int width, int height, int grayscale, int bitDepth)
{
    (void)row;
    int rowStride = width * (grayscale ? 1 : 3) * (bitDepth > 8 ? 2 : 1);
    int i;
    if (bitDepth > 8) {
        guint16 *pixbuf16 = (guint16 *)pixbuf;
        for (i = 0; i < 3 * width * height; i++)
            pixbuf16[i] = g_htons(pixbuf16[i]);
    }
    for (i = 0; i < height; i++) {
        if ((int)fwrite(pixbuf + i * width * (bitDepth > 8 ? 6 : 3), rowStride, 1, out) < 1) {
            ufraw_set_error(uf, _("Error creating file '%s'."),
                            uf->conf->outputFilename);
            ufraw_set_error(uf, g_strerror(errno));
            return UFRAW_ERROR;
        }
    }
    return UFRAW_SUCCESS;
}

#ifdef HAVE_LIBTIFF
// There seem to be no way to get the libtiff message without a static variable
// Therefore the folloing code is not thread-safe.
static char ufraw_tiff_message[max_path];

static void tiff_messenger(const char *module, const char *fmt, va_list ap)
{
    (void)module;
    vsnprintf(ufraw_tiff_message, max_path, fmt, ap);
}

int tiff_row_writer(ufraw_data *uf, void *volatile out, void *pixbuf,
                    int row, int width, int height, int grayscale, int bitDepth)
{
    (void)grayscale;
    int rowStride = width * (bitDepth > 8 ? 6 : 3);
    int i;
    for (i = 0; i < height; i++) {
        if (TIFFWriteScanline(out, pixbuf + i * rowStride, row + i, 0) < 0) {
            // 'errno' does seem to contain useful information
            ufraw_set_error(uf, _("Error creating file."));
            ufraw_set_error(uf, ufraw_tiff_message);
            ufraw_tiff_message[0] = '\0';
            return UFRAW_ERROR;
        }
    }
    return UFRAW_SUCCESS;
}
#endif /*HAVE_LIBTIFF*/

#ifdef HAVE_LIBJPEG
static void jpeg_warning_handler(j_common_ptr cinfo)
{
    ufraw_data *uf = cinfo->client_data;
    ufraw_set_warning(uf,
                      cinfo->err->jpeg_message_table[cinfo->err->msg_code],
                      cinfo->err->msg_parm.i[0],
                      cinfo->err->msg_parm.i[1],
                      cinfo->err->msg_parm.i[2],
                      cinfo->err->msg_parm.i[3]);
}

static void jpeg_error_handler(j_common_ptr cinfo)
{
    /* We ignore the SOI error if second byte is 0xd8 since Minolta's
     * SOI is known to be wrong */
    ufraw_data *uf = cinfo->client_data;
    if (cinfo->err->msg_code == JERR_NO_SOI &&
            cinfo->err->msg_parm.i[1] == 0xd8) {
        ufraw_set_info(uf,
                       cinfo->err->jpeg_message_table[cinfo->err->msg_code],
                       cinfo->err->msg_parm.i[0],
                       cinfo->err->msg_parm.i[1],
                       cinfo->err->msg_parm.i[2],
                       cinfo->err->msg_parm.i[3]);
        return;
    }
    ufraw_set_error(uf,
                    cinfo->err->jpeg_message_table[cinfo->err->msg_code],
                    cinfo->err->msg_parm.i[0],
                    cinfo->err->msg_parm.i[1],
                    cinfo->err->msg_parm.i[2],
                    cinfo->err->msg_parm.i[3]);
}

static int jpeg_row_writer(ufraw_data *uf, void *volatile out, void *pixbuf,
                           int row, int width, int height, int grayscale, int bitDepth)
{
    (void)row;
    (void)grayscale;
    (void)bitDepth;
    int i;
    for (i = 0; i < height; i++) {
        guint8 *pixbuf8 = pixbuf + 3 * width * i;
        jpeg_write_scanlines((struct jpeg_compress_struct *)out, &pixbuf8, 1);
        if (ufraw_is_error(uf))
            return UFRAW_ERROR;
    }
    return UFRAW_SUCCESS;
}
#endif /*HAVE_LIBJPEG*/

#ifdef HAVE_LIBPNG
static void png_error_handler(png_structp png,
                              png_const_charp error_msg)
{
    ufraw_data *uf = png_get_error_ptr(png);
    ufraw_set_error(uf, "%s: %s.", error_msg, g_strerror(errno));
    longjmp(png_jmpbuf(png), 1);
}

static void png_warning_handler(png_structp png,
                                png_const_charp warning_msg)
{
    ufraw_data *uf = png_get_error_ptr(png);
    ufraw_set_warning(uf, "%s.", warning_msg);
}

static void PNGwriteRawProfile(png_struct *ping,
                               png_info *ping_info, char *profile_type, guint8 *profile_data,
                               png_uint_32 length);

int png_row_writer(ufraw_data *uf, void *volatile out, void *pixbuf,
                   int row, int width, int height, int grayscale, int bitDepth)
{
    (void)uf;
    (void)row;
    (void)grayscale;
    int rowStride = width * (bitDepth > 8 ? 6 : 3);

    int i;
    for (i = 0; i < height; i++)
        png_write_row(out, (guint8 *)pixbuf + rowStride * i);

    return UFRAW_SUCCESS;
}
#endif /*HAVE_LIBPNG*/

#if defined(HAVE_LIBCFITSIO) && defined(_WIN32)
/* localtime_r() is not included in the _WIN32 API. */
static struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    struct tm *p = localtime(timep);
    memset(result, 0, sizeof(*result));
    if (p) {
        *result = *p;
        p = result;
    }
    return p;
}
#endif /*HAVE_LIBCFITSIO && _WIN32*/

void ufraw_write_image_data(
    ufraw_data *uf, void * volatile out,
    const UFRectangle *Crop, int bitDepth, int grayscaleMode,
    int (*row_writer)(ufraw_data *, void * volatile, void *, int, int, int, int, int))
{
    int row, row0;
    int rowStride = uf->Images[ufraw_first_phase].width;
    ufraw_image_type *rawImage =
        (ufraw_image_type *)uf->Images[ufraw_first_phase].buffer;
    int byteDepth = (bitDepth + 7) / 8;
    guint8 *pixbuf8 = g_new(guint8,
                            Crop->width * 3 * byteDepth * DEVELOP_BATCH);

    progress(PROGRESS_SAVE, -Crop->height);
    for (row0 = 0; row0 < Crop->height; row0 += DEVELOP_BATCH) {
        progress(PROGRESS_SAVE, DEVELOP_BATCH);
#ifdef _OPENMP
        #pragma omp parallel for default(shared) private(row)
#endif
        for (row = 0; row < DEVELOP_BATCH; row++) {
            if (row + row0 >= Crop->height)
                continue;
            guint8 *rowbuf = &pixbuf8[row * Crop->width * 3 * byteDepth];
            develop(rowbuf, rawImage[(Crop->y + row + row0)*rowStride + Crop->x],
                    uf->developer, bitDepth, Crop->width);
            if (grayscaleMode)
                grayscale_buffer(rowbuf, Crop->width, bitDepth);
        }
        int batchHeight = MIN(Crop->height - row0, DEVELOP_BATCH);
        if (row_writer(uf, out, pixbuf8, row0, Crop->width, batchHeight,
                       grayscaleMode, bitDepth) != UFRAW_SUCCESS)
            break;
    }
    g_free(pixbuf8);
}

int ufraw_write_image(ufraw_data *uf)
{
    /* 'volatile' supresses clobbering warning */
    void * volatile out; /* out is a pointer to FILE or TIFF */
#ifdef HAVE_LIBCFITSIO
    fitsfile *fitsFile;
#endif
    char * volatile confFilename = NULL;
    int volatile grayscaleMode = uf->conf->grayscaleMode != grayscale_none ||
                                 uf->colors == 1;
    ufraw_message_reset(uf);

    if (uf->conf->createID == only_id ||
            uf->conf->createID == also_id) {
        confFilename = uf_file_set_type(uf->conf->outputFilename, ".ufraw");
        if (!strcmp(confFilename, uf->conf->outputFilename)) {
            ufraw_set_error(uf, _("Image filename can not be the "
                                  "same as ID filename '%s'"), confFilename);
            g_free(confFilename);
            return ufraw_get_status(uf);
        }
    }
    if (uf->conf->createID == only_id) {
        if (uf->conf->autoCrop && !uf->LoadingID) {
            ufraw_get_image_dimensions(uf);
            uf->conf->CropX1 = (uf->rotatedWidth - uf->autoCropWidth) / 2;
            uf->conf->CropX2 = uf->conf->CropX1 + uf->autoCropWidth;
            uf->conf->CropY1 = (uf->rotatedHeight - uf->autoCropHeight) / 2;
            uf->conf->CropY2 = uf->conf->CropY1 + uf->autoCropHeight;
        }
        int status = conf_save(uf->conf, confFilename, NULL);
        g_free(confFilename);
        return status;
    }
#ifdef HAVE_LIBTIFF
    if (uf->conf->type == tiff_type) {
        TIFFSetErrorHandler(tiff_messenger);
        TIFFSetWarningHandler(tiff_messenger);
        ufraw_tiff_message[0] = '\0';
        if (!strcmp(uf->conf->outputFilename, "-")) {
            out = TIFFFdOpen(fileno((FILE *)stdout),
                             uf->conf->outputFilename, "w");
        } else {
            char *filename =
                uf_win32_locale_filename_from_utf8(uf->conf->outputFilename);
            out = TIFFOpen(filename, "w");
            uf_win32_locale_filename_free(filename);
        }
        if (out == NULL) {
            ufraw_set_error(uf, _("Error creating file."));
            ufraw_set_error(uf, ufraw_tiff_message);
            ufraw_set_error(uf, g_strerror(errno));
            ufraw_tiff_message[0] = '\0';
            return ufraw_get_status(uf);
        }
    } else
#endif
#ifdef HAVE_LIBCFITSIO
        if (uf->conf->type == fits_type) {
            if (strcmp(uf->conf->outputFilename, "-") != 0) {
                if (g_file_test(uf->conf->outputFilename, G_FILE_TEST_EXISTS)) {
                    if (g_unlink(uf->conf->outputFilename)) {
                        ufraw_set_error(uf, _("Error creating file '%s'."),
                                        uf->conf->outputFilename);
                        ufraw_set_error(uf, g_strerror(errno));
                        return ufraw_get_status(uf);
                    }
                }
            }
            int status = 0;
            char *filename =
                uf_win32_locale_filename_from_utf8(uf->conf->outputFilename);
            if (strcmp(filename, "-") != 0)
                // Use fits_create_diskfile() to allow more characters in
                // filenames.
                fits_create_diskfile(&fitsFile, filename, &status);
            else
                // fits_create_file() can write to stdout.
                fits_create_file(&fitsFile, filename, &status);
            uf_win32_locale_filename_free(filename);
            if (status) {
                ufraw_set_error(uf, _("Error creating file '%s'."),
                                uf->conf->outputFilename);
                char errBuffer[max_name];
                fits_get_errstatus(status, errBuffer);
                ufraw_set_error(uf, errBuffer);
                while (fits_read_errmsg(errBuffer))
                    ufraw_set_error(uf, errBuffer);
                return ufraw_get_status(uf);
            }
        } else
#endif
        {
            if (!strcmp(uf->conf->outputFilename, "-")) {
                out = stdout;
            } else {
                if ((out = g_fopen(uf->conf->outputFilename, "wb")) == NULL) {
                    ufraw_set_error(uf, _("Error creating file '%s'."),
                                    uf->conf->outputFilename);
                    ufraw_set_error(uf, g_strerror(errno));
                    return ufraw_get_status(uf);
                }
            }
        }
    // TODO: error handling
    ufraw_convert_image(uf);
    UFRectangle Crop;
    ufraw_get_scaled_crop(uf, &Crop);
    volatile int BitDepth = uf->conf->profile[out_profile]
                            [uf->conf->profileIndex[out_profile]].BitDepth;
    if (BitDepth != 16) BitDepth = 8;
    if (uf->conf->type == ppm_type && BitDepth == 8) {
        fprintf(out, "P%c\n%d %d\n%d\n",
                grayscaleMode ? '5' : '6', Crop.width, Crop.height, 0xFF);
        ufraw_write_image_data(uf, out, &Crop, BitDepth, grayscaleMode,
                               ppm_row_writer);
    } else if (uf->conf->type == ppm_type && BitDepth == 16) {
        fprintf(out, "P%c\n%d %d\n%d\n",
                grayscaleMode ? '5' : '6', Crop.width, Crop.height, 0xFFFF);
        ufraw_write_image_data(uf, out, &Crop, BitDepth, grayscaleMode,
                               ppm_row_writer);
#ifdef HAVE_LIBTIFF
    } else if (uf->conf->type == tiff_type) {
        TIFFSetField(out, TIFFTAG_IMAGEWIDTH, Crop.width);
        TIFFSetField(out, TIFFTAG_IMAGELENGTH, Crop.height);
        TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, grayscaleMode ? 1 : 3);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, BitDepth);
        TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, grayscaleMode
                     ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
#ifdef HAVE_LIBZ
        if (uf->conf->losslessCompress) {
            TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE);
            TIFFSetField(out, TIFFTAG_ZIPQUALITY, 9);
            TIFFSetField(out, TIFFTAG_PREDICTOR, 2);
        } else
#endif
            TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        /* Embed output profile if it is not the internal sRGB. */
        if (strcmp(uf->developer->profileFile[out_profile], "")) {
            char *buf;
            gsize len;
            if (g_file_get_contents(uf->developer->profileFile[out_profile],
                                    &buf, &len, NULL)) {
                TIFFSetField(out, TIFFTAG_ICCPROFILE, len, buf);
                g_free(buf);
            } else {
                ufraw_set_warning(uf,
                                  _("Failed to embed output profile '%s' in '%s'."),
                                  uf->developer->profileFile[out_profile],
                                  uf->conf->outputFilename);
            }
        } else if (uf->conf->profileIndex[out_profile] == 1) { // Embed sRGB.
            cmsHPROFILE hOutProfile = uf_colorspaces_create_srgb_profile();
            cmsUInt32Number len = 0;
            cmsSaveProfileToMem(hOutProfile, 0, &len); // Calculate len.
            if (len > 0) {
                unsigned char buf[len];
                cmsSaveProfileToMem(hOutProfile, buf, &len);
                TIFFSetField(out, TIFFTAG_ICCPROFILE, len, buf);
            } else {
                ufraw_set_warning(uf,
                                  _("Failed to embed output profile '%s' in '%s'."),
                                  uf->conf->profile[out_profile]
                                  [uf->conf->profileIndex[out_profile]].name,
                                  uf->conf->outputFilename);
            }
            cmsCloseProfile(hOutProfile);
        }
        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, 0));

        ufraw_write_image_data(uf, out, &Crop, BitDepth, grayscaleMode,
                               tiff_row_writer);

#endif /*HAVE_LIBTIFF*/
#ifdef HAVE_LIBJPEG
    } else if (uf->conf->type == jpeg_type) {
        if (BitDepth != 8)
            ufraw_set_warning(uf,
                              _("Unsupported bit depth '%d' ignored."), BitDepth);
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->output_message = jpeg_warning_handler;
        cinfo.err->error_exit = jpeg_error_handler;
        cinfo.client_data = uf;
        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, out);
        cinfo.image_width = Crop.width;
        cinfo.image_height = Crop.height;
        if (grayscaleMode) {
            cinfo.input_components = 1;
            cinfo.in_color_space = JCS_GRAYSCALE;
        } else {
            cinfo.input_components = 3;
            cinfo.in_color_space = JCS_RGB;
        }
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, uf->conf->compression, TRUE);
        if (uf->conf->compression > 90)
            cinfo.comp_info[0].v_samp_factor = 1;
        if (uf->conf->compression > 92)
            cinfo.comp_info[0].h_samp_factor = 1;
        if (uf->conf->progressiveJPEG)
            jpeg_simple_progression(&cinfo);

        cinfo.optimize_coding = 1;

        jpeg_start_compress(&cinfo, TRUE);

        /* Embed output profile if it is not the internal sRGB. */
        if (strcmp(uf->developer->profileFile[out_profile], "")) {
            char *buf;
            gsize len;
            if (g_file_get_contents(uf->developer->profileFile[out_profile],
                                    &buf, &len, NULL)) {
                write_icc_profile(&cinfo, (unsigned char *)buf, len);
                g_free(buf);
            } else {
                ufraw_set_warning(uf,
                                  _("Failed to embed output profile '%s' in '%s'."),
                                  uf->developer->profileFile[out_profile],
                                  uf->conf->outputFilename);
            }
        } else if (uf->conf->profileIndex[out_profile] == 1) { // Embed sRGB.
            cmsHPROFILE hOutProfile = uf_colorspaces_create_srgb_profile();
            cmsUInt32Number len = 0;
            cmsSaveProfileToMem(hOutProfile, 0, &len); // Calculate len.
            if (len > 0) {
                unsigned char buf[len];
                cmsSaveProfileToMem(hOutProfile, buf, &len);
                write_icc_profile(&cinfo, buf, len);
            } else {
                ufraw_set_warning(uf,
                                  _("Failed to embed output profile '%s' in '%s'."),
                                  uf->conf->profile[out_profile]
                                  [uf->conf->profileIndex[out_profile]].name,
                                  uf->conf->outputFilename);
            }
            cmsCloseProfile(hOutProfile);
        }
        if (uf->conf->embedExif) {
            ufraw_exif_prepare_output(uf);
            if (uf->outputExifBuf != NULL) {
                if (uf->outputExifBufLen > 65533) {
                    ufraw_set_warning(uf,
                                      _("EXIF buffer length %d, too long, ignored."),
                                      uf->outputExifBufLen);
                } else {
                    jpeg_write_marker(&cinfo, JPEG_APP0 + 1,
                                      uf->outputExifBuf, uf->outputExifBufLen);
                }
            }
        }

        ufraw_write_image_data(uf, &cinfo, &Crop, 8, grayscaleMode,
                               jpeg_row_writer);

        if (ufraw_is_error(uf)) {
            char *message = g_strdup(ufraw_get_message(uf));
            ufraw_message_reset(uf);
            ufraw_set_error(uf, _("Error creating file '%s'."),
                            uf->conf->outputFilename);
            ufraw_set_error(uf, message);
            g_free(message);
        } else
            jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
#endif /*HAVE_LIBJPEG*/
#ifdef HAVE_LIBPNG
    } else if (uf->conf->type == png_type) {
        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                          uf, png_error_handler, png_warning_handler);
        png_infop info = png_create_info_struct(png);
        if (setjmp(png_jmpbuf(png))) {
            char *message = g_strdup(ufraw_get_message(uf));
            ufraw_message_reset(uf);
            ufraw_set_error(uf, _("Error creating file '%s'."),
                            uf->conf->outputFilename);
            ufraw_set_error(uf, message);
            g_free(message);
            png_destroy_write_struct(&png, &info);
        } else {
            png_init_io(png, out);
            png_set_IHDR(png, info, Crop.width, Crop.height, BitDepth,
                         grayscaleMode ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB,
                         PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
                         PNG_FILTER_TYPE_BASE);
            png_set_compression_level(png, Z_BEST_COMPRESSION);
            png_text text[2];
            text[0].compression = PNG_TEXT_COMPRESSION_NONE;
            text[0].key = "Software";
            text[0].text = "UFRaw";
            text[1].compression = PNG_TEXT_COMPRESSION_NONE;
            text[1].key = "Source";
            text[1].text = g_strdup_printf("%s%s",
                                           uf->conf->make, uf->conf->model);
            png_set_text(png, info, text, 2);
            g_free(text[1].text);
            /* Embed output profile if it is not the internal sRGB. */
            if (strcmp(uf->developer->profileFile[out_profile], "")) {
                char *buf;
                gsize len;
                if (g_file_get_contents(uf->developer->profileFile[out_profile],
                                        &buf, &len, NULL)) {
                    png_set_iCCP(png, info,
                                 uf->developer->profileFile[out_profile],
                                 PNG_COMPRESSION_TYPE_BASE,
                                 (png_const_bytep) buf, len);
                    g_free(buf);
                } else {
                    ufraw_set_warning(uf,
                                      _("Failed to embed output profile '%s' in '%s'."),
                                      uf->developer->profileFile[out_profile],
                                      uf->conf->outputFilename);
                }
            } else if (uf->conf->profileIndex[out_profile] == 1) { // Embed sRGB.
                cmsHPROFILE hOutProfile = uf_colorspaces_create_srgb_profile();
                cmsUInt32Number len = 0;
                cmsSaveProfileToMem(hOutProfile, 0, &len); // Calculate len.
                if (len > 0) {
                    char buf[len];
                    cmsSaveProfileToMem(hOutProfile, buf, &len);
                    png_set_iCCP(png, info,
                                 uf->conf->profile[out_profile]
                                 [uf->conf->profileIndex[out_profile]].name,
                                 PNG_COMPRESSION_TYPE_BASE,
                                 (png_const_bytep) buf, len);
                } else {
                    ufraw_set_warning(uf,
                                      _("Failed to embed output profile '%s' in '%s'."),
                                      uf->conf->profile[out_profile]
                                      [uf->conf->profileIndex[out_profile]].name,
                                      uf->conf->outputFilename);
                }
                cmsCloseProfile(hOutProfile);
            }
            if (uf->conf->embedExif) {
                ufraw_exif_prepare_output(uf);
                if (uf->outputExifBuf != NULL)
                    PNGwriteRawProfile(png, info, "exif",
                                       uf->outputExifBuf, uf->outputExifBufLen);
            }
            png_write_info(png, info);
            if (BitDepth != 8 && G_BYTE_ORDER == G_LITTLE_ENDIAN)
                png_set_swap(png); // Swap byte order to big-endian

            ufraw_write_image_data(uf, png, &Crop, BitDepth, grayscaleMode,
                                   png_row_writer);

            png_write_end(png, NULL);
            png_destroy_write_struct(&png, &info);
        }
#endif /*HAVE_LIBPNG*/
#ifdef HAVE_LIBCFITSIO
    } else if (uf->conf->type == fits_type) {

        // image data and min/max values
        guint16 *image;
        guint16 max[3] = { 0, 0, 0 }, min[3] = { 65535, 65535, 65535 };
        guint64 sum[3] = { 0, 0, 0 };

        // FITS Header (taken from cookbook.c)
        int bitpix = USHORT_IMG;    // Use float format
        int naxis  = 3;		    // 3-dimensional image
        int status = 0;		    // status variable for fitsio

        long naxes[3]  = { Crop.width, Crop.height, 3 };
        long dim = Crop.width * Crop.height;
        long offset = 0;

        image = g_new(guint16, 3 * dim);

        int row;
        int i;
        ufraw_image_type *rawImage =
            (ufraw_image_type *)uf->Images[ufraw_first_phase].buffer;
        int rowStride = uf->Images[ufraw_first_phase].width;
        guint16 pixbuf16[3];

        // Avoid FITS images being saved upside down
        ufraw_flip_image(uf, 2);

        progress(PROGRESS_SAVE, -Crop.height);
        for (row = 0; row < Crop.height; row++) {
            progress(PROGRESS_SAVE, 1);
            for (i = 0; i < Crop.width; i++) {
                offset = row * Crop.width + i;
                develop_linear(rawImage[(Crop.y + row)*rowStride + Crop.x + i], pixbuf16,
                               uf->developer);
                int c;
                for (c = 0; c < 3; c++) {
                    sum[c] += image[c * dim + offset] = pixbuf16[c];
                    max[c] = MAX(pixbuf16[c], max[c]);
                    min[c] = MIN(pixbuf16[c], min[c]);
                }
            }
        }
        // calculate averages
        float average[3];
        int c;
        for (c = 0; c < 3; c++)
            average[c] = (float)sum[c] / dim;

        guint16 maxAll = MAX(MAX(max[0], max[1]), max[2]);
        guint16 minAll = MIN(MIN(min[0], min[1]), min[2]);

        fits_create_img(fitsFile, bitpix, naxis, naxes, &status);

        fits_write_img(fitsFile, TUSHORT, 1, 3 * dim, image, &status);
        g_free(image);

        fits_update_key(fitsFile, TUSHORT, "DATAMIN", &minAll,
                        "minimum data (overall)", &status);
        fits_update_key(fitsFile, TUSHORT, "DATAMAX", &maxAll,
                        "maximum data (overall)", &status);

        fits_update_key(fitsFile, TUSHORT, "DATAMINR", &min[0],
                        "minimum data (red channel)", &status);
        fits_update_key(fitsFile, TUSHORT, "DATAMAXR", &max[0],
                        "maximum data (red channel)", &status);

        fits_update_key(fitsFile, TUSHORT, "DATAMING", &min[1],
                        "minimum data (green channel)", &status);
        fits_update_key(fitsFile, TUSHORT, "DATAMAXG", &max[1],
                        "maximum data (green channel)", &status);

        fits_update_key(fitsFile, TUSHORT, "DATAMINB", &min[2],
                        "minimum data (blue channel)", &status);
        fits_update_key(fitsFile, TUSHORT, "DATAMAXB", &max[2],
                        "maximum data (blue channel)", &status);

        fits_update_key(fitsFile, TFLOAT, "AVERAGER", &average[0],
                        "average (red channel)", &status);
        fits_update_key(fitsFile, TFLOAT, "AVERAGEG", &average[1],
                        "average (green channel)", &status);
        fits_update_key(fitsFile, TFLOAT, "AVERAGEB", &average[2],
                        "average (blue channel)", &status);

        // Save known EXIF properties
        if (strlen(uf->conf->shutterText) > 0)
            fits_update_key(fitsFile, TSTRING, "EXPOSURE",
                            &uf->conf->shutterText, "Exposure Time", &status);

        if (strlen(uf->conf->isoText) > 0)
            fits_update_key(fitsFile, TSTRING, "ISO", &uf->conf->isoText,
                            "ISO Speed", &status);

        if (strlen(uf->conf->apertureText) > 0)
            fits_update_key(fitsFile, TSTRING, "APERTURE",
                            &uf->conf->apertureText, "Aperture", &status);

        if (strlen(uf->conf->focalLenText) > 0)
            fits_update_key(fitsFile, TSTRING, "FOCALLEN",
                            &uf->conf->focalLenText, "Focal Length", &status);

        if (strlen(uf->conf->focalLen35Text) > 0)
            fits_update_key(fitsFile, TSTRING, "FOCALLE2",
                            &uf->conf->focalLen35Text, "Focal Length (resp. 35mm)",
                            &status);

        if (strlen(uf->conf->lensText) > 0)
            fits_update_key(fitsFile, TSTRING, "LENS",
                            &uf->conf->lensText, "Lens", &status);

        // formating the date according to the FITS standard
        // http://archive.stsci.edu/fits/fits_standard/node40.html#s:dhist
        if (uf->conf->timestamp != 0) {
            char *time = g_new(char, 40);
            struct tm tmStamp;
            strftime(time, 40, "%Y-%m-%dT%H:%M:%S",
                     localtime_r(&uf->conf->timestamp, &tmStamp));
            fits_update_key(fitsFile, TSTRING, "DATE", time,
                            "Image taken at this date", &status);
            g_free(time);
        }

        if (strlen(uf->conf->make) > 0)
            fits_update_key(fitsFile, TSTRING, "MANUFACT", &uf->conf->make,
                            "Camera Manufacturer", &status);

        if (strlen(uf->conf->model) > 0)
            fits_update_key(fitsFile, TSTRING, "INSTRUME", &uf->conf->model,
                            "Camera Model", &status);

        fits_write_comment(fitsFile, "This file contains one RGB color image.",
                           &status);

        // Creator Ufraw
        fits_update_key(fitsFile, TSTRING, "CREATOR",  "UFRaw " VERSION,
                        "Creator Software", &status);

        fits_close_file(fitsFile, &status);

        if (status) {
            ufraw_set_error(uf, _("Error creating file '%s'."),
                            uf->conf->outputFilename);
            char errBuffer[max_name];
            fits_get_errstatus(status, errBuffer);
            ufraw_set_error(uf, errBuffer);
            while (fits_read_errmsg(errBuffer))
                ufraw_set_error(uf, errBuffer);
            return ufraw_get_status(uf);
        }
#endif /* HAVE_LIBCFITSIO */
    } else {
        ufraw_set_error(uf, _("Error creating file '%s'."),
                        uf->conf->outputFilename);
        ufraw_set_error(uf, _("Unknown file type %d."), uf->conf->type);
    }
#ifdef HAVE_LIBTIFF
    if (uf->conf->type == tiff_type) {
        TIFFClose(out);
        if (ufraw_tiff_message[0] != '\0') {
            if (!ufraw_is_error(uf)) {   // Error was not already set before
                ufraw_set_error(uf, _("Error creating file."));
                ufraw_set_error(uf, ufraw_tiff_message);
            }
            ufraw_tiff_message[0] = '\0';
        } else {
            if (uf->conf->embedExif)
                ufraw_exif_write(uf);
        }
    } else
#endif
#ifdef HAVE_LIBCFITSIO
        // Dummy to prevent fclose
        if (uf->conf->type == fits_type) {}
        else
#endif
        {
            if (strcmp(uf->conf->outputFilename, "-"))
                if (fclose(out) != 0) {
                    if (!ufraw_is_error(uf)) {   // Error was not already set before
                        ufraw_set_error(uf, _("Error creating file '%s'."),
                                        uf->conf->outputFilename);
                        ufraw_set_error(uf, g_strerror(errno));
                    }
                }
        }
    if (uf->conf->createID == also_id) {
        if (ufraw_get_message(uf) != NULL)
            ufraw_message(UFRAW_SET_LOG, ufraw_get_message(uf));
        // TODO: error handling
        conf_save(uf->conf, confFilename, NULL);
        g_free(confFilename);
    }
    return ufraw_get_status(uf);
}


/* Write EXIF data to PNG file.
 * Code copied from DigiKam's libs/dimg/loaders/pngloader.cpp.
 * The EXIF embeding is defined by ImageMagicK.
 * It is documented in the ExifTool page:
 * http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html
 */

#ifdef HAVE_LIBPNG
static void PNGwriteRawProfile(png_struct *ping,
                               png_info *ping_info, char *profile_type, guint8 *profile_data,
                               png_uint_32 length)
{
    png_textp text;
    long i;
    guint8 *sp;
    png_charp dp;
    png_uint_32 allocated_length, description_length;

    const guint8 hex[16] =
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    text = png_malloc(ping, sizeof(png_text));
    description_length = strlen(profile_type);
    allocated_length = length * 2 + (length >> 5) + 20 + description_length;

    text[0].text = png_malloc(ping, allocated_length);
    text[0].key = png_malloc(ping, 80);
    text[0].key[0] = '\0';

    g_strlcat(text[0].key, "Raw profile type ", 80);
    g_strlcat(text[0].key, profile_type, 80);

    sp = profile_data;
    dp = text[0].text;
    *dp++ = '\n';

    g_strlcpy(dp, profile_type, allocated_length);

    dp += description_length;
    *dp++ = '\n';
    *dp = '\0';

#if (PNG_LIBPNG_VER_MAJOR > 1 || (PNG_LIBPNG_VER_MAJOR == 1 && \
    PNG_LIBPNG_VER_MINOR > 2)) && (defined(INT_MAX) && INT_MAX > 0x7ffffffeL)
    g_snprintf(dp, allocated_length - strlen(text[0].text), "%8u ", length);
#else
    g_snprintf(dp, allocated_length - strlen(text[0].text), "%8lu ", length);
#endif

    dp += 8;

    for (i = 0; i < (long) length; i++) {
        if (i % 36 == 0)
            *dp++ = '\n';

        *(dp++) = hex[((*sp >> 4) & 0x0f)];
        *(dp++) = hex[((*sp++) & 0x0f)];
    }

    *dp++ = '\n';
    *dp = '\0';
    text[0].text_length = (dp - text[0].text);
    text[0].compression = -1;

    if (text[0].text_length <= allocated_length)
        png_set_text(ping, ping_info, text, 1);

    png_free(ping, text[0].text);
    png_free(ping, text[0].key);
    png_free(ping, text);
}
#endif /*HAVE_LIBPNG*/

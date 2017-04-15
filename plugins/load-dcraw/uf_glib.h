/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * uf_glib.h - glib compatibility header
 * Copyright 2004-2015 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UF_GLIB_H
#define _UF_GLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <glib/gstdio.h>

// g_win32_locale_filename_from_utf8 is needed only on win32
#ifdef _WIN32
#define uf_win32_locale_filename_from_utf8(__some_string__) \
    g_win32_locale_filename_from_utf8(__some_string__)
#define uf_win32_locale_filename_free(__some_string__) g_free(__some_string__)
#else
#define uf_win32_locale_filename_from_utf8(__some_string__) (__some_string__)
#define uf_win32_locale_filename_free(__some_string__) (void)(__some_string__)
#endif

// On win32 command-line arguments need to be translated to UTF-8
#ifdef _WIN32
#define uf_win32_locale_to_utf8(__some_string__) \
    g_locale_to_utf8(__some_string__, -1, NULL, NULL, NULL)
#define uf_win32_locale_free(__some_string__) g_free(__some_string__)
#else
#define uf_win32_locale_to_utf8(__some_string__) (__some_string__)
#define uf_win32_locale_free(__some_string__) (void)(__some_string__)
#endif

#ifdef __cplusplus
}
#endif

#endif /*_UF_GLIB_H*/

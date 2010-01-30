/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef RS_CACHE_H
#define RS_CACHE_H

#include <libxml/xmlwriter.h>

extern gchar *rs_cache_get_name(const gchar *src);
extern void rs_cache_save(RS_PHOTO *photo, const RSSettingsMask mask);
extern void rs_cache_save_settings(RSSettings *rss, const RSSettingsMask mask, xmlTextWriterPtr writer);
extern guint rs_cache_load(RS_PHOTO *photo);
extern guint rs_cache_load_setting(RSSettings *rss, xmlDocPtr doc, xmlNodePtr cur, gint version);
extern void rs_cache_load_quick(const gchar *filename, gint *priority, gboolean *exported);
extern void rs_cache_save_flags(const gchar *filename, const guint *priority, const gboolean *exported);

#endif /* RS_CACHE_H */

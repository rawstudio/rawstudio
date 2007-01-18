/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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

#define CONF_LWD "last_working_directory"
#define CONF_PREBGCOLOR "preview_background_color"
#define CONF_HISTHEIGHT "histogram_height"
#define CONF_PASTE_MASK "paste_mask"
#define CONF_DEFAULT_EXPORT_TEMPLATE "default_export_template"
#define CONF_CACHEDIR_IS_LOCAL "cache_in_home"
#define CONF_LOAD_GDK "open_8bit_images"
#define CONF_SAVE_FILETYPE "save_filetype"
#define CONF_BATCH_DIRECTORY "batch_directory"
#define CONF_BATCH_FILENAME "batch_filename"
#define CONF_BATCH_FILETYPE "batch_filetype"
#define CONF_BATCH_WIDTH "batch_width"
#define CONF_BATCH_HEIGHT "batch_height"
#define CONF_BATCH_JPEG_QUALITY "batch_jpeg_quality"
#define CONF_BATCH_TIFF_UNCOMPRESSED "batch_tiff_uncompressed"
#define CONF_EXPORT_DIRECTORY "export_directory"
#define CONF_EXPORT_FILENAME "export_filename"
#define CONF_EXPORT_FILETYPE "export_filetype"
#define CONF_EXPORT_JPEG_QUALITY "export_jpeg_quality"
#define CONF_EXPORT_TIFF_UNCOMPRESSED "export_tiff_uncompressed"
#define CONF_CMS_ENABLED "cms_enabled"
#define CONF_CMS_INTENT "cms_intent"
#define CONF_CMS_IN_PROFILE_LIST "cms_in_profile_list"
#define CONF_CMS_IN_PROFILE_SELECTED "cms_in_profile_selected"
#define CONF_CMS_DI_PROFILE_LIST "cms_di_profile_list"
#define CONF_CMS_DI_PROFILE_SELECTED "cms_di_profile_selected"
#define CONF_CMS_EX_PROFILE_LIST "cms_ex_profile_list"
#define CONF_CMS_EX_PROFILE_SELECTED "cms_ex_profile_selected"
#define CONF_ROI_GRID "roi_grid"
#define CONF_CROP_ASPECT "crop_aspect"


#define DEFAULT_CONF_EXPORT_DIRECTORY "exports/"
#define DEFAULT_CONF_EXPORT_FILENAME "%f_%2c"
#define DEFAULT_CONF_EXPORT_FILETYPE "jpeg"
#define DEFAULT_CONF_EXPORT_JPEG_QUALITY "100"
#define DEFAULT_CONF_BATCH_DIRECTORY "batch_exports/"
#define DEFAULT_CONF_BATCH_FILENAME "%f_%2c"
#define DEFAULT_CONF_BATCH_FILETYPE "jpeg"
#define DEFAULT_CONF_BATCH_JPEG_QUALITY "100"


// get the last working directory from gconf
void rs_set_last_working_directory(const char *lwd);

// save the current working directory to gconf
gchar *rs_get_last_working_directory(void);

gboolean rs_conf_get_boolean(const gchar *name, gboolean *boolean_value);
gboolean rs_conf_get_boolean_with_default(const gchar *name, gboolean *boolean_value, gboolean default_value);
gboolean rs_conf_set_boolean(const gchar *name, gboolean bool_value);
gchar *rs_conf_get_string(const gchar *path);
gboolean rs_conf_set_string(const gchar *path, const gchar *string);
gboolean rs_conf_get_integer(const gchar *name, gint *integer_value);
gboolean rs_conf_set_integer(const gchar *name, const gint integer_value);
gboolean rs_conf_get_color(const gchar *name, GdkColor *color);
gboolean rs_conf_set_color(const gchar *name, GdkColor *color);
gboolean rs_conf_get_cms_intent(const gchar *name, gint *intent);
gboolean rs_conf_set_cms_intent(const gchar *name, gint *intent);
gboolean rs_conf_get_filetype(const gchar *name, RS_FILETYPE **target);
gboolean rs_conf_set_filetype(const gchar *name, const RS_FILETYPE *filetype);
gboolean rs_conf_get_double(const gchar *name, gdouble *float_value);
gboolean rs_conf_set_double(const gchar *name, const gdouble float_value);
GSList *rs_conf_get_list_string(const gchar *name);
gboolean rs_conf_set_list_string(const gchar *name, GSList *list);
gboolean rs_conf_add_string_to_list_string(const gchar *name, gchar *value);
gchar *rs_conf_get_nth_string_from_list_string(const gchar *name, gint num);

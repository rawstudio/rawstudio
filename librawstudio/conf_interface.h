/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
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

#define CONF_LWD "last_working_directory"
#define CONF_PREBGCOLOR "preview_background_color"
#define CONF_HISTHEIGHT "histogram_height"
#define CONF_PASTE_MASK "paste_mask"
#define CONF_DEFAULT_EXPORT_TEMPLATE "default_export_template"
#define CONF_CACHEDIR_IS_LOCAL "cache_in_home"
#define CONF_LOAD_GDK "open_8bit_images"
#define CONF_LOAD_RECURSIVE "load_recursive"
#define CONF_PRELOAD "preload_photos"
#define CONF_SAVE_FILETYPE "save_filetype"
#define CONF_BATCH_DIRECTORY "batch_directory"
#define CONF_BATCH_FILENAME "batch_filename"
#define CONF_BATCH_FILETYPE "batch_filetype"
#define CONF_BATCH_WIDTH "batch_width"
#define CONF_BATCH_HEIGHT "batch_height"
#define CONF_BATCH_JPEG_QUALITY "batch_jpeg_quality"
#define CONF_BATCH_TIFF_UNCOMPRESSED "batch_tiff_uncompressed"
#define CONF_BATCH_SIZE_LOCK "batch_size_lock"
#define CONF_BATCH_SIZE_WIDTH "batch_size_width"
#define CONF_BATCH_SIZE_HEIGHT "batch_size_height"
#define CONF_BATCH_SIZE_SCALE "batch_size_scale"
#define CONF_ROI_GRID "roi_grid"
#define CONF_CROP_ASPECT "crop_aspect"
#define CONF_SHOW_FILENAMES "show_filenames_in_iconview"
#define CONF_USE_SYSTEM_THEME "use_system_theme"
#define CONF_FULLSCREEN "fullscreen"
#define CONF_SHOW_TOOLBOX_FULLSCREEN "show_toolbox_fullscreen"
#define CONF_SHOW_TOOLBOX "show_toolbox"
#define CONF_SHOW_ICONBOX_FULLSCREEN "show_iconbox_fullscreen"
#define CONF_SHOW_ICONBOX "show_iconbox"
#define CONF_SHOW_TOOLBOX_EXPOSURE "show_toolbox_exposure"
#define CONF_SHOW_TOOLBOX_SATURATION "show_toolbox_saturation"
#define CONF_SHOW_TOOLBOX_HUE "show_toolbox_hue"
#define CONF_SHOW_TOOLBOX_CONTRAST "show_toolbox_contrast"
#define CONF_SHOW_TOOLBOX_WARMTH "show_toolbox_warmth"
#define CONF_SHOW_TOOLBOX_SHARPEN "show_toolbox_sharpen"
#define CONF_SHOW_TOOLBOX_DENOISE_LUMA "show_toolbox_denoise_luma"
#define CONF_SHOW_TOOLBOX_DENOISE_CHROMA "show_toolbox_denoise_chroma"
#define CONF_SHOW_TOOLBOX_CURVE "show_toolbox_curve"
#define CONF_SHOW_TOOLBOX_TRANSFORM "show_toolbox_transform"
#define CONF_SHOW_TOOLBOX_HIST "show_toolbox_hist"
#define CONF_TOOLBOX_WIDTH "toolbox_width"
#define CONF_SPLIT_CONTINUOUS "split_continuous"
#define CONF_LAST_PRIORITY_PAGE "last_priority_page"
#define CONF_STORE_SORT_METHOD "store_sort_method"
#define CONF_LIBRARY_AUTOTAG "library_autotag"
#define CONF_LIBRARY_TAG_SEARCH "library_tag_search"
#define CONF_EXPORT_AS_FOLDER "export_as_folder"
#define CONF_EXPORT_AS_SIZE_PERCENT "export_as_size_percent"
#define CONF_MAIN_WINDOW_WIDTH "main_window_width"
#define CONF_MAIN_WINDOW_HEIGHT "main_window_height"
#define CONF_MAIN_WINDOW_POS_X "main_window_pos_x"
#define CONF_MAIN_WINDOW_POS_Y "main_window_pos_y"
#define CONF_MAIN_WINDOW_MAXIMIZED "main_window_maximized"
#define CONF_PICASA_CLIENT_AUTH_TOKEN "picasa_client_auth_token"
#define CONF_PICASA_CLIENT_USERNAME "picasa_client_username"
#define CONF_PICASA_CLIENT_ALBUM_ID "picasa_client_album_id"
#define CONF_ENFUSE_METHOD "conf_enfuse_method"
#define CONF_ENFUSE_ALIGN_IMAGES "conf_enfuse_align_images"
#define CONF_ENFUSE_EXTEND "conf_enfuse_extend"
#define CONF_ENFUSE_SIZE "conf_enfuse_size"
#define CONF_ENFUSE_EXTEND_NEGATIVE_SINGLE "conf_enfuse_extend_negative_single"
#define CONF_ENFUSE_EXTEND_POSITIVE_SINGLE "conf_enfuse_extend_positive_single"
#define CONF_ENFUSE_EXTEND_STEP_SINGLE "conf_enfuse_extend_step_single"
#define CONF_ENFUSE_EXTEND_NEGATIVE_MULTI "conf_enfuse_extend_negative_multi"
#define CONF_ENFUSE_EXTEND_POSITIVE_MULTI "conf_enfuse_extend_positive_multi"
#define CONF_ENFUSE_EXTEND_STEP_MULTI "conf_enfuse_extend_step_multi"
#define CONF_ENFUSE_CACHE "conf_enfuse_cache"

#define DEFAULT_CONF_EXPORT_FILENAME "%f_%2c"
#define DEFAULT_CONF_BATCH_DIRECTORY "batch_exports/"
#define DEFAULT_CONF_BATCH_FILENAME "%f_%2c"
#define DEFAULT_CONF_BATCH_FILETYPE "jpeg"
#define DEFAULT_CONF_BATCH_JPEG_QUALITY "100"
#define DEFAULT_CONF_FULLSCREEN FALSE
#define DEFAULT_CONF_SHOW_TOOLBOX_FULLSCREEN TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX TRUE
#define DEFAULT_CONF_SHOW_ICONBOX_FULLSCREEN FALSE
#define DEFAULT_CONF_SHOW_ICONBOX TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_EXPOSURE TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_SATURATION TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_HUE TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_CONTRAST TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_WARMTH TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_SHARPEN TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_DENOISE_LUMA TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_DENOISE_CHROMA TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_CURVE TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_TRANSFORM TRUE
#define DEFAULT_CONF_SHOW_TOOLBOX_HIST TRUE
#define DEFAULT_CONF_LOAD_RECURSIVE FALSE
#define DEFAULT_CONF_USE_SYSTEM_THEME FALSE
#define DEFAULT_CONF_SHOW_FILENAMES FALSE
#define DEFAULT_CONF_LIBRARY_AUTOTAG FALSE
#define DEFAULT_CONF_MAIN_WINDOW_WIDTH 800
#define DEFAULT_CONF_MAIN_WINDOW_HEIGHT 600
#define DEFAULT_CONF_MAIN_WINDOW_POS_X 50
#define DEFAULT_CONF_MAIN_WINDOW_POS_Y 50
#define DEFAULT_CONF_MAIN_WINDOW_MAXIMIZED FALSE
#define DEFAULT_CONF_ENFUSE_METHOD 0
#define DEFAULT_CONF_ENFUSE_ALIGN_IMAGES TRUE
#define DEFAULT_CONF_ENFUSE_EXTEND TRUE
#define DEFAULT_CONF_ENFUSE_SIZE 0
#define DEFAULT_CONF_ENFUSE_EXTEND_NEGATIVE_SINGLE 3.0
#define DEFAULT_CONF_ENFUSE_EXTEND_POSITIVE_SINGLE 3.0
#define DEFAULT_CONF_ENFUSE_EXTEND_STEP_SINGLE 1.0
#define DEFAULT_CONF_ENFUSE_EXTEND_NEGATIVE_MULTI 1.0
#define DEFAULT_CONF_ENFUSE_EXTEND_POSITIVE_MULTI 1.0
#define DEFAULT_CONF_ENFUSE_EXTEND_STEP_MULTI 2.0
#define DEFAULT_CONF_ENFUSE_CACHE TRUE

/* get the last working directory from gconf */
void rs_set_last_working_directory(const char *lwd);

/* save the current working directory to gconf */
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
gboolean rs_conf_get_double(const gchar *name, gdouble *float_value);
gboolean rs_conf_set_double(const gchar *name, const gdouble float_value);
GSList *rs_conf_get_list_string(const gchar *name);
gboolean rs_conf_set_list_string(const gchar *name, GSList *list);
gboolean rs_conf_add_string_to_list_string(const gchar *name, gchar *value);
gchar *rs_conf_get_nth_string_from_list_string(const gchar *name, gint num);
gboolean rs_conf_unset(const gchar *name);

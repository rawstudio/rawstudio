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


#include <rawstudio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <config.h>
#include <gettext.h>
#include "rs-tethered-shooting.h"
#include "rs-preview-widget.h"
#include <gphoto2/gphoto2-camera.h>
#include <stdlib.h>
#include <fcntl.h>
#include "filename.h"
#include <rs-store.h>
#include <rs-library.h>
#ifdef WITH_GCONF
#include <gconf/gconf-client.h>
#endif
#include "conf_interface.h"
#include "gtk-helper.h"
#include "rs-photo.h"
#include "rs-cache.h"

enum
{
  NAME_COLUMN,
  VALUE_COLUMN,
  N_COLUMNS
};

enum 
{
	ASYNC_THREAD_TYPE_NONE,
	ASYNC_THREAD_TYPE_MONITOR,
	ASYNC_THREAD_TYPE_INTERVAL
};

typedef struct {
	Camera *camera;
	GPContext *context;
	GtkWidget *window;
	GtkListStore *camera_store;
	GtkTextBuffer *status_buffer;
	GtkComboBox *camera_selector;
	GtkTextView *status_textview;
	RS_BLOB *rs;
	GThread *async_thread_id;
	gboolean keep_thread_running;
	gint thread_type;
	GtkWidget *interval_toggle_button;
	gint interval_toggle_button_signal;
} TetherInfo;

typedef struct {
	GtkWidget *example_label;
	GtkWidget *event;
	const gchar *output_type;
	const gchar *filename;
} CAMERA_FILENAME;

static void shutdown_async_thread(TetherInfo *t);
static void closeconnection(TetherInfo *t);
static void start_interval_shooting(GObject *entry, gpointer user_data);


static void
append_status_va_list(TetherInfo *t, const gchar *format, va_list args)
{
	gdk_threads_lock();
	gchar result_buffer[512];
	gint str_len = g_vsnprintf(result_buffer, 512, format, args);
	GtkTextIter iter;
	gtk_text_buffer_get_end_iter(t->status_buffer, &iter);
	gtk_text_buffer_insert(t->status_buffer, &iter, result_buffer, str_len);
	gtk_text_buffer_get_end_iter(t->status_buffer, &iter);
	if (t->status_textview)
	{
		/* get the current ( cursor )mark name */
		GtkTextMark* insert_mark = gtk_text_buffer_get_insert (t->status_buffer);
 
		/* move mark and selection bound to the end */
		gtk_text_buffer_place_cursor(t->status_buffer, &iter);

		/* scroll to the end view */
		gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW (t->status_textview), insert_mark, 0.0, TRUE, 0.0, 1.0); 
	}
	gdk_threads_unlock();
}

static void
append_status(TetherInfo *t, const gchar *format, ...)
{
	va_list argptr;
	va_start(argptr,format);
	append_status_va_list(t, format, argptr);
	va_end(argptr);
}

static void
ctx_error_func (GPContext *context, const char *format, va_list args, void *data)
{
	gdk_threads_lock();
	TetherInfo *t = (TetherInfo*)data;
	append_status (t, _("Gphoto2 reported Context Error:\n"));
	append_status_va_list(t, format, args);
	append_status  (t, "\n");
	if (t->async_thread_id && t->async_thread_id != g_thread_self())
		shutdown_async_thread(t);
	t->keep_thread_running = FALSE;
	gdk_threads_unlock();
}

static void
ctx_status_func (GPContext *context, const char *format, va_list args, void *data)
{
	TetherInfo *t = (TetherInfo*)data;
	gdk_threads_lock();
	append_status_va_list(t, format, args);
	append_status  (t, "\n");
	gdk_threads_unlock();
}

int
enumerate_cameras(GtkListStore *camera_store, GPContext *context) {
	int ret, i, count;
	CameraList		*xlist = NULL;
	GPPortInfoList		*portinfolist = NULL;
	CameraAbilitiesList	*abilities = NULL;
	GtkTreeIter iter;

	count = 0;
	ret = gp_list_new (&xlist);
	if (ret < GP_OK) goto out;
	if (!portinfolist) {
		/* Load all the port drivers we have... */
		ret = gp_port_info_list_new (&portinfolist);
		if (ret < GP_OK) goto out;
		ret = gp_port_info_list_load (portinfolist);
		if (ret < 0) goto out;
		ret = gp_port_info_list_count (portinfolist);
		if (ret < 0) goto out;
	}
	/* Load all the camera drivers we have... */
	ret = gp_abilities_list_new (&abilities);
	if (ret < GP_OK) goto out;
	ret = gp_abilities_list_load (abilities, context);
	if (ret < GP_OK) goto out;

	/* ... and autodetect the currently attached cameras. */
        ret = gp_abilities_list_detect (abilities, portinfolist, xlist, context);
	if (ret < GP_OK) goto out;

	/* Filter out the "usb:" entry */
	ret = gp_list_count (xlist);
	if (ret < GP_OK) goto out;
	for (i=0;i<ret;i++) {
		const char *name, *value;

		gp_list_get_name (xlist, i, &name);
		gp_list_get_value (xlist, i, &value);
		if (!strcmp ("usb:",value)) continue;
		gtk_list_store_append(camera_store, &iter); 
		gtk_list_store_set (camera_store, &iter,
			NAME_COLUMN, name,
			VALUE_COLUMN, value,
			-1);
		count++;
	}
out:
	gp_list_free (xlist);
	return count;
}

/*
 * This function looks up a label or key entry of
 * a configuration widget.
 * The functions descend recursively, so you can just
 * specify the last component.
 */
/* Should work, but currently not used */
#if 0

static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child) {
	int ret;
	ret = gp_widget_get_child_by_name (widget, key, child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label (widget, key, child);
	return ret;
}

/* Gets a string configuration value.
 * This can be:
 *  - A Text widget
 *  - The current selection of a Radio Button choice
 *  - The current selection of a Menu choice
 *
 * Sample (for Canons eg):
 *   get_config_value_string (camera, "owner", &ownerstr, context);
 */

static int
get_config_value_string (Camera *camera, const char *key, char **str, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;
	char			*val;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		g_warning("camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		g_warning("lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		g_warning("widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
		break;
	default:
		g_warning("widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This is the actual query call. Note that we just
	 * a pointer reference to the string, not a copy... */
	ret = gp_widget_get_value (child, &val);
	if (ret < GP_OK) {
		g_warning("could not query widget value: %d\n", ret);
		goto out;
	}
	/* Create a new copy for our caller. */
	*str = strdup (val);
out:
	gp_widget_free (widget);
	return ret;
}
#endif

#define CHECKRETVAL(A) if (A < GP_OK) {\
	append_status(t, _("ERROR: Gphoto2 returned error value %d\nError message is: %s\n"), A, gp_result_as_string(A));\
	gp_camera_free (t->camera);\
	t->camera = NULL;\
	return A;}

gpointer start_thread_monitor(gpointer _thread_info);

static int
enable_capture(TetherInfo *t) 
{
  int retval;

	if (!t->camera)
		return -1;

  g_debug("Get root config.");
  CameraWidget *rootconfig; // okay, not really
  CameraWidget *actualrootconfig;
	CameraWidget *child;

	retval = gp_camera_get_config(t->camera, &rootconfig, t->context);
	CHECKRETVAL(retval);
		actualrootconfig = rootconfig;

	/* Enable on Canon */
	if (retval >= 0)
	{
		retval = gp_widget_get_child_by_name(rootconfig, "main", &child);
	}
	if (retval >= 0)
	{
		rootconfig = child;
		retval = gp_widget_get_child_by_name(rootconfig, "settings", &child);
	}
	
	if (retval >= 0)
	{
		rootconfig = child;
		retval = gp_widget_get_child_by_name(rootconfig, "capture", &child);
	}
	if (retval >= 0)
	{
		CameraWidget *capture = child;

		const char *widgetinfo;
		gp_widget_get_name(capture, &widgetinfo);
		const char *widgetlabel;
		gp_widget_get_label(capture, &widgetlabel);
		int widgetid;
		gp_widget_get_id(capture, &widgetid);
		CameraWidgetType widgettype;
		gp_widget_get_type(capture, &widgettype);
		int one=1;
		retval = gp_widget_set_value(capture, &one);
		append_status(t, _("Enabling capture mode for Canon cameras.\n"));
	}

	/* Nikon may need this*/
	retval = gp_widget_get_child_by_name(actualrootconfig, "main", &child);

	if (retval >= 0)
	{
		rootconfig = child;
		retval = gp_widget_get_child_by_name(rootconfig, "settings", &child);
	}
	
	if (retval >= 0)
	{
		rootconfig = child;
		retval = gp_widget_get_child_by_name(rootconfig, "recordingmedia", &child);
	}
	if (retval >= 0)
	{
		CameraWidget *capture = child;
		CameraWidgetType widgettype;
		gp_widget_get_type(capture, &widgettype);
		const gchar* one = "SDRAM";
		retval = gp_widget_set_value(capture, one);
		append_status(t, _("Enabling capture mode for Nikon cameras.\n"));
	}
  g_debug("Enabling capture.");
  retval = gp_camera_set_config(t->camera, actualrootconfig, t->context);
	CHECKRETVAL(retval);

  g_debug("Capture Enabled.");
	append_status(t, _("Capture Enabled.\n"));

	return GP_OK;
}

static int
open_camera (TetherInfo *t, const char *model, const char *port) 
{
	Camera **camera = &t->camera;
	int ret, m, p;
	CameraAbilities	a;
	GPPortInfo	pi;
	GPPortInfoList		*portinfolist = NULL;
	CameraAbilitiesList	*abilities = NULL;

	ret = gp_camera_new (camera);
	CHECKRETVAL(ret);

	/* First lookup the model / driver */
	ret = gp_abilities_list_new(&abilities);
	CHECKRETVAL(ret);

	ret = gp_abilities_list_load(abilities, t->context);
	CHECKRETVAL(ret);

	m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK)
		return ret;

	ret = gp_abilities_list_get_abilities (abilities, m, &a);
	CHECKRETVAL(ret);

	ret = gp_camera_set_abilities (*camera, a);
	CHECKRETVAL(ret);

	ret = gp_port_info_list_new(&portinfolist);
	CHECKRETVAL(ret);

	ret = gp_port_info_list_load(portinfolist);
	CHECKRETVAL(ret);

	/* Then associate the camera with the specified port */
	p = gp_port_info_list_lookup_path (portinfolist, port);
	CHECKRETVAL(p);

	switch (p) 
	{
		case GP_ERROR_UNKNOWN_PORT:
			append_status (t, _("The port you specified ('%s') can not be found."), port);
			break;
		default:
			break;
	}

	CHECKRETVAL(ret);
	ret = gp_port_info_list_get_info (portinfolist, p, &pi);
	CHECKRETVAL(ret);
	ret = gp_camera_set_port_info (*camera, pi);
	CHECKRETVAL(ret);

	return GP_OK;
}


static void add_tags_to_photo(TetherInfo* t, RS_PHOTO *photo)
{
	const gchar* photo_tags = rs_conf_get_string("tether-tags-for-new-images");

	if (!photo_tags)
		return;

	g_assert(photo != NULL);
	g_assert(photo->metadata != NULL);

	RSLibrary *lib = rs_library_get_singleton();

	rs_library_add_photo_with_metadata(lib, photo->filename, photo->metadata);

	gchar** split_tags = g_strsplit_set(photo_tags, " .,/;:~^*|&",0);
	int i = 0;
	while (split_tags[i] != NULL)
	{
		gint tag_id = rs_library_add_tag(lib, split_tags[i]);
		rs_io_idle_add_tag(photo->filename, tag_id, FALSE, -1);
		i++;
	}
	rs_io_idle_add_tag(photo->filename, -2, FALSE, -1);
	g_strfreev(split_tags);
}


static gchar*
add_file_to_store(TetherInfo* t, const char* tmp_name) 
{
	RSMetadata *metadata;
	gchar *lwd;
	gchar* org_template = rs_conf_get_string("tether-export-filename");
	lwd = rs_conf_get_string(CONF_LWD);
	GString *filename_template = g_string_new(lwd);
	g_string_append(filename_template, G_DIR_SEPARATOR_S);
	g_string_append(filename_template, org_template);
	g_string_append(filename_template, g_strrstr(tmp_name, "."));
	
	gchar* filename = filename_parse(g_string_free(filename_template, FALSE),tmp_name, 0);

	GFile* src = g_file_new_for_path(tmp_name);
	GFile* dst = g_file_new_for_path(filename);

	gdk_threads_unlock();
	if (!g_file_move(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL))
	{
		gdk_threads_lock();
		append_status(t, _("Moving file to current directory failed!\n"));
		return NULL;
	}
	g_object_unref(src);
	g_object_unref(dst);

	gboolean add_image = TRUE;
	rs_conf_get_boolean_with_default("tether-add-image", &add_image, TRUE);

	if (add_image)
	{
		gdk_threads_lock();
		rs_store_set_iconview_size(t->rs->store, rs_store_get_iconview_size(t->rs->store)+1);
		rs_store_load_file(t->rs->store, filename);
		gdk_threads_unlock();
	}

	/* Make sure we rotate this right */
	metadata = rs_metadata_new_from_file(filename);
	g_object_unref(metadata);
	gdk_threads_lock();
	return filename;
}

#define RS_NUM_SETTINGS 3

static gint
transfer_file_captured(TetherInfo* t, CameraFilePath* camera_file_path) 
{
	CameraFile *canonfile;
	int fd, retval, i;
	append_status(t,_("Downloading and adding image.\n"));
	char *tmp_name_ptr;
	tmp_name_ptr = g_build_filename(g_get_tmp_dir(), g_strdup_printf("rs-tether-%d.tmp", g_random_int()), NULL);

	if (NULL == tmp_name_ptr)
		return GP_ERROR;

	char *extension = g_strrstr(camera_file_path->name, ".");
	tmp_name_ptr = g_strconcat(tmp_name_ptr, extension, NULL);

	fd = open(tmp_name_ptr, O_CREAT | O_WRONLY, 0644);
	if (fd == -1)
	{
		append_status(t,_("Could not open temporary file on disk for writing"));
		return GP_ERROR;
	}

	gdk_threads_unlock();
	retval = gp_file_new_from_fd(&canonfile, fd);
	CHECKRETVAL(retval);
	retval = gp_camera_file_get(t->camera, camera_file_path->folder, camera_file_path->name, GP_FILE_TYPE_NORMAL, canonfile, t->context);
	CHECKRETVAL(retval);
	retval = gp_camera_file_delete(t->camera, camera_file_path->folder, camera_file_path->name, t->context);
	CHECKRETVAL(retval);

	/* Be sure there isn't a quick export still running */
	while (NULL != t->rs->post_open_event)
		g_usleep(100*1000);

	gdk_threads_lock();

	/* Copy settings */
	gboolean copy_settings = TRUE;
	rs_conf_get_boolean_with_default("tether-copy-current-settings", &copy_settings, FALSE);
	RSSettings *settings_buffer[RS_NUM_SETTINGS];

	if (copy_settings && t->rs->photo)
	{
		for (i = 0; i < RS_NUM_SETTINGS; i++)
		{
			settings_buffer[i] = rs_settings_new();
			rs_settings_copy(t->rs->photo->settings[i], MASK_ALL, settings_buffer[i]);
		}
	}
	else 
		copy_settings = FALSE;

	gp_file_free(canonfile);
	gchar *filename = add_file_to_store(t, tmp_name_ptr);
	if (!filename)
		return GP_ERROR;

	gdk_threads_unlock();
	RS_PHOTO *photo = rs_photo_new();
	photo->filename = g_strdup(filename);

	/* Paste settings */
	if (copy_settings)
	{
		/* Make sure we rotate this right */
		RSMetadata *metadata = rs_metadata_new_from_file(photo->filename);
		switch (metadata->orientation)
		{
			case 90: ORIENTATION_90(photo->orientation);
				break;
			case 180: ORIENTATION_180(photo->orientation);
				break;
			case 270: ORIENTATION_270(photo->orientation);
				break;
		}
		g_object_unref(metadata);

		for (i = 0; i < RS_NUM_SETTINGS; i++)
		{
			rs_settings_copy(settings_buffer[i], MASK_ALL, photo->settings[i]);
			g_object_unref(settings_buffer[i]);
		}
		rs_cache_save(photo, MASK_ALL);
	}

	/* Add Tags */
	add_tags_to_photo(t, photo);
	g_object_unref(photo);
	photo = NULL;
	gdk_threads_lock();

	gboolean minimize = TRUE;
	rs_conf_get_boolean_with_default("tether-minimize-window", &minimize, TRUE);

	/* Open image, if this has been selected */
	gboolean open_image = TRUE;
	rs_conf_get_boolean_with_default("tether-open-image", &open_image, TRUE);
	gboolean quick_export = FALSE;
	rs_conf_get_boolean_with_default("tether-quick-export", &quick_export, FALSE);
	if (open_image || quick_export)
	{
		if (!rs_store_set_selected_name(t->rs->store, filename, TRUE))
		{
			append_status(t, _("Could not open image!\n"));
			minimize = FALSE;
		}
	}

	/* Minimize window */
	if (minimize)
		gtk_window_iconify(GTK_WINDOW(t->window));

	if (quick_export)
		t->rs->post_open_event = "QuickExport";

	g_free(tmp_name_ptr);
	return GP_OK;
}

#undef RS_NUM_SETTINGS

static gint
capture_to_file(TetherInfo* t) 
{
	int retval;
	CameraFilePath camera_file_path;
	gboolean blank = FALSE;
	rs_conf_get_boolean_with_default("tether-blank-screen", &blank, FALSE);

	append_status(t, _("Capturing.\n"));
	if (blank)
		rs_preview_widget_blank(RS_PREVIEW_WIDGET(t->rs->preview));
	gdk_threads_leave();
	retval = gp_camera_capture(t->camera, GP_CAPTURE_IMAGE, &camera_file_path, t->context);
	gdk_threads_enter();
	if (blank)
		rs_preview_widget_unblank(RS_PREVIEW_WIDGET(t->rs->preview));
	CHECKRETVAL(retval);
	retval = transfer_file_captured(t, &camera_file_path);
	return retval;
}

/* 
 * Threads are purely synchronized by gdk_threads_lock/unlock
 * Whenever they are idle, or doing heavy non-gui processing or IO, 
 * the lock is released.
*/

gpointer
start_thread_monitor(gpointer _thread_info)
{
	TetherInfo *t = (TetherInfo*) _thread_info;
	gdk_threads_enter();
	int retval;
	while (t->keep_thread_running)
	{
		Camera *cam = t->camera;
		CameraEventType type;
		void * event_data = NULL;
		if (NULL == cam)
		{
			t->keep_thread_running = FALSE;
			continue;
		}
		gdk_threads_leave();
		retval = gp_camera_wait_for_event(cam, 100, &type, &event_data, t->context);
		gdk_threads_enter();

		if (retval < GP_OK)
		{
			append_status(t, _("Monitor recieved error %d, while waiting for camera.\nError text is: %s\n"), retval, gp_result_as_string(retval));
			t->keep_thread_running = FALSE;
		}
		else
		{
			if (type == GP_EVENT_FILE_ADDED)
			{
				CameraFilePath* camera_file_path = (CameraFilePath*)event_data;
				retval = transfer_file_captured(t, camera_file_path);
				if (retval < GP_OK)
				{
					append_status(t, _("Recieved error %d, while downloading image from camera.\nError text is: %s\n"), retval, gp_result_as_string(retval));
					t->keep_thread_running = FALSE;
				}
				else
					append_status(t, _("File Downloaded Succesfully.\n"));
			}
		}
	}
	append_status(t, _("Camera monitor shutting down.\n"));
	gdk_threads_leave();
	t->thread_type = ASYNC_THREAD_TYPE_NONE;
	return NULL;
}

gpointer
start_thread_interval(gpointer _thread_info)
{
	TetherInfo *t = (TetherInfo*) _thread_info;
	gdk_threads_enter();
	int retval;
	GTimer* capture_timer = g_timer_new();
	while (t->keep_thread_running)
	{
		retval = capture_to_file(t);
		if (retval < GP_OK)
		{
			append_status(t, _("Recieved error %d, while capturing image.\nError text is: %s\n"), retval, gp_result_as_string(retval));
			t->keep_thread_running = FALSE;
		}
		if (t->keep_thread_running)
		{
			gdouble interval = 10.0;
			rs_conf_get_double("tether-interval-interval", &interval);

			gboolean take_next = g_timer_elapsed(capture_timer, NULL) > interval;

			if (take_next)
				append_status(t, _("Warning: It took longer time to capture the image than the set interval\nIt took %.1f seconds to download the image.\nConsider increasing the interval.\n"), g_timer_elapsed(capture_timer, NULL) + 0.1);

			append_status(t, _("Waiting for next image.\n"));

			while (t->keep_thread_running && !take_next)
			{
				if (g_timer_elapsed(capture_timer, NULL) > interval)
					take_next = TRUE;
				else
				{
					gdk_threads_leave();
					/* Sleep 100ms */
					g_usleep(100*1000);
					gdk_threads_enter();
				}
			}
			g_timer_reset(capture_timer);
			
			if (t->keep_thread_running)
			{
				GTK_CATCHUP();
				gdk_threads_leave();
				/* Sleep 10ms, just to let GUI become responsive */
				g_usleep(10*1000);
				gdk_threads_enter();
			}
		}
	}
	g_signal_handler_disconnect(G_OBJECT(t->interval_toggle_button), t->interval_toggle_button_signal);
	t->interval_toggle_button_signal =  g_signal_connect(G_OBJECT(t->interval_toggle_button), "clicked", G_CALLBACK(start_interval_shooting), t);
	gtk_button_set_label(GTK_BUTTON(t->interval_toggle_button), _("Start Shooting"));
	append_status(t, _("Interval shooting shutting down.\n"));
	gdk_threads_leave();
	g_timer_destroy(capture_timer);
	t->thread_type = ASYNC_THREAD_TYPE_NONE;
	return NULL;
}

static void closeconnection(TetherInfo *t)
{
	if (!t->camera)
		return;
	append_status(t, _("Disconnecting current camera\n"));
	gp_camera_exit (t->camera, t->context);
	gp_camera_free (t->camera);
	t->camera = NULL;
}

static void initcamera(TetherInfo *t, GtkTreeIter *iter)
{
	gint ret;

	/* This call will autodetect cameras, take the
	 * first one from the list and use it. It will ignore
	 * any others... See the *multi* examples on how to
	 * detect and use more than the first one.
	 */
	const char	*name, *value;

	gtk_tree_model_get(GTK_TREE_MODEL(t->camera_store), iter,
		NAME_COLUMN, &name,
		VALUE_COLUMN, &value, -1);

	ret = open_camera(t, name, value);
	if (ret < GP_OK) 
	{
		append_status(t,_("Camera %s on port %s failed to open\n"), name, value);
		return;
	}
	
	ret = gp_camera_init (t->camera, t->context);
	if (ret < GP_OK) {
		append_status(t,_("ERROR: Init camera returned %d.\nError text is:%s\n"), ret, gp_result_as_string(ret));
		gp_camera_free (t->camera);
		t->camera = NULL;
		return;
	}

	enable_capture(t);
}


static void
update_example(CAMERA_FILENAME *filename)
{
	gchar *parsed;
	gchar *final = "";
	GtkLabel *example = GTK_LABEL(filename->example_label);

	parsed = filename_parse(filename->filename, "filename", 0);
	final = g_strdup_printf("%s.ext", parsed);

	gtk_label_set_markup(example, final);

	g_free(parsed);
	g_free(final);
}

/* When entering this, we must have gdk locked */
static void
shutdown_async_thread(TetherInfo *t)
{
	if (t->async_thread_id && t->keep_thread_running)
	{
		t->keep_thread_running = FALSE;
		gdk_threads_leave();
		g_thread_join(t->async_thread_id);
		gdk_threads_enter();
		t->async_thread_id = NULL;
		append_status(t, _("Shutting down asynchronous thread\n"));
	}
}


static void
refresh_cameralist(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	shutdown_async_thread(t);
	closeconnection(t);
	gtk_list_store_clear(t->camera_store);
	int i = enumerate_cameras(t->camera_store, t->context);
	append_status(t, _("Found %d cameras\n"), i);
	if (i > 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(t->camera_selector), 0);
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(t->camera_selector), -1);

}

static void
connect_camera(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	shutdown_async_thread(t);
	closeconnection(t);
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter(t->camera_selector, &iter))
		initcamera(t,&iter);
	else
		append_status(t, _("No camera selected - Cannot connect!\n"));
}

static void
take_photo(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	gint ret_val;
	if (!t->camera)
		connect_camera(entry, user_data);
	if (!t->camera)
		return;

	if (t->keep_thread_running)
	{
		append_status(t, _("Shutting down running thread to enable remote capture.\n"));
		shutdown_async_thread(t);
	}

	ret_val = capture_to_file(t);
	if (ret_val < GP_OK)
	{
		append_status(t, _("Recieved error %d, while capturing image.\nError text is: %s\n"), ret_val, gp_result_as_string(ret_val));
		closeconnection(t);
	}
}

static void
tags_entry_changed(GtkEntry *entry, gpointer user_data)
{
	const gchar* tags = gtk_entry_get_text(GTK_ENTRY(entry));
	rs_conf_set_string("tether-tags-for-new-images", tags);
}

static void
spin_button_entry_changed(GtkEntry *entry, gpointer user_data)
{
	gdouble value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry));
	rs_conf_set_double(user_data, value);
}

static void
filename_entry_changed(GtkEntry *entry, gpointer user_data)
{
	CAMERA_FILENAME *filename = (CAMERA_FILENAME *) user_data;
	filename->filename = gtk_entry_get_text(entry);
	update_example(filename);
}

static void
start_monitor(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (!t->camera)
		connect_camera(entry, user_data);
	if (!t->camera)
		return;

	if ((t->async_thread_id || t->keep_thread_running) && t->thread_type != ASYNC_THREAD_TYPE_MONITOR)
	{
		append_status(t, _("Shutting down already running thread.\n"));
		shutdown_async_thread(t);
	}
	if (!t->async_thread_id || !t->keep_thread_running)
	{
		t->keep_thread_running = TRUE;
		append_status(t, _("Starting Monitor Thread.\n"));
		t->thread_type = ASYNC_THREAD_TYPE_MONITOR;
		t->async_thread_id = g_thread_create(start_thread_monitor, t, TRUE, NULL);
	}
	else
		append_status(t, _("Monitor Thread already running.\n"));

}

static void
close_button_pressed(GtkEntry *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	shutdown_async_thread(t);
	closeconnection(t);
	gp_context_unref(t->context);
	gtk_widget_destroy(t->window);
}

static void
close_main_window(GtkEntry *entry, GdkEvent *event, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	shutdown_async_thread(t);
	if (t->camera)
		closeconnection(t);
	gp_context_unref(t->context);
	gtk_widget_destroy(GTK_WIDGET(entry));
}

static void 
stop_interval_shooting(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (t->keep_thread_running && t->thread_type == ASYNC_THREAD_TYPE_INTERVAL)
	{
		append_status(t, _("Shutting down interval capture thread.\n"));
		shutdown_async_thread(t);
	}
	rs_preview_widget_quick_end(RS_PREVIEW_WIDGET(t->rs->preview)); 
}

static void 
disconnect_camera_action(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (!t->camera)
	{
		append_status(t, _("No camera connected.\n"));
		return;
	}
	shutdown_async_thread(t);
	closeconnection(t);
}

static void 
start_interval_shooting(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (!t->camera)
		connect_camera(entry, user_data);
	if (!t->camera)
		return;
	if (t->keep_thread_running)
		shutdown_async_thread(t);

	rs_preview_widget_quick_start(RS_PREVIEW_WIDGET(t->rs->preview), TRUE); 

	t->thread_type = ASYNC_THREAD_TYPE_INTERVAL;
	t->keep_thread_running = TRUE;
	append_status(t, _("Staring Interval Shooting Thread.\n"));
	g_signal_handler_disconnect(G_OBJECT(t->interval_toggle_button), t->interval_toggle_button_signal);
	t->interval_toggle_button_signal = g_signal_connect(G_OBJECT(t->interval_toggle_button), "clicked", G_CALLBACK(stop_interval_shooting), t);
	gtk_button_set_label(GTK_BUTTON(t->interval_toggle_button), _("Stop Shooting"));
	GTK_CATCHUP();
	t->async_thread_id = g_thread_create(start_thread_interval, t, TRUE, NULL);
}

static void 
build_tether_gui(TetherInfo *t)
{

	GtkWidget *button;
	GtkWidget* label;
	GtkBox *box, *h_box;
	GtkWidget *filename_hbox;
	GtkWidget *filename_label;
	GtkWidget *filename_chooser;
	GtkWidget *filename_entry;
	GtkWidget *check_button;

	GtkWidget *example_hbox;
	GtkWidget *example_label1;
	GtkWidget *example_label2;
	CAMERA_FILENAME *filename;

	GtkWidget *status_window;
	GtkWidget *status_textview;
	GtkWidget *tags_entry;
	GtkWidget *number_spin;

	/* A box to hold everything */
	GtkBox *main_box = GTK_BOX(gtk_vbox_new (FALSE, 7));

	/* A box for main constrols */
	box = GTK_BOX(gtk_vbox_new (FALSE, 5));

	label = gtk_label_new(_("Select camera:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC(label), 7,3);
	gtk_box_pack_start(box, label, FALSE, FALSE, 0);

	/* Camera */
	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));

	/* Camera selector box */
	GtkWidget *camera_selector = gtk_combo_box_new_with_model(GTK_TREE_MODEL(t->camera_store));
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (camera_selector), cell, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (camera_selector), cell, "text", NAME_COLUMN); 
	gtk_box_pack_start(h_box, camera_selector, TRUE, TRUE, 2);
	gtk_combo_box_set_active(GTK_COMBO_BOX(camera_selector), 0);
	t->camera_selector = GTK_COMBO_BOX(camera_selector);

	/* Refresh / Connect buttons */
	button = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(refresh_cameralist), t);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 1);

	/* Add this box */
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 5);

	/* "Take photo" & Monitor button */
	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	button = gtk_button_new_from_stock(GTK_STOCK_CONNECT);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(connect_camera), t);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 1);

	button = gtk_button_new_with_label(_("Take Photo"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(take_photo), t);
	gtk_button_set_alignment (GTK_BUTTON(button), 0.0, 0.5);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 5);

	button = gtk_button_new_with_label(_("Monitor Camera"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(start_monitor), t);
	gtk_button_set_alignment (GTK_BUTTON(button), 0.0, 0.5);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 5);

	button = gtk_button_new_with_label(_("Disconnect Camera"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(disconnect_camera_action), t);
	gtk_button_set_alignment (GTK_BUTTON(button), 0.0, 0.5);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 5);

	/* Add this box */
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 5);

	/* Status window */
	label = gtk_label_new(_("Status:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(box, label, FALSE, FALSE, 5);
	status_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(status_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	/* Status text */
	status_textview = gtk_text_view_new_with_buffer(t->status_buffer);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(status_textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(status_textview), FALSE);
	gtk_container_add ( GTK_CONTAINER(status_window), status_textview);
	gtk_box_pack_start(GTK_BOX(box), status_window, TRUE, FALSE, 0);
	t->status_textview = GTK_TEXT_VIEW(status_textview);

	/* Add main box */
	gtk_box_pack_start(GTK_BOX(main_box), gui_box(_("Master Control"), GTK_WIDGET(box), "tether_controls", TRUE), FALSE, FALSE, 0);

	/* FILENAME & TAGS */
	box = GTK_BOX(gtk_vbox_new (FALSE, 5));
		/* Filename template*/
	filename = g_new0(CAMERA_FILENAME, 1);
	filename_hbox = gtk_hbox_new(FALSE, 0);
	filename_label = gtk_label_new(_("Filename template:"));
	filename_chooser = rs_filename_chooser_button_new(NULL, "tether-export-filename");
	filename_entry = g_object_get_data(G_OBJECT(filename_chooser), "entry");
	g_signal_connect(filename_entry, "changed", G_CALLBACK(filename_entry_changed), filename);
	filename->filename = gtk_entry_get_text(GTK_ENTRY(filename_entry));

	gtk_misc_set_alignment(GTK_MISC(filename_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(filename_hbox), filename_label, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(filename_hbox), filename_chooser, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), filename_hbox, FALSE, TRUE, 0);

	/* Example filename */
	example_hbox = gtk_hbox_new(FALSE, 0);
	example_label1 = gtk_label_new(_("Filename example:"));
	example_label2 = gtk_label_new(NULL);
	filename->example_label = example_label2;

	gtk_misc_set_alignment(GTK_MISC(example_label1), 0.0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(example_label2), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(example_hbox), example_label1, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(example_hbox), example_label2, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), example_hbox, FALSE, TRUE, 0);
	update_example(filename);

	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	label = gtk_label_new(_("Tags for new images:"));
	gtk_box_pack_start(GTK_BOX(h_box), label, FALSE, TRUE, 5);

	tags_entry = gtk_entry_new();
	gchar* tags = rs_conf_get_string("tether-tags-for-new-images");
	if (tags)
		gtk_entry_set_text(GTK_ENTRY(tags_entry), tags);
	g_signal_connect(tags_entry, "changed", G_CALLBACK(tags_entry_changed), NULL);
	gtk_box_pack_start(GTK_BOX(h_box), tags_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(h_box), FALSE, TRUE, 0);


	/* Add filename& tags box */
	gtk_box_pack_start(GTK_BOX(main_box), gui_box(_("Filename &amp; Tags"), GTK_WIDGET(box), "tether_filename_tags", TRUE), FALSE, FALSE, 0);

	/* INTERVAL SHOOTING */
	box = GTK_BOX(gtk_vbox_new (FALSE, 2));

	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	label = gtk_label_new(_("Seconds between each shot:"));
	gtk_box_pack_start(h_box, label, FALSE, FALSE, 5);
	number_spin = gtk_spin_button_new_with_range(1.0, 24.0*60*60, 0.2);
	gdouble interval = 10.0;
	rs_conf_get_double("tether-interval-interval", &interval);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(number_spin), interval);
	g_signal_connect(number_spin, "changed", G_CALLBACK(spin_button_entry_changed), "tether-interval-interval");
	gtk_box_pack_start(h_box, number_spin, FALSE, FALSE, 7);
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 2);

	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	button = gtk_button_new_with_label(_("Start Shooting"));
	t->interval_toggle_button_signal = g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(start_interval_shooting), t);
	gtk_button_set_alignment (GTK_BUTTON(button), 0.0, 0.5);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 5);
	t->interval_toggle_button = button;
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 2);

	/* Add interval shooting box */
	gtk_box_pack_start(GTK_BOX(main_box), gui_box(_("Interval Shooting"), GTK_WIDGET(box), "tether_interval_shooting_box", TRUE), FALSE, FALSE, 0);


	/* PREFERENCES */
	box = GTK_BOX(gtk_vbox_new (FALSE, 5));

	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	check_button = checkbox_from_conf("tether-minimize-window", _("Minimize this window after capture"), TRUE);
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);

	check_button = checkbox_from_conf("tether-copy-current-settings", _("Copy settings from active to new image"), FALSE);
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 0);

	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	check_button = checkbox_from_conf("tether-open-image", _("Open new images after capture"), TRUE);
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);

	check_button = checkbox_from_conf("tether-quick-export", _("Quick Export"), FALSE);
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 0);
	
	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	check_button = checkbox_from_conf("tether-add-image", _("Add Image to Icon Bar"), TRUE);
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);

	check_button = checkbox_from_conf("tether-blank-screen", _("Blank Screen"), FALSE);
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 0);

	/* Add preferences box */
	gtk_box_pack_start(GTK_BOX(main_box), gui_box(_("Preferences"), GTK_WIDGET(box), "tether_preferences", TRUE), FALSE, FALSE, 0);

		/* Close button */
	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_button_pressed), t);
	gtk_box_pack_end(h_box, button, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(main_box), GTK_WIDGET(h_box), FALSE, FALSE, 5);

	/* All all to window */
	gtk_container_set_border_width (GTK_CONTAINER(t->window), 5);
	gtk_container_add(GTK_CONTAINER(t->window), GTK_WIDGET(main_box));

}


void
rs_tethered_shooting_open(RS_BLOB *rs) 
{
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Rawstudio Tethered Shooting"));
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(rs->window));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
	gchar* filename_template = rs_conf_get_string("tether-export-filename");

	/* Initialize filename_template to default if nothing is saved in config */
	if (!filename_template)
		rs_conf_set_string("tether-export-filename","Rawstudio_%2c");
	else
		g_free(filename_template);

	TetherInfo *tether_info = NULL;

	if (tether_info == NULL)
	{
		tether_info = g_malloc0(sizeof(TetherInfo));
	}
	tether_info->window = window;
	tether_info->rs = rs;
	tether_info->status_buffer = gtk_text_buffer_new(NULL);
	tether_info->keep_thread_running = FALSE;
	tether_info->thread_type = ASYNC_THREAD_TYPE_NONE;

	gtk_text_buffer_set_text(tether_info->status_buffer,_("Welcome to Tethered shooting!\nMake sure your camera is NOT mounted in your operating system.\n"),-1);
	g_signal_connect(window, "delete-event", G_CALLBACK(close_main_window), tether_info);

	/* Initialize context */
	tether_info->context = gp_context_new();
	gp_context_set_error_func (tether_info->context, ctx_error_func, tether_info);
	gp_context_set_status_func (tether_info->context, ctx_status_func, tether_info);	

	/* Enumerate cameras */
	tether_info->camera_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	int i = enumerate_cameras(tether_info->camera_store, tether_info->context);

	append_status(tether_info, _("Found %d cameras\n"), i);

	build_tether_gui(tether_info);
	gtk_window_resize(GTK_WINDOW(window), 500, 400);
	gtk_widget_show_all(GTK_WIDGET(window));
}

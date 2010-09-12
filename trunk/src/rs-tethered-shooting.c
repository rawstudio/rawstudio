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

/*
 * The following functions is more or less grabbed from UFraw:
 * lens_set(), lens_menu_select(), ptr_array_insert_sorted(),
 * ptr_array_find_sorted(), ptr_array_insert_index() and lens_menu_fill()
 */

#include <rawstudio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <config.h>
#include <gettext.h>
#include "rs-tethered-shooting.h"
#include <gphoto2/gphoto2-camera.h>
#include <stdlib.h>
#include <fcntl.h>
#include "filename.h"
#include <rs-store.h>
#ifdef WITH_GCONF
#include <gconf/gconf-client.h>
#endif
#include "conf_interface.h"
#include "gtk-helper.h"

enum
{
  NAME_COLUMN,
  VALUE_COLUMN,
  N_COLUMNS
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
	GThread *monitor_thread_id;
	GMutex *monitor_mutex;
	gboolean keep_monitor_running;
} TetherInfo;

typedef struct {
	GtkWidget *example_label;
	GtkWidget *event;
	const gchar *output_type;
	const gchar *filename;
} CAMERA_FILENAME;

static void shutdown_monitor(TetherInfo *t);
static void closeconnection(TetherInfo *t);


static void
append_status_va_list(TetherInfo *t, const gchar *format, va_list args)
{
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
	TetherInfo *t = (TetherInfo*)data;
	append_status (t, "*** Contexterror ***\n");
	append_status_va_list(t, format, args);
	append_status  (t, "\n");
	shutdown_monitor(t);
	if (t->camera)
		closeconnection(t);

}

static void
ctx_status_func (GPContext *context, const char *format, va_list args, void *data)
{
	TetherInfo *t = (TetherInfo*)data;
	append_status_va_list(t, format, args);
	append_status  (t, "\n");
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
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This is the actual query call. Note that we just
	 * a pointer reference to the string, not a copy... */
	ret = gp_widget_get_value (child, &val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not query widget value: %d\n", ret);
		goto out;
	}
	/* Create a new copy for our caller. */
	*str = strdup (val);
out:
	gp_widget_free (widget);
	return ret;
}

#define CHECKRETVAL(A) if (A < GP_OK) {\
	append_status(t, "ERROR: Gphoto2 returned error value %d\nTranslated error message is: %s\n", A, gp_result_as_string(A));\
	gp_camera_free (t->camera);\
	t->camera = NULL;\
	return A;}

gpointer start_thread_monitor(gpointer _thread_info);

static int
enable_capture(TetherInfo *t) 
{
  int retval;

	append_status(t, "Enabling capture mode\n");

  printf("Get root config.\n");
  CameraWidget *rootconfig; // okay, not really
  CameraWidget *actualrootconfig;

  retval = gp_camera_get_config(t->camera, &rootconfig, t->context);
	CHECKRETVAL(retval);
  actualrootconfig = rootconfig;

  printf("Get main config.\n");
  CameraWidget *child;
  retval = gp_widget_get_child_by_name(rootconfig, "main", &child);
	CHECKRETVAL(retval);

  printf("Get settings config.\n");
  rootconfig = child;
  retval = gp_widget_get_child_by_name(rootconfig, "settings", &child);
	CHECKRETVAL(retval);

  printf("Get capture config.\n");
  rootconfig = child;
  retval = gp_widget_get_child_by_name(rootconfig, "capture", &child);
	CHECKRETVAL(retval);

  CameraWidget *capture = child;

  const char *widgetinfo;
  gp_widget_get_name(capture, &widgetinfo);
  printf("config name: %s\n", widgetinfo );

  const char *widgetlabel;
  gp_widget_get_label(capture, &widgetlabel);
  printf("config label: %s\n", widgetlabel);

  int widgetid;
  gp_widget_get_id(capture, &widgetid);
  printf("config id: %d\n", widgetid);

  CameraWidgetType widgettype;
  gp_widget_get_type(capture, &widgettype);
  printf("config type: %d == %d \n", widgettype, GP_WIDGET_TOGGLE);

  int one=1;
  retval = gp_widget_set_value(capture, &one);
	CHECKRETVAL(retval);

  printf("Enabling capture.\n");
  retval = gp_camera_set_config(t->camera, actualrootconfig, t->context);
	CHECKRETVAL(retval);

  printf("Capture Enabled.\n");
	append_status(t, "Capture Enabled.\n");

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
	m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK)
		return ret;

	ret = gp_abilities_list_get_abilities (abilities, m, &a);
	CHECKRETVAL(ret);

	ret = gp_camera_set_abilities (*camera, a);
	CHECKRETVAL(ret);

	/* Then associate the camera with the specified port */
	p = gp_port_info_list_lookup_path (portinfolist, port);
	CHECKRETVAL(ret);
        switch (p) {
        case GP_ERROR_UNKNOWN_PORT:
                append_status (t, "The port you specified "
                        "('%s') can not be found. Please "
                        "specify one of the ports found by "
                        "'gphoto2 --list-ports' and make "
                        "sure the spelling is correct "
                        "(i.e. with prefix 'serial:' or 'usb:').",
                                port);
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



static void
add_file_to_store(TetherInfo* t, const char* tmp_name) 
{
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

	if (!g_file_move(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL))
	{
		append_status(t, "Moving file to current directory failed!\n");
		return;
	}
	g_object_unref(src);
	g_object_unref(dst);

	rs_store_set_iconview_size(t->rs->store, rs_store_get_iconview_size(t->rs->store)+1);
	rs_store_load_file(t->rs->store, filename);
	if (!rs_store_set_selected_name(t->rs->store, filename, TRUE))
		append_status(t, "Could not open image!\n");
}

static gint
transfer_file_captured(TetherInfo* t, CameraFilePath* camera_file_path) 
{
	CameraFile *canonfile;
	int fd, retval;
	append_status(t,"Downloading and adding image.\n");
	char tmp_name[L_tmpnam];
	char *tmp_name_ptr;
	tmp_name_ptr = tmpnam(tmp_name);

	if (NULL == tmp_name_ptr)
		return -1;

	char *extension = g_strrstr(camera_file_path->name, ".");
	tmp_name_ptr = g_strconcat(tmp_name_ptr, extension, NULL);

	fd = open(tmp_name_ptr, O_CREAT | O_WRONLY, 0644);
	if (fd == -1)
	{
		append_status(t,"Could not open temporary file on disk for writing");
		return -1;
	}

	retval = gp_file_new_from_fd(&canonfile, fd);
	CHECKRETVAL(retval);
	retval = gp_camera_file_get(t->camera, camera_file_path->folder, camera_file_path->name, GP_FILE_TYPE_NORMAL, canonfile, t->context);
	CHECKRETVAL(retval);
	retval = gp_camera_file_delete(t->camera, camera_file_path->folder, camera_file_path->name, t->context);
	CHECKRETVAL(retval);

	gp_file_free(canonfile);
	add_file_to_store(t, tmp_name_ptr);
	
	gboolean minimize = TRUE;
	rs_conf_get_boolean_with_default("tether-minimize-window", &minimize, TRUE);
	if (minimize)
		gtk_window_iconify(GTK_WINDOW(t->window));

	g_free(tmp_name_ptr);
	return GP_OK;
}

static gint
capture_to_file(TetherInfo* t) 
{
	int retval;
	CameraFilePath camera_file_path;

	append_status(t, "Capturing.\n");
	retval = gp_camera_capture(t->camera, GP_CAPTURE_IMAGE, &camera_file_path, t->context);
	CHECKRETVAL(retval);
	retval = transfer_file_captured(t, &camera_file_path);
	return GP_OK;
}

gpointer
start_thread_monitor(gpointer _thread_info)
{
	TetherInfo *t = (TetherInfo*) _thread_info;
	g_mutex_lock(t->monitor_mutex);
	int retval;
	while (t->keep_monitor_running)
	{
		Camera *cam = t->camera;
		CameraEventType type;
		void * event_data = NULL;
		g_mutex_unlock(t->monitor_mutex);
		retval = gp_camera_wait_for_event(cam, 100, &type, &event_data, t->context);
		g_mutex_lock(t->monitor_mutex);
		if (retval < GP_OK)
		{
			gdk_threads_enter();
			append_status(t, "Monitor recieved error %d, while waiting for camera.\nError text is: %s", retval, gp_result_as_string(retval));
			gdk_threads_leave();
			t->keep_monitor_running = FALSE;
		}
		else
		{
			if (type == GP_EVENT_FILE_ADDED)
			{
				gdk_threads_enter();
				CameraFilePath* camera_file_path = (CameraFilePath*)event_data;
				transfer_file_captured(t, camera_file_path);
				gdk_threads_leave();
			}
		}
	}
	t->keep_monitor_running = FALSE;
	g_mutex_unlock(t->monitor_mutex);
	return NULL;
}

static void closeconnection(TetherInfo *t)
{
	shutdown_monitor(t);
	if (!t->camera)
		return;
	append_status(t, "Disconnecting current camera\n");
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
		append_status(t,"Camera %s on port %s failed to open\n", name, value);
		return;
	}
	
	ret = gp_camera_init (t->camera, t->context);
	if (ret < GP_OK) {
		append_status(t,"ERROR: Init camera returned %d.\nError text is:%s\n", ret, gp_result_as_string(ret));
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

static void
shutdown_monitor(TetherInfo *t)
{
	g_mutex_lock(t->monitor_mutex);
	if (t->monitor_thread_id && t->keep_monitor_running)
	{
		t->keep_monitor_running = FALSE;
		g_mutex_unlock(t->monitor_mutex);
		g_thread_join(t->monitor_thread_id);
		t->monitor_thread_id = NULL;
		append_status(t, "Monitor shut down\n");
	}
	else 
		g_mutex_unlock(t->monitor_mutex);
}


static void
refresh_cameralist(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (t->camera)
		closeconnection(t);
	gtk_list_store_clear(t->camera_store);
	int i = enumerate_cameras(t->camera_store, t->context);
	append_status(t, "Found %d cameras\n", i);
	if (i > 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(t->camera_selector), 0);
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(t->camera_selector), -1);

}

static void
connect_camera(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (t->camera)
		closeconnection(t);
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter(t->camera_selector, &iter))
		initcamera(t,&iter);
	else
		append_status(t, "No camera selected - Cannot connect!\n");
}

static void
take_photo(GObject *entry, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (!t->camera)
		connect_camera(entry, user_data);
	if (!t->camera)
		return;

	if (t->keep_monitor_running)
	{
		append_status(t, "Shutting down monitor thread to enable remote capture.\n");
		shutdown_monitor(t);
	}

	capture_to_file(t);
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
	g_mutex_lock(t->monitor_mutex);
	if (!t->monitor_thread_id || !t->keep_monitor_running)
	{
		t->keep_monitor_running = TRUE;
		append_status(t, "Staring Monitor Thread.\n");
		t->monitor_thread_id = g_thread_create(start_thread_monitor, t, TRUE, NULL);
	}
	else
		append_status(t, "Monitor Thread already running.\n");

	g_mutex_unlock(t->monitor_mutex);
}

static void
close_main_window(GtkEntry *entry, gint response_id, gpointer user_data)
{
	TetherInfo *t = (TetherInfo*)user_data;
	if (t->camera)
		closeconnection(t);
	gp_context_unref(t->context);
	gtk_widget_destroy(GTK_WIDGET(entry));
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

	button = gtk_button_new_from_stock(GTK_STOCK_CONNECT);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(connect_camera), t);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 1);

	/* Add this box */
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 5);

	/* "Take photo" & Monitor button */
	h_box = GTK_BOX(gtk_hbox_new (FALSE, 0));
	button = gtk_button_new_with_label(_("Take Photo"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(take_photo), t);
	gtk_button_set_alignment (GTK_BUTTON(button), 0.0, 0.5);
	gtk_box_pack_start(h_box, button, FALSE, FALSE, 5);

	button = gtk_button_new_with_label(_("Monitor Camera"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(start_monitor), t);
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

		/* PREFERENCES */
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
	check_button = checkbox_from_conf("tether-minimize-window", _("Minimize this window after capture"), TRUE);
	//gtk_check_button_new_with_label(_("Minimize this window after capture"));
	gtk_button_set_alignment (GTK_BUTTON(check_button), 0.0, 0.5);
	gtk_box_pack_start(h_box, check_button, FALSE, FALSE, 5);
	gtk_box_pack_start(box, GTK_WIDGET(h_box), FALSE, FALSE, 5);

	/* Add preferences box */
	gtk_box_pack_start(GTK_BOX(main_box), gui_box(_("Preferences"), GTK_WIDGET(box), "tether_preferences", TRUE), FALSE, FALSE, 0);

	/* All all to window */
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG(t->window)->vbox), GTK_WIDGET(main_box), TRUE, TRUE, 0);

	GtkWidget *button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (t->window), button_close, GTK_RESPONSE_CLOSE);

}


void
rs_tethered_shooting_open(RS_BLOB *rs) 
{
	GtkWidget *window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(window), _("Rawstudio Tethered Shooting"));
	gtk_dialog_set_has_separator (GTK_DIALOG(window), FALSE);
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
	tether_info->monitor_mutex = g_mutex_new ();
	tether_info->keep_monitor_running = FALSE;
	gtk_text_buffer_set_text(tether_info->status_buffer,_("Welcome to Tethered shooting!\nMake sure your camera is NOT mounted in your operating system.\n"),-1);
	g_signal_connect(window, "response", G_CALLBACK(close_main_window), tether_info);

	/* Initialize context */
	tether_info->context = gp_context_new();
	gp_context_set_error_func (tether_info->context, ctx_error_func, tether_info);
	gp_context_set_status_func (tether_info->context, ctx_status_func, tether_info);	

	/* Enumerate cameras */
	tether_info->camera_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	int i = enumerate_cameras(tether_info->camera_store, tether_info->context);

	append_status(tether_info, "Found %d cameras\n", i);

	build_tether_gui(tether_info);
	gtk_window_resize(GTK_WINDOW(window), 500, 400);
	gtk_widget_show_all(GTK_WIDGET(window));
}

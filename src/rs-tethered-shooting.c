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

typedef struct {
	Camera		*camera;
	GPContext	*context;
	GtkWidget *window;
	RS_BLOB *rs;
} TetherInfo;

static TetherInfo *tether_info = NULL;

static void
ctx_error_func (GPContext *context, const char *format, va_list args, void *data)
{
        fprintf  (stderr, "\n");
        fprintf  (stderr, "*** Contexterror ***              \n");
        vfprintf (stderr, format, args);
        fprintf  (stderr, "\n");
        fflush   (stderr);
}

static void
ctx_status_func (GPContext *context, const char *format, va_list args, void *data)
{
        vfprintf (stderr, format, args);
        fprintf  (stderr, "\n");
        fflush   (stderr);
}

int
enumerate_cameras(CameraList *list, GPContext *context) {
	int			ret, i;
	CameraList		*xlist = NULL;
	GPPortInfoList		*portinfolist = NULL;
	CameraAbilitiesList	*abilities = NULL;

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
		gp_list_append (list, name, value);
	}
out:
	gp_list_free (xlist);
	return gp_list_count(list);
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

static int
open_camera (Camera ** camera, const char *model, const char *port) {
	int		ret, m, p;
	CameraAbilities	a;
	GPPortInfo	pi;
	GPPortInfoList		*portinfolist = NULL;
	CameraAbilitiesList	*abilities = NULL;

	ret = gp_camera_new (camera);
	if (ret < GP_OK) return ret;

	/* First lookup the model / driver */
        m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK) return ret;
        ret = gp_abilities_list_get_abilities (abilities, m, &a);
	if (ret < GP_OK) return ret;
        ret = gp_camera_set_abilities (*camera, a);
	if (ret < GP_OK) return ret;

	/* Then associate the camera with the specified port */
        p = gp_port_info_list_lookup_path (portinfolist, port);
        if (ret < GP_OK) return ret;
        switch (p) {
        case GP_ERROR_UNKNOWN_PORT:
                fprintf (stderr, "The port you specified "
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
        if (ret < GP_OK) return ret;
        ret = gp_port_info_list_get_info (portinfolist, p, &pi);
        if (ret < GP_OK) return ret;
        ret = gp_camera_set_port_info (*camera, pi);
        if (ret < GP_OK) return ret;
	return GP_OK;
}


static void enable_capture(TetherInfo *t) {
  int retval;

  printf("Get root config.\n");
  CameraWidget *rootconfig; // okay, not really
  CameraWidget *actualrootconfig;

  retval = gp_camera_get_config(t->camera, &rootconfig, t->context);
  actualrootconfig = rootconfig;

  printf("Get main config.\n");
  CameraWidget *child;
  retval = gp_widget_get_child_by_name(rootconfig, "main", &child);

  printf("Get settings config.\n");
  rootconfig = child;
  retval = gp_widget_get_child_by_name(rootconfig, "settings", &child);

  printf("Get capture config.\n");
  rootconfig = child;
  retval = gp_widget_get_child_by_name(rootconfig, "capture", &child);

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

  printf("Enabling capture.\n");
  retval = gp_camera_set_config(t->camera, actualrootconfig, t->context);
}

static void
add_file_to_store(TetherInfo* t, const char* tmp_name) 
{
	gchar *lwd;
	lwd = rs_conf_get_string(CONF_LWD);
	GString *filename_template = g_string_new(lwd);
	g_string_append(filename_template, G_DIR_SEPARATOR_S);
	g_string_append(filename_template, "Rawstudio_%2c.cr2");
	gchar* filename = filename_parse(g_string_free(filename_template, FALSE),tmp_name, 0);

	GFile* src = g_file_new_for_path(tmp_name);
	GFile* dst = g_file_new_for_path(filename);

	if (!g_file_move(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL))
	{
		printf("Move failed!\n");
		return;
	}
	g_object_unref(src);
	g_object_unref(dst);

	rs_store_load_file(t->rs->store, filename);
	if (!rs_store_set_selected_name(t->rs->store, filename, TRUE))
		printf("Could not open image!\n");
}

static void
capture_to_file(TetherInfo* t) 
{
	int fd, retval;
	CameraFile *canonfile;
	CameraFilePath camera_file_path;

			/* Generate a temporary name */
	/* The reason for using a temporary file is that we need to read the */
	/* metadata before we can generate a filename */
	char tmp_name[L_tmpnam];
	char *tmp_name_ptr;
	tmp_name_ptr = tmpnam(tmp_name);
	
	if (NULL == tmp_name_ptr)
		return;

	printf("Capturing.\n");

	/* NOP: This gets overridden in the library to /capt0000.jpg */
	strcpy(camera_file_path.folder, "/");
	strcpy(camera_file_path.name, "foo.jpg");

	retval = gp_camera_capture(t->camera, GP_CAPTURE_IMAGE, &camera_file_path, t->context);
	printf("  Retval: %d\n", retval);

	fd = open(tmp_name_ptr, O_CREAT | O_WRONLY, 0644);
	retval = gp_file_new_from_fd(&canonfile, fd);
	retval = gp_camera_file_get(t->camera, camera_file_path.folder, camera_file_path.name,
		     GP_FILE_TYPE_NORMAL, canonfile, t->context);
	retval = gp_camera_file_delete(t->camera, camera_file_path.folder, camera_file_path.name,
			t->context);

	gp_file_free(canonfile);
	add_file_to_store(t, tmp_name_ptr);
}


static void closeconnection(TetherInfo *t)
{
	gp_camera_exit (t->camera, t->context);
	gp_camera_free (t->camera);
}

static void initcamera(TetherInfo *t)
{
	gint ret;
	t->context = gp_context_new();
	gp_context_set_error_func (t->context, ctx_error_func, NULL);
	gp_context_set_status_func (t->context, ctx_status_func, NULL);	

	CameraList* list;
	ret = gp_list_new(&list);
	int i = enumerate_cameras(list, t->context);

	printf("Found %d cameras\n", i);
	if (i < 1)
		return;

	/* This call will autodetect cameras, take the
	 * first one from the list and use it. It will ignore
	 * any others... See the *multi* examples on how to
	 * detect and use more than the first one.
	 */
	const char	*name, *value;

	gp_list_get_name  (list, 0, &name);
	gp_list_get_value (list, 0, &value);

	ret = open_camera(&t->camera, name, value);
	if (ret < GP_OK) 
		fprintf(stderr,"Camera %s on port %s failed to open\n", name, value);
	
	ret = gp_camera_init (t->camera, t->context);
	if (ret < GP_OK) {
		printf("After init:returned %d.\n", ret);
		gp_camera_free (t->camera);
		return;
	}

	CameraText	text;
		/* Simple query the camera summary text */
	ret = gp_camera_get_summary (t->camera, &text, t->context);
	if (ret < GP_OK) {
		printf("Camera failed retrieving summary.\n");
		gp_camera_free (t->camera);
		return;
	}
	printf("Summary:\n%s\n", text.text);

	char	*owner;
	/* Simple query of a string configuration variable. */
	ret = get_config_value_string (t->camera, "owner", &owner, t->context);
	if (ret >= GP_OK) {
		printf("Owner: %s\n", owner);
		free(owner);
	}
}

void
rs_tethered_shooting_open(RS_BLOB *rs) 
{
	GtkWidget *window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(window), _("Rawstudio Tethered Shooting"));
	gtk_dialog_set_has_separator (GTK_DIALOG(window), FALSE);
	g_signal_connect_swapped(window, "delete_event",
													G_CALLBACK (gtk_widget_destroy), window);
													g_signal_connect_swapped(window, "response", 
													G_CALLBACK (gtk_widget_destroy), window);

	gtk_window_resize(GTK_WINDOW(window), 400, 400);

//	GtkWidget *frame = gtk_frame_new("");
//	gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->vbox), frame, TRUE, TRUE, 0);

	GtkWidget *button_close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (window), button_close, GTK_RESPONSE_CLOSE);

	gtk_widget_show_all(GTK_WIDGET(window));

	if (tether_info == NULL)
	{
		tether_info = g_malloc0(sizeof(TetherInfo));
	}
	tether_info->window = window;
	tether_info->rs = rs;
	initcamera(tether_info);
	enable_capture(tether_info);
	capture_to_file(tether_info);
	closeconnection(tether_info);
}

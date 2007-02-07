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

#include <glib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "rawstudio.h"
#include "rs-batch.h"
#include "conf_interface.h"
#include "gettext.h"
#include "gtk-helper.h"
#include "filename.h"
#include "rs-cache.h"
#include "rs-render.h"
#include "rs-image.h"

extern GtkWindow *rawstudio_window;

static gboolean batch_exists_in_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);
static GtkWidget *make_batchview(RS_QUEUE *queue);
static GtkSpinButton *size_spin;
static GtkWidget *size_label;

RS_QUEUE* rs_batch_new_queue(void)
{
	RS_QUEUE *queue = g_new(RS_QUEUE, 1);
	RS_FILETYPE *filetype;

	queue->list = GTK_TREE_MODEL(gtk_list_store_new(6, G_TYPE_STRING,G_TYPE_STRING,
									G_TYPE_INT,G_TYPE_STRING,
									G_TYPE_POINTER, GDK_TYPE_PIXBUF));


	queue->directory = rs_conf_get_string(CONF_BATCH_DIRECTORY);
	if (queue->directory == NULL)
	{
		rs_conf_set_string(CONF_BATCH_DIRECTORY, DEFAULT_CONF_BATCH_DIRECTORY);
		queue->directory = rs_conf_get_string(CONF_BATCH_DIRECTORY);
	}

	queue->filename = rs_conf_get_string(CONF_BATCH_FILENAME);
	if (queue->filename == NULL)
	{
		rs_conf_set_string(CONF_BATCH_FILENAME, DEFAULT_CONF_BATCH_FILENAME);
		queue->filename = rs_conf_get_string(CONF_BATCH_FILENAME);
	}

	rs_conf_get_filetype(CONF_BATCH_FILETYPE, &filetype);
	queue->filetype = filetype->filetype;
	queue->size_lock = LOCK_SCALE;
	queue->size = 100;

	return queue;
}

gboolean
rs_batch_add_element_to_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element)
{
	if (!batch_exists_in_queue(queue, element->filename, element->setting_id))
	{
		gchar *filename_short, *setting_id_abc;
		RS_FILETYPE *filetype;
		GdkPixbuf *pixbuf = NULL, *missing_thumb, *pixbuf_temp;

		filename_short = g_path_get_basename(element->filename);

		switch(element->setting_id)
		{
			case 0:
				setting_id_abc = _("A");
				break;
			case 1:
				setting_id_abc = _("B");
				break;
			case 2:
				setting_id_abc = _("C");
				break;
			default:
				return FALSE;
		}

		filetype = rs_filetype_get(element->filename, TRUE);
		if (filetype)
		{
			missing_thumb = gtk_widget_render_icon(GTK_WIDGET(rawstudio_window),
				GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG, NULL);

			if (filetype->thumb)
				pixbuf = filetype->thumb(element->filename);
			if (pixbuf)
			{
				gint w,h,temp,size = 48;

				w = gdk_pixbuf_get_width(pixbuf);
				h = gdk_pixbuf_get_height(pixbuf);

				if (w > h)
				{
					temp = 1000*h/w;
					pixbuf_temp = gdk_pixbuf_scale_simple(pixbuf, size, size*temp/1000, GDK_INTERP_BILINEAR);
					g_object_unref(pixbuf);
					pixbuf = pixbuf_temp;
				}
				else
				{
					temp = 1000*w/h;
					pixbuf_temp = gdk_pixbuf_scale_simple(pixbuf, size*temp/1000, size, GDK_INTERP_BILINEAR);
					g_object_unref(pixbuf);
					pixbuf = pixbuf_temp;
				}
			}
			else
			{
				pixbuf = missing_thumb;
				g_object_ref (pixbuf);
			}
		}

		rs_batch_add_to_queue(queue, element->filename, filename_short, element->setting_id, setting_id_abc, element, pixbuf);

		g_free(element);
		return TRUE;
	}
	else
	{
		g_free(element);
		return FALSE;
	}
}

gboolean
rs_batch_remove_element_from_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element)
{
	return rs_batch_remove_from_queue(queue, element->filename, element->setting_id);
	g_free(element);
}

RS_QUEUE_ELEMENT*
rs_batch_get_first_element_in_queue(RS_QUEUE *queue)
{
	GtkTreeIter iter;
	gchar *filename_temp;
	gint setting_id_temp;

	if (gtk_tree_model_get_iter_first(queue->list, &iter))
	{	
		if (gtk_list_store_iter_is_valid(GTK_LIST_STORE(queue->list), &iter)) 
		{
			RS_QUEUE_ELEMENT *element = g_new(RS_QUEUE_ELEMENT,1);

			gtk_tree_model_get(queue->list, &iter,
					RS_QUEUE_ELEMENT_FILENAME, &filename_temp,
					RS_QUEUE_ELEMENT_SETTING_ID, &setting_id_temp,
					-1);

			element->filename = filename_temp;
			element->setting_id = setting_id_temp;

			return element;
		}
		else
			return NULL;
	}
	else
		return NULL;
}

gboolean
rs_batch_add_to_queue(RS_QUEUE *queue, const gchar *filename, 
						const gchar *filename_short, gint setting_id, 
						const gchar *setting_id_abc, RS_QUEUE_ELEMENT *element,
						GdkPixbuf *thumbnail)
{
	if (!batch_exists_in_queue(queue, filename, setting_id))
	{
		GtkTreeIter iter;
		
		gtk_list_store_append (GTK_LIST_STORE(queue->list), &iter);
 		gtk_list_store_set (GTK_LIST_STORE(queue->list), &iter,
 					RS_QUEUE_ELEMENT_FILENAME, filename,
					RS_QUEUE_ELEMENT_FILENAME_SHORT, filename_short,
 					RS_QUEUE_ELEMENT_SETTING_ID, setting_id,
					RS_QUEUE_ELEMENT_SETTING_ID_ABC, setting_id_abc,
					RS_QUEUE_ELEMENT_ELEMENT, element,
					RS_QUEUE_ELEMENT_THUMBNAIL, thumbnail,
 					-1);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean
rs_batch_remove_from_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id)
{
	GtkTreeIter iter;

	gchar *filename_temp = "init";
	gint setting_id_temp;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(queue->list), &iter);

	if (gtk_list_store_iter_is_valid(GTK_LIST_STORE(queue->list), &iter))
	{
		do
		{
			gtk_tree_model_get(queue->list, &iter,
				RS_QUEUE_ELEMENT_FILENAME, &filename_temp,
				RS_QUEUE_ELEMENT_SETTING_ID, &setting_id_temp,
				-1);

			if (g_str_equal(filename, filename_temp))
			{
				if (setting_id == setting_id_temp)
				{
					gtk_list_store_remove(GTK_LIST_STORE(queue->list), &iter); /* FIXME: returns false even though the iter is valid and it removes correctly */
					return TRUE;
				}
			}
		} while (gtk_tree_model_iter_next(queue->list, &iter));
		return FALSE;
	}
	else
		return FALSE;
}

static gboolean
batch_exists_in_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id)
{
	GtkTreeIter iter;

	gchar *filename_temp;
	gint setting_id_temp;
	
	gtk_tree_model_get_iter_first(queue->list, &iter);

	if (gtk_list_store_iter_is_valid(GTK_LIST_STORE(queue->list), &iter))
	{
		do
		{
			gtk_tree_model_get(queue->list, &iter,
				RS_QUEUE_ELEMENT_FILENAME, &filename_temp,
				RS_QUEUE_ELEMENT_SETTING_ID, &setting_id_temp,
				-1);

			if (g_str_equal(filename, filename_temp))
			{
				if (setting_id == setting_id_temp)
					return TRUE;
			}
		} while (gtk_tree_model_iter_next(queue->list, &iter));
		return FALSE;
	}
	else
		return FALSE;
}

static gboolean
window_destroy(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gboolean *abort_render = (gboolean *) user_data;
	*abort_render = TRUE;
	return(TRUE);
}

static void
cancel_clicked(GtkButton *button, gpointer user_data)
{
	gboolean *abort_render = (gboolean *) user_data;
	*abort_render = TRUE;
	return;
}

void
rs_batch_process(RS_QUEUE *queue)
{
	RS_QUEUE_ELEMENT *e;
	RS_PHOTO *photo = NULL;
	RS_FILETYPE *filetype;
	RS_IMAGE16 *image;
	GtkWidget *preview = gtk_image_new();
	GdkPixbuf *pixbuf = NULL;
	gint width = -1, height = -1;
	gdouble scale = -1.0;
	gchar *parsed_filename, *basename;
	GString *filename;
	GString *status = g_string_new(NULL);
	GtkWidget *window;
	GtkWidget *label = gtk_label_new(NULL);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 4);
	GtkWidget *cancel;
	gboolean abort_render = FALSE;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(window), rawstudio_window);
	gtk_window_set_title(GTK_WINDOW(window), _("Processing photos"));
	gtk_window_resize(GTK_WINDOW(window), 250, 250);
	g_signal_connect((gpointer) window, "delete_event", G_CALLBACK(window_destroy), &abort_render);

	cancel = gtk_button_new_with_label(_("Cancel"));
	g_signal_connect (G_OBJECT(cancel), "clicked",
		G_CALLBACK(cancel_clicked), &abort_render);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), gui_framed(preview, _("Last image:"), GTK_SHADOW_IN), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), cancel, FALSE, FALSE, 0);

	gtk_widget_hide(GTK_WIDGET(rawstudio_window));
	gtk_widget_show_all(window);
	while (gtk_events_pending()) gtk_main_iteration();

	g_mkdir_with_parents(queue->directory, 00755);
	while((e = rs_batch_get_first_element_in_queue(queue)) && (!abort_render))
	{
		if ((filetype = rs_filetype_get(e->filename, TRUE)))
		{
			basename = g_path_get_basename(e->filename);
			g_string_printf(status, _("Loading %s ..."), basename);
			gtk_label_set_text(GTK_LABEL(label), status->str);
			while (gtk_events_pending()) gtk_main_iteration();
			g_free(basename);

			photo = filetype->load(e->filename);
			if (photo)
			{
				if (filetype->load_meta)
					filetype->load_meta(e->filename, photo->metadata);
				filename = g_string_new(queue->directory);
				g_string_append(filename, G_DIR_SEPARATOR_S);
				g_string_append(filename, queue->filename);
				g_string_append(filename, ".");

				switch (queue->filetype)
				{
					case FILETYPE_JPEG:
						g_string_append(filename, "jpg");
						break;
					case FILETYPE_PNG:
						g_string_append(filename, "png");
						break;
					case FILETYPE_TIFF8:
						g_string_append(filename, "tif");
						break;
					case FILETYPE_TIFF16:
						g_string_append(filename, "tif");
						break;
				}

				parsed_filename = filename_parse(filename->str, photo);
				rs_cache_load(photo);
				rs_photo_prepare(photo);

				switch (queue->size_lock)
				{
					case LOCK_SCALE:
						scale = gtk_spin_button_get_value(size_spin)/100.0;
						break;
					case LOCK_WIDTH:
						width = (gint) gtk_spin_button_get_value(size_spin);
						break;
					case LOCK_HEIGHT:
						height = (gint) gtk_spin_button_get_value(size_spin);
						break;
				}

				image = rs_image16_transform(photo->input, NULL,
					NULL, NULL, photo->crop, 200, 200, TRUE, -1.0,
					photo->angle, photo->orientation);
				if (pixbuf) g_object_unref(pixbuf);
				pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, image->w, image->h);
				rs_render(photo, image->w, image->h, image->pixels,
					image->rowstride, image->channels,
					gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_rowstride(pixbuf),
					rs_cms_get_transform(queue->cms, TRANSFORM_DISPLAY));
				gtk_image_set_from_pixbuf((GtkImage *) preview, pixbuf);
				rs_image16_free(image);

				basename = g_path_get_basename(parsed_filename);
				g_string_printf(status, _("Saving %s ..."), basename);
				gtk_label_set_text(GTK_LABEL(label), status->str);
				while (gtk_events_pending()) gtk_main_iteration();
				g_free(basename);

				rs_photo_save(photo, parsed_filename, queue->filetype,
					width, height, scale, queue->cms);
				g_free(parsed_filename);
				g_string_free(filename, TRUE);
				rs_photo_free(photo);
			}
			photo = NULL;
		}
		rs_batch_remove_element_from_queue(queue, e);
	}
	gtk_widget_destroy(window);
	gtk_widget_show_all(GTK_WIDGET(rawstudio_window));
}

void
rs_batch_start_queue(RS_QUEUE *queue)
{
	rs_batch_process(queue);
	return;
}

static GtkWidget *
make_batchview(RS_QUEUE *queue)
{
	GtkWidget *scroller;
	GtkWidget *view;
	GtkCellRenderer *renderer_text, *renderer_pixbuf;
	GtkTreeViewColumn *column_filename, *column_setting_id, *column_pixbuf;
	
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	view = gtk_tree_view_new_with_model(queue->list);
	queue->view = GTK_TREE_VIEW(view);

	gtk_container_add (GTK_CONTAINER (scroller), view);

	renderer_text = gtk_cell_renderer_text_new();
	renderer_pixbuf = gtk_cell_renderer_pixbuf_new();

	column_pixbuf = gtk_tree_view_column_new_with_attributes (_("Icon"),
					renderer_pixbuf,
					"pixbuf", RS_QUEUE_ELEMENT_THUMBNAIL,
					NULL);
	gtk_tree_view_column_set_resizable(column_pixbuf, TRUE);
	gtk_tree_view_column_set_sizing(column_pixbuf, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	column_filename = gtk_tree_view_column_new_with_attributes (_("Filename"),
					renderer_text,
					"text", RS_QUEUE_ELEMENT_FILENAME_SHORT,
					NULL);
	gtk_tree_view_column_set_resizable(column_filename, TRUE);
	gtk_tree_view_column_set_sizing(column_filename, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	column_setting_id = gtk_tree_view_column_new_with_attributes (_("Setting"),
					renderer_text,
					"text", RS_QUEUE_ELEMENT_SETTING_ID_ABC,
					NULL);
	gtk_tree_view_column_set_resizable(column_setting_id, TRUE);
	gtk_tree_view_column_set_sizing(column_setting_id, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_pixbuf);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_filename);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column_setting_id);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (view), FALSE);

	return scroller;
}

static void
batch_button_remove_clicked(GtkWidget *button, RS_QUEUE *queue)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	gtk_tree_view_get_cursor(queue->view,&path,&column);

	if(path && column)
	{
		GtkTreeIter iter;

		if(gtk_tree_model_get_iter(queue->list,&iter,path))
			gtk_list_store_remove(GTK_LIST_STORE(queue->list), &iter);
	}
	return;
}

static void
batch_button_remove_all_clicked(GtkWidget *button, RS_QUEUE *queue)
{
	gtk_list_store_clear(GTK_LIST_STORE(queue->list));
	return;
}

static void
batch_button_start_clicked(GtkWidget *button, RS_QUEUE *queue)
{
	rs_batch_start_queue(queue);
	return;
}

GtkWidget *
make_batchbuttons(RS_QUEUE *queue)
{
		GtkWidget *box;
		GtkWidget *start_button;
		GtkWidget *remove_button;
		GtkWidget *remove_all_button;

		box = gtk_hbox_new(FALSE,4);

		start_button = gtk_button_new_with_label(_("Start"));
		g_signal_connect ((gpointer) start_button, "clicked", G_CALLBACK (batch_button_start_clicked), queue);

		remove_button = gtk_button_new();
		gtk_button_set_label(GTK_BUTTON(remove_button), "Remove");
		g_signal_connect ((gpointer) remove_button, "clicked", G_CALLBACK (batch_button_remove_clicked), queue);

		remove_all_button = gtk_button_new();
		gtk_button_set_label(GTK_BUTTON(remove_all_button), "Remove all");
		g_signal_connect ((gpointer) remove_all_button, "clicked", G_CALLBACK (batch_button_remove_all_clicked), queue);

		gtk_box_pack_start(GTK_BOX (box), start_button, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX (box), remove_button, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX (box), remove_all_button, FALSE, FALSE, 0);

		return box;
}

static void
lockbox_changed(gpointer selected, gpointer user_data)
{
	gdouble tmp;
	RS_QUEUE *queue = (RS_QUEUE *) user_data;
	queue->size_lock = GPOINTER_TO_INT(selected);

	switch (queue->size_lock)
	{
		case LOCK_SCALE:
			gtk_spin_button_set_range(size_spin, 10.0, 100.0);
			if (rs_conf_get_double(CONF_BATCH_SIZE_SCALE, &tmp))
				gtk_spin_button_set_value(size_spin, tmp);
			break;
		case LOCK_WIDTH:
			gtk_spin_button_set_range(size_spin, 10.0, 65535.0);
			if (rs_conf_get_double(CONF_BATCH_SIZE_WIDTH, &tmp))
				gtk_spin_button_set_value(size_spin, tmp);
			break;
		case LOCK_HEIGHT:
			gtk_spin_button_set_range(size_spin, 10.0, 65535.0);
			if (rs_conf_get_double(CONF_BATCH_SIZE_HEIGHT, &tmp))
				gtk_spin_button_set_value(size_spin, tmp);
			break;
	}
	return;
}

static void
size_spin_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	RS_QUEUE *queue = (RS_QUEUE *) user_data;
	queue->size = gtk_spin_button_get_value(spinbutton);

	switch (queue->size_lock)
	{
		case LOCK_SCALE:
			rs_conf_set_double(CONF_BATCH_SIZE_SCALE, gtk_spin_button_get_value(size_spin));
			gtk_label_set_text(GTK_LABEL(size_label), _("%"));
			break;
		case LOCK_WIDTH:
			rs_conf_set_double(CONF_BATCH_SIZE_WIDTH, gtk_spin_button_get_value(size_spin));
			gtk_label_set_text(GTK_LABEL(size_label), _("px"));
			break;
		case LOCK_HEIGHT:
			rs_conf_set_double(CONF_BATCH_SIZE_HEIGHT, gtk_spin_button_get_value(size_spin));
			gtk_label_set_text(GTK_LABEL(size_label), _("px"));
			break;
	}

	return;
}

static void
chooser_changed(GtkFileChooser *chooser, gpointer user_data)
{
	RS_QUEUE *queue = (RS_QUEUE *) user_data;
	g_free(queue->directory);
	queue->directory = gtk_file_chooser_get_current_folder(chooser);
	rs_conf_set_string(CONF_BATCH_DIRECTORY, queue->directory);
	return;
}

static void
filetype_changed(gpointer active, gpointer user_data)
{
	RS_QUEUE *queue = (RS_QUEUE *) user_data;
	RS_FILETYPE *filetype = (RS_FILETYPE *) active;
	queue->filetype = filetype->filetype;
}

static GtkWidget *
make_batch_options(RS_QUEUE *queue)
{
	RS_CONFBOX *lockbox = gui_confbox_new(CONF_BATCH_SIZE_LOCK);
	GtkWidget *chooser;
	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 4);
	GtkWidget *filename;
	RS_CONFBOX *filetype_confbox;

	size_spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(10.0, 132.0, 1.0));
	size_label = gtk_label_new(NULL);

	chooser = gtk_file_chooser_button_new(_("Choose output directory"),
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser), TRUE);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), queue->directory);
	g_signal_connect (chooser, "selection-changed",
		G_CALLBACK (chooser_changed), queue);
	gtk_box_pack_start (GTK_BOX (vbox), gui_framed(chooser,
		_("Output directory:"), GTK_SHADOW_NONE), FALSE, FALSE, 0);

	filename = rs_filename_chooser_button_new(&queue->filename, CONF_BATCH_FILENAME);
	gtk_box_pack_start (GTK_BOX (vbox), gui_framed(filename,
		_("Filename template:"), GTK_SHADOW_NONE), FALSE, FALSE, 0);

	gui_confbox_set_callback(lockbox, queue, lockbox_changed);
	gui_confbox_add_entry(lockbox, "scale", _("Set image size by scale:"), GINT_TO_POINTER(LOCK_SCALE));
	gui_confbox_add_entry(lockbox, "width", _("Set image size by width:"), GINT_TO_POINTER(LOCK_WIDTH));
	gui_confbox_add_entry(lockbox, "height", _("Set image size by height:"), GINT_TO_POINTER(LOCK_HEIGHT));
	gui_confbox_load_conf(lockbox, "scale");

	gtk_widget_set(GTK_WIDGET(size_spin), "receives-default", TRUE, NULL);
	g_signal_connect(G_OBJECT(size_spin), "value_changed",
		G_CALLBACK(size_spin_changed), queue);

	gtk_box_pack_start (GTK_BOX (hbox), gui_confbox_get_widget(lockbox), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(size_spin), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), size_label, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	filetype_confbox = gui_confbox_filetype_new(CONF_BATCH_FILETYPE);
	gui_confbox_set_callback(filetype_confbox, queue, filetype_changed);
	gtk_box_pack_start (GTK_BOX (vbox), gui_confbox_get_widget(filetype_confbox), FALSE, TRUE, 0);

	return(vbox);
}

GtkWidget *
make_batchbox(RS_QUEUE *queue)
{
	GtkWidget *batchbox;

	batchbox = gtk_vbox_new(FALSE,4);
	gtk_box_pack_start (GTK_BOX (batchbox), make_batch_options(queue), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (batchbox), make_batchview(queue), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (batchbox), make_batchbuttons(queue), FALSE, FALSE, 0);

	return batchbox;
}

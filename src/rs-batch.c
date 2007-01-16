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

extern GtkWindow *rawstudio_window;

static gboolean batch_exists_in_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);
static GtkWidget *make_batchview(RS_QUEUE *queue);

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
		queue->directory = rs_conf_get_string(CONF_BATCH_FILENAME);
	}

	rs_conf_get_filetype(CONF_BATCH_FILETYPE, &filetype);
	queue->filetype = filetype->filetype;

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

static GtkWidget *
make_batchview(RS_QUEUE *queue)
{
	GtkWidget *batchboxscroller;
	GtkWidget *batchview;
	GtkCellRenderer *renderer_text, *renderer_pixbuf;
	GtkTreeViewColumn *column_filename, *column_setting_id, *column_pixbuf;
	
	batchboxscroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (batchboxscroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	batchview = gtk_tree_view_new_with_model(queue->list);
	gtk_container_add (GTK_CONTAINER (batchboxscroller), batchview);

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

	gtk_tree_view_append_column (GTK_TREE_VIEW (batchview), column_pixbuf);
	gtk_tree_view_append_column (GTK_TREE_VIEW (batchview), column_filename);
	gtk_tree_view_append_column (GTK_TREE_VIEW (batchview), column_setting_id);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (batchview), FALSE);

	return batchboxscroller;
}

GtkWidget *
make_batchbox(RS_QUEUE *queue)
{
	GtkWidget *batchbox;

	batchbox = gtk_vbox_new(FALSE,4);
	gtk_box_pack_start (GTK_BOX (batchbox), make_batchview(queue), TRUE, TRUE, 0);

	return batchbox;
}

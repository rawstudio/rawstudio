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

static gboolean batch_exists_in_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);

RS_QUEUE* rs_batch_new_queue(void)
{
	RS_QUEUE *queue = g_new(RS_QUEUE, 1);
	RS_FILETYPE *filetype;

	queue->list = GTK_TREE_MODEL(gtk_list_store_new(2, G_TYPE_STRING,G_TYPE_INT));

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
		rs_batch_add_to_queue(queue, element->filename, element->setting_id);
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
rs_batch_add_to_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id)
{
	if (!batch_exists_in_queue(queue, filename, setting_id))
	{
		GtkTreeIter iter;
		
		gtk_list_store_append (GTK_LIST_STORE(queue->list), &iter);
		gtk_list_store_set (GTK_LIST_STORE(queue->list), &iter,
					RS_QUEUE_ELEMENT_FILENAME, filename,
					RS_QUEUE_ELEMENT_SETTING_ID, setting_id,
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

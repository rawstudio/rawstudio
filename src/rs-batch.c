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
#include "rs-batch.h"

RS_QUEUE* batch_new_queue()
{
	RS_QUEUE *queue;
	queue = g_malloc(sizeof(RS_QUEUE));

	queue->array = g_array_new(TRUE, TRUE, sizeof(RS_QUEUE_ELEMENT *));
	return queue;
}

gboolean
batch_add_element_to_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element)
{
	gint index = batch_find_in_queue(queue, element->path_file, element->setting_id);
	if (index == -1)
	{
		g_array_append_val(queue->array, element);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean
batch_remove_element_from_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element)
{
	return batch_remove_from_queue(queue, element->path_file, element->setting_id);
}

gboolean
batch_add_to_queue(RS_QUEUE *queue, const gchar *path_file, gint setting_id, gchar *output_file)
{
	gint index = batch_find_in_queue(queue, path_file, setting_id);
	RS_QUEUE_ELEMENT *element;

	if (index == -1)
	{
		element = g_malloc(sizeof(RS_QUEUE_ELEMENT));
		element->path_file = g_strdup(path_file);
		element->output_file = g_strdup(output_file);
		element->setting_id = setting_id;
		g_array_append_val(queue->array, element);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean
batch_remove_from_queue(RS_QUEUE *queue, const gchar *path_file, gint setting_id)
{
	gint index = batch_find_in_queue(queue, path_file, setting_id);

	if (index != -1)
	{
		RS_QUEUE_ELEMENT *element = batch_get_element(queue, index);
		g_array_remove_index(queue->array, index);
		g_free((gchar *) element->path_file);
		g_free((gchar *) element->output_file);
		g_free(element);
		return TRUE;
	}
	else
		return FALSE;
}

RS_QUEUE_ELEMENT*
batch_get_next_in_queue(RS_QUEUE *queue)
{
	if (queue->array->len > 0)
	{
		RS_QUEUE_ELEMENT *element = g_array_index(queue->array, RS_QUEUE_ELEMENT *, 0);
		return element;
	}
	else
		return NULL;
}

void
batch_remove_next_in_queue(RS_QUEUE *queue)
{
	RS_QUEUE_ELEMENT *element = batch_get_element(queue, 0);
	g_array_remove_index(queue->array, 0);
	g_free((gchar *) element->path_file);
	g_free((gchar *) element->output_file);
	g_free(element);
	return;
}

gint
batch_find_in_queue(RS_QUEUE *queue, const gchar *path_file, gint setting_id)
{
	RS_QUEUE_ELEMENT *element;
	gint n = 0;
	gint retval = -1;
	
	while(queue->array->len > n)
	{
		element = g_array_index(queue->array, RS_QUEUE_ELEMENT *, n);
		if (g_str_equal(element->path_file, path_file) && setting_id == element->setting_id)
			retval = n;
		n++;
	}
	return retval;
}

RS_QUEUE_ELEMENT*
batch_get_element(RS_QUEUE *queue, gint index)
{
	return g_array_index(queue->array, RS_QUEUE_ELEMENT *, index);
}

#include <glib.h>
#include <stdio.h>
#include "rs-batch.h"

RS_QUEUE* batch_new_queue()
{
	RS_QUEUE *queue;
	queue = g_malloc(sizeof(queue));

	queue->array = g_array_new(TRUE, TRUE, sizeof(RS_QUEUE_ELEMENT));
	return queue;
}

gboolean
batch_add_element_to_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element)
{
	gint index = batch_find_in_queue(queue, element->path_file, element->setting_id);
	if (index == -1)
	{
		g_array_append_vals(queue->array, element, 1);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean
batch_add_to_queue(RS_QUEUE *queue, const gchar *path_file, gint setting_id, gchar *output_file)
{
	gint index = batch_find_in_queue(queue, path_file, setting_id);
	RS_QUEUE_ELEMENT *element;

	if (index == -1)
	{
		element = g_malloc(sizeof(RS_QUEUE_ELEMENT));
		element->path_file = path_file;
		element->output_file = output_file;
		element->setting_id = setting_id;
		g_array_append_vals(queue->array, element,1);
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
		g_array_remove_index(queue->array, index);
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
		RS_QUEUE_ELEMENT *element = &g_array_index(queue->array, RS_QUEUE_ELEMENT, 0);
		return element;
	}
	else
		return NULL;
}

void
batch_remove_next_in_queue(RS_QUEUE *queue)
{
	g_array_remove_index(queue->array, 0);
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
		element = &g_array_index(queue->array, RS_QUEUE_ELEMENT, n);
		if (g_str_equal(element->path_file, path_file) && setting_id == element->setting_id)
			retval = n;
		n++;
	}
	return retval;
}

RS_QUEUE_ELEMENT*
batch_get_element(RS_QUEUE *queue, gint index)
{
	return &g_array_index(queue->array, RS_QUEUE_ELEMENT, index);
}

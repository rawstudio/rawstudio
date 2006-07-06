/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

typedef struct {
	const gchar *path_file;
	gchar *output_file;
	gint setting_id;
} RS_QUEUE_ELEMENT;

typedef struct {
	GArray *array;
} RS_QUEUE;


RS_QUEUE* batch_new_queue();
gboolean batch_add_element_to_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element);
gboolean batch_add_to_queue(RS_QUEUE *queue, const gchar *file_path, gint setting_id, gchar *output_file);
gboolean batch_remove_from_queue(RS_QUEUE *queue, const gchar *path_file, gint setting_id);
RS_QUEUE_ELEMENT* batch_get_next_in_queue(RS_QUEUE *queue);
void batch_remove_next_in_queue(RS_QUEUE *queue);
gint batch_find_in_queue(RS_QUEUE *queue, const gchar *file_path, gint setting_id);
RS_QUEUE_ELEMENT* batch_get_element(RS_QUEUE *queue, gint index);

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

#ifndef RS_BATCH_H
#define RS_BATCH_H

typedef struct {
	const gchar *filename;
	gint setting_id;
} RS_QUEUE_ELEMENT;

typedef struct {
	GtkTreeModel *list;
	gchar *directory;
	gchar *filename;
	gint filetype;
} RS_QUEUE;

enum {
	RS_QUEUE_ELEMENT_FILENAME = 0,
	RS_QUEUE_ELEMENT_SETTING_ID
};

extern RS_QUEUE* rs_batch_new_queue(void);
extern gboolean rs_batch_add_element_to_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element);
extern gboolean rs_batch_remove_element_from_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element);
extern RS_QUEUE_ELEMENT* rs_batch_get_next_element_in_queue(RS_QUEUE *queue);
extern gboolean rs_batch_add_to_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);
extern gboolean rs_batch_remove_from_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);

#endif /* RS_BATCH_H */

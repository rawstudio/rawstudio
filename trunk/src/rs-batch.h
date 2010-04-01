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

#ifndef RS_BATCH_H
#define RS_BATCH_H

#include "application.h"

typedef enum {
	LOCK_SCALE = 0,
	LOCK_WIDTH,
	LOCK_HEIGHT,
	LOCK_BOUNDING_BOX,
} RS_QUEUE_SIZE_LOCK;

struct _RS_QUEUE {
	GtkTreeModel *list;
	GtkTreeView *view;
	gchar *directory;
	gchar *filename;
	RSOutput *output;
	RS_QUEUE_SIZE_LOCK size_lock;
	gdouble size;
	gint width;
	gint height;
	gint scale;
	GtkWidget *start_button;
	GtkWidget *remove_button;
	GtkWidget *remove_all_button;
	GtkWidget *size_window;
	GtkWidget *size_label;
	GtkWidget *size_width[3];
	GtkWidget *size_height[3];
	GtkWidget *size_scale[3];
};

enum {
	RS_QUEUE_ELEMENT_FILENAME = 0,
	RS_QUEUE_ELEMENT_FILENAME_SHORT,
	RS_QUEUE_ELEMENT_SETTING_ID,
	RS_QUEUE_ELEMENT_SETTING_ID_ABC,
	RS_QUEUE_ELEMENT_THUMBNAIL
};

extern RS_QUEUE* rs_batch_new_queue(void);
extern gboolean rs_batch_add_to_queue(RS_QUEUE *queue, const gchar *filename, const gint setting_id);
extern gboolean rs_batch_remove_from_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);
extern gboolean rs_batch_exists_in_queue(RS_QUEUE *queue, const gchar *filename, gint setting_id);
extern void rs_batch_process(RS_QUEUE *queue);
extern GtkWidget *make_batchbox(RS_QUEUE *queue);

/**
 * Returns the number of entries in the batch queue
 * @param queue A RS_QUEUE
 * @return The number of entries in the queue
 */
extern gint rs_batch_num_entries(RS_QUEUE *queue);

#endif /* RS_BATCH_H */

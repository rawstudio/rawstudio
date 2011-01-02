/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
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

#include <rawstudio.h>

#ifndef RS_IO_H
#define RS_IO_H

/**
 * Add a RSIoJob to be executed later
 * @param job A RSIoJob. This will be unreffed upon completion
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @param priority Lower value means higher priority
 * @param user_data A pointer to pass to the callback
 */
void
rs_io_idle_add_job(RSIoJob *job, gint idle_class, gint priority, gpointer user_data);

/**
 * Prefetch a file
 * @param path Absolute path to a file to prefetch
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_prefetch_file(const gchar *path, gint idle_class);

/**
 * Load metadata belonging to a photo
 * @param path Absolute path to a photo
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @param callback A callback to call when the data is ready or NULL
 * @param user_data Data to pass to the callback
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_read_metadata(const gchar *path, gint idle_class, RSGotMetadataCB callback, gpointer user_data);

/**
 * Compute a "Rawstudio checksum" of a file
 * @param path Absolute path to a file
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @param callback A callback to call when the data is ready or NULL
 * @param user_data Data to pass to the callback
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_read_checksum(const gchar *path, gint idle_class, RSGotChecksumCB callback, gpointer user_data);

/**
 * Restore tags of a new directory or add tags to a photo
 * @param filename Absolute path to a file to tags to
 * @param tag_id The id of the tag to add.
 * @param auto_tag Is the tag an automatically generated tag
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_add_tag(const gchar *filename, gint tag_id, gboolean auto_tag, gint idle_class);

/**
 * Restore tags of a new directory
 * @param path Absolute path to a directory to restore tags to
 * @param idle_class A user defined variable, this can be used with rs_io_idle_cancel_class() to cancel a batch of queued reads
 * @return A pointer to a RSIoJob, this can be used with rs_io_idle_cancel()
 */
const RSIoJob *
rs_io_idle_restore_tags(const gchar *path, gint idle_class);

/**
 * Cancel a complete class of idle requests
 * @param idle_class The class identifier
 */
void
rs_io_idle_cancel_class(gint idle_class);

/**
 * Cancel a idle request
 * @param request_id A request_id as returned by rs_io_idle_read_complete_file()
 */
void
rs_io_idle_cancel(RSIoJob *job);

/**
 * Pause the worker threads
 */
void
rs_io_idle_pause();

/**
 * Unpause the worker threads
 */
void
rs_io_idle_unpause();

/**
 * Aquire the IO lock
 */
void
rs_io_lock();

/**
 * Release the IO lock
 */
void
rs_io_unlock();

/**
 * Returns the number of jobs left
 */
gint
rs_io_get_jobs_left();

#endif /* RS_IO_H */

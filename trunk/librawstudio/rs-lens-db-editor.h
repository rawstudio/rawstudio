/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
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

#ifndef RS_LENS_DB_EDITOR_H
#define RS_LENS_DB_EDITOR_H

enum {
        RS_LENS_DB_EDITOR_IDENTIFIER = 0,
        RS_LENS_DB_EDITOR_HUMAN_FOCAL,
        RS_LENS_DB_EDITOR_HUMAN_APERTURE,
        RS_LENS_DB_EDITOR_LENS_MAKE,
        RS_LENS_DB_EDITOR_LENS_MODEL,
	RS_LENS_DB_EDITOR_CAMERA_MAKE,
	RS_LENS_DB_EDITOR_CAMERA_MODEL,
	RS_LENS_DB_EDITOR_ENABLED,
	RS_LENS_DB_EDITOR_ENABLED_ACTIVATABLE,
	RS_LENS_DB_EDITOR_LENS
};


extern void rs_lens_db_editor();
extern GtkDialog * rs_lens_db_editor_single_lens(RSLens *lens);

#endif /* RS_LENS_DB_EDITOR_H */

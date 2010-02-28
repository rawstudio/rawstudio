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

#ifndef RS_TIFF_IFD_H
#define RS_TIFF_IFD_H

#include <glib-object.h>
#include "rs-tiff.h"

G_BEGIN_DECLS

#define RS_TYPE_TIFF_IFD rs_tiff_ifd_get_type()
#define RS_TIFF_IFD(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TIFF_IFD, RSTiffIfd))
#define RS_TIFF_IFD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TIFF_IFD, RSTiffIfdClass))
#define RS_IS_TIFF_IFD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TIFF_IFD))
#define RS_IS_TIFF_IFD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_TIFF_IFD))
#define RS_TIFF_IFD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_TIFF_IFD, RSTiffIfdClass))

typedef struct {
	GObject parent;

	gboolean dispose_has_run;

	RSTiff *tiff;
	guint offset;

	gushort num_entries;
	GList *entries;
	guint next_ifd;
} RSTiffIfd;

typedef struct {
	GObjectClass parent_class;

	void (*read)(RSTiffIfd *ifd);
} RSTiffIfdClass;

GType rs_tiff_ifd_get_type(void);

RSTiffIfd *rs_tiff_ifd_new(RSTiff *tiff, guint offset);

guint rs_tiff_ifd_get_next(RSTiffIfd *ifd);

RSTiffIfdEntry *rs_tiff_ifd_get_entry_by_tag(RSTiffIfd *ifd, gushort tag);

G_END_DECLS

#endif /* RS_TIFF_IFD_H */

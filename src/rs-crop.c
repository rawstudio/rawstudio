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
#include "rawstudio.h"

extern gint state;

void
rs_crop_start(RS_BLOB *rs)
{
	if (!rs->photo) return;
	rs->roi_scaled.x1 = 0;
	rs->roi_scaled.y1 = 0;
	rs->roi_scaled.x2 = rs->photo->scaled->w-1;
	rs->roi_scaled.y2 = rs->photo->scaled->h-1;
	rs_rect_scale(&rs->roi_scaled, &rs->roi, 1.0/GETVAL(rs->scale));
	rs_mark_roi(rs, TRUE);
	state = STATE_CROP;
	update_preview(rs, FALSE, FALSE);
	return;
}

void
rs_crop_end(RS_BLOB *rs, gboolean accept)
{
	if (accept)
	{
		if (!rs->photo->crop)
			rs->photo->crop = (RS_RECT *) g_malloc(sizeof(RS_RECT));
		rs->photo->crop->x1 = rs->roi.x1;
		rs->photo->crop->y1 = rs->roi.y1;
		rs->photo->crop->x2 = rs->roi.x2;
		rs->photo->crop->y2 = rs->roi.y2;
	}
	rs_mark_roi(rs, FALSE);
	state = STATE_NORMAL;
	update_preview(rs, FALSE, TRUE);
	return;
}

void
rs_crop_uncrop(RS_BLOB *rs)
{
	if (!rs->photo) return;
	if (rs->photo->crop)
	{
		g_free(rs->photo->crop);
		rs->photo->crop = NULL;
	}
	update_preview(rs, FALSE, TRUE);
	return;
}

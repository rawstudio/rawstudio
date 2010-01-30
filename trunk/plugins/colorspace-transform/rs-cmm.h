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

#ifndef RS_CMM_H
#define RS_CMM_H

#include <rawstudio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_CMM rs_cmm_get_type()
#define RS_CMM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_CMM, RSCmm))
#define RS_CMM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_CMM, RSCmmClass))
#define RS_IS_CMM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_CMM))
#define RS_IS_CMM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_CMM))
#define RS_CMM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_CMM, RSCmmClass))

typedef struct _RSCmm RSCmm;

typedef struct {
	GObjectClass parent_class;
} RSCmmClass;

GType rs_cmm_get_type(void);

RSCmm *rs_cmm_new(void);

void rs_cmm_set_input_profile(RSCmm *cmm, const RSIccProfile *input_profile);

void rs_cmm_set_output_profile(RSCmm *cmm, const RSIccProfile *output_profile);

void rs_cmm_set_num_threads(RSCmm *cmm, const gint num_threads);

void rs_cmm_set_premul(RSCmm *cmm, const gfloat premul[3]);

gboolean rs_cmm_transform16(RSCmm *cmm, RS_IMAGE16 *input, RS_IMAGE16 *output);

gboolean rs_cmm_transform8(RSCmm *cmm, RS_IMAGE16 *input, GdkPixbuf *output);

G_END_DECLS

#endif /* RS_CMM_H */

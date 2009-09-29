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

#ifndef RS_DCP_FILE_H
#define RS_DCP_FILE_H

#include <rawstudio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_DCP_FILE rs_dcp_file_get_type()
#define RS_DCP_FILE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DCP_FILE, RSDcpFile))
#define RS_DCP_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DCP_FILE, RSDcpFileClass))
#define RS_IS_DCP_FILE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DCP_FILE))
#define RS_IS_DCP_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_DCP_FILE))
#define RS_DCP_FILE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_DCP_FILE, RSDcpFileClass))

typedef struct _RSDcpFile RSDcpFile;

typedef struct {
	RSTiffClass parent_class;
} RSDcpFileClass;

GType rs_dcp_file_get_type(void);

RSDcpFile *rs_dcp_file_new_from_file(const gchar *path);

const gchar *rs_dcp_file_get_model(RSDcpFile *dcp_file);

gboolean rs_dcp_file_get_color_matrix1(RSDcpFile *dcp_file, RS_MATRIX3 *matrix);

gboolean rs_dcp_file_get_color_matrix2(RSDcpFile *dcp_file, RS_MATRIX3 *matrix);

gfloat rs_dcp_file_get_illuminant1(RSDcpFile *dcp_file);

gfloat rs_dcp_file_get_illuminant2(RSDcpFile *dcp_file);

const gchar *rs_dcp_file_get_signature(RSDcpFile *dcp_file);

const gchar *rs_dcp_file_get_name(RSDcpFile *dcp_file);

RSHuesatMap *rs_dcp_file_get_huesatmap1(RSDcpFile *dcp_file);

RSHuesatMap *rs_dcp_file_get_huesatmap2(RSDcpFile *dcp_file);

RSSpline *rs_dcp_file_get_tonecurve(RSDcpFile *dcp_file);

const gchar *rs_dcp_file_get_copyright(RSDcpFile *dcp_file);

gboolean rs_dcp_file_get_forward_matrix1(RSDcpFile *dcp_file, RS_MATRIX3 *matrix);

gboolean rs_dcp_file_get_forward_matrix2(RSDcpFile *dcp_file, RS_MATRIX3 *matrix);

RSHuesatMap *rs_dcp_file_get_looktable(RSDcpFile *dcp_file);

G_END_DECLS

#endif /* RS_DCP_FILE_H */

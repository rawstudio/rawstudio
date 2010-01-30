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

#ifndef RS_PLUGIN_H
#define RS_PLUGIN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_PLUGIN (rs_plugin_get_type ())
#define RS_PLUGIN(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), RS_TYPE_PLUGIN, RSPlugin))
#define RS_PLUGIN_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), RS_TYPE_PLUGIN, RSPluginClass))
#define RS_IS_PLUGIN(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), RS_TYPE_PLUGIN))
#define RS_IS_PLUGIN_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), RS_TYPE_PLUGIN))
#define RS_PLUGIN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), RS_TYPE_PLUGIN, RSPluginClass))

typedef struct _RSPlugin RSPlugin;
typedef struct _RSPluginClass RSPluginClass;

GType rs_plugin_get_type(void) G_GNUC_CONST;

RSPlugin *rs_plugin_new(const gchar *filename);

void rs_plugin_load(RSPlugin *plugin);
void rs_plugin_unload(RSPlugin *plugin);

G_END_DECLS

#endif /* RS_PLUGIN_H */

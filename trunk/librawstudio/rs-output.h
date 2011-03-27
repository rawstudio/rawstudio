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

#ifndef RS_OUTPUT_H
#define RS_OUTPUT_H

#include "rawstudio.h"
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * Convenience macro to define generic output module
 */
#define RS_DEFINE_OUTPUT(type_name, TypeName) \
static GType type_name##_get_type (GTypeModule *module); \
static void type_name##_class_init(TypeName##Class *klass); \
static void type_name##_init(TypeName *output); \
static GType type_name##_type = 0; \
static GType \
type_name##_get_type(GTypeModule *module) \
{ \
	if (!type_name##_type) \
	{ \
		static const GTypeInfo output_info = \
		{ \
			sizeof (TypeName##Class), \
			(GBaseInitFunc) NULL, \
			(GBaseFinalizeFunc) NULL, \
			(GClassInitFunc) type_name##_class_init, \
			NULL, \
			NULL, \
			sizeof (TypeName), \
			0, \
			(GInstanceInitFunc) type_name##_init \
		}; \
 \
		type_name##_type = g_type_module_register_type( \
			module, \
			RS_TYPE_OUTPUT, \
			#TypeName, \
			&output_info, \
			0); \
	} \
	return type_name##_type; \
}

#define RS_TYPE_OUTPUT rs_output_get_type()
#define RS_OUTPUT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_OUTPUT, RSOutput))
#define RS_OUTPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_OUTPUT, RSOutputClass))
#define RS_IS_OUTPUT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_OUTPUT))
#define RS_IS_OUTPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_OUTPUT))
#define RS_OUTPUT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_OUTPUT, RSOutputClass))

#define RS_OUTPUT_NAME(output) (((output)) ? g_type_name(G_TYPE_FROM_CLASS(RS_OUTPUT_GET_CLASS ((output)))) : "(nil)")

typedef struct _RSOutput RSOutput;
typedef struct _RSOutputClass RSOutputClass;

struct _RSOutput {
	GObject parent;
};

struct _RSOutputClass {
	GObjectClass parent_class;
	gchar *extension;
	gchar *display_name;
	gboolean (*execute)(RSOutput *output, RSFilter *filter);
};

GType rs_output_get_type(void) G_GNUC_CONST;

/**
 * Instantiate a new RSOutput type
 * @param identifier A string representing a type, for example "RSJpegfile"
 * @return A new RSOutput or NULL on failure
 */
extern RSOutput *
rs_output_new(const gchar *identifier);

/**
 * Get a filename extension as announced by a RSOutput module
 * @param output A RSOutput
 * @return A proposed filename extension excluding the ., this should not be freed.
 */
const gchar *
rs_output_get_extension(RSOutput *output);

/**
 * Actually execute the saver
 * @param output A RSOutput
 * @param filter A RSFilter to get image data from
 * @return TRUE on success, FALSE on error
 */
extern gboolean
rs_output_execute(RSOutput *output, RSFilter *filter);

/**
 * Load parameters from config for a RSOutput
 * @param output A RSOutput
 * @param conf_prefix The prefix to prepend on config-keys.
 */
void
rs_output_set_from_conf(RSOutput *output, const gchar *conf_prefix);

/**
 * Build a GtkWidget that can edit parameters of a RSOutput
 * @param output A RSOutput
 * @param conf_prefix If this is non-NULL, the value will be saved in config,
 *                    and reloaded next time.
 * @return A new GtkWidget representing all parameters of output
 */
extern GtkWidget *
rs_output_get_parameter_widget(RSOutput *output, const gchar *conf_prefix);

G_END_DECLS

#endif /* RS_OUTPUT_H */

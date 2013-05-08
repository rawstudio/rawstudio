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

#ifndef RS_METADATA_H
#define RS_METADATA_H

#include <glib-object.h>

G_BEGIN_DECLS

#define DOTDIR_METACACHE "metacache.xml"
#define DOTDIR_THUMB "thumb.jpg"
#define DOTDIR_THUMB_PNG "thumb.png"

#define RS_TYPE_METADATA rs_metadata_get_type()
#define RS_METADATA(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_METADATA, RSMetadata))
#define RS_METADATA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_METADATA, RSMetadataClass))
#define RS_IS_METADATA(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_METADATA))
#define RS_IS_METADATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_METADATA))
#define RS_METADATA_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_METADATA, RSMetadataClass))

typedef enum {
	MAKE_UNKNOWN = 0,
	MAKE_CANON,
	MAKE_CASIO,
	MAKE_EPSON,
	MAKE_FUJIFILM,
	MAKE_HASSELBLAD,
	MAKE_KODAK,
	MAKE_LEICA,
	MAKE_MAMIYA,
	MAKE_MINOLTA,
	MAKE_NIKON,
	MAKE_OLYMPUS,
	MAKE_PANASONIC,
	MAKE_PENTAX,
	MAKE_PHASEONE,
	MAKE_POLAROID,
	MAKE_RICOH,
	MAKE_SAMSUNG,
	MAKE_SIGMA,
	MAKE_SONY,
} RS_MAKE;

struct _RSMetadata {
	GObject parent;
	gboolean dispose_has_run;
	RS_MAKE make;
	gchar *make_ascii;
	gchar *model_ascii;
	gchar *time_ascii;
	GTime timestamp;
	gushort orientation;
	gfloat aperture;
	gfloat exposurebias;
	gushort iso;
	gfloat shutterspeed;
	guint thumbnail_start;
	guint thumbnail_length;
	guint preview_start;
	guint preview_length;
	guint16 preview_planar_config;
	guint preview_width;
	guint preview_height;
	guint16 preview_bits [3];
	gdouble cam_mul[4];
	gdouble contrast;
	gdouble saturation;
	gdouble color_tone;
	gshort focallength;
	GdkPixbuf *thumbnail;

	/* Lens info */
	gint lens_id;
	gdouble lens_min_focal;
	gdouble lens_max_focal;
	gdouble lens_min_aperture;
	gdouble lens_max_aperture;
	gchar *fixed_lens_identifier;
	gchar *lens_identifier;
};

typedef struct {
  GObjectClass parent_class;
} RSMetadataClass;

GType rs_metadata_get_type (void);

extern RSMetadata *rs_metadata_new (void);
extern RSMetadata *rs_metadata_new_from_file(const gchar *filename);
extern gboolean rs_metadata_load_from_file(RSMetadata *metadata, const gchar *filename);
extern void rs_metadata_normalize_wb(RSMetadata *metadata);
extern gchar *rs_metadata_get_short_description(RSMetadata *metadata);
extern GdkPixbuf *rs_metadata_get_thumbnail(RSMetadata *metadata);

/* Attempts to load cached metadata first, then falls back to reading from file */
extern gboolean rs_metadata_load(RSMetadata *metadata, const gchar *filename);

/* Save metadata to cache xml file and sidecar thumbnail*/
extern void rs_metadata_cache_save(RSMetadata *metadata, const gchar *filename);

/**
 * Deletes the on-disk cache (if any) for a photo
 * @param filename The filename of the PHOTO - not the cache itself
 */
extern void rs_metadata_delete_cache(const gchar *filename);

extern gchar * rs_metadata_dotdir_helper(const gchar *filename, const gchar *extension);

G_END_DECLS

#endif /* RS_METADATA_H */


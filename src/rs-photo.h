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

#ifndef RS_PHOTO_H
#define RS_PHOTO_H

#include "application.h"
#include <glib-object.h>

#define RS_TYPE_PHOTO        (rs_photo_get_type ())
#define RS_PHOTO(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PHOTO, RS_PHOTO))
#define RS_PHOTO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PHOTO, RS_PHOTOClass))
#define RS_IS_PHOTO(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PHOTO))
#define RS_IS_PHOTO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_PHOTO))
#define RS_PHOTO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_PHOTO, RS_PHOTOClass))

typedef struct _RS_PHOTOClass RS_PHOTOClass;

struct _RS_PHOTOClass {
	GObjectClass parent;
};

GType rs_photo_get_type (void);

/* Please note that this is not a bitmask */
enum {
	PRIO_U = 0,
	PRIO_D = 51,
	PRIO_1 = 1,
	PRIO_2 = 2,
	PRIO_3 = 3,
	PRIO_ALL = 255
};

/**
 * Allocates a new RS_PHOTO
 * @return A new RS_PHOTO
 */
extern RS_PHOTO *rs_photo_new();

/**
 * Rotates a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param quarterturns How many quarters to turn
 * @param angle The angle in degrees (360 is whole circle) to turn the image
 */
extern void rs_photo_rotate(RS_PHOTO *photo, const gint quarterturns, const gdouble angle);

/**
 * Sets a new crop of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param crop The new crop or NULL to remove previous cropping
 */
extern void rs_photo_set_crop(RS_PHOTO *photo, const RS_RECT *crop);

/**
 * Gets the crop of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return The crop as a RS_RECT or NULL if the photo is uncropped
 */
extern RS_RECT *rs_photo_get_crop(RS_PHOTO *photo);

/**
 * Set the angle of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param angle The new angle
 * @param relative If set to TRUE, angle will be relative to existing angle
 */
extern void rs_photo_set_angle(RS_PHOTO *photo, gdouble angle, gboolean relative);

/**
 * Get the angle of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return The current angle
 */
extern gdouble rs_photo_get_angle(RS_PHOTO *photo);

/**
 * Get the exposure of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_exposure(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the saturation of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_saturation(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the hue of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_hue(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the contrast of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_contrast(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the warmth of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_warmth(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the tint of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_tint(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the sharpen of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot A snapshot
 * @return The current value
 */
extern gdouble rs_photo_get_sharpen(RS_PHOTO *photo, const gint snapshot);

/**
 * Set the exposure of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_exposure(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Set the saturation of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_saturation(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Set the hue of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_hue(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Set the contrast of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_contrast(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Set the warmth of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_warmth(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Set the tint of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_tint(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Set the sharpen of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param value The new value
 */
extern void rs_photo_set_sharpen(RS_PHOTO *photo, const gint snapshot, const gdouble value);

/**
 * Apply settings to a RS_PHOTO from a RSSettings
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param rs_settings The settings to apply
 * @param mask A mask for defining which settings to apply
 */
extern void rs_photo_apply_settings(RS_PHOTO *photo, const gint snapshot, RSSettings *settings, RSSettingsMask mask);

/**
 * Flips a RS_PHOTO
 * @param photo A RS_PHOTO
 */
extern void rs_photo_flip(RS_PHOTO *photo);

/**
 * Mirrors a RS_PHOTO
 * @param photo A RS_PHOTO
 */
extern void rs_photo_mirror(RS_PHOTO *photo);

/**
 * Assign a DCP profile to a photo
 * @param photo A RS_PHOTO
 * @param dcp A DCP profile
 */
extern void rs_photo_set_dcp_profile(RS_PHOTO *photo, RSDcpFile *dcp);

/**
 * Get the assigned DCP profile for a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return A DCP profile or NULL
 */
extern RSDcpFile *rs_photo_get_dcp_profile(RS_PHOTO *photo);

/**
 * Assign a ICC profile to a photo
 * @param photo A RS_PHOTO
 * @param dcp An ICC profile
 */
extern void rs_photo_set_icc_profile(RS_PHOTO *photo, RSIccProfile *icc);

/**
 * Get the assigned ICC profile for a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return An ICC profile or NULL
 */
extern RSIccProfile *rs_photo_get_icc_profile(RS_PHOTO *photo);

/**
 * Sets the white balance of a RS_PHOTO using warmth and tint variables
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param warmth
 * @param tint
 */
extern void rs_photo_set_wb_from_wt(RS_PHOTO *photo, const gint snapshot, const gdouble warmth, const gdouble tint);

/**
 * Sets the white balance of a RS_PHOTO using multipliers
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param mul A pointer to an array of at least 3 multipliers
 */
extern void rs_photo_set_wb_from_mul(RS_PHOTO *photo, const gint snapshot, const gdouble *mul, const gchar *ascii);

/**
 * Sets the white balance by neutralizing the colors provided
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param r The red color
 * @param g The green color
 * @param b The blue color
 */
extern void rs_photo_set_wb_from_color(RS_PHOTO *photo, const gint snapshot, const gdouble r, const gdouble g, const gdouble b);

/**
 * Autoadjust white balance of a RS_PHOTO using the greyworld algorithm
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 */
extern void rs_photo_set_wb_auto(RS_PHOTO *photo, const gint snapshot);

/**
 * Autoadjust white balance from the in-camera settings
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @return TRUE on success, FALSE on error
 */
extern gboolean rs_photo_set_wb_from_camera(RS_PHOTO *photo, const gint snapshot);

/**
 * Get the metadata belonging to the RS_PHOTO
 * @param photo A RS_PHOTO
 * @return A RSMetadata, this must be unref'ed
 */
extern RSMetadata *rs_photo_get_metadata(RS_PHOTO *photo);

/**
 * Closes a RS_PHOTO - this basically means saving cache
 * @param photo A RS_PHOTO
 */
extern void rs_photo_close(RS_PHOTO *photo);

/**
 * Indicate that the lens has changed.
 * @param photo A RS_PHOTO
 */
extern void rs_photo_lens_updated(RS_PHOTO *photo);

/**
 * Loads a photo in to a RS_PHOTO including metadata
 * @param filename The filename to load
 * @return A RS_PHOTO on success, NULL on error
 */
extern RS_PHOTO *
rs_photo_load_from_file(const gchar *filename);

/**
 * Loads a photo in to a RS_PHOTO including metadata
 * @param photo A RS_PHOTO
 * @param rotated Set to TRUE to get the rotated sizes, FALSE to get the RAW size.
 * @param width output variable that will be set to the width
 * @param height output variable that will be set to the height
 * @return A boolean, TRUE on success, false if no photo has been loaded
 */
extern gboolean
rs_photo_get_original_size(RS_PHOTO *photo, gboolean rotated, gint* width, gint* height);

extern void
rs_photo_rotate_rect_inverse(RS_PHOTO *photo, RS_RECT *coords);

#endif /* RS_PHOTO_H */

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

#include <rawstudio.h>
#include "rs-photo.h"
#include "rs-cache.h"
#include "rs-camera-db.h"

static void rs_photo_class_init (RS_PHOTOClass *klass);

G_DEFINE_TYPE (RS_PHOTO, rs_photo, G_TYPE_OBJECT);

enum {
	SPATIAL_CHANGED,
	SETTINGS_CHANGED,
	PROFILE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void photo_settings_changed_cb(RSSettings *settings, RSSettingsMask mask, gpointer user_data);
static void
rs_photo_dispose (GObject *obj)
{
	RS_PHOTO *self = (RS_PHOTO *)obj;

	if (self->dispose_has_run)
		return;
	self->dispose_has_run = TRUE;

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
rs_photo_finalize (GObject *obj)
{
	gint c;
	RS_PHOTO *photo = (RS_PHOTO *)obj;

	if (photo->filename)
		g_free(photo->filename);

	if (photo->metadata)
		g_object_unref(photo->metadata);

	if (photo->input)
		g_object_unref(photo->input);

	for(c=0;c<3;c++)
	{
		g_signal_handler_disconnect(photo->settings[c], photo->settings_signal[c]);
		g_object_unref(photo->settings[c]);
	}
	if (photo->crop)
		g_free(photo->crop);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
rs_photo_class_init (RS_PHOTOClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = rs_photo_dispose;
	gobject_class->finalize = rs_photo_finalize;

	signals[SETTINGS_CHANGED] = g_signal_new ("settings-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
	signals[SPATIAL_CHANGED] = g_signal_new ("spatial-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	signals[PROFILE_CHANGED] = g_signal_new ("profile-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, /* Is this right? */
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1, G_TYPE_POINTER);

	parent_class = g_type_class_peek_parent (klass);
}

static void
rs_photo_init (RS_PHOTO *photo)
{
	guint c;

	photo->filename = NULL;
	photo->input = NULL;
	ORIENTATION_RESET(photo->orientation);
	photo->priority = PRIO_U;
	photo->metadata = rs_metadata_new();
	for(c=0;c<3;c++)
	{
		photo->settings[c] = rs_settings_new();
		photo->settings_signal[c] = g_signal_connect(photo->settings[c], "settings-changed", G_CALLBACK(photo_settings_changed_cb), photo);
	}
	photo->crop = NULL;
	photo->angle = 0.0;
	photo->exported = FALSE;
}

static void
photo_settings_changed_cb(RSSettings *settings, RSSettingsMask mask, gpointer user_data)
{
	gint i;
	RS_PHOTO *photo = RS_PHOTO(user_data);

	if (mask)
		/* Find changed snapshot */
		for(i=0;i<3;i++)
			if (settings == photo->settings[i])
			{
				g_signal_emit(photo, signals[SETTINGS_CHANGED], 0, mask|(i<<24));
				break;
			}
}

/**
 * Allocates a new RS_PHOTO
 * @return A new RS_PHOTO
 */
RS_PHOTO *
rs_photo_new()
{
	RS_PHOTO *photo;

	photo = g_object_new(RS_TYPE_PHOTO, NULL);

	return photo;
}

/**
 * Rotates a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param quarterturns How many quarters to turn
 * @param angle The angle in degrees (360 is whole circle) to turn the image
 */
void
rs_photo_rotate(RS_PHOTO *photo, const gint quarterturns, const gdouble angle)
{
	gint n;

	if (!photo) return;

	photo->angle += angle;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_rotate(photo->crop, photo->crop, w, h, quarterturns);
	}

	for(n=0;n<quarterturns;n++)
		ORIENTATION_90(photo->orientation);

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Sets a new crop of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param crop The new crop or NULL to remove previous cropping
 */
void
rs_photo_set_crop(RS_PHOTO *photo, const RS_RECT *crop)
{
	if (!photo) return;

	if (photo->crop)
		g_free(photo->crop);
	photo->crop = NULL;

	if (crop)
	{
		photo->crop = g_new(RS_RECT, 1);
		*photo->crop = *crop;
	}

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Gets the crop of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return The crop as a RS_RECT or NULL if the photo is uncropped
 */
RS_RECT *
rs_photo_get_crop(RS_PHOTO *photo)
{
	if (!photo) return NULL;

	return photo->crop;
}

/**
 * Set the angle of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @param angle The new angle
 * @param relative If set to TRUE, angle will be relative to existing angle
 */
extern void
rs_photo_set_angle(RS_PHOTO *photo, gdouble angle, gboolean relative)
{
	gdouble previous;
	if (!photo) return;

	previous = photo->angle;

	if (relative)
		photo->angle += angle;
	else
		photo->angle = angle;

	if (ABS(previous - photo->angle) > 0.01)
		g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Get the angle of a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return The current angle
 */
extern gdouble
rs_photo_get_angle(RS_PHOTO *photo)
{
	if (!photo) return 0.0;

	return photo->angle;
}

/* Macro to create functions for getting single parameters */
#define RS_PHOTO_GET_GDOUBLE_VALUE(setting) \
gdouble \
rs_photo_get_##setting(RS_PHOTO *photo, const gint snapshot) \
{ \
	g_assert (RS_IS_PHOTO(photo)); \
	g_assert ((snapshot>=0) && (snapshot<=2)); \
	return photo->settings[snapshot]->setting; \
}

RS_PHOTO_GET_GDOUBLE_VALUE(exposure)
RS_PHOTO_GET_GDOUBLE_VALUE(saturation)
RS_PHOTO_GET_GDOUBLE_VALUE(hue)
RS_PHOTO_GET_GDOUBLE_VALUE(contrast)
RS_PHOTO_GET_GDOUBLE_VALUE(warmth)
RS_PHOTO_GET_GDOUBLE_VALUE(tint)
RS_PHOTO_GET_GDOUBLE_VALUE(sharpen)
RS_PHOTO_GET_GDOUBLE_VALUE(denoise_luma)
RS_PHOTO_GET_GDOUBLE_VALUE(denoise_chroma)

#undef RS_PHOTO_GET_GDOUBLE_VALUE

/* Macro to create functions for changing single parameters */
#define RS_PHOTO_SET_GDOUBLE_VALUE(setting, uppersetting) \
void \
rs_photo_set_##setting(RS_PHOTO *photo, const gint snapshot, const gdouble value) \
{ \
	/*if (!photo) return;*/ \
	/*g_return_if_fail ((snapshot>=0) && (snapshot<=2));*/ \
	photo->settings[snapshot]->setting = value; \
	g_signal_emit(photo, signals[SETTINGS_CHANGED], 0, MASK_##uppersetting|(snapshot<<24)); \
}

RS_PHOTO_SET_GDOUBLE_VALUE(exposure, EXPOSURE)
RS_PHOTO_SET_GDOUBLE_VALUE(saturation, SATURATION)
RS_PHOTO_SET_GDOUBLE_VALUE(hue, HUE)
RS_PHOTO_SET_GDOUBLE_VALUE(contrast, CONTRAST)
RS_PHOTO_SET_GDOUBLE_VALUE(warmth, WARMTH)
RS_PHOTO_SET_GDOUBLE_VALUE(tint, TINT)
RS_PHOTO_SET_GDOUBLE_VALUE(sharpen, SHARPEN)
/* FIXME: denoise! */

#undef RS_PHOTO_SET_GDOUBLE_VALUE

/**
 * Apply settings to a RS_PHOTO from a RSSettings
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param rs_settings The settings to apply
 * @param mask A mask for defining which settings to apply
 */
void
rs_photo_apply_settings(RS_PHOTO *photo, const gint snapshot, RSSettings *settings, RSSettingsMask mask)
{
	g_assert(RS_IS_PHOTO(photo));
	g_assert(RS_IS_SETTINGS(settings));
	g_assert((snapshot>=0) && (snapshot<=2));

	if (mask == 0)
		return;

	rs_settings_copy(settings, mask, photo->settings[snapshot]);

        /* Check if we need to update WB to camera or auto */
	gint i;
	for(i = 0; i < 3; i++)
	{
		if (mask & MASK_WB && photo->settings[i]->wb_ascii)
		{
			if (g_strcmp0(photo->settings[i]->wb_ascii, PRESET_WB_AUTO) == 0)
				rs_photo_set_wb_auto(photo, i);
			else if (g_strcmp0(photo->settings[i]->wb_ascii, PRESET_WB_CAMERA) == 0)
				rs_photo_set_wb_from_camera(photo, i);
		}
	}

}

/**
 * Flips a RS_PHOTO
 * @param photo A RS_PHOTO
 */
void
rs_photo_flip(RS_PHOTO *photo)
{
	if (!photo) return;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_flip(photo->crop, photo->crop, w, h);
	}
	ORIENTATION_FLIP(photo->orientation);

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Mirrors a RS_PHOTO
 * @param photo A RS_PHOTO
 */
void
rs_photo_mirror(RS_PHOTO *photo)
{
	if (!photo) return;

	if (photo->crop)
	{
		gint w,h;
		rs_image16_transform_getwh(photo->input, NULL, photo->angle, photo->orientation, &w, &h);
		rs_rect_mirror(photo->crop, photo->crop, w, h);
	}
	ORIENTATION_MIRROR(photo->orientation);

	g_signal_emit(photo, signals[SPATIAL_CHANGED], 0, NULL);
}

/**
 * Assign a DCP profile to a photo
 * @param photo A RS_PHOTO
 * @param dcp A DCP profile
 */
void
rs_photo_set_dcp_profile(RS_PHOTO *photo, RSDcpFile *dcp)
{
	g_assert(RS_IS_PHOTO(photo));

	photo->dcp = dcp;
	photo->icc = NULL;

	g_signal_emit(photo, signals[PROFILE_CHANGED], 0, photo->dcp);
}

/**
 * Get the assigned DCP profile for a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return A DCP profile or NULL
 */
extern RSDcpFile *rs_photo_get_dcp_profile(RS_PHOTO *photo)
{
	g_assert(RS_IS_PHOTO(photo));

	return photo->dcp;
}

/**
 * Assign a ICC profile to a photo
 * @param photo A RS_PHOTO
 * @param dcp An ICC profile
 */
void
rs_photo_set_icc_profile(RS_PHOTO *photo, RSIccProfile *icc)
{
	g_assert(RS_IS_PHOTO(photo));

	photo->icc = icc;
	photo->dcp = NULL;

	g_signal_emit(photo, signals[PROFILE_CHANGED], 0, photo->icc);
}

/**
 * Get the assigned ICC profile for a RS_PHOTO
 * @param photo A RS_PHOTO
 * @return An ICC profile or NULL
 */
RSIccProfile *rs_photo_get_icc_profile(RS_PHOTO *photo)
{
	g_assert(RS_IS_PHOTO(photo));

	return photo->icc;
}

/**
 * Sets the white balance of a RS_PHOTO using warmth and tint variables
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param warmth
 * @param tint
 */
void
rs_photo_set_wb_from_wt(RS_PHOTO *photo, const gint snapshot, const gdouble warmth, const gdouble tint)
{
	g_assert(RS_IS_PHOTO(photo));
	g_return_if_fail ((snapshot>=0) && (snapshot<=2));

	rs_settings_set_wb(photo->settings[snapshot], warmth, tint, NULL);

	g_signal_emit(photo, signals[SETTINGS_CHANGED], 0, MASK_WB|(snapshot<<24));
}

/**
 * Sets the white balance of a RS_PHOTO using multipliers
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param mul A pointer to an array of at least 3 multipliers
 */
void
rs_photo_set_wb_from_mul(RS_PHOTO *photo, const gint snapshot, const gdouble *mul, const gchar *ascii)
{
	gint c;
	gdouble max=0.0, warmth, tint;
	gdouble buf[3];

	g_assert(RS_IS_PHOTO(photo));
	g_return_if_fail ((snapshot>=0) && (snapshot<=2));
	g_assert(mul != NULL);

	for (c=0; c < 3; c++)
		buf[c] = mul[c];

	for (c=0; c < 3; c++)
		if (max < buf[c])
			max = buf[c];

	for(c=0;c<3;c++)
		buf[c] /= max;

	buf[R] *= (1.0/buf[G]);
	buf[B] *= (1.0/buf[G]);
	buf[G] = 1.0;

	tint = (buf[B] + buf[R] - 4.0)/-2.0;
	warmth = (buf[R]/(2.0-tint))-1.0;
	rs_settings_set_wb(photo->settings[snapshot], warmth, tint, ascii);
}

/**
 * Sets the white balance by neutralizing the colors provided
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @param r The red color
 * @param g The green color
 * @param b The blue color
 */
void
rs_photo_set_wb_from_color(RS_PHOTO *photo, const gint snapshot, const gdouble r, const gdouble g, const gdouble b)
{
	gdouble warmth, tint;

	g_assert(RS_IS_PHOTO(photo));
	g_return_if_fail ((snapshot>=0) && (snapshot<=2));

	warmth = (b-r)/(r+b); /* r*(1+warmth) = b*(1-warmth) */
	tint = -g/(r+r*warmth)+2.0; /* magic */

	rs_photo_set_wb_from_wt(photo, snapshot, warmth, tint);
}

/**
 * Autoadjust white balance of a RS_PHOTO using the greyworld algorithm
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 */
void
rs_photo_set_wb_auto(RS_PHOTO *photo, const gint snapshot)
{
	gint row, col, x, y, c, val;
	gint sum[8];
	gdouble pre_mul[4];
	gdouble dsum[8];

	g_assert(RS_IS_PHOTO(photo));
	g_return_if_fail ((snapshot>=0) && (snapshot<=2));

	for (c=0; c < 8; c++)
		dsum[c] = 0.0;

	for (row=0; row < photo->input->h-15; row += 8)
		for (col=0; col < photo->input->w-15; col += 8)
		{
			memset (sum, 0, sizeof sum);
			for (y=row; y < row+8; y++)
				for (x=col; x < col+8; x++)
					for(c=0;c<4;c++)
					{
						val = photo->input->pixels[y*photo->input->rowstride+x*4+c];
						if (!val) continue;
						if (val > 65100)
							goto skip_block; /* I'm sorry mom */
						sum[c] += val;
						sum[c+4]++;
					}
			for (c=0; c < 8; c++)
				dsum[c] += sum[c];
skip_block:
							continue;
		}
	for(c=0;c<4;c++)
		if (dsum[c])
			pre_mul[c] = dsum[c+4] / dsum[c];
	rs_photo_set_wb_from_mul(photo, snapshot, pre_mul, PRESET_WB_AUTO);
}

/**
 * Autoadjust white balance from the in-camera settings
 * @param photo A RS_PHOTO
 * @param snapshot Which snapshot to affect
 * @return TRUE on success, FALSE on error
 */
gboolean
rs_photo_set_wb_from_camera(RS_PHOTO *photo, const gint snapshot)
{
	gboolean ret = FALSE;

	g_assert(RS_IS_PHOTO(photo));

	if (!((snapshot>=0) && (snapshot<=2))) return FALSE;

	if (photo->metadata->cam_mul[R] != -1.0)
	{
		rs_photo_set_wb_from_mul(photo, snapshot, photo->metadata->cam_mul, PRESET_WB_CAMERA);
		ret = TRUE;
	}

	return ret;
}

/**
 * Loads a photo in to a RS_PHOTO including metadata
 * @param filename The filename to load
 * @return A RS_PHOTO on success, NULL on error
 */
RS_PHOTO *
rs_photo_load_from_file(const gchar *filename)
{
	RS_PHOTO *photo = NULL;
	RS_IMAGE16 *image;
	RSSettingsMask mask;
	gint i;

	image = rs_filetype_load(filename);
	if (image)
	{
		photo = rs_photo_new();

		/* Set filename */
		photo->filename = g_strdup(filename);

		/* Set input image */
		photo->input = image;
	}

	/* If photo available, read & process metadata */
	if (photo)
	{
		/* Load metadata */
		if (rs_metadata_load_from_file(photo->metadata, filename))
		{
			/* Rotate photo inplace */
			switch (photo->metadata->orientation)
			{
				case 90: ORIENTATION_90(photo->orientation);
					break;
				case 180: ORIENTATION_180(photo->orientation);
					break;
				case 270: ORIENTATION_270(photo->orientation);
					break;
			}
		}

		/* Load defaults */
		rs_camera_db_photo_set_defaults(rs_camera_db_get_singleton(), photo);

		/* Load cache */
		mask = rs_cache_load(photo);
		/* If we have no cache, try to set some sensible defaults */
		for (i=0;i<3;i++)
		{
			/* White balance */
			if (!(mask & MASK_WB))
				if (!rs_photo_set_wb_from_camera(photo, i))
					rs_photo_set_wb_auto(photo, i);

			/* Contrast */
			if (!(mask & MASK_CONTRAST) && (photo->metadata->contrast != -1.0))
				rs_photo_set_contrast(photo, i, photo->metadata->contrast);

			/* Saturation */
			if (!(mask & MASK_SATURATION) && (photo->metadata->saturation != -1.0))
				rs_photo_set_saturation(photo, i, photo->metadata->saturation);
		}
	}

	return photo;
}

/**
 * Get the metadata belonging to the RS_PHOTO
 * @param photo A RS_PHOTO
 * @return A RSMetadata, this must be unref'ed
 */
extern RSMetadata *rs_photo_get_metadata(RS_PHOTO *photo)
{
	return g_object_ref(photo->metadata);
}

/**
 * Closes a RS_PHOTO - this basically means saving cache
 * @param photo A RS_PHOTO
 */
void
rs_photo_close(RS_PHOTO *photo)
{
	if (!photo) return;

	rs_cache_save(photo, MASK_ALL);
}

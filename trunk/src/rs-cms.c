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

#include <glib/gstdio.h>
#include <config.h>
#include "application.h"
#include "conf_interface.h"
#include "rs-cms.h"

struct _RS_CMS {
	gboolean enabled;
	gint intent;
	void *genericLoadProfile;
	void *genericRGBProfile;
	void *transforms[TRANSFORMS];
	void *profiles[CMS_PROFILES];
	gchar *profile_filenames[CMS_PROFILES];
};

/* LCMS is not guaranteed to be thread-safe, we have to do this for now :( */
GStaticMutex cms_spinlock = G_STATIC_MUTEX_INIT;
#define CMS_LOCK() g_static_mutex_lock(&cms_spinlock)
#define CMS_UNLOCK() g_static_mutex_unlock(&cms_spinlock)

static gushort gammatable22[65536];
static cmsHPROFILE genericLoadProfile = NULL;
static cmsHPROFILE genericRGBProfile = NULL;

static void make_gammatable16(gushort *table, gdouble gamma);
static guchar *cms_pack_rgb_b(void *info, register WORD wOut[], register LPBYTE output);
static guchar *cms_pack_rgb_w(void *info, register WORD wOut[], register LPBYTE output);
static guchar *cms_unroll_rgb_w(void *info, register WORD wIn[], register LPBYTE accum);
static guchar *cms_unroll_rgb_w_gammatable22(void *info, register WORD wIn[], register LPBYTE accum);

void
rs_cms_enable(RS_CMS *cms, gboolean enable)
{
	cms->enabled = enable;
	rs_cms_prepare_transforms(cms);
}

gboolean
rs_cms_is_profile_valid(const gchar *path, const CMS_PROFILE profile)
{
	gboolean ret = FALSE;
	cmsHPROFILE lcms_profile;

	if (path)
	{
		CMS_LOCK();
		lcms_profile = cmsOpenProfileFromFile(path, "r");
		if (lcms_profile)
		{
			if (cmsGetColorSpace(lcms_profile) == 0x52474220) /* we only support RGB-profiles */
			{
				/* Perceptual is the fall-back method of LittleCMS */
				if (profile == CMS_PROFILE_INPUT)
					ret = cmsIsIntentSupported(lcms_profile, INTENT_PERCEPTUAL, LCMS_USED_AS_INPUT);
				else
					ret = cmsIsIntentSupported(lcms_profile, INTENT_PERCEPTUAL, LCMS_USED_AS_OUTPUT);
			}
			cmsCloseProfile(lcms_profile);
		}
		CMS_UNLOCK();
	}
	return(ret);
}

void
rs_cms_set_profile(RS_CMS *cms, CMS_PROFILE profile, const gchar *filename)
{
	if (profile > (CMS_PROFILES-1)) return;

	CMS_LOCK();

	/* free old filename */
	if (cms->profile_filenames[profile])
		g_free(cms->profile_filenames[profile]);
	cms->profile_filenames[profile] = NULL;

	/* free old profile */
	if (cms->profiles[profile])
		cmsCloseProfile(cms->profiles[profile]);
	cms->profiles[profile] = NULL;

	/* try to load new profile */
	if (filename)
		cms->profiles[profile] = cmsOpenProfileFromFile(filename, "r");

	/* if we could load it, save the filename */
	if (cms->profiles[profile])
		cms->profile_filenames[profile] = g_strdup(filename);

	CMS_UNLOCK();

	/* update transforms */
	rs_cms_prepare_transforms(cms);
	return;
}

void
rs_cms_set_intent(RS_CMS *cms, gint intent)
{
	cms->intent = intent;
	rs_cms_prepare_transforms(cms);
	return;
}

gint
rs_cms_get_intent(RS_CMS *cms)
{
	return(cms->intent);
}

void *
rs_cms_get_transform(RS_CMS *cms, CMS_TRANSFORM transform)
{
	if (!cms->enabled) return NULL;
	if (transform > (TRANSFORMS-1)) return(NULL);
	return(cms->transforms[transform]);
}

static gdouble
rs_cms_guess_gamma(void *transform)
{
	gushort buffer[27];
	gint n;
	gint lin = 0;
	gint g045 = 0;
	gdouble gamma = 1.0;

	gushort table_lin[] = {
		6553,   6553,  6553,
		13107, 13107, 13107,
		19661, 19661, 19661,
		26214, 26214, 26214,
		32768, 32768, 32768,
		39321, 39321, 39321,
		45875, 45875, 45875,
		52428, 52428, 52428,
		58981, 58981, 58981
	};
	const gushort table_g045[] = {
		  392,   392,   392,
		 1833,  1833,  1833,
		 4514,  4514,  4514,
		 8554,  8554,  8554,
		14045, 14045, 14045,
		21061, 21061, 21061,
		29665, 29665, 29665,
		39913, 39913, 39913,
		51855, 51855, 51855
	};
	CMS_LOCK();
	cmsDoTransform(transform, table_lin, buffer, 9);
	CMS_UNLOCK();
	for (n=0;n<9;n++)
	{
		lin += abs(buffer[n*3]-table_lin[n*3]);
		lin += abs(buffer[n*3+1]-table_lin[n*3+1]);
		lin += abs(buffer[n*3+2]-table_lin[n*3+2]);
		g045 += abs(buffer[n*3]-table_g045[n*3]);
		g045 += abs(buffer[n*3+1]-table_g045[n*3+1]);
		g045 += abs(buffer[n*3+2]-table_g045[n*3+2]);
	}
	if (g045 < lin)
		gamma = 2.2;

	return(gamma);
}

void
rs_cms_prepare_transforms(RS_CMS *cms)
{
	gfloat gamma;
	cmsHPROFILE in, di, ex;
	cmsHTRANSFORM testtransform;

	if (cms->enabled)
	{
		if (cms->profiles[CMS_PROFILE_INPUT] && cmsIsIntentSupported(cms->profiles[CMS_PROFILE_INPUT], cms->intent, LCMS_USED_AS_INPUT))
			in = cms->profiles[CMS_PROFILE_INPUT];
		else
			in = genericLoadProfile;

		if (cms->profiles[CMS_PROFILE_DISPLAY] && cmsIsIntentSupported(cms->profiles[CMS_PROFILE_DISPLAY], cms->intent, LCMS_USED_AS_OUTPUT))
			di = cms->profiles[CMS_PROFILE_DISPLAY];
		else
			di = genericRGBProfile;

		if (cms->profiles[CMS_PROFILE_EXPORT] && cmsIsIntentSupported(cms->profiles[CMS_PROFILE_EXPORT], cms->intent, LCMS_USED_AS_OUTPUT))
			ex = cms->profiles[CMS_PROFILE_EXPORT];
		else
			ex = genericRGBProfile;

		CMS_LOCK();
		if (cms->transforms[TRANSFORM_DISPLAY])
			cmsDeleteTransform(cms->transforms[TRANSFORM_DISPLAY]);
		cms->transforms[TRANSFORM_DISPLAY] = cmsCreateTransform(in, TYPE_RGB_16,
			di, TYPE_RGB_8, cms->intent, 0);

		if (cms->transforms[TRANSFORM_EXPORT])
			cmsDeleteTransform(cms->transforms[TRANSFORM_EXPORT]);
		cms->transforms[TRANSFORM_EXPORT] = cmsCreateTransform(in, TYPE_RGB_16,
			ex, TYPE_RGB_8, cms->intent, 0);

		if (cms->transforms[TRANSFORM_EXPORT16])
			cmsDeleteTransform(cms->transforms[TRANSFORM_EXPORT16]);
		cms->transforms[TRANSFORM_EXPORT16] = cmsCreateTransform(in, TYPE_RGB_16,
			ex, TYPE_RGB_16, cms->intent, 0);

		if (cms->transforms[TRANSFORM_SRGB])
			cmsDeleteTransform(cms->transforms[TRANSFORM_SRGB]);
		cms->transforms[TRANSFORM_SRGB] = cmsCreateTransform(in, TYPE_RGB_16,
			genericRGBProfile, TYPE_RGB_8, cms->intent, 0);

		testtransform = cmsCreateTransform(in, TYPE_RGB_16,
			genericLoadProfile, TYPE_RGB_16, cms->intent, 0);

		cmsSetUserFormatters(testtransform, TYPE_RGB_16, cms_unroll_rgb_w, TYPE_RGB_16, cms_pack_rgb_w);
		CMS_UNLOCK();
		gamma = rs_cms_guess_gamma(testtransform);
		CMS_LOCK();
		cmsDeleteTransform(testtransform);
		if (gamma != 1.0)
		{
			cmsSetUserFormatters(cms->transforms[TRANSFORM_DISPLAY], TYPE_RGB_16, cms_unroll_rgb_w_gammatable22, TYPE_RGB_8, cms_pack_rgb_b);
			cmsSetUserFormatters(cms->transforms[TRANSFORM_EXPORT], TYPE_RGB_16, cms_unroll_rgb_w_gammatable22, TYPE_RGB_8, cms_pack_rgb_b);
			cmsSetUserFormatters(cms->transforms[TRANSFORM_EXPORT16], TYPE_RGB_16, cms_unroll_rgb_w_gammatable22, TYPE_RGB_8, cms_pack_rgb_w);
			cmsSetUserFormatters(cms->transforms[TRANSFORM_SRGB], TYPE_RGB_16, cms_unroll_rgb_w_gammatable22, TYPE_RGB_8, cms_pack_rgb_b);
		}
		else
		{
			cmsSetUserFormatters(cms->transforms[TRANSFORM_DISPLAY], TYPE_RGB_16, cms_unroll_rgb_w, TYPE_RGB_8, cms_pack_rgb_b);
			cmsSetUserFormatters(cms->transforms[TRANSFORM_EXPORT], TYPE_RGB_16, cms_unroll_rgb_w, TYPE_RGB_8, cms_pack_rgb_b);
			cmsSetUserFormatters(cms->transforms[TRANSFORM_EXPORT16], TYPE_RGB_16, cms_unroll_rgb_w, TYPE_RGB_8, cms_pack_rgb_w);
			cmsSetUserFormatters(cms->transforms[TRANSFORM_SRGB], TYPE_RGB_16, cms_unroll_rgb_w, TYPE_RGB_8, cms_pack_rgb_b);
		}
		CMS_UNLOCK();
	}
	return;
}

void
rs_cms_do_transform(gpointer transform, gpointer input, gpointer output, guint size)
{
	if (transform && input && output && size)
		cmsDoTransform((cmsHTRANSFORM) transform, input, output, size);
}

RS_CMS *
rs_cms_init()
{
	RS_CMS *cms = g_new0(RS_CMS, 1);
	gint n;
	gchar *filename;
	cmsCIExyY D65;
	LPGAMMATABLE gamma[3];
	cmsCIExyYTRIPLE genericLoadPrimaries = { /* sRGB primaries */
		{0.64, 0.33, 0.212656},
		{0.115, 0.826, 0.724938},
		{0.157, 0.018, 0.016875}};

	CMS_LOCK();
	cmsErrorAction(LCMS_ERROR_IGNORE);
	cmsWhitePointFromTemp(6504, &D65);
	gamma[0] = gamma[1] = gamma[2] = cmsBuildGamma(2,1.0);

	/* set up builtin profiles */
	if (!genericRGBProfile)
		genericRGBProfile = cmsCreate_sRGBProfile();
	if (!genericLoadProfile)
		genericLoadProfile = cmsCreateRGBProfile(&D65, &genericLoadPrimaries, gamma);
	CMS_UNLOCK();

	/* initialize arrays */
	for (n=0;n<TRANSFORMS;n++)
		cms->transforms[n] = NULL;
	for (n=0;n<CMS_PROFILES;n++)
	{
		cms->profiles[n] = NULL;
		cms->profile_filenames[n] = NULL;
	}

	filename = rs_conf_get_cms_profile(CMS_PROFILE_INPUT);
	if (filename)
	{
		rs_cms_set_profile(cms, CMS_PROFILE_INPUT, filename);
		g_free(filename);
	}

	filename = rs_conf_get_cms_profile(CMS_PROFILE_DISPLAY);
	if (filename)
	{
		rs_cms_set_profile(cms, CMS_PROFILE_DISPLAY, filename);
		g_free(filename);
	}

	filename = rs_conf_get_cms_profile(CMS_PROFILE_EXPORT);
	if (filename)
	{
		rs_cms_set_profile(cms, CMS_PROFILE_EXPORT, filename);
		g_free(filename);
	}

	rs_cms_set_intent(cms, INTENT_PERCEPTUAL); /* default intent */
	rs_conf_get_cms_intent(CONF_CMS_INTENT, &cms->intent);

	cms->enabled = FALSE;
	rs_conf_get_boolean(CONF_CMS_ENABLED, &cms->enabled);
	rs_cms_prepare_transforms(cms);
	make_gammatable16(gammatable22, 2.2);
	return(cms);
}

static void
make_gammatable16(gushort *table, gdouble gamma)
{
	gint n;
	const gdouble gammavalue = (1.0/gamma);
	gdouble nd;
	gint res;

	for (n=0;n<0x10000;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		nd = pow(nd, gammavalue);
		res = (gint) (nd*65535.0);
		_CLAMP65535(res);
		table[n] = res;
	}
	return;
}

static guchar *
cms_pack_rgb_b(void *info, register WORD wOut[], register LPBYTE output)
{
	*output++ = RGB_16_TO_8(wOut[0]);
	*output++ = RGB_16_TO_8(wOut[1]);
	*output++ = RGB_16_TO_8(wOut[2]);
	return(output);
}

static guchar *
cms_pack_rgb_w(void *info, register WORD wOut[], register LPBYTE output)
{
	*(LPWORD) output = wOut[0]; output+= 2;
	*(LPWORD) output = wOut[1]; output+= 2;
	*(LPWORD) output = wOut[2]; output+= 2;
	return(output);
}

static guchar *
cms_unroll_rgb_w(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = *(LPWORD) accum; accum+= 2;
	wIn[1] = *(LPWORD) accum; accum+= 2;
	wIn[2] = *(LPWORD) accum; accum+= 2;
	return(accum);
}

static guchar *
cms_unroll_rgb_w_gammatable22(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = gammatable22[*(LPWORD) accum]; accum+= 2;
	wIn[1] = gammatable22[*(LPWORD) accum]; accum+= 2;
	wIn[2] = gammatable22[*(LPWORD) accum]; accum+= 2;
	return(accum);
}

gboolean
cms_get_profile_info_from_file(const gchar *filename, gchar **name, gchar **info, gchar **description)
{
	gboolean ret = FALSE;
	cmsHPROFILE profile;

	g_assert(filename);

	CMS_LOCK();
	if ((profile = cmsOpenProfileFromFile(filename, "r")))
	{
		if (name)
			*name = g_strdup(cmsTakeProductName(profile));

		if (info)
			*info = g_strdup(cmsTakeProductInfo(profile));

		if (description)
			*description = g_strdup(cmsTakeProductDesc(profile));

		ret = TRUE;
	}
	CMS_UNLOCK();

	return ret;
}

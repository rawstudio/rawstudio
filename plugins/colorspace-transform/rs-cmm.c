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

#include <lcms.h>
#include "rs-cmm.h"

static gushort gammatable22[65536];

struct _RSCmm {
	GObject parent;

	const RSIccProfile *input_profile;
	const RSIccProfile *output_profile;
	gint num_threads;

	gboolean dirty8;
	gboolean dirty16;

	gfloat premul[3];
	gushort clip[3];

	cmsHPROFILE lcms_input_profile;
	cmsHPROFILE lcms_output_profile;

	cmsHTRANSFORM lcms_transform8;
	cmsHTRANSFORM lcms_transform16;
};

G_DEFINE_TYPE (RSCmm, rs_cmm, G_TYPE_OBJECT)

static void load_profile(RSCmm *cmm, const RSIccProfile *profile, const RSIccProfile **profile_target, cmsHPROFILE *lcms_target);
static void prepare8(RSCmm *cmm);
static void prepare16(RSCmm *cmm);

static GMutex *is_profile_gamma_22_corrected_linear_lock = NULL;

static void
rs_cmm_dispose(GObject *object)
{
	G_OBJECT_CLASS(rs_cmm_parent_class)->dispose (object);
}

static void
rs_cmm_class_init(RSCmmClass *klass)
{
	gint n;
	gdouble nd;
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = rs_cmm_dispose;

	/* Build our 2.2 gamma table */
	for (n=0;n<65536;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		nd = pow(nd, 1.0/2.2);
		gammatable22[n] = CLAMP((gint) (nd*65535.0), 0, 65535);
	}

	/* GObject locking will protect us here */
	if (!is_profile_gamma_22_corrected_linear_lock)
		is_profile_gamma_22_corrected_linear_lock = g_mutex_new();
}

static void
rs_cmm_init(RSCmm *cmm)
{
}

RSCmm *
rs_cmm_new(void)
{
	return g_object_new(RS_TYPE_CMM, NULL);
}

void
rs_cmm_set_input_profile(RSCmm *cmm, const RSIccProfile *input_profile)
{
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_ICC_PROFILE(input_profile));

	load_profile(cmm, input_profile, &cmm->input_profile, &cmm->lcms_input_profile);
}

void
rs_cmm_set_output_profile(RSCmm *cmm, const RSIccProfile *output_profile)
{
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_ICC_PROFILE(output_profile));

	load_profile(cmm, output_profile, &cmm->output_profile, &cmm->lcms_output_profile);
}

void
rs_cmm_set_num_threads(RSCmm *cmm, const gint num_threads)
{
	g_assert(RS_IS_CMM(cmm));

	cmm->num_threads = MAX(1, num_threads);
}

void
rs_cmm_set_premul(RSCmm *cmm, const gfloat premul[3])
{
	g_assert(RS_IS_CMM(cmm));

	cmm->premul[R] = CLAMP(premul[R], 0.0001, 100.0);
	cmm->premul[G] = CLAMP(premul[G], 0.0001, 100.0);
	cmm->premul[B] = CLAMP(premul[B], 0.0001, 100.0);

	cmm->clip[R] = (gushort) 65535.0 / cmm->premul[R];
	cmm->clip[G] = (gushort) 65535.0 / cmm->premul[G];
	cmm->clip[B] = (gushort) 65535.0 / cmm->premul[B];
}

gboolean
rs_cmm_transform16(RSCmm *cmm, RS_IMAGE16 *input, RS_IMAGE16 *output)
{
	gushort *buffer;
	printf("rs_cms_transform16()\n");
	gint y, x;
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_IMAGE16(input));
	g_assert(RS_IS_IMAGE16(output));

	g_return_val_if_fail(input->w == output->w, FALSE);
	g_return_val_if_fail(input->h == output->h, FALSE);
	g_return_val_if_fail(input->pixelsize == 4, FALSE);

	if (cmm->dirty16)
		prepare16(cmm);

	buffer = g_new(gushort, input->w * 4);
	for(y=0;y<input->h;y++)
	{
		gushort *in = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);
		gushort *buffer_pointer = buffer;
		for(x=0;x<input->w;x++)
		{
			register gfloat r = (gfloat) MIN(*in, cmm->clip[R]); in++;
			register gfloat g = (gfloat) MIN(*in, cmm->clip[G]); in++;
			register gfloat b = (gfloat) MIN(*in, cmm->clip[B]); in++;
			in++;

			r = MIN(r, cmm->clip[R]);
			g = MIN(g, cmm->clip[G]);
			b = MIN(b, cmm->clip[B]);

			r = r * cmm->premul[R];
			g = g * cmm->premul[G];
			b = b * cmm->premul[B];

			r = MIN(r, 65535.0);
			g = MIN(g, 65535.0);
			b = MIN(b, 65535.0);

			*(buffer_pointer++) = (gushort) r;
			*(buffer_pointer++) = (gushort) g;
			*(buffer_pointer++) = (gushort) b;
			buffer_pointer++;
		}
		cmsDoTransform(cmm->lcms_transform16, buffer, out, input->w);
	}
	g_free(buffer);
	return TRUE;
}

gboolean
rs_cmm_transform8(RSCmm *cmm, RS_IMAGE16 *input, GdkPixbuf *output)
{
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_IMAGE16(input));
	g_assert(GDK_IS_PIXBUF(output));

	g_return_val_if_fail(input->w == gdk_pixbuf_get_width(output), FALSE);
	g_return_val_if_fail(input->h == gdk_pixbuf_get_height(output), FALSE);
	g_return_val_if_fail(input->pixelsize == 4, FALSE);

	if (cmm->dirty8)
		prepare8(cmm);

	/* FIXME: Render */
	g_warning("rs_cmm_transform8() is a stub");

	return TRUE;
}

static guchar *
pack_rgb_w4(void *info, register WORD wOut[], register LPBYTE output)
{
	*(LPWORD) output = wOut[0]; output+= 2;
	*(LPWORD) output = wOut[1]; output+= 2;
	*(LPWORD) output = wOut[2]; output+= 4;

	return(output);
}

static guchar *
unroll_rgb_w4(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = *(LPWORD) accum; accum+= 2;
	wIn[1] = *(LPWORD) accum; accum+= 2;
	wIn[2] = *(LPWORD) accum; accum+= 4;

	return(accum);
}

static guchar *
unroll_rgb_w4_gammatable22(void *info, register WORD wIn[], register LPBYTE accum)
{
	wIn[0] = gammatable22[*(LPWORD) accum]; accum+= 2;
	wIn[1] = gammatable22[*(LPWORD) accum]; accum+= 2;
	wIn[2] = gammatable22[*(LPWORD) accum]; accum+= 4;

	return(accum);
}

static void
load_profile(RSCmm *cmm, const RSIccProfile *profile, const RSIccProfile **profile_target, cmsHPROFILE *lcms_target)
{
// DEBUG START
	gchar *filename;
	g_object_get((void *) profile, "filename", &filename, NULL);
	printf("load_profile(%p [%s])\n", profile, filename);
// DEBUG END
	gchar *data;
	gsize length;

	if (*profile_target == profile)
		return;

	*profile_target = profile;

	if (*lcms_target)
		cmsCloseProfile(*lcms_target);

	if (rs_icc_profile_get_data(profile, &data, &length))
		*lcms_target = cmsOpenProfileFromMem(data, length);

	g_warn_if_fail(*lcms_target != NULL);

	cmm->dirty8 = TRUE;
	cmm->dirty16 = TRUE;
	printf("load_profile() DONE\n");
}

static void
prepare8(RSCmm *cmm)
{
}

static gboolean
is_profile_gamma_22_corrected(cmsHPROFILE *profile)
{
	cmsHTRANSFORM testtransform;
	cmsHPROFILE linear = NULL;
	gint n;
	gint lin = 0;
	gint g045 = 0;

	gushort buffer[27];
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

	g_mutex_lock(is_profile_gamma_22_corrected_linear_lock);
	if (linear == NULL)
	{
		static cmsCIExyYTRIPLE srgb_primaries = {
			{0.64, 0.33, 0.212656},
			{0.115, 0.826, 0.724938},
			{0.157, 0.018, 0.016875}};
		cmsCIExyY D65;
		LPGAMMATABLE gamma[3];

		cmsWhitePointFromTemp(6504, &D65);
		gamma[0] = gamma[1] = gamma[2] = cmsBuildGamma(2,1.0);
		linear = cmsCreateRGBProfile(&D65, &srgb_primaries, gamma);
	}
	g_mutex_unlock(is_profile_gamma_22_corrected_linear_lock);

	testtransform = cmsCreateTransform(profile, TYPE_RGB_16, linear, TYPE_RGB_16, INTENT_PERCEPTUAL, 0);
	cmsDoTransform(testtransform, table_lin, buffer, 9);
	cmsDeleteTransform(testtransform);

	/* We compare the transformed values to the original linear values to
	   determine if we're closer to a gamma 2.2 correction or 1.0 */
	for (n=0;n<9;n++)
	{
		lin += abs(buffer[n*3]-table_lin[n*3]);
		lin += abs(buffer[n*3+1]-table_lin[n*3+1]);
		lin += abs(buffer[n*3+2]-table_lin[n*3+2]);
		g045 += abs(buffer[n*3]-table_g045[n*3]);
		g045 += abs(buffer[n*3+1]-table_g045[n*3+1]);
		g045 += abs(buffer[n*3+2]-table_g045[n*3+2]);
	}

	return (g045 < lin);
}

static void
prepare16(RSCmm *cmm)
{
	if (!cmm->dirty16)
		return;

	if (cmm->lcms_transform16)
		cmsDeleteTransform(cmm->lcms_transform16);

	cmm->lcms_transform16 = cmsCreateTransform(
		cmm->lcms_input_profile, TYPE_RGB_16,
		cmm->lcms_output_profile, TYPE_RGB_16,
		INTENT_PERCEPTUAL, 0);

	g_warn_if_fail(cmm->lcms_transform16 != NULL);

	/* Enable packing/unpacking for pixelsize==4 */
	/* If we estimate that the input profile will apply gamma correction,
	   we try to undo it in 16 bit transform */
	if (is_profile_gamma_22_corrected(cmm->lcms_input_profile))
		cmsSetUserFormatters(cmm->lcms_transform16,
			TYPE_RGB_16, unroll_rgb_w4_gammatable22,
			TYPE_RGB_16, pack_rgb_w4);
	else
		cmsSetUserFormatters(cmm->lcms_transform16,
			TYPE_RGB_16, unroll_rgb_w4,
			TYPE_RGB_16, pack_rgb_w4);

	cmm->dirty16 = FALSE;
}

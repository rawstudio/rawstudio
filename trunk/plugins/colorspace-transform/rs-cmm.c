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

#include <lcms2.h>
#include <math.h>
#include <stdlib.h>
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
	const GdkRectangle *roi;
	gboolean is_gamma_corrected;
};

G_DEFINE_TYPE (RSCmm, rs_cmm, G_TYPE_OBJECT)

static void load_profile(RSCmm *cmm, const RSIccProfile *profile, const RSIccProfile **profile_target, cmsHPROFILE *lcms_target);
static void prepare8(RSCmm *cmm);
static void prepare16(RSCmm *cmm);

static GMutex *is_profile_gamma_22_corrected_linear_lock = NULL;

typedef struct {
	RSCmm *cmm;
	GThread *threadid;
	gint start_y;
	gint end_y;
	gint start_x;
	gint end_x;
	RS_IMAGE16 *input;
	void *output;
	gboolean sixteen16;
} ThreadInfo;


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
	cmm->num_threads = 1;
}

RSCmm *
rs_cmm_new(void)
{
	return g_object_new(RS_TYPE_CMM, NULL);
}

void
rs_cmm_set_roi(RSCmm *cmm, const GdkRectangle *roi)
{
	cmm->roi = roi;
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

void
rs_cmm_transform16(RSCmm *cmm, RS_IMAGE16 *input, RS_IMAGE16 *output, gint start_x, gint end_x, gint start_y, gint end_y)
{
	gushort *buffer;
	gint y, x, w;
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_IMAGE16(input));
	g_assert(RS_IS_IMAGE16(output));

	g_return_if_fail(input->w == output->w);
	g_return_if_fail(input->h == output->h);
	g_return_if_fail(input->pixelsize == 4);
	w = end_x - start_x;

	buffer = g_new(gushort, w * 4);
	for(y=start_y;y<end_y;y++)
	{
		gushort *in = GET_PIXEL(input, start_x, y);
		gushort *out = GET_PIXEL(output, start_x, y);
		gushort *buffer_pointer = buffer;
		if (cmm->is_gamma_corrected)
		{
			for(x=start_x; x<end_x;x++)
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

				*(buffer_pointer++) = gammatable22[(gushort) r];
				*(buffer_pointer++) = gammatable22[(gushort) g];
				*(buffer_pointer++) = gammatable22[(gushort) b];
				buffer_pointer++;
			}
		} 
		else
		{
			for(x=start_x; x<end_x;x++)
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
		}
		cmsDoTransform(cmm->lcms_transform16, buffer, out, w);
	}
	g_free(buffer);
}

void
rs_cmm_transform8(RSCmm *cmm, RS_IMAGE16 *input, GdkPixbuf *output, gint start_x, gint end_x, gint start_y, gint end_y)
{
	gint y,i,w;
	g_assert(RS_IS_CMM(cmm));
	g_assert(RS_IS_IMAGE16(input));
	g_assert(GDK_IS_PIXBUF(output));

	g_return_if_fail(input->w == gdk_pixbuf_get_width(output));
	g_return_if_fail(input->h == gdk_pixbuf_get_height(output));
	g_return_if_fail(input->pixelsize == 4);
	w = end_x - start_x;

	for(y=start_y;y<end_y;y++)
	{
		gushort *in = GET_PIXEL(input, start_x, y);
		guchar *out = GET_PIXBUF_PIXEL(output, start_x, y);
		cmsDoTransform(cmm->lcms_transform8, in, out, w);
		/* Set alpha */
		guint *outi = (guint*) out;
		for (i = 0; i < w; i++)
			outi[i] &= 0xff000000;
	}
}

gpointer
start_single_transform_thread(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;

	if (t->sixteen16)
	{
		rs_cmm_transform16(t->cmm, t->input, t->output, t->start_x, t->end_x, t->start_y, t->end_y);
	}
	else /* 16 -> 8 bit */
	{
		rs_cmm_transform8(t->cmm, t->input, t->output, t->start_x, t->end_x, t->start_y, t->end_y);
	}
	return NULL;
}

void
rs_cmm_transform(RSCmm *cmm, RS_IMAGE16 *input, void *output, gboolean sixteen_to_16)
{
	gint i;
	guint y_offset, y_per_thread, threaded_h;
	gint threads = cmm->num_threads;
	ThreadInfo *t = g_new(ThreadInfo, threads);

	const GdkRectangle *roi = cmm->roi;
	threaded_h = roi->height;
	y_per_thread = (threaded_h + threads-1)/threads;
	y_offset = roi->y;

	if (sixteen_to_16)
	{
		if (cmm->dirty16)
			prepare16(cmm);
	}
	else
	{
		if (cmm->dirty8)
			prepare8(cmm);
	}

	for (i = 0; i < threads; i++)
	{
		t[i].cmm = cmm;
		t[i].sixteen16 = sixteen_to_16;
		t[i].input = input;
		t[i].output = output;
		t[i].start_y = y_offset;
		t[i].start_x = roi->x;
		t[i].end_x = roi->x + roi->width;
		y_offset += y_per_thread;
		y_offset = MIN(input->h, y_offset);
		t[i].end_y = y_offset;

		t[i].threadid = g_thread_create(start_single_transform_thread, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);
}

static void
load_profile(RSCmm *cmm, const RSIccProfile *profile, const RSIccProfile **profile_target, cmsHPROFILE *lcms_target)
{
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
}

static void
prepare8(RSCmm *cmm)
{
	if (!cmm->dirty8)
		return;

	if (cmm->lcms_transform8)
		cmsDeleteTransform(cmm->lcms_transform8);

	cmm->lcms_transform8 = cmsCreateTransform(
		cmm->lcms_input_profile, TYPE_RGBA_16,
		cmm->lcms_output_profile, TYPE_RGBA_8,
		INTENT_PERCEPTUAL, 0);

	g_warn_if_fail(cmm->lcms_transform8 != NULL);
	cmm->dirty8 = FALSE;
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
		cmsToneCurve* gamma[3];
		gint context = 1337;

		cmsWhitePointFromTemp(&D65, 6504);
		gamma[0] = gamma[1] = gamma[2] = cmsBuildGamma(&context,1.0);
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
		cmm->lcms_input_profile, TYPE_RGBA_16,
		cmm->lcms_output_profile, TYPE_RGBA_16,
		INTENT_PERCEPTUAL, cmsFLAGS_NOCACHE);

	g_warn_if_fail(cmm->lcms_transform16 != NULL);

	/* Enable packing/unpacking for pixelsize==4 */
	/* If we estimate that the input profile will apply gamma correction,
	   we try to undo it in 16 bit transform */
	cmm->is_gamma_corrected = is_profile_gamma_22_corrected(cmm->lcms_input_profile);

	cmm->dirty16 = FALSE;
}

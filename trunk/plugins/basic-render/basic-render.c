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

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include <math.h> /* pow() */
#include <lcms.h>
#include <config.h>

#define RS_TYPE_BASIC_RENDER (rs_basic_render_type)
#define RS_BASIC_RENDER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_BASIC_RENDER, RSBasicRender))
#define RS_BASIC_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_BASIC_RENDER, RSBasicRenderClass))
#define RS_IS_BASIC_RENDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_BASIC_RENDER))
#define RS_IS_BASIC_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_BASIC_RENDER))
#define RS_BASIC_RENDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_BASIC_RENDER, RSBasicRenderClass))

static const gfloat top[4] align(16) = {65535.f, 65535.f, 65535.f, 65535.f};

typedef struct _RSBasicRender RSBasicRender;
typedef struct _RSBasicRenderClass RSBasicRenderClass;

struct _RSBasicRender {
	RSFilter parent;
	gboolean dispose_has_run;

	gboolean dirty_tables;
	gboolean dirty_matrix;
	gboolean dirty_lcms;

	RSSettings *settings;
	gulong settings_signal_id;

	gfloat gamma;

	/* These will be kept in sync with "settings" */
	gfloat exposure;
	gfloat saturation;
	gfloat hue;
	gfloat contrast;
	gfloat warmth;
	gfloat tint;

	gfloat pre_mul[4] align(16);
//	guint bits_per_color;
//	guint pixelsize;
	RS_MATRIX4 color_matrix;
//	RS_MATRIX4 adobe_matrix;
	guchar *table8;
	gushort *table16;
	gint nknots;
	gfloat *curve_samples;
	void *cms_transform;
	RSIccProfile *icc_profile;
	cmsHPROFILE lcms_input_profile;
	cmsHPROFILE lcms_output_profile;
	cmsHTRANSFORM lcms_transform8;
	cmsHTRANSFORM lcms_transform16;
};

/*
 Little-CMS FIXME's:
  - Check guess_gamma().
  - Check types of profiles.
  - Fix 16 bit output.
  - Fix output on non SSE-capable platforms.
*/

struct _RSBasicRenderClass {
	RSFilterClass parent_class;
	gpointer (*thread_func16)(gpointer _thread_info);
	gpointer (*thread_func8)(gpointer _thread_info);
	gpointer (*thread_func8_cms)(gpointer _thread_info);
};

typedef struct {
	RSBasicRender *basic_render;
	GThread *threadid;
	gint width;
	gint height;
	gushort *in;
	gint in_rowstride;
	gint in_pixelsize;
	void *out;
	gint out_rowstride;
} ThreadInfo;

RS_DEFINE_FILTER(rs_basic_render, RSBasicRender)

enum {
	PROP_0,
	PROP_GAMMA,
	PROP_EXPOSURE,
	PROP_SETTINGS,
	PROP_ICC_PROFILE
};

static void dispose(GObject *object);
static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSBasicRender *basic_render);
static void settings_weak_notify(gpointer data, GObject *where_the_object_was);
static void render_tables(RSBasicRender *basic_render);
static void render_matrix(RSBasicRender *basic_render);
static gpointer thread_func_float16(gpointer _thread_info);
static gpointer thread_func_float8(gpointer _thread_info);
#if defined (__i386__) || defined (__x86_64__)
static gpointer thread_func_sse8(gpointer _thread_info);
static gpointer thread_func_sse8_cms(gpointer _thread_info);
#endif /* __i386__ || __x86_64__ */
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static RSFilterResponse *get_image8(RSFilter *filter, const RSFilterRequest *request);
static RSIccProfile *get_icc_profile(RSFilter *filter);

static RSFilterClass *rs_basic_render_parent_class = NULL;

/* FIXME: finalize
	basic_render(rct->curve_samples);
	basic_render(rct->table8);
	basic_render(rct->table16);
*/

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_basic_render_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_basic_render_class_init(RSBasicRenderClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_basic_render_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	g_object_class_install_property(object_class,
		PROP_GAMMA, g_param_spec_float(
			"gamma", "Gamma", "Gamma to render output as",
			0.1, 10.0, 2.2, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_SETTINGS, g_param_spec_object(
			"settings", "Settings", "Settings to render from",
			RS_TYPE_SETTINGS, G_PARAM_READWRITE)
	);

	g_object_class_install_property(object_class,
		PROP_ICC_PROFILE, g_param_spec_object(
		"icc-profile", "icc-profile", "ICC Profile",
		RS_TYPE_ICC_PROFILE, G_PARAM_READWRITE));

	filter_class->name = "BasicRender filter";
	filter_class->get_image = get_image;
	filter_class->get_image8 = get_image8;
	filter_class->get_icc_profile = get_icc_profile;
	filter_class->previous_changed = previous_changed;

	klass->thread_func16 = thread_func_float16;
	klass->thread_func8 = thread_func_float8;
	klass->thread_func8_cms = thread_func_float8; /* FIXME: Implement non-SSE version */
#if defined (__i386__) || defined (__x86_64__)
	/* Use SSE version if possible */
	if (rs_detect_cpu_features() & RS_CPU_FLAG_SSE)
	{
		klass->thread_func8 = thread_func_sse8;
		klass->thread_func8_cms = thread_func_sse8_cms;
	}
#endif /* __i386__ || __x86_64__ */
}

static void
rs_basic_render_init(RSBasicRender *basic_render)
{
	gint i;
	RSSettings *settings = rs_settings_new();

	/* Initialize default from RSSettings */
	g_object_get(settings, 
		"exposure", &basic_render->exposure,
		"saturation", &basic_render->saturation,
		"hue", &basic_render->hue,
		"contrast", &basic_render->contrast,
		"warmth", &basic_render->warmth,
		"tint", &basic_render->tint,
		NULL);
	settings_changed(settings, MASK_ALL, basic_render);
	g_object_unref(settings);

	basic_render->gamma = 2.2;
	basic_render->dirty_tables = TRUE;
	basic_render->dirty_matrix = TRUE;
	basic_render->table8 = g_new(guchar, 65536);
	basic_render->table16 = g_new(gushort, 65536);
	basic_render->nknots = 0;
	basic_render->curve_samples = g_new(gfloat, 65536);

	for(i=0;i<65536;i++)
		basic_render->curve_samples[i] = ((gfloat)i)/65536.0;

	for(i=0;i<4;i++)
		basic_render->pre_mul[i] = 1.0;

	matrix4_identity(&basic_render->color_matrix);

	basic_render->settings = NULL;

	basic_render->icc_profile = NULL;
	basic_render->lcms_input_profile = NULL;
	basic_render->lcms_output_profile = NULL;
	basic_render->lcms_transform8 = NULL;
	basic_render->lcms_transform16 = NULL;
	basic_render->dirty_lcms = TRUE;
}

static void
dispose(GObject *object)
{
	RSBasicRender *basic_render = RS_BASIC_RENDER(object);

	if (!basic_render->dispose_has_run)
	{
		basic_render->dispose_has_run = TRUE;

		if (basic_render->settings && basic_render->settings_signal_id)
			g_signal_handler_disconnect(basic_render->settings, basic_render->settings_signal_id);
	}
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSBasicRender *basic_render = RS_BASIC_RENDER(object);

	switch (property_id)
	{
		case PROP_GAMMA:
			g_value_set_float(value, basic_render->gamma);
			break;
		case PROP_SETTINGS:
			g_value_set_object(value, basic_render->settings);
			break;
		case PROP_ICC_PROFILE:
			g_value_set_object(value, basic_render->icc_profile);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSBasicRender *basic_render = RS_BASIC_RENDER(object);

	switch (property_id)
	{
		case PROP_GAMMA:
			basic_render->gamma = g_value_get_float(value);
			basic_render->dirty_tables = TRUE;
			rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_SETTINGS:
			if (basic_render->settings && basic_render->settings_signal_id)
				g_signal_handler_disconnect(basic_render->settings, basic_render->settings_signal_id);
			basic_render->settings = g_value_get_object(value);
			basic_render->settings_signal_id = g_signal_connect(basic_render->settings, "settings-changed", G_CALLBACK(settings_changed), basic_render);

			/* FIXME: Quick hack to force updating RSBasicRender before RSSettings
			 * sends a "settings-changed"-signal. Should be replaced by some
			 * dirty_settings mechanics. */
			settings_changed(basic_render->settings, MASK_ALL, basic_render);
			g_object_weak_ref(G_OBJECT(basic_render->settings), settings_weak_notify, basic_render);
			rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_ICC_PROFILE:

			if (basic_render->icc_profile)
				g_object_unref(basic_render->icc_profile);
			basic_render->icc_profile = g_object_ref(g_value_get_object(value));

			if (basic_render->lcms_output_profile)
			{
				cmsCloseProfile(basic_render->lcms_output_profile);
				basic_render->lcms_output_profile = NULL;
			}

			basic_render->dirty_lcms = TRUE;
			rs_filter_changed(RS_FILTER(basic_render), RS_FILTER_CHANGED_ICC_PROFILE|RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RSIccProfile *
get_icc_profile(RSFilter *filter)
{
	RSBasicRender *basic_render = RS_BASIC_RENDER(filter);

	if (basic_render->icc_profile)
		return g_object_ref(basic_render->icc_profile);
	else
		return rs_filter_get_icc_profile(filter->previous);
}

static void
previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask)
{
	RSBasicRender *basic_render = RS_BASIC_RENDER(filter);
	if (mask & RS_FILTER_CHANGED_ICC_PROFILE)
	{
		if (basic_render->lcms_input_profile)
		{
			cmsCloseProfile(basic_render->lcms_input_profile);
			basic_render->lcms_input_profile = NULL;
		}
		basic_render->dirty_lcms = TRUE;
		rs_filter_changed(filter, RS_FILTER_CHANGED_ICC_PROFILE | RS_FILTER_CHANGED_PIXELDATA);
	}
	else
		rs_filter_changed(filter, mask);
}

static void
settings_changed(RSSettings *settings, RSSettingsMask mask, RSBasicRender *basic_render)
{
	gboolean changed = FALSE;

	if (mask & (MASK_EXPOSURE|MASK_SATURATION|MASK_HUE))
	{
		g_object_get(settings,
			"exposure", &basic_render->exposure,
			"saturation", &basic_render->saturation,
			"hue", &basic_render->hue,
			NULL);
		basic_render->dirty_matrix = TRUE;
		changed = TRUE;

	}

	if (mask & MASK_CONTRAST)
	{
		gfloat contrast;
		g_object_get(settings, "contrast", &contrast, NULL);

		if (basic_render->contrast != contrast)
		{
			basic_render->contrast = contrast;
			basic_render->dirty_tables = TRUE;
			changed = TRUE;
		}
	}

	if ((mask & MASK_WB) || (mask & MASK_CHANNELMIXER))
	{
		const gfloat warmth;
		const gfloat tint;
		const gfloat channelmixer_red;
		const gfloat channelmixer_green;
		const gfloat channelmixer_blue;

		g_object_get(settings,
			"warmth", &warmth,
			"tint", &tint,
			"channelmixer_red", &channelmixer_red,
			"channelmixer_green", &channelmixer_green,
			"channelmixer_blue", &channelmixer_blue,
			NULL);

        basic_render->pre_mul[R] = (1.0+warmth)*(2.0-tint)*(channelmixer_red/100.0);
        basic_render->pre_mul[G] = 1.0*(channelmixer_green/100.0);
        basic_render->pre_mul[B] = (1.0-warmth)*(2.0-tint)*(channelmixer_blue/100.0);
        basic_render->pre_mul[G2] = 1.0*(channelmixer_green/100.0);

		changed = TRUE;
	}

	if (mask & MASK_CURVE)
	{
		const gint nknots = rs_settings_get_curve_nknots(settings);

		if (nknots > 1)
		{
			gfloat *knots = rs_settings_get_curve_knots(settings);
			if (knots)
			{
				basic_render->nknots = nknots;
				RSSpline *spline = rs_spline_new(knots, basic_render->nknots, NATURAL);
				rs_spline_sample(spline, basic_render->curve_samples, 65536);
				g_object_unref(spline);
				g_free(knots);
				basic_render->dirty_tables = TRUE;
			}
		}
		else
		{
			gint i;
			for(i=0;i<65536;i++)
				basic_render->curve_samples[i] = ((gfloat)i)/65536.0;
		}
		changed = TRUE;
	}

	if (changed)
		rs_filter_changed(RS_FILTER(basic_render), RS_FILTER_CHANGED_PIXELDATA);
}

static void
settings_weak_notify(gpointer data, GObject *where_the_object_was)
{
	RSBasicRender *basic_render = RS_BASIC_RENDER(data);

	basic_render->settings = NULL;
}

static void
render_tables(RSBasicRender *basic_render)
{
	static const gdouble rec65535 = (1.0f / 65536.0f);
	register gint n;
	gdouble nd;
	register gint res;
	const gdouble contrast = basic_render->contrast + 0.01f; /* magic */
	const gdouble postadd = 0.5f - (contrast/2.0f);
	const gdouble gammavalue = (1.0f/basic_render->gamma);

	if (!basic_render->dirty_tables)
		return;

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) * rec65535;
		nd = pow(nd, gammavalue);

		if (likely(basic_render->curve_samples))
			nd = (gdouble) basic_render->curve_samples[((gint) (nd*65535.0f))];

		nd = nd*contrast + postadd;

		/* 8 bit output */
		res = (gint) (nd*255.0f);
		_CLAMP255(res);
		basic_render->table8[n] = res;

		/* 16 bit output */
		res = (gint) (nd*65535.0f);
		_CLAMP65535(res);
		basic_render->table16[n] = res;
	}

	basic_render->dirty_tables = FALSE;
}

static void
render_matrix(RSBasicRender *basic_render)
{
	if (!basic_render->dirty_matrix)
		return;

	/* FIXME: Deal with CMS! */
//			rct->color_matrix = rct->adobe_matrix;

	matrix4_identity(&basic_render->color_matrix);
	matrix4_color_exposure(&basic_render->color_matrix, basic_render->exposure);
	matrix4_color_saturate(&basic_render->color_matrix, basic_render->saturation);
	matrix4_color_hue(&basic_render->color_matrix, basic_render->hue);

	basic_render->dirty_matrix = FALSE;
}

static cmsHPROFILE
lcms_profile_from_rs_icc_profile(RSIccProfile *profile)
{
	cmsHPROFILE ret;
	gchar *map;
	gsize map_length;

	rs_icc_profile_get_data(profile, &map, &map_length);

	ret = cmsOpenProfileFromMem(map, map_length);

	g_free(map);

	return ret;
}

static gdouble
lcms_guess_gamma(void *transform)
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
	cmsDoTransform(transform, table_lin, buffer, 9);
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

static void
prepare_lcms(RSBasicRender *basic_render)
{
	/* FIXME: Clean up all this at some point */
	if (!basic_render->dirty_lcms)
		return;

	if (!basic_render->lcms_input_profile)
	{
		RSIccProfile *previous_profile = rs_filter_get_icc_profile(RS_FILTER(basic_render)->previous);
		if (previous_profile)
			basic_render->lcms_input_profile = lcms_profile_from_rs_icc_profile(previous_profile);
		g_object_unref(previous_profile);
	}

	if (basic_render->icc_profile && !basic_render->lcms_output_profile)
	{
		basic_render->lcms_output_profile = lcms_profile_from_rs_icc_profile(basic_render->icc_profile);
	}

	if (basic_render->lcms_input_profile && basic_render->lcms_output_profile)
	{
		/* Free transforms */
		if (basic_render->lcms_transform8)
			cmsDeleteTransform(basic_render->lcms_transform8);
		if (basic_render->lcms_transform16)
			cmsDeleteTransform(basic_render->lcms_transform16);

		basic_render->lcms_transform8 = cmsCreateTransform(
			basic_render->lcms_input_profile, TYPE_RGB_16,
			basic_render->lcms_output_profile, TYPE_RGB_8,
			INTENT_PERCEPTUAL, 0);
		basic_render->lcms_transform16 = cmsCreateTransform(
			basic_render->lcms_input_profile, TYPE_RGB_16,
			basic_render->lcms_output_profile, TYPE_RGB_16,
			INTENT_PERCEPTUAL, 0);
	}

	/* FIXME: Build all this gamma compensating crap directly into the tables
	   generated by RSBasicRender and port guess_gamma() from old render */
	if (basic_render->lcms_transform16 && basic_render->lcms_transform8)
//	{
//		basic_render->gamma = 2.2;
//		g_object_set(basic_render, "gamma", 2.2, NULL);
//		basic_render->dirty_tables = TRUE;
//		render_tables(basic_render);
//	}
	{
		cmsHPROFILE generic = cmsOpenProfileFromFile(PACKAGE_DATA_DIR "/" PACKAGE "/profiles/generic_camera_profile.icc", "r");
		cmsHTRANSFORM testtransform = cmsCreateTransform(
			basic_render->lcms_input_profile, TYPE_RGB_16,
			generic, TYPE_RGB_16,
			INTENT_PERCEPTUAL, 0);
		gdouble gamma = lcms_guess_gamma(testtransform);

		/* This seems fucked. Why isn't this reversed?! */
		if (gamma == 1.0)
		{
			basic_render->gamma = 1.0;
			basic_render->dirty_tables = TRUE;
			render_tables(basic_render);
		}
		else
		{
			basic_render->gamma = 2.2;
			basic_render->dirty_tables = TRUE;
			render_tables(basic_render);
		}
	}

	basic_render->dirty_lcms = FALSE;
}

static gpointer
thread_func_float16(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	gint srcoffset;
	register gint x,y;
	register gint r,g,b;
	gfloat r1, r2, g1, g2, b1, b2;

//	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
//		return;
	printf("width: %d, height: %d, in: %p, in_rowstride: %d, out: %p, out_rowstride: %d\n",
		t->width, t->height, t->in, t->in_rowstride, t->out, t->out_rowstride);

	for(y=0 ; y<t->height ; y++)
	{
		gushort *d16 = ((gushort *)t->out) + y * t->out_rowstride;
		srcoffset = y * t->in_rowstride;
		for(x=0 ; x<t->width ; x++)
		{
			/* pre multipliers */
			r1 = t->in[srcoffset+R] * t->basic_render->pre_mul[R];
			g1 = t->in[srcoffset+G] * t->basic_render->pre_mul[G];
			b1 = t->in[srcoffset+B] * t->basic_render->pre_mul[B];

			/* clamp top */
			if (r1>65535.0) r1 = 65535.0;
			if (g1>65535.0) g1 = 65535.0;
			if (b1>65535.0) b1 = 65535.0;

			/* apply color matrix */
			r2 = (gint) (r1*t->basic_render->color_matrix.coeff[0][0]
				+ g1*t->basic_render->color_matrix.coeff[0][1]
				+ b1*t->basic_render->color_matrix.coeff[0][2]);
			g2 = (gint) (r1*t->basic_render->color_matrix.coeff[1][0]
				+ g1*t->basic_render->color_matrix.coeff[1][1]
				+ b1*t->basic_render->color_matrix.coeff[1][2]);
			b2 = (gint) (r1*t->basic_render->color_matrix.coeff[2][0]
				+ g1*t->basic_render->color_matrix.coeff[2][1]
				+ b1*t->basic_render->color_matrix.coeff[2][2]);

			/* we need integers for lookup */
			r = r2;
			g = g2;
			b = b2;

			/* clamp to unsigned short */
			_CLAMP65535_TRIPLET(r,g,b);

			*d16++ = t->basic_render->table16[r];
			*d16++ = t->basic_render->table16[g];
			*d16++ = t->basic_render->table16[b];
			d16++;

			/* Increment input */
			srcoffset += t->in_pixelsize;
		}
	}
	return NULL;
}

static gpointer
thread_func_float8(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	gint srcoffset;
	register gint x,y;
	register gint r,g,b;
	gfloat r1, r2, g1, g2, b1, b2;

//	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
//		return;

	for(y=0 ; y<t->height ; y++)
	{
		guchar *d8 = t->out + y * t->out_rowstride;
		srcoffset = y * t->in_rowstride;
		for(x=0 ; x<t->width ; x++)
		{
			/* pre multipliers */
			r1 = t->in[srcoffset+R] * t->basic_render->pre_mul[R];
			g1 = t->in[srcoffset+G] * t->basic_render->pre_mul[G];
			b1 = t->in[srcoffset+B] * t->basic_render->pre_mul[B];

			/* clamp top */
			if (r1>65535.0) r1 = 65535.0;
			if (g1>65535.0) g1 = 65535.0;
			if (b1>65535.0) b1 = 65535.0;

			/* apply color matrix */
			r2 = (r1*t->basic_render->color_matrix.coeff[0][0]
				+ g1*t->basic_render->color_matrix.coeff[0][1]
				+ b1*t->basic_render->color_matrix.coeff[0][2]);
			g2 = (r1*t->basic_render->color_matrix.coeff[1][0]
				+ g1*t->basic_render->color_matrix.coeff[1][1]
				+ b1*t->basic_render->color_matrix.coeff[1][2]);
			b2 = (r1*t->basic_render->color_matrix.coeff[2][0]
				+ g1*t->basic_render->color_matrix.coeff[2][1]
				+ b1*t->basic_render->color_matrix.coeff[2][2]);

			/* we need integers for lookup */
			r = r2;
			g = g2;
			b = b2;

			/* clamp to unsigned short */
			_CLAMP65535_TRIPLET(r,g,b);

			/* look up all colors in gammatable */
			*d8++ = t->basic_render->table8[r];
			*d8++ = t->basic_render->table8[g];
			*d8++ = t->basic_render->table8[b];

			/* input is always aligned to 64 bits */
			srcoffset += t->in_pixelsize;
		}
	}

	return NULL;
}

#if defined (__i386__) || defined (__x86_64__)
static gpointer
thread_func_sse8(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	register glong r,g,b;
	gint destoffset;
	gint col;
	RS_DECLARE_ALIGNED(gfloat, mat, 4, 3, 16);

//	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
//		return;

	mat[0] = t->basic_render->color_matrix.coeff[0][0];
	mat[1] = t->basic_render->color_matrix.coeff[1][0];
	mat[2] = t->basic_render->color_matrix.coeff[2][0];
	mat[3] = 0.f;

	mat[4] = t->basic_render->color_matrix.coeff[0][1];
	mat[5] = t->basic_render->color_matrix.coeff[1][1];
	mat[6] = t->basic_render->color_matrix.coeff[2][1];
	mat[7] = 0.f;

	mat[8]  = t->basic_render->color_matrix.coeff[0][2];
	mat[9]  = t->basic_render->color_matrix.coeff[1][2];
	mat[10] = t->basic_render->color_matrix.coeff[2][2];
	mat[11] = 0.f;

	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"xorps %%xmm7, %%xmm7\n\t" /* 0x0 */
		:
		: "r" (&mat[0]), "r" (&top[0]), "r" (t->basic_render->pre_mul)
		: "memory"
	);
	while(t->height--)
	{
		destoffset = 0;
		col = t->width;
		gushort *s = t->in + t->height * t->in_rowstride;
		guchar *d = t->out + t->height * t->out_rowstride;
		while(col--)
		{
			asm volatile (
				/* load */
				"movq (%3), %%mm0\n\t" /* R | G | B | G2 */
				"movq %%mm0, %%mm1\n\t" /* R | G | B | G2 */
				"punpcklwd %%mm7, %%mm0\n\t" /* R | G */
				"punpckhwd %%mm7, %%mm1\n\t" /* B | G2 */
				"cvtpi2ps %%mm1, %%xmm0\n\t" /* B | G2 | ? | ? */
				"shufps $0x4E, %%xmm0, %%xmm0\n\t" /* ? | ? | B | G2 */
				"cvtpi2ps %%mm0, %%xmm0\n\t" /* R | G | B | G2 */

				"mulps %%xmm2, %%xmm0\n\t" /* (R | G | B | _) * premul */
				"maxps %%xmm7, %%xmm0\n\t" /* MAX (0.0, in) */
				"minps %%xmm6, %%xmm0\n\t" /* MIN (65535.0, in) */

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0x0, %%xmm0, %%xmm1\n\t" /* R | R | R | R */
				"mulps %%xmm3, %%xmm1\n\t"
				"addps %%xmm1, %%xmm7\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0x55, %%xmm1, %%xmm1\n\t" /* G | G | G | G */
				"mulps %%xmm4, %%xmm1\n\t"
				"addps %%xmm1, %%xmm7\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0xAA, %%xmm1, %%xmm1\n\t" /* B | B | B | B */
				"mulps %%xmm5, %%xmm1\n\t"
				"addps %%xmm7, %%xmm1\n\t"

				"xorps %%xmm7, %%xmm7\n\t" /* 0 | 0 | 0 | 0 */
				"minps %%xmm6, %%xmm1\n\t" /* MIN (65535.0, in) */
				"maxps %%xmm7, %%xmm1\n\t" /* MAX (0.0, in) */

				/* xmm1: R | G | B | _ */
//				"shufps $0xFF, %%xmm1, %%xmm1\n\t"
				"cvtss2si %%xmm1, %0\n\t"
				"shufps $0xF9, %%xmm1, %%xmm1\n\t" /* xmm1: G | B | _ | _ */
				"cvtss2si %%xmm1, %1\n\t"
				"shufps $0xF9, %%xmm1, %%xmm1\n\t" /* xmm1: B | _ | _ | _ */
				"cvtss2si %%xmm1, %2\n\t"
				: "=r" (r), "=r" (g), "=r" (b)
				: "r" (s)
				: "memory"
			);
			d[destoffset++] = t->basic_render->table8[r];
			d[destoffset++] = t->basic_render->table8[g];
			d[destoffset++] = t->basic_render->table8[b];
			s += t->in_pixelsize;
		}
	}
	asm volatile("emms\n\t");

	return NULL;
}

static gpointer
thread_func_sse8_cms(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	gushort *buffer = g_new(gushort, t->width*3);
	register glong r,g,b;
	gint destoffset;
	gint col;
	RS_DECLARE_ALIGNED(gfloat, mat, 4, 3, 16);

//	if ((rct==NULL) || (width<1) || (height<1) || (in == NULL) || (in_rowstride<8) || (out == NULL) || (out_rowstride<1))
//		return;

	mat[0] = t->basic_render->color_matrix.coeff[0][0];
	mat[1] = t->basic_render->color_matrix.coeff[1][0];
	mat[2] = t->basic_render->color_matrix.coeff[2][0];
	mat[3] = 0.f;

	mat[4] = t->basic_render->color_matrix.coeff[0][1];
	mat[5] = t->basic_render->color_matrix.coeff[1][1];
	mat[6] = t->basic_render->color_matrix.coeff[2][1];
	mat[7] = 0.f;

	mat[8]  = t->basic_render->color_matrix.coeff[0][2];
	mat[9]  = t->basic_render->color_matrix.coeff[1][2];
	mat[10] = t->basic_render->color_matrix.coeff[2][2];
	mat[11] = 0.f;

	asm volatile (
		"movups (%2), %%xmm2\n\t" /* rs->pre_mul */
		"movaps (%0), %%xmm3\n\t" /* matrix */
		"movaps 16(%0), %%xmm4\n\t"
		"movaps 32(%0), %%xmm5\n\t"
		"movaps (%1), %%xmm6\n\t" /* top */
		"pxor %%mm7, %%mm7\n\t" /* 0x0 */
		"xorps %%xmm7, %%xmm7\n\t" /* 0x0 */
		:
		: "r" (&mat[0]), "r" (&top[0]), "r" (t->basic_render->pre_mul)
		: "memory"
	);

	while(t->height--)
	{
		destoffset = 0;
		col = t->width;
		gushort *s = t->in + t->height * t->in_rowstride;
		guchar *d = t->out + t->height * t->out_rowstride;
		while(col--)
		{
			asm volatile (
				/* load */
				"movq (%3), %%mm0\n\t" /* R | G | B | G2 */
				"movq %%mm0, %%mm1\n\t" /* R | G | B | G2 */
				"punpcklwd %%mm7, %%mm0\n\t" /* R | G */
				"punpckhwd %%mm7, %%mm1\n\t" /* B | G2 */
				"cvtpi2ps %%mm1, %%xmm0\n\t" /* B | G2 | ? | ? */
				"shufps $0x4E, %%xmm0, %%xmm0\n\t" /* ? | ? | B | G2 */
				"cvtpi2ps %%mm0, %%xmm0\n\t" /* R | G | B | G2 */

				"mulps %%xmm2, %%xmm0\n\t"
				"maxps %%xmm7, %%xmm0\n\t"
				"minps %%xmm6, %%xmm0\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0x0, %%xmm0, %%xmm1\n\t"
				"mulps %%xmm3, %%xmm1\n\t"
				"addps %%xmm1, %%xmm7\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0x55, %%xmm1, %%xmm1\n\t"
				"mulps %%xmm4, %%xmm1\n\t"
				"addps %%xmm1, %%xmm7\n\t"

				"movaps %%xmm0, %%xmm1\n\t"
				"shufps $0xAA, %%xmm1, %%xmm1\n\t"
				"mulps %%xmm5, %%xmm1\n\t"
				"addps %%xmm7, %%xmm1\n\t"

				"xorps %%xmm7, %%xmm7\n\t"
				"minps %%xmm6, %%xmm1\n\t"
				"maxps %%xmm7, %%xmm1\n\t"

				"cvtss2si %%xmm1, %0\n\t"
				"shufps $0xF9, %%xmm1, %%xmm1\n\t"
				"cvtss2si %%xmm1, %1\n\t"
				"shufps $0xF9, %%xmm1, %%xmm1\n\t"
				"cvtss2si %%xmm1, %2\n\t"
				: "=r" (r), "=r" (g), "=r" (b)
				: "r" (s)
				: "memory"
			);
			buffer[destoffset++] = t->basic_render->table16[r];
			buffer[destoffset++] = t->basic_render->table16[g];
			buffer[destoffset++] = t->basic_render->table16[b];
			s += t->in_pixelsize;
		}
		cmsDoTransform(t->basic_render->lcms_transform8, buffer, d, t->width);
	}
	asm volatile("emms\n\t");
	g_free(buffer);
	return NULL;
}

#endif /* __i386__ || __x86_64__ */

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	RSBasicRenderClass *klass = RS_BASIC_RENDER_GET_CLASS(filter);
	guint i, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	RSBasicRender *basic_render = RS_BASIC_RENDER(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = rs_image16_copy(input, FALSE);
	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	render_tables(basic_render);
	render_matrix(basic_render);
	/* FIXME: ICC support for 16 bit output */

	ThreadInfo *t = g_new(ThreadInfo, threads);
	threaded_h = input->h;
	y_per_thread = (threaded_h + threads-1)/threads;
	y_offset = 0;

	/* Set up job description for individual threads */
	for (i = 0; i < threads; i++)
	{
		t[i].basic_render = basic_render;
		t[i].width = input->w;
		t[i].height = MIN((input->h - y_offset), y_per_thread);
		t[i].in = GET_PIXEL(input, 0, y_offset);
		t[i].in_rowstride = input->rowstride;
		t[i].in_pixelsize = input->pixelsize;
		t[i].out = GET_PIXEL(output, 0, y_offset);
		t[i].out_rowstride = output->rowstride;

		y_offset += y_per_thread;
		y_offset = MIN(input->h, y_offset);

		t[i].threadid = g_thread_create(klass->thread_func16, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);

	g_object_unref(input);

	return response;
}

static RSFilterResponse *
get_image8(RSFilter *filter, const RSFilterRequest *request)
{
	RSBasicRenderClass *klass = RS_BASIC_RENDER_GET_CLASS(filter);
	guint i, x_offset, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	RSBasicRender *basic_render = RS_BASIC_RENDER(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	GdkPixbuf *output = NULL;
	gint width, height;
	GdkRectangle *roi;

	previous_response = rs_filter_get_image(filter->previous, request);
	input = rs_filter_response_get_image(previous_response);
	if (!RS_IS_IMAGE16(input))
		return previous_response;

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, input->w, input->h);
	rs_filter_response_set_image8(response, output);
	g_object_unref(output);

	render_tables(basic_render);
	render_matrix(basic_render);
	prepare_lcms(basic_render);

	if ((roi = rs_filter_request_get_roi(request)))
	{
		width = MIN(roi->width, input->w);
		height = MIN(roi->height, input->h);
		x_offset = MAX(roi->x, 0);
		y_offset = MAX(roi->y, 0);
	}
	else
	{
		width = input->w;
		height = input->h;
		x_offset = 0;
		y_offset = 0;
	}

	ThreadInfo *t = g_new(ThreadInfo, threads);
	threaded_h = height;
	y_per_thread = (threaded_h + threads-1)/threads;

	/* Set up job description for individual threads */
	for (i = 0; i < threads; i++)
	{
		t[i].basic_render = basic_render;
		t[i].width = width;
		t[i].height = MIN((input->h - y_offset), y_per_thread);
		t[i].in = GET_PIXEL(input, x_offset, y_offset);
		t[i].in_rowstride = input->rowstride;
		t[i].in_pixelsize = input->pixelsize;
		t[i].out = GET_PIXBUF_PIXEL(output, x_offset, y_offset);
		t[i].out_rowstride = gdk_pixbuf_get_rowstride(output);

		y_offset += y_per_thread;
		y_offset = MIN(input->h, y_offset);

		if (basic_render->lcms_transform8)
			t[i].threadid = g_thread_create(klass->thread_func8_cms, &t[i], TRUE, NULL);
		else
			t[i].threadid = g_thread_create(klass->thread_func8, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);

	g_object_unref(input);

	return response;
}

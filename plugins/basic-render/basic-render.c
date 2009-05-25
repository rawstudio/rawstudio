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

	gboolean dirty_tables;
	gboolean dirty_matrix;

	RSSettings *settings;

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
};

struct _RSBasicRenderClass {
	RSFilterClass parent_class;
	gpointer (*thread_func16)(gpointer _thread_info);
	gpointer (*thread_func8)(gpointer _thread_info);
};

typedef struct {
	RSBasicRender *basic_render;
	GThread *threadid;
	gint width;
	gint height;
	gushort *in;
	gint in_rowstride;
	void *out;
	gint out_rowstride;
} ThreadInfo;

RS_DEFINE_FILTER(rs_basic_render, RSBasicRender)

enum {
	PROP_0,
	PROP_GAMMA,
	PROP_EXPOSURE,
	PROP_SETTINGS
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void settings_changed(RSSettings *settings, RSSettingsMask mask, RSBasicRender *basic_render);
static void render_tables(RSBasicRender *basic_render);
static void render_matrix(RSBasicRender *basic_render);
static gpointer thread_func_float16(gpointer _thread_info);
static gpointer thread_func_float8(gpointer _thread_info);
#if defined (__i386__) || defined (__x86_64__)
static gpointer thread_func_sse8(gpointer _thread_info);
#endif /* __i386__ || __x86_64__ */
static RS_IMAGE16 *get_image(RSFilter *filter);
static GdkPixbuf *get_image8(RSFilter *filter);

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

	filter_class->name = "BasicRender filter";
	filter_class->get_image = get_image;
	filter_class->get_image8 = get_image8;

	klass->thread_func16 = thread_func_float16;
	klass->thread_func8 = thread_func_float8;
#if defined (__i386__) || defined (__x86_64__)
	/* Use SSE version if possible */
	if (rs_detect_cpu_features() & RS_CPU_FLAG_SSE)
		klass->thread_func8 = thread_func_sse8;
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

	matrix4_identity(&basic_render->color_matrix);

	basic_render->settings = NULL;
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
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSSettings *settings;
	RSBasicRender *basic_render = RS_BASIC_RENDER(object);

	switch (property_id)
	{
		case PROP_GAMMA:
			basic_render->gamma = g_value_get_float(value);
			basic_render->dirty_tables = TRUE;
			rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_PIXELDATA);
			break;
		case PROP_SETTINGS:
			settings = g_value_get_object(value);
			g_signal_connect(settings, "settings-changed", G_CALLBACK(settings_changed), basic_render);

			/* FIXME: Quick hack to force updating RSBasicRender before RSSettings
			 * sends a "settings-changed"-signal. Should be replaced by some
			 * dirty_settings mechanics. */
			settings_changed(settings, MASK_ALL, basic_render);
//			g_object_unref(settings);
			rs_filter_changed(RS_FILTER(object), RS_FILTER_CHANGED_PIXELDATA);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
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

	if (mask & MASK_WB)
	{
		const gfloat warmth;
		const gfloat tint;
		g_object_get(settings, "warmth", &warmth, "tint", &tint, NULL);

		basic_render->pre_mul[R] = (1.0+warmth)*(2.0-tint);
		basic_render->pre_mul[G] = 1.0;
		basic_render->pre_mul[B] = (1.0-warmth)*(2.0-tint);
		basic_render->pre_mul[G2] = 1.0;

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
				rs_spline_t *spline = rs_spline_new(knots, basic_render->nknots, NATURAL);
				rs_spline_sample(spline, basic_render->curve_samples, 65536);
				rs_spline_destroy(spline);
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
		nd = pow(nd, basic_render->gamma);
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

			/* input is always aligned to 64 bits */
			srcoffset += 4;
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
			srcoffset += 4;
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
			s += 4;
		}
	}
	asm volatile("emms\n\t");

	return NULL;
}
#endif /* __i386__ || __x86_64__ */

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSBasicRenderClass *klass = RS_BASIC_RENDER_GET_CLASS(filter);
	guint i, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	RSBasicRender *basic_render = RS_BASIC_RENDER(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	input = rs_filter_get_image(filter->previous);
	if (!RS_IS_IMAGE16(input))
		return input;

	output = rs_image16_copy(input, FALSE);

	render_tables(basic_render);
	render_matrix(basic_render);

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

	return output;
}

static GdkPixbuf *
get_image8(RSFilter *filter)
{
	RSBasicRenderClass *klass = RS_BASIC_RENDER_GET_CLASS(filter);
	guint i, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	RSBasicRender *basic_render = RS_BASIC_RENDER(filter);
	RS_IMAGE16 *input;
	GdkPixbuf *output = NULL;

	input = rs_filter_get_image(filter->previous);
	if (!RS_IS_IMAGE16(input))
		return NULL;

	output = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, input->w, input->h);

	render_tables(basic_render);
	render_matrix(basic_render);

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
		t[i].out = GET_PIXBUF_PIXEL(output, 0, y_offset);
		t[i].out_rowstride = gdk_pixbuf_get_rowstride(output);

		y_offset += y_per_thread;
		y_offset = MIN(input->h, y_offset);

		t[i].threadid = g_thread_create(klass->thread_func8, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);

	g_object_unref(input);

	return output;
}

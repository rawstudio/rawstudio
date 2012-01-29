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

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include <math.h>
#include <string.h>  /*memcpy */



#define RS_TYPE_RESAMPLE (rs_resample_type)
#define RS_RESAMPLE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_RESAMPLE, RSResample))
#define RS_RESAMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_RESAMPLE, RSResampleClass))
#define RS_IS_RESAMPLE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_RESAMPLE))

typedef struct _RSResample RSResample;
typedef struct _RSResampleClass RSResampleClass;

struct _RSResample {
	RSFilter parent;

	gint target_width;
	gint target_height;
	gint new_width;
	gint new_height;
	gfloat scale;
	gboolean bounding_box;
	gboolean never_quick;
};

struct _RSResampleClass {
	RSFilterClass parent_class;
};

typedef struct {
	RS_IMAGE16 *input;			/* Input Image to Resampler */
	RS_IMAGE16 *output;			/* Output Image from Resampler */
	guint old_size;				/* Old dimension in the direction of the resampler*/
	guint new_size;				/* New size in the direction of the resampler */
	guint dest_offset_other;	/* Where in the unchanged direction should we begin writing? */
	guint dest_end_other;		/* Where in the unchanged direction should we stop writing? */
	guint (*resample_support)(void);
	gfloat (*resample_func)(gfloat);
	GThread *threadid;
	gboolean use_compatible;	/* Use compatible resampler if pixelsize != 4 */
	gboolean use_fast;		/* Use nearest neighbour resampler, also compatible*/
} ResampleInfo;

RS_DEFINE_FILTER(rs_resample, RSResample)

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_BOUNDING_BOX,
	PROP_NEVER_QUICK,
	PROP_SCALE
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask);
static RSFilterChangedMask recalculate_dimensions(RSResample *resample);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterRequest *request);
static RSFilterResponse *get_size(RSFilter *filter, const RSFilterRequest *request);
static void ResizeH(ResampleInfo *info);
void ResizeV(ResampleInfo *info);
extern void ResizeV_SSE2(ResampleInfo *info);
extern void ResizeV_SSE4(ResampleInfo *info);
extern void ResizeV_AVX(ResampleInfo *info);
static void ResizeH_compatible(ResampleInfo *info);
static void ResizeV_compatible(ResampleInfo *info);
static void ResizeH_fast(ResampleInfo *info);
void ResizeV_fast(ResampleInfo *info);

static RSFilterClass *rs_resample_parent_class = NULL;
static inline guint clampbits(gint x, guint n) { guint32 _y_temp; if( (_y_temp=x>>n) ) x = ~_y_temp >> (32-n); return x;}
static GStaticRecMutex resampler_mutex = G_STATIC_REC_MUTEX_INIT;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_resample_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_resample_class_init(RSResampleClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_resample_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_WIDTH, g_param_spec_int(
			"width", "width", "The width of the scaled image",
			6, 65535, 100, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_HEIGHT, g_param_spec_int(
			"height", "height", "The height of the scaled image",
			6, 65535, 100, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_BOUNDING_BOX, g_param_spec_boolean(
			"bounding-box", "bounding-box", "Use width/height as a bounding box",
			FALSE, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_SCALE, g_param_spec_float(
			"scale", "scale", "The expected scaling factor in bounding box mode",
			0.0, 100.0, 1.0, G_PARAM_READABLE)
	);
	g_object_class_install_property(object_class,
		PROP_NEVER_QUICK, g_param_spec_boolean(
			"never-quick", "never-quick", "Never use quick function, even if allowed by request",
			FALSE, G_PARAM_READWRITE)
	);

	filter_class->name = "Resample filter";
	filter_class->get_image = get_image;
	filter_class->get_size = get_size;
	filter_class->previous_changed = previous_changed;
}

static void
rs_resample_init(RSResample *resample)
{
	resample->target_width = -1;
	resample->target_height = -1;
	resample->new_width = -1;
	resample->new_height = -1;
	resample->bounding_box = FALSE;
	resample->scale = 1.0;
	resample->never_quick = FALSE;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSResample *resample = RS_RESAMPLE(object);

	switch (property_id)
	{
		case PROP_WIDTH:
			g_value_set_int(value, resample->target_width);
			break;
		case PROP_HEIGHT:
			g_value_set_int(value, resample->target_height);
			break;
		case PROP_BOUNDING_BOX:
			g_value_set_boolean(value, resample->bounding_box);
			break;
		case PROP_NEVER_QUICK:
			g_value_set_boolean(value, resample->never_quick);
			break;
		case PROP_SCALE:
			g_value_set_float(value, resample->scale);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSResample *resample = RS_RESAMPLE(object);
	RSFilterChangedMask mask = 0;

	g_static_rec_mutex_lock(&resampler_mutex);

	switch (property_id)
	{
		case PROP_WIDTH:
			if (g_value_get_int(value) != resample->target_width)
			{
				resample->target_width = g_value_get_int(value);
				mask |= recalculate_dimensions(resample);
			}
			break;
		case PROP_HEIGHT:
			if (g_value_get_int(value) != resample->target_height)
			{
				resample->target_height = g_value_get_int(value);
				mask |= recalculate_dimensions(resample);
			}
			break;
		case PROP_BOUNDING_BOX:
			if (g_value_get_boolean(value) != resample->bounding_box)
			{
				resample->bounding_box = g_value_get_boolean(value);
				mask |= recalculate_dimensions(resample);
			}
			break;
		case PROP_NEVER_QUICK:
			if (g_value_get_boolean(value) != resample->never_quick)
			{
				resample->never_quick = g_value_get_boolean(value);
				mask |= RS_FILTER_CHANGED_PIXELDATA;
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	g_static_rec_mutex_unlock(&resampler_mutex);
	if (mask)
		rs_filter_changed(RS_FILTER(object), mask);
}

static void
previous_changed(RSFilter *filter, RSFilter *parent, RSFilterChangedMask mask)
{
	if (mask & RS_FILTER_CHANGED_DIMENSION)
		mask |= recalculate_dimensions(RS_RESAMPLE(filter));

	rs_filter_changed(filter, mask);
}

static RSFilterChangedMask
recalculate_dimensions(RSResample *resample)
{
	RSFilterChangedMask mask = 0;
	gint new_width, new_height;
	gint previous_width = 0;
	gint previous_height = 0;
	g_static_rec_mutex_lock(&resampler_mutex);

	if (RS_FILTER(resample)->previous)
		rs_filter_get_size_simple(RS_FILTER(resample)->previous, RS_FILTER_REQUEST_QUICK, &previous_width, &previous_height);

	if (resample->bounding_box && RS_FILTER(resample)->previous)
	{
		new_width = previous_width;
		new_height = previous_height;
		rs_constrain_to_bounding_box(resample->target_width, resample->target_height, &new_width, &new_height);
		resample->scale = ((((gfloat) new_width)/ previous_width) + (((gfloat) new_height)/ previous_height))/2.0;
	}
	else
	{
		new_width = resample->target_width;
		new_height = resample->target_height;
		if (RS_FILTER(resample)->previous)
		{
			if (previous_width > 0 && previous_height > 0)
				resample->scale = MIN((gfloat)new_width / previous_width, (gfloat)new_height / previous_height);
			else
				resample->scale = 1.0f;
		}
		else
		{
			resample->scale = 1.0f;
		}
	}

	if ((new_width != resample->new_width) || (new_height != resample->new_height))
	{
		resample->new_width = new_width;
		resample->new_height = new_height;
		mask |= RS_FILTER_CHANGED_DIMENSION;
	}

	if (new_width < 0 || new_height < 0)
		resample->scale = 1.0f;

	g_static_rec_mutex_unlock(&resampler_mutex);
	return mask;
}

static inline void 
bit_blt(char* dstp, int dst_pitch, const char* srcp, int src_pitch, int row_size, int height) 
{
	if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)) 
	{
		memcpy(dstp, srcp, row_size*height);
		return;
	}

	int y;
	for (y = height; y > 0; --y)
	{
		memcpy(dstp, srcp, row_size);
		dstp += dst_pitch;
		srcp += src_pitch;
	}
}

gpointer
start_thread_resampler(gpointer _thread_info)
{
	ResampleInfo* t = _thread_info;

	if (!t->input)
	{
		g_debug("Resampler: input is NULL");
		g_thread_exit(NULL);
		return NULL;
	}

	if (!t->output)
	{
		g_debug("Resampler: output is NULL");
		g_thread_exit(NULL);
		return NULL;
	}

	if (t->input->h != t->output->h)
	{
		gboolean sse2_available = !!(rs_detect_cpu_features() & RS_CPU_FLAG_SSE2);
#if 0  // FIXME: Test and enable
		gboolean sse4_available = !!(rs_detect_cpu_features() & RS_CPU_FLAG_SSE4_1);
		gboolean avx_available = !!(rs_detect_cpu_features() & RS_CPU_FLAG_AVX);
#endif
		if (t->use_fast)
			ResizeV_fast(t);
		else if (t->use_compatible)
			ResizeV_compatible(t);
#if 0  // FIXME: Test and enable
		else if (avx_available)
			ResizeV_AVX(t);
		else if (sse4_available)
			ResizeV_SSE4(t);
#endif
		else if (sse2_available)
			ResizeV_SSE2(t);
		else
			ResizeV(t);
	} 
	else if (t->input->w != t->output->w)
	{
		if (t->use_fast)
			ResizeH_fast(t);
		else if (t->use_compatible)
			ResizeH_compatible(t);
		else
			ResizeH(t);
	}
	/* Unchanged in both directions, have thread 0 copy all the image */
	else if (t->dest_offset_other == 0)
		bit_blt((char*)GET_PIXEL(t->output,0,0), t->output->rowstride * 2, 
			(const char*)GET_PIXEL(t->input,0,0), t->input->rowstride * 2, t->input->rowstride * 2, t->input->h);

	g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterRequest *request)
{
	gboolean use_fast = FALSE;
	RSResample *resample = RS_RESAMPLE(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *afterVertical;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	gint input_width;
	gint input_height;

	rs_filter_get_size_simple(filter->previous, request, &input_width, &input_height);

	/* Return the input, if the new size is uninitialized */
	if ((resample->new_width == -1) || (resample->new_height == -1))
		return rs_filter_get_image(filter->previous, request);

	/* Simply return the input, if we don't scale */
	if ((input_width == resample->new_width) && (input_height == resample->new_height))
		return rs_filter_get_image(filter->previous, request);	
	
	/* Remove ROI, it doesn't make sense across resampler */
	if (rs_filter_request_get_roi(request))
	{
		RSFilterRequest *new_request = rs_filter_request_clone(request);
		rs_filter_request_set_roi(new_request, NULL);
		previous_response = rs_filter_get_image(filter->previous, new_request);
		g_object_unref(new_request);
	}
	else
		previous_response = rs_filter_get_image(filter->previous, request);

	input = rs_filter_response_get_image(previous_response);

	if (!RS_IS_IMAGE16(input))
		return previous_response;

	g_static_rec_mutex_lock(&resampler_mutex);
	input_width = input->w;
	input_height = input->h;	

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	previous_response = NULL;

	/* Use compatible (and slow) version if input isn't 3 channels and pixelsize 4 */
	gboolean use_compatible = ( ! ( input->pixelsize == 4 && input->channels == 3));

	if (!resample->never_quick && rs_filter_request_get_quick(request))
	{
		use_fast = TRUE;
		rs_filter_response_set_quick(response);
	}

	if (input_width < 32 || input_height < 32)
		use_compatible = TRUE;

	guint threads = rs_get_number_of_processor_cores();

	ResampleInfo* h_resample = g_new(ResampleInfo,  threads);
	ResampleInfo* v_resample = g_new(ResampleInfo,  threads);

	/* Create intermediate and output images*/
	afterVertical = rs_image16_new(input_width, resample->new_height, input->channels, input->pixelsize);

	// Only even count
	guint output_x_per_thread = ((input_width + threads - 1 ) / threads );
	while (((output_x_per_thread * input->pixelsize) & 15) != 0)
		output_x_per_thread++;
	guint output_x_offset = 0;

	guint i;
	for (i = 0; i < threads; i++)
	{
		/* Set info for Vertical resampler */
		ResampleInfo *v = &v_resample[i];
		v->input = input;
		v->output  = afterVertical;
		v->old_size = input_height;
		v->new_size = resample->new_height;
		v->dest_offset_other = output_x_offset;
		v->dest_end_other  = MIN(output_x_offset + output_x_per_thread, input_width);
		v->use_compatible = use_compatible;
		v->use_fast = use_fast;

		/* Start it up */
		v->threadid = g_thread_create(start_thread_resampler, v, TRUE, NULL);

		/* Update offset */
		output_x_offset = v->dest_end_other;
	}

	/* Wait for vertical threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(v_resample[i].threadid);

	/* input no longer needed */
	g_object_unref(input);
	input = NULL;

	/* create output */
	output = rs_image16_new(resample->new_width,  resample->new_height, afterVertical->channels, afterVertical->pixelsize);

	guint input_y_offset = 0;
	guint input_y_per_thread = (resample->new_height+threads-1) / threads;

	for (i = 0; i < threads; i++)
	{
		/* Set info for Horizontal resampler */
		ResampleInfo *h = &h_resample[i];
		h->input = afterVertical;
		h->output  = output;
		h->old_size = input_width;
		h->new_size = resample->new_width;
		h->dest_offset_other = input_y_offset;
		h->dest_end_other  = MIN(input_y_offset+input_y_per_thread, resample->new_height);
		h->use_compatible = use_compatible;
		h->use_fast = use_fast;

		/* Start it up */
		h->threadid = g_thread_create(start_thread_resampler, h, TRUE, NULL);

		/* Update offset */
		input_y_offset = h->dest_end_other;

	}

	/* Wait for horizontal threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(h_resample[i].threadid);

	/* Clean up */
	g_free(h_resample);
	g_free(v_resample);
	g_object_unref(afterVertical);

	rs_filter_response_set_image(response, output);
	rs_filter_param_set_boolean(RS_FILTER_PARAM(response), "half-size", FALSE);
	g_object_unref(output);
	g_static_rec_mutex_unlock(&resampler_mutex);
	return response;
}

static RSFilterResponse *
get_size(RSFilter *filter, const RSFilterRequest *request)
{
	RSResample *resample = RS_RESAMPLE(filter);
	RSFilterResponse *previous_response = rs_filter_get_size(filter->previous, request);

	if ((resample->new_width == -1) || (resample->new_height == -1))
		return previous_response;

	RSFilterResponse *response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);

	rs_filter_response_set_width(response, resample->new_width);
	rs_filter_response_set_height(response, resample->new_height);

	return response;
}

static guint
lanczos_taps(void)
{
	return 3;
}

static gfloat
sinc(gfloat value)
{
	if (value != 0.0f)
	{
		value *= M_PI;
		return sinf(value) / value;
	}
	else
		return 1.0f;
}

static gfloat
lanczos_weight(gfloat value)
{
	value = fabsf(value);
	if (value < lanczos_taps())
	{
		return (sinc(value) * sinc(value / lanczos_taps()));
	}
	else
		return 0.0f;
}

const static gint FPScale = 16384; /* fixed point scaler */
const static gint FPScaleShift = 14; /* fixed point scaler */

static void
ResizeH(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);
	gfloat filter_step = MIN(1.0 / pos_step, 1.0);
	gfloat filter_support = (gfloat) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeH_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gfloat pos = 0.0f;
	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);

		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos * 4;

		/* the following code ensures that the coefficients add to exactly FPScale */
		gfloat total = 0.0;

		/* Ensure that we have a valid position */
		gfloat ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gfloat total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gfloat total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}

	g_assert(input->pixelsize == 4);
	g_assert(input->channels == 3);

	guint y,x;
	for (y = info->dest_offset_other; y < info->dest_end_other ; y++)
	{
		gushort *in_line = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);
		gint *wg = weights;

		for (x = 0; x < new_size; x++)
		{
			guint i;
			gushort *in = &in_line[offsets[x]];
			gint acc1 = 0;
			gint acc2 = 0;
			gint acc3 = 0;

			for (i = 0; i <fir_filter_size; i++)
			{
				gint w = *wg++;
				acc1 += in[i*4]*w;
				acc2 += in[i*4+1]*w;
				acc3 += in[i*4+2]*w;
			}
			out[x*4] = clampbits((acc1 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+1] = clampbits((acc2 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+2] = clampbits((acc3 + (FPScale/2))>>FPScaleShift, 16);
		}
	}

	g_free(weights);
	g_free(offsets);

}

void
ResizeV(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);
	gfloat filter_step = MIN(1.0 / pos_step, 1.0);
	gfloat filter_support = (gfloat) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gfloat pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gfloat total = 0.0;

		/* Ensure that we have a valid position */
		gfloat ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gfloat total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gfloat total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}

	g_assert(input->pixelsize == 4);
	g_assert(input->channels == 3);

	guint y,x;
    gint *wg = weights;

	for (y = 0; y < new_size ; y++)
	{
		gushort *in = GET_PIXEL(input, start_x, offsets[y]);
		gushort *out = GET_PIXEL(output, 0, y);
		for (x = start_x; x < end_x; x++)
		{
			gint acc1 = 0;
			gint acc2 = 0;
			gint acc3 = 0;
			for (i = 0; i < fir_filter_size; i++)
			{
				acc1 += in[i*input->rowstride]* wg[i];
				acc2 += in[i*input->rowstride+1] * wg[i];
				acc3 += in[i*input->rowstride+2] * wg[i];
			}
			out[x*4] = clampbits((acc1 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+1] = clampbits((acc2 + (FPScale/2))>>FPScaleShift, 16);
			out[x*4+2] = clampbits((acc3 + (FPScale/2))>>FPScaleShift, 16);
			in+=4;
		}
		wg+=fir_filter_size;
	}

	g_free(weights);
	g_free(offsets);

}

static void
ResizeH_compatible(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);
	gfloat filter_step = MIN(1.0 / pos_step, 1.0);
	gfloat filter_support = (gfloat) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeH_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gfloat pos = 0.0;
	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);

		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos * pixelsize;

		/* the following code ensures that the coefficients add to exactly FPScale */
		gfloat total = 0.0;

		/* Ensure that we have a valid position */
		gfloat ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gfloat total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gfloat total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}


	guint y,x,c;
	for (y = info->dest_offset_other; y < info->dest_end_other ; y++)
	{
		gint *wg = weights;
		gushort *in_line = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);

		for (x = 0; x < new_size; x++)
		{
			guint i;
			gushort *in = &in_line[offsets[x]];
			for (c = 0 ; c < ch; c++)
			{
				gint acc = 0;

				for (i = 0; i <fir_filter_size; i++)
				{
					acc += in[i*pixelsize+c]*wg[i];
				}
				out[x*pixelsize+c] = clampbits((acc + (FPScale/2))>>FPScaleShift, 16);
			}
			wg += fir_filter_size;
		}
	}

	g_free(weights);
	g_free(offsets);

}

static void
ResizeV_compatible(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);
	gfloat filter_step = MIN(1.0 / pos_step, 1.0);
	gfloat filter_support = (gfloat) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));

	if (old_size <= fir_filter_size)
		return ResizeV_fast(info);

	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	gfloat pos = 0.0;

	gint i,j,k;

	for (i=0; i<new_size; ++i)
	{
		gint end_pos = (gint) (pos + filter_support);
		if (end_pos > old_size-1)
			end_pos = old_size-1;

		gint start_pos = end_pos - fir_filter_size + 1;

		if (start_pos < 0)
			start_pos = 0;

		offsets[i] = start_pos;

		/* The following code ensures that the coefficients add to exactly FPScale */
		gfloat total = 0.0;

		/* Ensure that we have a valid position */
		gfloat ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gfloat total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gfloat total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
			weights[i*fir_filter_size+k] = (gint) (total3*FPScale+0.5) - (gint) (total2*FPScale+0.5);
			total2 = total3;
		}
		pos += pos_step;
	}

	guint y,x,c;
	gint *wg = weights;

	for (y = 0; y < new_size ; y++)
	{
		gushort *out = GET_PIXEL(output, 0, y);
		for (x = start_x; x < end_x; x++)
		{
			gushort *in = GET_PIXEL(input, x, offsets[y]);
			for (c = 0; c < ch; c++)
			{
				gint acc = 0;
				for (i = 0; i < fir_filter_size; i++)
				{
					acc += in[i*input->rowstride]* wg[i];
				}
				out[x*pixelsize+c] = clampbits((acc + (FPScale/2))>>FPScaleShift, 16);
				in++;
			}
		}
		wg+=fir_filter_size;
	}

	g_free(weights);
	g_free(offsets);
}

void
ResizeV_fast(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);

	gint pos = 0;
	gint delta = (gint)(pos_step * 65536.0);

	guint y,x,c;

	for (y = 0; y < new_size ; y++)
	{
		gushort *in = GET_PIXEL(input, start_x, pos>>16);
		gushort *out = GET_PIXEL(output, start_x, y);
		int out_pos = 0;
		for (x = start_x; x < end_x; x++)
		{
			for (c = 0; c < ch; c++)
			{
				out[out_pos+c] = in[c];
			}
			out_pos += pixelsize;
			in+=pixelsize;
		}
		pos += delta;
	}
}



static void
ResizeH_fast(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;

	gint pixelsize = input->pixelsize;
	gint ch = input->channels;

	gfloat pos_step = ((gfloat) old_size) / ((gfloat)new_size);

	gint pos;
	gint delta = (gint)(pos_step * 65536.0);

	guint y,x,c;
	for (y = info->dest_offset_other; y < info->dest_end_other ; y++)
	{
		gushort *in_line = GET_PIXEL(input, 0, y);
		gushort *out = GET_PIXEL(output, 0, y);
		pos = 0;
		int out_pos = 0;

		for (x = 0; x < new_size; x++)
		{
			gushort* start_pos = &in_line[(pos>>16)*pixelsize];
			for (c = 0 ; c < ch; c++)
			{
				out[out_pos+c] = start_pos[c];
			}
			out_pos += pixelsize;
			pos += delta;
		}
	}
}



/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
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
#include <math.h>

#define RS_TYPE_RESAMPLE (rs_resample_type)
#define RS_RESAMPLE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_RESAMPLE, RSResample))
#define RS_RESAMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_RESAMPLE, RSResampleClass))
#define RS_IS_RESAMPLE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_RESAMPLE))

typedef struct _RSResample RSResample;
typedef struct _RSResampleClass RSResampleClass;

struct _RSResample {
	RSFilter parent;

	gint new_width;
	gint new_height;
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
	guint (*resample_support)();
	gdouble (*resample_func)(gdouble);
	GThread *threadid;
	gboolean use_compatible;
} ResampleInfo;

RS_DEFINE_FILTER(rs_resample, RSResample)

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);
static gint get_width(RSFilter *filter);
static gint get_height(RSFilter *filter);
static void ResizeH(ResampleInfo *info);
static void ResizeV(ResampleInfo *info);
static void ResizeH_compatible(ResampleInfo *info);
static void ResizeV_compatible(ResampleInfo *info);

static RSFilterClass *rs_resample_parent_class = NULL;
inline guint clampbits(gint x, guint n) { guint32 _y_temp; if( (_y_temp=x>>n) ) x = ~_y_temp >> (32-n); return x;}

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

	filter_class->name = "Resample filter";
	filter_class->get_image = get_image;
	filter_class->get_width = get_width;
	filter_class->get_height = get_height;
}

static void
rs_resample_init(RSResample *resample)
{
	resample->new_width = 400;
	resample->new_height = 400;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSResample *resample = RS_RESAMPLE(object);

	switch (property_id)
	{
		case PROP_WIDTH:
			g_value_set_int(value, resample->new_width);
			break;
		case PROP_HEIGHT:
			g_value_set_int(value, resample->new_height);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSResample *resample = RS_RESAMPLE(object);

	switch (property_id)
	{
		case PROP_WIDTH:
			resample->new_width = g_value_get_int(value);
			rs_filter_changed(RS_FILTER(object));
			break;
		case PROP_HEIGHT:
			resample->new_height = g_value_get_int(value);
			rs_filter_changed(RS_FILTER(object));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

gpointer
start_thread_resampler(gpointer _thread_info)
{
	ResampleInfo* t = _thread_info;

	if (t->input->w == t->output->w) 
	{
		if (t->use_compatible)
			ResizeV_compatible(t);
		else 
			ResizeV(t);		
	} else 	{
		if (t->use_compatible)
			ResizeH_compatible(t);
		else 
			ResizeH(t);	
	}
	g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSResample *resample = RS_RESAMPLE(filter);
	RS_IMAGE16 *afterHorizontal;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	gint input_width = rs_filter_get_width(filter->previous);
	gint input_height = rs_filter_get_height(filter->previous);

	input = rs_filter_get_image(filter->previous);

	/* Simply return the input, if we don't scale */
	if ((input_width == resample->new_width) && (input_height == resample->new_height))
		return input;

	/* Use compatible (and slow) version if input isn't 3 channels and pixelsize 4 */ 
	gboolean use_compatible = ( ! ( input->pixelsize == 4 && input->channels == 3));
	guint threads = rs_get_number_of_processor_cores();

	ResampleInfo* h_resample = g_new(ResampleInfo,  threads);
	ResampleInfo* v_resample = g_new(ResampleInfo,  threads);

	/* Create intermediate and output images*/
	afterHorizontal = rs_image16_new(resample->new_width, input_height, input->channels, input->pixelsize);

	guint input_y_offset = 0;
	guint input_y_per_thread = (input_height+threads-1) / threads;
	
	guint i;
	for (i = 0; i < threads; i++) 
	{
		/* Set info for Horizontal resampler */
		ResampleInfo *h = &h_resample[i];
		h->input = input;
		h->output  = afterHorizontal;
		h->old_size = input_width;
		h->new_size = resample->new_width;
		h->dest_offset_other = input_y_offset;
		h->dest_end_other  = MIN(input_y_offset+input_y_per_thread, input_height);
		h->use_compatible = use_compatible;

		/* Start it up */
		h->threadid = g_thread_create(start_thread_resampler, h, TRUE, NULL);

		/* Update offset */
		input_y_offset = h->dest_end_other;

	}

	/* Wait for horizontal threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(h_resample[i].threadid);

	/* input no longer needed */
	g_object_unref(input);

	/* create output */
	output = rs_image16_new(resample->new_width,  resample->new_height, input->channels, input->pixelsize);

	guint output_x_offset = 0;
	guint output_x_per_thread = (resample->new_width+threads-1) / threads;

	for (i = 0; i < threads; i++) 
	{
		/* Set info for Vertical resampler */
		ResampleInfo *v = &v_resample[i];
		v->input = afterHorizontal;
		v->output  = output;
		v->old_size = input_height;
		v->new_size = resample->new_height;
		v->dest_offset_other = output_x_offset;
		v->dest_end_other  = MIN(output_x_offset + output_x_per_thread, resample->new_width);
		v->use_compatible = use_compatible;

		/* Start it up */
		v->threadid = g_thread_create(start_thread_resampler, v, TRUE, NULL);

		/* Update offset */
		output_x_offset = v->dest_end_other;
	}

	/* Wait for vertical threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(v_resample[i].threadid);

	/* Clean up */
	g_free(h_resample);
	g_free(v_resample);
	g_object_unref(afterHorizontal);

	return output;
}

static gint
get_width(RSFilter *filter)
{
	RSResample *resample = RS_RESAMPLE(filter);

	return resample->new_width;
}

static gint
get_height(RSFilter *filter)
{
	RSResample *resample = RS_RESAMPLE(filter);

	return resample->new_height;
}

static guint
lanczos_taps()
{
	return 3;
}

static gdouble
sinc(gdouble value)
{
	if (value != 0.0)
	{
		value *= M_PI;
		return sin(value) / value;
	}
	else
		return 1.0;
}

static gdouble
lanczos_weight(gdouble value)
{
	value = fabs(value);
	if (value < lanczos_taps())
	{
		return (sinc(value) * sinc(value / lanczos_taps()));
	}
	else
		return 0.0;
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

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));
	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	g_assert(new_size > fir_filter_size);

	gdouble pos = 0.0;
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
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
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

static void
ResizeV(ResampleInfo *info)
{
	const RS_IMAGE16 *input = info->input;
	const RS_IMAGE16 *output = info->output;
	const guint old_size = info->old_size;
	const guint new_size = info->new_size;
	const guint start_x = info->dest_offset_other;
	const guint end_x = info->dest_end_other;

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));
	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	g_assert(new_size > fir_filter_size);

	gdouble pos = 0.0;

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
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
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

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));
	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	g_assert(new_size > fir_filter_size);

	gdouble pos = 0.0;
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
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
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

	gdouble pos_step = ((gdouble) old_size) / ((gdouble)new_size);
	gdouble filter_step = MIN(1.0 / pos_step, 1.0);
	gdouble filter_support = (gdouble) lanczos_taps() / filter_step;
	gint fir_filter_size = (gint) (ceil(filter_support*2));
	gint *weights = g_new(gint, new_size * fir_filter_size);
	gint *offsets = g_new(gint, new_size);

	g_assert(new_size > fir_filter_size);

	gdouble pos = 0.0;

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
		gdouble total = 0.0;

		/* Ensure that we have a valid position */
		gdouble ok_pos = MAX(0.0,MIN(old_size-1,pos));

		for (j=0; j<fir_filter_size; ++j)
		{
			/* Accumulate all coefficients */
			total += lanczos_weight((start_pos+j - ok_pos) * filter_step);
		}

		g_assert(total > 0.0f);

		gdouble total2 = 0.0;

		for (k=0; k<fir_filter_size; ++k)
		{
			gdouble total3 = total2 + lanczos_weight((start_pos+k - ok_pos) * filter_step) / total;
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

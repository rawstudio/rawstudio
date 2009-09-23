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
#include <string.h>

#define RS_TYPE_DEMOSAIC (rs_demosaic_type)
#define RS_DEMOSAIC(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DEMOSAIC, RSDemosaic))
#define RS_DEMOSAIC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DEMOSAIC, RSDemosaicClass))
#define RS_IS_DEMOSAIC(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DEMOSAIC))

typedef struct {
	gint start_y;
	gint end_y;
	RS_IMAGE16 *image;
	RS_IMAGE16 *output;
	guint filters;
	GThread *threadid;
} ThreadInfo;

typedef enum {
	RS_DEMOSAIC_NONE,
	RS_DEMOSAIC_BILINEAR,
	RS_DEMOSAIC_PPG,
	RS_DEMOSAIC_MAX
} RS_DEMOSAIC;

const static gchar *rs_demosaic_ascii[RS_DEMOSAIC_MAX] = {
	"none",
	"bilinear",
	"pixel-grouping"
};

typedef struct _RSDemosaic RSDemosaic;
typedef struct _RSDemosaicClass RSDemosaicClass;

struct _RSDemosaic {
	RSFilter parent;

	RS_DEMOSAIC method;
};

struct _RSDemosaicClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_demosaic, RSDemosaic)

enum {
	PROP_0,
	PROP_METHOD
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RSFilterResponse *get_image(RSFilter *filter, const RSFilterParam *param);
static inline int fc_INDI (const unsigned int filters, const int row, const int col);
static void border_interpolate_INDI (const ThreadInfo* t, int colors, int border);
static void lin_interpolate_INDI(RS_IMAGE16 *image, RS_IMAGE16 *output, const unsigned int filters, const int colors);
static void ppg_interpolate_INDI(RS_IMAGE16 *image, RS_IMAGE16 *output, const unsigned int filters, const int colors);
static void none_interpolate_INDI(RS_IMAGE16 *in, RS_IMAGE16 *out, const unsigned int filters, const int colors);
static void hotpixel_detect(const ThreadInfo* t);
static void expand_cfa_data(const ThreadInfo* t);


static RSFilterClass *rs_demosaic_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_demosaic_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_demosaic_class_init(RSDemosaicClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_demosaic_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_METHOD, g_param_spec_string(
			"method", "demosaic method", "The demosaic algorithm to use (\"bilinear\" or \"pixel-grouping\")",
			rs_demosaic_ascii[RS_DEMOSAIC_PPG], G_PARAM_READWRITE)
	);

	filter_class->name = "Demosaic filter";
	filter_class->get_image = get_image;
}

static void
rs_demosaic_init(RSDemosaic *demosaic)
{
	demosaic->method = RS_DEMOSAIC_PPG;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSDemosaic *demosaic = RS_DEMOSAIC(object);

	switch (property_id)
	{
		case PROP_METHOD:
			g_value_set_string(value, rs_demosaic_ascii[demosaic->method]);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	gint i;
	const gchar *str;
	RSDemosaic *demosaic = RS_DEMOSAIC(object);

	switch (property_id)
	{
		case PROP_METHOD:
			str = g_value_get_string(value);
			for(i=0;i<RS_DEMOSAIC_MAX;i++)
			{
				if (g_str_equal(rs_demosaic_ascii[i], str))
					demosaic->method = i;
			}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2
 */
#define FC(row,col) \
  (int)(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

static RSFilterResponse *
get_image(RSFilter *filter, const RSFilterParam *param)
{
	RSDemosaic *demosaic = RS_DEMOSAIC(filter);
	RSFilterResponse *previous_response;
	RSFilterResponse *response;
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	guint filters;
	RS_DEMOSAIC method;

	previous_response = rs_filter_get_image(filter->previous, param);

	input = rs_filter_response_get_image(previous_response);

	if (!RS_IS_IMAGE16(input))
		return previous_response;

	/* Just pass on output from previous filter if the image is not CFA */
	if (input->filters == 0)
	{
		g_object_unref(input);
		return previous_response;
	}

	g_assert(input->channels == 1);
	g_assert(input->filters != 0);

	response = rs_filter_response_clone(previous_response);
	g_object_unref(previous_response);
	output = rs_image16_new(input->w, input->h, 3, 4);
	rs_filter_response_set_image(response, output);
	g_object_unref(output);

	method = demosaic->method;
	if (rs_filter_param_get_quick(param))
	{
		method = RS_DEMOSAIC_NONE;
		rs_filter_response_set_quick(response);
	}

	/* Magic - Ask Dave ;) */
	filters = input->filters;
	filters &= ~((filters & 0x55555555) << 1);

	/* Check if pattern is 2x2, otherwise we cannot do "none" demosaic */
	if (method == RS_DEMOSAIC_NONE)
		if (! ( (filters & 0xff ) == ((filters >> 8) & 0xff) &&
			((filters >> 16) & 0xff) == ((filters >> 24) & 0xff) &&
			(filters & 0xff) == ((filters >> 24) &0xff)))
				method = RS_DEMOSAIC_PPG;


	switch (method)
	{
	  case RS_DEMOSAIC_BILINEAR:
			lin_interpolate_INDI(input, output, filters, 3);
			break;
	  case RS_DEMOSAIC_PPG:
			ppg_interpolate_INDI(input,output, filters, 3);
			break;
		case RS_DEMOSAIC_NONE:
			none_interpolate_INDI(input, output, filters, 3);
			break;
		default:
			/* Do nothing */
			break;
		}

	g_object_unref(input);
	return response;
}

/*
The rest of this file is pretty much copied verbatim from dcraw/ufraw
*/

#define FORCC for (c=0; c < colors; c++)


#define BAYER(row,col) \
	image[((row) >> shrink)*iwidth + ((col) >> shrink)][FC(row,col)]

static inline int
fc_INDI (const unsigned int filters, const int row, const int col)
{
  static const char filter[16][16] =
  { { 2,1,1,3,2,3,2,0,3,2,3,0,1,2,1,0 },
    { 0,3,0,2,0,1,3,1,0,1,1,2,0,3,3,2 },
    { 2,3,3,2,3,1,1,3,3,1,2,1,2,0,0,3 },
    { 0,1,0,1,0,2,0,2,2,0,3,0,1,3,2,1 },
    { 3,1,1,2,0,1,0,2,1,3,1,3,0,1,3,0 },
    { 2,0,0,3,3,2,3,1,2,0,2,0,3,2,2,1 },
    { 2,3,3,1,2,1,2,1,2,1,1,2,3,0,0,1 },
    { 1,0,0,2,3,0,0,3,0,3,0,3,2,1,2,3 },
    { 2,3,3,1,1,2,1,0,3,2,3,0,2,3,1,3 },
    { 1,0,2,0,3,0,3,2,0,1,1,2,0,1,0,2 },
    { 0,1,1,3,3,2,2,1,1,3,3,0,2,1,3,2 },
    { 2,3,2,0,0,1,3,0,2,0,1,2,3,0,1,0 },
    { 1,3,1,2,3,2,3,2,0,2,0,1,1,0,3,0 },
    { 0,2,0,3,1,0,0,1,1,3,3,2,3,2,2,1 },
    { 2,1,3,2,3,1,2,1,0,3,0,2,0,2,0,2 },
    { 0,3,1,0,0,2,0,3,2,1,3,1,1,3,1,3 } };

  if (filters != 1) return FC(row,col);
  /* Assume that we are handling the Leaf CatchLight with
   * top_margin = 8; left_margin = 18; */
//  return filter[(row+top_margin) & 15][(col+left_margin) & 15];
  return filter[(row+8) & 15][(col+18) & 15];
}


static void
border_interpolate_INDI (const ThreadInfo* t, int colors, int border)
{
	int row, col, y, x, f, c, sum[8];
	RS_IMAGE16* image = t->output;
	guint filters = t->filters;

	for (row=t->start_y; row < t->end_y; row++)
		for (col=0; col < image->w; col++)
		{
			if (col==border && row >= border && row < image->h-border)
				col = image->w-border;
			memset (sum, 0, sizeof sum);
			for (y=row-1; y != row+2; y++)
				for (x=col-1; x != col+2; x++)
					if (y >= 0 && y < image->h && x >= 0 && x < image->w)
					{
						f = FC(y, x);
						sum[f] += GET_PIXEL(image, x, y)[f];
						sum[f+4]++;
					}
			f = FC(row,col);
			for (c=0; c < colors; c++)
				if (c != f && sum[c+4])
					image->pixels[row*image->rowstride+col*4+c] = sum[c] / sum[c+4];
		}
}

static void
lin_interpolate_INDI(RS_IMAGE16 *input, RS_IMAGE16 *output, const unsigned int filters, const int colors) /*UF*/
{
	ThreadInfo *t = g_new(ThreadInfo, 1);
	t->image = input;
	t->output = output;
	t->filters = filters;
	t->start_y = 0;
	t->end_y = input->w;

	expand_cfa_data(t);
	RS_IMAGE16* image = output;

  int code[16][16][32], *ip, sum[4];
  int c, i, x, y, row, col, shift, color;
  ushort *pix;

  border_interpolate_INDI(t, colors, 1);
  for (row=0; row < 16; row++)
    for (col=0; col < 16; col++) {
      ip = code[row][col];
      memset (sum, 0, sizeof sum);
      for (y=-1; y <= 1; y++)
	for (x=-1; x <= 1; x++) {
	  shift = (y==0) + (x==0);
	  if (shift == 2) continue;
	  color = fc_INDI(filters,row+y,col+x);
	  *ip++ = (image->pitch*y + x)*4 + color;
	  *ip++ = shift;
	  *ip++ = color;
	  sum[color] += 1 << shift;
	}
      FORCC
	if (c != fc_INDI(filters,row,col)) {
	  *ip++ = c;
	  *ip++ = 256 / sum[c];
	}
    }
  for (row=1; row < image->h-1; row++)
    for (col=1; col < image->w-1; col++) {
      pix = GET_PIXEL(image, col, row);
      ip = code[row & 15][col & 15];
      memset (sum, 0, sizeof sum);
      for (i=8; i--; ip+=3)
	sum[ip[2]] += pix[ip[0]] << ip[1];
      for (i=colors; --i; ip+=2)
	pix[ip[0]] = sum[ip[0]] * ip[1] >> 8;
    }
}

static void
expand_cfa_data(const ThreadInfo* t) {

	RS_IMAGE16* input  = t->image;
	RS_IMAGE16* output = t->output;
	guint filters = t->filters;
	guint col, row;

	/* Populate new image with bayer data */
	for(row=t->start_y; row<t->end_y; row++)
	{
		gushort* src = GET_PIXEL(input, 0, row);
		gushort* dest = GET_PIXEL(output, 0, row);
		for(col=0;col<output->w;col++)
		{
			dest[fc_INDI(filters, row, col)] = *src;
			dest += output->pixelsize;
			src++;
		}
	}
}

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
inline guint clampbits16(gint x) { guint32 _y_temp; if( (_y_temp=x>>16) ) x = ~_y_temp >> 16; return x;}

#define CLIP(x) clampbits16(x)
#define ULIM(x,y,z) ((y) < (z) ? CLAMP(x,y,z) : CLAMP(x,z,y))

static void
interpolate_INDI_part(ThreadInfo *t)
{
  RS_IMAGE16 *image = t->output;
  const unsigned int filters = t->filters;
  
  /* Subtract 3 from top and bottom  */
  const int start_y = MAX(3, t->start_y);
  const int end_y = MIN(image->h-3, t->end_y);
  int row, col, c, d;
	int diffA, diffB, guessA, guessB;
	int p = image->pitch;
  ushort (*pix)[4];

  {
/*  Fill in the green layer with gradients and pattern recognition: */
  for (row=start_y; row < end_y; row++)
    for (col=3+(FC(row,3) & 1), c=FC(row,col); col < image->w-3; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);

	guessA = (pix[-1][1] + pix[0][c] + pix[1][1]) * 2
		      - pix[-2*1][c] - pix[2*1][c];
	diffA = ( ABS(pix[-2*1][c] - pix[ 0][c]) +
		    ABS(pix[ 2*1][c] - pix[ 0][c]) +
		    ABS(pix[  -1][1] - pix[ 1][1]) ) * 3 +
		  ( ABS(pix[ 3*1][1] - pix[ 1][1]) +
		    ABS(pix[-3*1][1] - pix[-1][1]) ) * 2;

	guessB = (pix[-p][1] + pix[0][c] + pix[p][1]) * 2
		      - pix[-2*p][c] - pix[2*p][c];
	diffB = ( ABS(pix[-2*p][c] - pix[ 0][c]) +
		    ABS(pix[ 2*p][c] - pix[ 0][c]) +
		    ABS(pix[  -p][1] - pix[ p][1]) ) * 3 +
		  ( ABS(pix[ 3*p][1] - pix[ p][1]) +
		    ABS(pix[-3*p][1] - pix[-p][1]) ) * 2;

		if (diffA > diffB)
			pix[0][1] = ULIM(guessB >> 2, pix[p][1], pix[-p][1]);
		else
			pix[0][1] = ULIM(guessA >> 2, pix[1][1], pix[-1][1]);
    }
/*  Calculate red and blue for each green pixel:		*/
  for (row=start_y-2; row < end_y+2; row++)
    for (col=1+(FC(row,2) & 1), c=FC(row,col+1); col < image->w-1; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);
      pix[0][c] = CLIP((pix[-1][c] + pix[1][c] + 2*pix[0][1]
          - pix[-1][1] - pix[1][1]) >> 1);
      c=2-c;
      pix[0][c] = CLIP((pix[-p][c] + pix[p][c] + 2*pix[0][1]
          - pix[-p][1] - pix[p][1]) >> 1);
      c=2-c;
    }

/*  Calculate blue for red pixels and vice versa:		*/
	for (row=start_y-2; row < end_y+2; row++)
		for (col=1+(FC(row,1) & 1), c=2-FC(row,col); col < image->w-1; col+=2) {
			pix = (ushort (*)[4])GET_PIXEL(image, col, row);
			d = 1 + p;
			diffA = ABS(pix[-d][c] - pix[d][c]) +
				ABS(pix[-d][1] - pix[0][1]) +
				ABS(pix[ d][1] - pix[0][1]);
			guessA = pix[-d][c] + pix[d][c] + 2*pix[0][1]
			- pix[-d][1] - pix[d][1];

			d = p - 1;
			diffB = ABS(pix[-d][c] - pix[d][c]) +
				ABS(pix[-d][1] - pix[0][1]) +
				ABS(pix[ d][1] - pix[0][1]);
			guessB = pix[-d][c] + pix[d][c] + 2*pix[0][1]
				- pix[-d][1] - pix[d][1];

			if (diffA > diffB)
				pix[0][c] = CLIP(guessB >> 1);
			else
				pix[0][c] = CLIP(guessA >> 1);
		}
	}
}

gpointer
start_interp_thread(gpointer _thread_info)
{
	ThreadInfo* t = _thread_info;
	hotpixel_detect(t);
	expand_cfa_data(t);
	border_interpolate_INDI (t, 3, 3);
	interpolate_INDI_part(t);
	g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static void
ppg_interpolate_INDI(RS_IMAGE16 *image, RS_IMAGE16 *output, const unsigned int filters, const int colors)
{
	guint i, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	ThreadInfo *t = g_new(ThreadInfo, threads);

	threaded_h = image->h;
	y_per_thread = (threaded_h + threads-1)/threads;
	y_offset = 0;

	for (i = 0; i < threads; i++)
	{
		t[i].image = image;
		t[i].output = output;
		t[i].filters = filters;
		t[i].start_y = y_offset;
		y_offset += y_per_thread;
		y_offset = MIN(image->h, y_offset);
		t[i].end_y = y_offset;

		t[i].threadid = g_thread_create(start_interp_thread, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);
}

gpointer
start_none_thread(gpointer _thread_info)
{
	gint row, col;
	gushort *src;
	gushort *dest;

	ThreadInfo* t = _thread_info;
	gint ops = t->output->pixelsize;
	gint ors = t->output->rowstride;
	guint filters = t->filters;

	for(row=t->start_y; row < t->end_y; row++)
	{
		src = GET_PIXEL(t->image, 0, row);
		dest = GET_PIXEL(t->output, 0, row);
		guint first = FC(row, 0);
		guint second = FC(row, 1);
		gint col_end = t->output->w & 0xfffe;

		/* Green first or second?*/
		if (first == 1) {
			/* Green first, then red or blue */
			/* Copy non-green to this and pixel below */
			dest[second] = dest[second+ors] = src[1];
			/* Copy green down */
			dest[1+ors] = *src;
			for(col=0 ; col < col_end; col += 2)
			{
				dest[1] = dest[1+ops]= *src;
				/* Move to next pixel */
				src++;
				dest += ops;

				dest[second] = dest[second+ops] =
				dest[second+ors] = dest[second+ops+ors] = *src;

				/* Move to next pixel */
				dest += ops;
				src++;
			}
			/* If uneven pixel width, copy last pixel */
			if (t->output->w & 1) {
				dest[0] = dest[-ops];
				dest[1] = dest[-ops+1];
				dest[2] = dest[-ops+2];
			}
		} else {
			for(col=0 ; col < col_end; col += 2)
			{
				dest[first] = dest[first+ops] =
				dest[first+ors] = dest[first+ops+ors] = *src;

				dest += ops;
				src++;

				dest[1] = dest[1+ops]= *src;

				dest += ops;
				src++;
			}
			/* If uneven pixel width, copy last pixel */
			if (t->output->w & 1) {
				dest[0] = dest[-ops];
				dest[1] = dest[-ops+1];
				dest[2] = dest[-ops+2];
			}
		}
		/*  Duplicate first & last line */
		if (t->end_y == t->output->h - 1) 
		{
			memcpy(GET_PIXEL(t->output, 0, t->end_y), GET_PIXEL(t->output, 0, t->end_y - 1), t->output->rowstride * 2);
			memcpy(GET_PIXEL(t->output, 0, 0), GET_PIXEL(t->output, 0, 1), t->output->rowstride * 2);
		}
	}
	g_thread_exit(NULL);

	return NULL; /* Make the compiler shut up - we'll never return */
}

static void
none_interpolate_INDI(RS_IMAGE16 *in, RS_IMAGE16 *out, const unsigned int filters, const int colors)
{
	guint i, y_offset, y_per_thread, threaded_h;
	const guint threads = rs_get_number_of_processor_cores();
	ThreadInfo *t = g_new(ThreadInfo, threads);

	/* Subtract 1 from bottom  */
	threaded_h = out->h-1;
	y_per_thread = (threaded_h + threads-1)/threads;
	y_offset = 0;

	for (i = 0; i < threads; i++)
	{
		t[i].image = in;
		t[i].filters = filters;
		t[i].start_y = y_offset;
		t[i].output = out;
		y_offset += y_per_thread;
		y_offset = MIN(out->h-1, y_offset);
		t[i].end_y = y_offset;

		t[i].threadid = g_thread_create(start_none_thread, &t[i], TRUE, NULL);
	}

	/* Wait for threads to finish */
	for(i = 0; i < threads; i++)
		g_thread_join(t[i].threadid);

	g_free(t);
}

static void
hotpixel_detect(const ThreadInfo* t)
{

	RS_IMAGE16 *image = t->image;

	gint x, y, end_y;
	y = MAX( 4, t->start_y);
	end_y = MIN(t->end_y, image->h - 4);

	for(; y < end_y; y++)
	{
		gint col_end = image->w - 4;
		gushort* img = GET_PIXEL(image, 0, y);
		gint p = image->rowstride * 2;
		for (x = 4; x < col_end ; x++) {
			/* Calculate minimum difference to surrounding pixels */
			gint left = (int)img[x - 2];
			gint c = (int)img[x];
			gint right = (int)img[x + 2];
			gint up = (int)img[x - p];
			gint down = (int)img[x + p];

			gint d = ABS(c - left);
			d = MIN(d, ABS(c - right));
			d = MIN(d, ABS(c - up));
			d = MIN(d, ABS(c - down));

			/* Also calculate maximum difference between surrounding pixels themselves */
			gint d2 = ABS(left - right);
			d2 = MAX(d2, ABS(up - down));

			/* If difference larger than surrounding pixels by a factor of 4,
				replace with left/right pixel interpolation */

			if ((d > d2 * 10) && (d > 2000)) {
				/* Do extended test! */
				left = (int)img[x - 4];
				right = (int)img[x + 4];
				up = (int)img[x - p * 2];
				down = (int)img[x + p * 2];

				d = MIN(d, ABS(c - left));
				d = MIN(d, ABS(c - right));
				d = MIN(d, ABS(c - up));
				d = MIN(d, ABS(c - down));

				d2 = MAX(d2, ABS(left - right));
				d2 = MAX(d2, ABS(up - down));

				if ((d > d2 * 10) && (d > 2000)) {
					img[x] = (gushort)(((gint)img[x-2] + (gint)img[x+2] + 1) >> 1);
				}
			}

		}
	}
}

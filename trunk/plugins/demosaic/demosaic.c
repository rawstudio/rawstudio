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
#include <string.h>

#define RS_TYPE_DEMOSAIC (rs_demosaic_type)
#define RS_DEMOSAIC(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_DEMOSAIC, RSDemosaic))
#define RS_DEMOSAIC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_DEMOSAIC, RSDemosaicClass))
#define RS_IS_DEMOSAIC(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_DEMOSAIC))

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
static RS_IMAGE16 *get_image(RSFilter *filter);
static inline int fc_INDI (const unsigned filters, const int row, const int col);
static void border_interpolate_INDI (RS_IMAGE16 *image, const unsigned filters, int colors, int border);
static void lin_interpolate_INDI(RS_IMAGE16 *image, const unsigned filters, const int colors);
static void ppg_interpolate_INDI(RS_IMAGE16 *image, const unsigned filters, const int colors);

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


static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSDemosaic *demosaic = RS_DEMOSAIC(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;
	gint row, col;
	guint filters;
	gushort *src;
	gushort *dest;

	input = rs_filter_get_image(filter->previous);

	/* Just pass on output from previous filter if the image is not CFA */
	if (input->filters == 0)
		return input;

	g_assert(input->channels == 1);
	g_assert(input->filters != 0);

	output = rs_image16_new(input->w, input->h, 3, 4);

	/* Magic - Ask Dave ;) */
	filters = input->filters;
	filters &= ~((filters & 0x55555555) << 1);

	/* Populate new image with bayer data */
	for(row=0; row<output->h; row++)
	{
		src = GET_PIXEL(input, 0, row);
		dest = GET_PIXEL(output, 0, row);
		for(col=0;col<output->w;col++)
		{
			dest[fc_INDI(filters, row, col)] = *src;
			dest += output->pixelsize;
			src++;
		}
	}

	/* Do the actual demosaic */
	switch (demosaic->method)
	{
		case RS_DEMOSAIC_BILINEAR:
			lin_interpolate_INDI(output, filters, 3);
			break;
		case RS_DEMOSAIC_PPG:
			ppg_interpolate_INDI(output, filters, 3);
			break;
		default:
			/* Do nothing */
			break;
	}

	g_object_unref(input);
	return output;
}

/*
The rest of this file is pretty much copied verbatim from dcraw/ufraw
*/

#ifdef _OPENMP
 #error Check all code herein for openmp+gthread-issues
#endif

#define FORCC for (c=0; c < colors; c++)

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2
 */
#define FC(row,col) \
	(int)(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

#define BAYER(row,col) \
	image[((row) >> shrink)*iwidth + ((col) >> shrink)][FC(row,col)]

static inline int
fc_INDI (const unsigned filters, const int row, const int col)
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
border_interpolate_INDI (RS_IMAGE16 *image, const unsigned filters, int colors, int border)
{
  int row, col, y, x, f, c, sum[8];

  for (row=0; row < image->h; row++)
    for (col=0; col < image->w; col++) {
      if (col==border && row >= border && row < image->h-border)
	col = image->w-border;
      memset (sum, 0, sizeof sum);
      for (y=row-1; y != row+2; y++)
	for (x=col-1; x != col+2; x++)
	  if (y >= 0 && y < image->h && x >= 0 && x < image->w) {
	    f = fc_INDI(filters, y, x);
	    sum[f] += GET_PIXEL(image, x, y)[f];
	    sum[f+4]++;
	  }
      f = fc_INDI(filters,row,col);
      for (c=0; c < colors; c++)
		  if (c != f && sum[c+4])
	image->pixels[row*image->rowstride+col*4+c] = sum[c] / sum[c+4];
    }
}

static void
lin_interpolate_INDI(RS_IMAGE16 *image, const unsigned filters, const int colors) /*UF*/
{
  int code[16][16][32], *ip, sum[4];
  int c, i, x, y, row, col, shift, color;
  ushort *pix;

  border_interpolate_INDI(image, filters, colors, 1);
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
#ifdef _OPENMP
#pragma omp parallel for default(shared) private(row,col,pix,ip,sum,i)
#endif
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

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
#define CLIP(x) CLAMP(x,0,65535)
#define ULIM(x,y,z) ((y) < (z) ? CLAMP(x,y,z) : CLAMP(x,z,y))

static void
ppg_interpolate_INDI(RS_IMAGE16 *image, const unsigned filters, const int colors)
{
  int dir[5] = { 1, image->pitch, -1, -image->pitch, 1 };
  int row, col, diff[2], guess[2], c, d, i;
  ushort (*pix)[4];

  border_interpolate_INDI (image, filters, colors, 3);

#ifdef _OPENMP
#pragma omp parallel					\
  default(none)						\
  shared(image,dir)					\
  private(row,col,i,d,c,pix,diff,guess)
#endif
  {
/*  Fill in the green layer with gradients and pattern recognition: */
#ifdef _OPENMP
#pragma omp for
#endif
  for (row=3; row < image->h-3; row++)
    for (col=3+(FC(row,3) & 1), c=FC(row,col); col < image->w-3; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);
      for (i=0; (d=dir[i]) > 0; i++) {
	guess[i] = (pix[-d][1] + pix[0][c] + pix[d][1]) * 2
		      - pix[-2*d][c] - pix[2*d][c];
	diff[i] = ( ABS(pix[-2*d][c] - pix[ 0][c]) +
		    ABS(pix[ 2*d][c] - pix[ 0][c]) +
		    ABS(pix[  -d][1] - pix[ d][1]) ) * 3 +
		  ( ABS(pix[ 3*d][1] - pix[ d][1]) +
		    ABS(pix[-3*d][1] - pix[-d][1]) ) * 2;
      }
      d = dir[i = diff[0] > diff[1]];
      pix[0][1] = ULIM(guess[i] >> 2, pix[d][1], pix[-d][1]);
    }
/*  Calculate red and blue for each green pixel:		*/
#ifdef _OPENMP
#pragma omp for
#endif
  for (row=1; row < image->h-1; row++)
    for (col=1+(FC(row,2) & 1), c=FC(row,col+1); col < image->w-1; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);
      for (i=0; (d=dir[i]) > 0; c=2-c, i++)
	pix[0][c] = CLIP((pix[-d][c] + pix[d][c] + 2*pix[0][1]
			- pix[-d][1] - pix[d][1]) >> 1);
    }
/*  Calculate blue for red pixels and vice versa:		*/
#ifdef _OPENMP
#pragma omp for
#endif
  for (row=1; row < image->h-1; row++)
    for (col=1+(FC(row,1) & 1), c=2-FC(row,col); col < image->w-1; col+=2) {
      pix = (ushort (*)[4])GET_PIXEL(image, col, row);
      for (i=0; (d=dir[i]+dir[i+1]) > 0; i++) {
	diff[i] = ABS(pix[-d][c] - pix[d][c]) +
		  ABS(pix[-d][1] - pix[0][1]) +
		  ABS(pix[ d][1] - pix[0][1]);
	guess[i] = pix[-d][c] + pix[d][c] + 2*pix[0][1]
		 - pix[-d][1] - pix[d][1];
      }
      if (diff[0] != diff[1])
	pix[0][c] = CLIP(guess[diff[0] > diff[1]] >> 1);
      else
	pix[0][c] = CLIP((guess[0]+guess[1]) >> 2);
    }
  }
}

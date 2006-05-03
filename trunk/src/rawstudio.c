#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <math.h> /* pow() */
#include <string.h> /* memset() */
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"
#include "gtk-interface.h"
#include "rs-cache.h"
#include "color.h"

#define cpuid(n) \
  a = b = c = d = 0x0; \
  asm( \
  	"cpuid" \
  	: "=eax" (a), "=ebx" (b), "=ecx" (c), "=edx" (d) : "0" (n) \
	)

guint cpuflags = 0;
guchar previewtable[65536];

static RS_FILETYPE filetypes[] = {
	{"cr2", rs_load_raw_from_file, rs_thumb_grt},
	{"crw", rs_load_raw_from_file, rs_thumb_grt},
	{"nef", rs_load_raw_from_file, rs_thumb_grt},
	{"tif", rs_load_raw_from_file, rs_thumb_grt},
	{"orf", rs_load_raw_from_file, rs_thumb_grt},
	{"raw", rs_load_raw_from_file, NULL},
	{"jpg", rs_load_gdk, rs_thumb_gdk},
	{NULL, NULL}
};

void
update_previewtable(RS_BLOB *rs, const gdouble gamma, const gdouble contrast)
{
	gint n;
	gdouble nd;
	gint res;
	double gammavalue;
	const double postadd = 0.5 - (contrast/2.0);
	gammavalue = (1.0/gamma);

	for(n=0;n<65536;n++)
	{
		nd = ((gdouble) n) / 65535.0;
		nd *= contrast;
		res = (gint) ((pow(nd+postadd, gammavalue)) * 255.0);
		_CLAMP255(res);
		previewtable[n] = res;
	}
}

void
print_debug_line(const char *format, const gint value, const gboolean a)
{
	if (!a)
		printf("\033[31m");
	else
		printf("\033[33m");
	printf(format, value);
	printf("\033[0m");
}

void
rs_image16_debug(RS_IMAGE16 *rsi)
{
	print_debug_line("rsi: %d\n", (gint) rsi, (rsi!=NULL));
	print_debug_line("rsi->w: %d\n", rsi->w, ((rsi->w<5000)&&(rsi->w>0)));
	print_debug_line("rsi->h: %d\n", rsi->h, ((rsi->h<5000)&&(rsi->h>0)));
	print_debug_line("rsi->pitch: %d\n", rsi->pitch, (rsi->pitch == PITCH(rsi->w)));
	print_debug_line("rsi->rowstride: %d\n", rsi->rowstride, (rsi->rowstride == (PITCH(rsi->w)*rsi->channels)));
	print_debug_line("rsi->channels: %d\n", rsi->channels, ((rsi->channels<5)&&(rsi->channels>2)));
	print_debug_line("rsi->direction: %d\n", rsi->direction, ((rsi->channels<8)&&(rsi->channels>=0)));
	printf("\n");
	return;
}

void
rs_debug(RS_BLOB *rs)
{
	printf("rs: %d\n", (guint) rs);
	printf("rs->input: %d\n", (guint) rs->input);
	printf("rs->scaled: %d\n", (guint) rs->scaled);
	if(rs->input!=NULL)
	{
		printf("rs->input->w: %d\n", rs->input->w);
		printf("rs->input->h: %d\n", rs->input->h);
		printf("rs->input->pitch: %d\n", rs->input->pitch);
		printf("rs->input->channels: %d\n", rs->input->channels);
		printf("rs->input->pixels: %d\n", (guint) rs->input->pixels);
	}
	if(rs->scaled!=NULL)
	{
		printf("rs->scaled->w: %d\n", rs->scaled->w);
		printf("rs->scaled->h: %d\n", rs->scaled->h);
		printf("rs->scaled->pitch: %d\n", rs->scaled->pitch);
		printf("rs->preview_scale: %d\n", rs->preview_scale);
		printf("rs->scaled->pixels: %d\n", (guint) rs->scaled->pixels);
	}
	printf("\n");
	return;
}

void
update_scaled(RS_BLOB *rs)
{
	guint width, height;
	const guint scale = GETVAL(rs->scale);

	width=rs->input->w/scale;
	height=rs->input->h/scale;
	
	if (!rs->in_use) return;

	if (rs->scaled==NULL)
	{
		rs->scaled = rs_image16_new(width, height, rs->input->channels);
		rs->preview = rs_image8_new(width, height, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, width, height);
	}

	/* 16 bit downscaled */
	if (rs->preview_scale != GETVAL(rs->scale)) /* do we need to? */
	{
		rs->preview_scale = GETVAL(rs->scale);
		rs_image16_free(rs->scaled);
		rs->scaled = rs_image16_new(width, height, rs->input->channels);
		rs_image16_scale(rs->input, rs->scaled, rs->preview_scale);
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->scaled->w, rs->scaled->h);
	}

	if (rs->direction != rs->scaled->direction)
		rs_image16_direction(rs->scaled, rs->direction);

	if (rs->scaled->w != rs->preview->w)
	{
		rs_image8_free(rs->preview);
		rs->preview = rs_image8_new(rs->scaled->w, rs->scaled->h, 3);
		gtk_widget_set_size_request(rs->preview_drawingarea, rs->scaled->w, rs->scaled->h);
	}
	return;
}

void
update_preview(RS_BLOB *rs)
{
	RS_MATRIX4 mat;

	if(unlikely(!rs->in_use)) return;

	SETVAL(rs->scale, floor(GETVAL(rs->scale))); // we only do integer scaling
	update_scaled(rs);
	update_previewtable(rs, GETVAL(rs->settings[rs->current_setting]->gamma),
		GETVAL(rs->settings[rs->current_setting]->contrast));
	matrix4_identity(&mat);
	matrix4_color_exposure(&mat, GETVAL(rs->settings[rs->current_setting]->exposure));
	matrix4_color_mixer(&mat, GETVAL(rs->settings[rs->current_setting]->rgb_mixer[R]),
		GETVAL(rs->settings[rs->current_setting]->rgb_mixer[G]),
		GETVAL(rs->settings[rs->current_setting]->rgb_mixer[B]));
	matrix4_color_mixer(&mat, (1.0+GETVAL(rs->settings[rs->current_setting]->warmth))
		*(1.0+GETVAL(rs->settings[rs->current_setting]->tint)),
		1.0,
		(1.0-GETVAL(rs->settings[rs->current_setting]->warmth))
		*(1.0+GETVAL(rs->settings[rs->current_setting]->tint)));
	matrix4_color_saturate(&mat, GETVAL(rs->settings[rs->current_setting]->saturation));
	matrix4_color_hue(&mat, GETVAL(rs->settings[rs->current_setting]->hue));
	matrix4_to_matrix4int(&mat, &rs->mati);
	update_preview_region(rs, rs->preview_exposed->x1, rs->preview_exposed->y1,
		rs->preview_exposed->x2, rs->preview_exposed->y2);

	/* Reset histogram_table */
	if (GTK_WIDGET_VISIBLE(rs->histogram_image))
	{
		memset(rs->histogram_table, 0x00, sizeof(guint)*3*256);
		rs_histogram_update_table(rs->mati, rs->histogram_dataset, (guint *) rs->histogram_table);
		update_histogram(rs);
	}

	rs->preview_done = FALSE;
	rs->preview_idle_render_lastrow = 0;
	if (!rs->preview_idle_render)
	{
		rs->preview_idle_render = TRUE;
		g_idle_add((GSourceFunc) rs_render_idle, rs);
	}

	return;
}	

void
update_preview_region(RS_BLOB *rs, gint x1, gint y1, gint x2, gint y2)
{
	guchar *pixels;
	gushort *in;

	if (unlikely(!rs->in_use)) return;

	_CLAMP(x2, rs->scaled->w);
	_CLAMP(y2, rs->scaled->h);

	pixels = rs->preview->pixels+(y1*rs->preview->rowstride+x1*rs->preview->channels);
	in = rs->scaled->pixels+(y1*rs->scaled->rowstride+x1*rs->scaled->channels);
	rs_render(rs->mati, x2-x1, y2-y1, in, rs->scaled->rowstride,
		rs->scaled->channels, pixels, rs->preview->rowstride);
	gdk_draw_rgb_image(rs->preview_drawingarea->window, rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL],
		x1, y1, x2-x1, y2-y1,
		GDK_RGB_DITHER_NONE, pixels, rs->preview->rowstride);
	return;
}

gboolean
rs_render_idle(RS_BLOB *rs)
{
	gint row;
	gushort *in;
	guchar *out;

	if (rs->in_use && (!rs->preview_done))
		for(row=rs->preview_idle_render_lastrow; row<rs->scaled->h; row++)
		{
			if (gtk_events_pending()) return(TRUE);
			in = rs->scaled->pixels + row*rs->scaled->rowstride;
			out = rs->preview->pixels + row*rs->preview->rowstride;
			rs_render(rs->mati, rs->scaled->w, 1, in, rs->scaled->rowstride,
				rs->scaled->channels, out, rs->preview->rowstride);
			gdk_draw_rgb_image(rs->preview_backing,
				rs->preview_drawingarea->style->fg_gc[GTK_STATE_NORMAL], 0, row,
				rs->scaled->w, 1, GDK_RGB_DITHER_NONE, out,
				rs->preview->rowstride);
			rs->preview_idle_render_lastrow=row;
		}
	rs->preview_idle_render_lastrow = 0;
	rs->preview_done = TRUE;
	rs->preview_idle_render = FALSE;
	return(FALSE);
}

inline void
rs_render(RS_MATRIX4Int mati, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride)
{
	int srcoffset, destoffset;
	register int x,y;
	register int r,g,b;
	for(y=0 ; y<height ; y++)
	{
		destoffset = y * out_rowstride;
		srcoffset = y * in_rowstride;
		for(x=0 ; x<width ; x++)
		{
			r = (in[srcoffset+R]*mati.coeff[0][0]
				+ in[srcoffset+G]*mati.coeff[0][1]
				+ in[srcoffset+B]*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (in[srcoffset+R]*mati.coeff[1][0]
				+ in[srcoffset+G]*mati.coeff[1][1]
				+ in[srcoffset+B]*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (in[srcoffset+R]*mati.coeff[2][0]
				+ in[srcoffset+G]*mati.coeff[2][1]
				+ in[srcoffset+B]*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			out[destoffset++] = previewtable[r];
			out[destoffset++] = previewtable[g];
			out[destoffset++] = previewtable[b];
			srcoffset+=in_channels;
		}
	}
	return;
}

inline void
rs_histogram_update_table(RS_MATRIX4Int mati, RS_IMAGE16 *input, guint *table)
{
	gint y,x;
	gint srcoffset;
	gint r,g,b;
	gushort *in;

	if (unlikely(input==NULL)) return;

	in	= input->pixels;
	for(y=0 ; y<input->h ; y++)
	{
		srcoffset = y * input->rowstride;
		for(x=0 ; x<input->w ; x++)
		{
			r = (in[srcoffset+R]*mati.coeff[0][0]
				+ in[srcoffset+G]*mati.coeff[0][1]
				+ in[srcoffset+B]*mati.coeff[0][2])>>MATRIX_RESOLUTION;
			g = (in[srcoffset+R]*mati.coeff[1][0]
				+ in[srcoffset+G]*mati.coeff[1][1]
				+ in[srcoffset+B]*mati.coeff[1][2])>>MATRIX_RESOLUTION;
			b = (in[srcoffset+R]*mati.coeff[2][0]
				+ in[srcoffset+G]*mati.coeff[2][1]
				+ in[srcoffset+B]*mati.coeff[2][2])>>MATRIX_RESOLUTION;
			_CLAMP65535_TRIPLET(r,g,b);
			table[previewtable[r]]++;
			table[256+previewtable[g]]++;
			table[512+previewtable[b]]++;
			srcoffset+=input->channels;
		}
	}
	return;
}

void
rs_reset(RS_BLOB *rs)
{
	gint c;
	rs->preview_scale = 0;
	rs->priority = -1;
	DIRECTION_RESET(rs->direction);
	for(c=0;c<3;c++)
		rs_settings_reset(rs->settings[c]);
	return;
}

void
rs_free_raw(RS_BLOB *rs)
{
	dcraw_close(rs->raw);
	g_free(rs->raw);
	rs->raw = NULL;
}

void
rs_free(RS_BLOB *rs)
{
	if (rs->in_use)
	{
		g_free(rs->input->pixels);
		rs->input->pixels=0;
		rs->input->w=0;
		rs->input->h=0;
		if (rs->raw!=NULL)
			rs_free_raw(rs);
		if (rs->input!=NULL)
			rs_image16_free(rs->input);
		if (rs->scaled!=NULL)
			rs_image16_free(rs->scaled);
		rs->input=NULL;
		rs->scaled=NULL;
		rs->in_use=FALSE;
	}
}

void
rs_image16_direction(RS_IMAGE16 *rsi, const gint direction)
{
	const gint rot = ((direction&3)-(rsi->direction&3)+8)%4;

	rs_image16_rotate(rsi, rot);
	if (((rsi->direction)&4)^((direction)&4))
		rs_image16_flip(rsi);

	return;
}

void
rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns)
{
	gint width, height, pitch;
	gint y,x;
	gint offset,destoffset;
	gushort *swap;

	quarterturns %= 4;

	switch (quarterturns)
	{
		case 1:
			width = rsi->h;
			height = rsi->w;
			pitch = PITCH(width);
			swap = (gushort *) g_malloc(pitch*height*sizeof(gushort)*rsi->channels);
			for(y=0; y<rsi->h; y++)
			{
				offset = y * rsi->pitch * rsi->channels;
				for(x=0; x<rsi->w; x++)
				{
					destoffset = (width-1-y+pitch*x)*rsi->channels;
					swap[destoffset+R] = rsi->pixels[offset+R];
					swap[destoffset+G] = rsi->pixels[offset+G];
					swap[destoffset+B] = rsi->pixels[offset+B];
					if (rsi->channels==4) swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=rsi->channels;
				}
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			rsi->rowstride = pitch * rsi->channels;
			DIRECTION_90(rsi->direction);
			break;
		case 2:
			rs_image16_flip(rsi);
			rs_image16_mirror(rsi);
			break;
		case 3:
			width = rsi->h;
			height = rsi->w;
			pitch = PITCH(width);
			swap = (gushort *) g_malloc(pitch*height*sizeof(gushort)*rsi->channels);
			for(y=0; y<rsi->h; y++)
			{
				offset = y*rsi->pitch*rsi->channels;
				destoffset = ((height-1)*pitch + y)*rsi->channels;
				for(x=0; x<rsi->w; x++)
				{
					swap[destoffset+R] = rsi->pixels[offset+R];
					swap[destoffset+G] = rsi->pixels[offset+G];
					swap[destoffset+B] = rsi->pixels[offset+B];
					if (rsi->channels==4) swap[destoffset+G2] = rsi->pixels[offset+G2];
					offset+=rsi->channels;
					destoffset -= pitch*rsi->channels;
				}
				offset += rsi->pitch*rsi->channels;
			}
			g_free(rsi->pixels);
			rsi->pixels = swap;
			rsi->w = width;
			rsi->h = height;
			rsi->pitch = pitch;
			rsi->rowstride = pitch * rsi->channels;
			DIRECTION_270(rsi->direction);
			break;
		default:
			break;
	}
	return;
}

void
rs_image16_mirror(RS_IMAGE16 *rsi)
{
	gint row,col;
	gint offset,destoffset;
	for(row=0;row<rsi->h;row++)
	{
		offset = row*rsi->pitch*rsi->channels;
		destoffset = (row*rsi->pitch+rsi->w-1)*rsi->channels;
		for(col=0;col<rsi->w/2;col++)
		{
			SWAP(rsi->pixels[offset+R], rsi->pixels[destoffset+R]);
			SWAP(rsi->pixels[offset+G], rsi->pixels[destoffset+G]);
			SWAP(rsi->pixels[offset+B], rsi->pixels[destoffset+B]);
			SWAP(rsi->pixels[offset+G2], rsi->pixels[destoffset+G2]);
			offset+=rsi->channels;
			destoffset-=rsi->channels;
		}
	}
	DIRECTION_MIRROR(rsi->direction);
}

void
rs_image16_flip(RS_IMAGE16 *rsi)
{
	if (cpuflags & _MMX)
	{
		gint row,col;
		void *src, *dest;
		for(row=0;row<rsi->h/2;row++)
		{
			src = rsi->pixels + row * rsi->pitch*rsi->channels;
			dest = rsi->pixels + (rsi->h - row - 1) * rsi->pitch*rsi->channels;
			for(col=0;col<rsi->w*rsi->channels*2;col+=16)
			{
				asm volatile (
					"movq (%0), %%mm0\n\t"
					"movq (%1), %%mm1\n\t"
					"movq 8(%0), %%mm2\n\t"
					"movq 8(%1), %%mm3\n\t"
					"movq %%mm0, (%1)\n\t"
					"movq %%mm1, (%0)\n\t"
					"movq %%mm2, 8(%1)\n\t"
					"movq %%mm3, 8(%0)\n\t"
					"add $16, %0\n\t"
					"add $16, %1\n\t"
					: "+r" (src), "+r" (dest)
				);
			}
		}
		asm volatile ("emms");
	}
	else
	{
		gint row;
		const gint linel = rsi->pitch*rsi->channels*sizeof(gushort);
		gushort *tmp = (gushort *) g_malloc(linel);
		for(row=0;row<rsi->h/2;row++)
		{
			memcpy(tmp,
				rsi->pixels + row * rsi->pitch * rsi->channels, linel);
			memcpy(rsi->pixels + row * rsi->pitch * rsi->channels,
				rsi->pixels + (rsi->h-1-row) * rsi->pitch * rsi->channels, linel);
			memcpy(rsi->pixels + (rsi->h-1-row) * rsi->pitch * rsi->channels,
				tmp, linel);
		}
		g_free(tmp);
	}
	DIRECTION_FLIP(rsi->direction);
	return;
}

RS_IMAGE16 *
rs_image16_scale(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale)
{
	gint x,y;
	gint destoffset, srcoffset;
	gint iscale;

	iscale = (int) scale;
	if (iscale<1) iscale=1;

	if (out==NULL)
		out = rs_image16_new(in->w/iscale, in->h/iscale, in->channels);
	else
	{
		g_assert(in->channels == out->channels);
		g_assert(out->w == (in->w/iscale));
	}

	for(y=0; y<out->h; y++)
	{
		destoffset = y*out->pitch*out->channels;
		srcoffset = y*iscale*in->pitch*out->channels;
		for(x=0; x<out->w; x++)
		{
			out->pixels[destoffset+R] = in->pixels[srcoffset+R];
			out->pixels[destoffset+G] = in->pixels[srcoffset+G];
			out->pixels[destoffset+B] = in->pixels[srcoffset+B];
			if (in->channels==4)
				out->pixels[destoffset+G2] = in->pixels[srcoffset+G2];
			destoffset += out->channels;
			srcoffset += iscale*out->channels;
		}
	}
	return(out);
}

RS_IMAGE16 *
rs_image16_new(const guint width, const guint height, const guint channels)
{
	RS_IMAGE16 *rsi;
	rsi = (RS_IMAGE16 *) g_malloc(sizeof(RS_IMAGE16));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * channels;
	rsi->channels = channels;
	DIRECTION_RESET(rsi->direction);
	rsi->pixels = (gushort *) g_malloc(sizeof(gushort)*rsi->h*rsi->pitch*rsi->channels);
	return(rsi);
}

void
rs_image16_free(RS_IMAGE16 *rsi)
{
	if (rsi!=NULL)
	{
		g_assert(rsi->pixels!=NULL);
		g_free(rsi->pixels);
		g_assert(rsi!=NULL);
		g_free(rsi);
	}
	return;
}

RS_IMAGE8 *
rs_image8_new(const guint width, const guint height, const guint channels)
{
	RS_IMAGE8 *rsi;
	rsi = (RS_IMAGE8 *) g_malloc(sizeof(RS_IMAGE8));
	rsi->w = width;
	rsi->h = height;
	rsi->pitch = PITCH(width);
	rsi->rowstride = rsi->pitch * channels;
	rsi->channels = channels;
	DIRECTION_RESET(rsi->direction);
	rsi->pixels = (guchar *) g_malloc(sizeof(guchar)*rsi->h*rsi->pitch*rsi->channels);
	return(rsi);
}

void
rs_image8_free(RS_IMAGE8 *rsi)
{
	if (rsi!=NULL)
	{
		g_assert(rsi->pixels!=NULL);
		g_free(rsi->pixels);
		g_assert(rsi!=NULL);
		g_free(rsi);
	}
	return;
}

void
rs_settings_reset(RS_SETTINGS *rss)
{
	gtk_adjustment_set_value((GtkAdjustment *) rss->exposure, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->gamma, 2.2);
	gtk_adjustment_set_value((GtkAdjustment *) rss->saturation, 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->hue, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->rgb_mixer[0], 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->rgb_mixer[1], 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->rgb_mixer[2], 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->contrast, 1.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->warmth, 0.0);
	gtk_adjustment_set_value((GtkAdjustment *) rss->tint, 0.0);
	return;
}

RS_SETTINGS *
rs_settings_new()
{
	RS_SETTINGS *rss;
	rss = g_malloc(sizeof(RS_SETTINGS));
	rss->exposure = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	rss->gamma = gtk_adjustment_new(2.2, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->saturation = gtk_adjustment_new(1.0, 0.0, 3.0, 0.1, 0.5, 0.0);
	rss->hue = gtk_adjustment_new(0.0, 0.0, 360.0, 0.1, 30.0, 0.0);
	rss->rgb_mixer[0] = gtk_adjustment_new(1.0, 0.0, 5.0, 0.1, 0.5, 0.0);
	rss->rgb_mixer[1] = gtk_adjustment_new(1.0, 0.0, 5.0, 0.1, 0.5, 0.0);
	rss->rgb_mixer[2] = gtk_adjustment_new(1.0, 0.0, 5.0, 0.1, 0.5, 0.0);
	rss->contrast = gtk_adjustment_new(1.0, 0.0, 2.0, 0.1, 0.5, 0.0);
	rss->warmth = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	rss->tint = gtk_adjustment_new(0.0, -2.0, 2.0, 0.1, 0.5, 0.0);
	return(rss);
}

void
rs_settings_free(RS_SETTINGS *rss)
{
	if (rss!=NULL)
	{
		g_object_unref(rss->exposure);
		g_object_unref(rss->gamma);
		g_object_unref(rss->saturation);
		g_object_unref(rss->hue);
		g_object_unref(rss->rgb_mixer[0]);
		g_object_unref(rss->rgb_mixer[1]);
		g_object_unref(rss->rgb_mixer[2]);
		g_object_unref(rss->contrast);
		g_object_unref(rss->warmth);
		g_object_unref(rss->tint);
		g_free(rss);
	}
	return;
}

RS_BLOB *
rs_new()
{
	RS_BLOB *rs;
	guint c;
	rs = g_malloc(sizeof(RS_BLOB));
	rs->scale = gtk_adjustment_new(2.0, 1.0, 5.0, 1.0, 1.0, 0.0);
	gtk_signal_connect(GTK_OBJECT(rs->scale), "value_changed",
		G_CALLBACK(update_preview_callback), rs);
	rs->raw = NULL;
	rs->input = NULL;
	rs->scaled = NULL;
	rs->preview = NULL;
	rs->histogram_dataset = NULL;
	DIRECTION_RESET(rs->direction);
	rs->preview_exposed = (RS_RECT *) g_malloc(sizeof(RS_RECT));
	rs->preview_backing = NULL;
	rs->preview_done = FALSE;
	rs->preview_idle_render = FALSE;
	for(c=0;c<3;c++)
		rs->settings[c] = rs_settings_new();
	rs->current_setting = 0;
	rs->in_use = FALSE;
	return(rs);
}

void
rs_load_raw_from_file(RS_BLOB *rs, const gchar *filename)
{
	dcraw_data *raw;
	gushort *src;
	guint x,y;
	guint srcoffset, destoffset;
	gint64 shift;

	if (rs->raw!=NULL) rs_free_raw(rs);
	raw = (dcraw_data *) g_malloc(sizeof(dcraw_data));
	if (!dcraw_open(raw, (char *) filename))
	{
		rs->in_use=FALSE;
		dcraw_load_raw(raw);
		shift = (gint64) (16.0-log((gdouble) raw->rgbMax)/log(2.0)+0.5);
		rs_image16_free(rs->input); rs->input = NULL;
		rs_image16_free(rs->scaled); rs->scaled = NULL;
		rs_image16_free(rs->histogram_dataset); rs->histogram_dataset = NULL;
		rs_image8_free(rs->preview); rs->preview = NULL;
		rs->input = rs_image16_new(raw->raw.width, raw->raw.height, 4);
		rs->raw = raw;
		src  = (gushort *) rs->raw->raw.image;
		if (cpuflags & _MMX)
		{
			char b[8];
			gushort *sub = (gushort *) b;
			sub[0] = rs->raw->black;
			sub[1] = rs->raw->black;
			sub[2] = rs->raw->black;
			sub[3] = rs->raw->black;
			for (y=0; y<rs->raw->raw.height; y++)
			{
				destoffset = (guint) (rs->input->pixels + y*rs->input->pitch * rs->input->channels);
				srcoffset = (guint) (src + y * rs->input->w * rs->input->channels);
				x = rs->raw->raw.width;
				asm volatile (
					"movl %3, %%eax\n\t" /* copy x to %eax */
					"movq (%2), %%mm7\n\t" /* put black in %mm7 */
					"movq (%4), %%mm6\n\t" /* put shift in %mm6 */
					".p2align 4,,15\n"
					"load_raw_inner_loop:\n\t"
					"movq (%1), %%mm0\n\t" /* load source */
					"movq 8(%1), %%mm1\n\t"
					"movq 16(%1), %%mm2\n\t"
					"movq 24(%1), %%mm3\n\t"
					"psubusw %%mm7, %%mm0\n\t" /* subtract black */
					"psubusw %%mm7, %%mm1\n\t"
					"psubusw %%mm7, %%mm2\n\t"
					"psubusw %%mm7, %%mm3\n\t"
					"psllw %%mm6, %%mm0\n\t" /* bitshift */
					"psllw %%mm6, %%mm1\n\t"
					"psllw %%mm6, %%mm2\n\t"
					"psllw %%mm6, %%mm3\n\t"
					"movq %%mm0, (%0)\n\t" /* write destination */
					"movq %%mm1, 8(%0)\n\t"
					"movq %%mm2, 16(%0)\n\t"
					"movq %%mm3, 24(%0)\n\t"
					"sub $4, %%eax\n\t"
					"add $32, %0\n\t"
					"add $32, %1\n\t"
					"cmp $3, %%eax\n\t"
					"jg load_raw_inner_loop\n\t"
					"cmp $1, %%eax\n\t"
					"jb load_raw_inner_done\n\t"
					".p2align 4,,15\n"
					"load_raw_leftover:\n\t"
					"movq (%1), %%mm0\n\t" /* leftover pixels */
					"psubusw %%mm7, %%mm0\n\t"
					"psllw $4, %%mm0\n\t"
					"movq %%mm0, (%0)\n\t"
					"sub $1, %%eax\n\t"
					"cmp $0, %%eax\n\t"
					"jg load_raw_leftover\n\t"
					"load_raw_inner_done:\n\t"
					"emms\n\t" /* clean up */
					: "+r" (destoffset), "+r" (srcoffset)
					: "r" (sub), "r" (x), "r" (&shift)
					: "%eax"
				);
			}
		}
		else
		{
			for (y=0; y<rs->raw->raw.height; y++)
			{
				destoffset = y*rs->input->pitch*rs->input->channels;
				srcoffset = y*rs->input->w*rs->input->channels;
				for (x=0; x<rs->raw->raw.width; x++)
				{
					register gint r,g,b;
					r = (src[srcoffset++] - rs->raw->black)<<shift;
					g = (src[srcoffset++] - rs->raw->black)<<shift;
					b = (src[srcoffset++] - rs->raw->black)<<shift;
					_CLAMP65535_TRIPLET(r, g, b);
					rs->input->pixels[destoffset++] = r;
					rs->input->pixels[destoffset++] = g;
					rs->input->pixels[destoffset++] = b;

					if (rs->input->channels==4)
					{
						g = (src[srcoffset++] - rs->raw->black)<<4;
						_CLAMP65535(g);
						rs->input->pixels[destoffset++] = g;
					}
				}
			}
		}
		rs_reset(rs);
		rs->histogram_dataset = rs_image16_scale(rs->input, NULL,
			rs->input->w/HISTOGRAM_DATASET_WIDTH);
		for(x=0;x<4;x++)
			rs->pre_mul[x] = rs->raw->pre_mul[x];
		rs->filename = filename;
		rs_reset(rs);
		rs_cache_load(rs);
		rs->in_use=TRUE;
	}
	return;
}

RS_FILETYPE *
rs_filetype_get(const gchar *filename, gboolean load)
{
	RS_FILETYPE *filetype = NULL;
	gchar *iname;
	gint n;
	iname = g_ascii_strdown(filename,-1);
	n = 0;
	while(filetypes[n].ext)
	{
		if (g_str_has_suffix(iname, filetypes[n].ext))
		{
			if ((!load) || (filetypes[n].load))
			{
				filetype = &filetypes[n];
				break;
			}
		}
		n++;
	}
	g_free(iname);
	return(filetype);
}

void
rs_load_gdk(RS_BLOB *rs, const gchar *filename)
{
	GdkPixbuf *pixbuf;
	guchar *pixels;
	gint rowstride;
	gint width, height;
	gint row,col,n,res, src, dest;
	gdouble nd;
	gushort gammatable[256];
	if ((pixbuf = gdk_pixbuf_new_from_file(filename, NULL)))
	{
		for(n=0;n<256;n++)
		{
			nd = ((gdouble) n) / 255.0;
			res = (gint) (pow(nd, 2.2) * 65535.0);
			_CLAMP65535(res);
			gammatable[n] = res;
		}
		if (rs->raw!=NULL) rs_free_raw(rs);
		rs_image16_free(rs->input); rs->input = NULL;
		rs_image16_free(rs->scaled); rs->scaled = NULL;
		rs_image16_free(rs->histogram_dataset); rs->histogram_dataset = NULL;
		rs_image8_free(rs->preview); rs->preview = NULL;
		rowstride = gdk_pixbuf_get_rowstride(pixbuf);
		pixels = gdk_pixbuf_get_pixels(pixbuf);
		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);
		rs->input = rs_image16_new(width, height, 3);
		for(row=0;row<rs->input->h;row++)
		{
			dest = row * rs->input->rowstride;
			src = row * rowstride;
			for(col=0;col<rs->input->w;col++)
			{
				rs->input->pixels[dest++] = gammatable[pixels[src++]];
				rs->input->pixels[dest++] = gammatable[pixels[src++]];
				rs->input->pixels[dest++] = gammatable[pixels[src++]];
			}
		}
		g_object_unref(pixbuf);
		rs_reset(rs);
		rs->histogram_dataset = rs_image16_scale(rs->input, NULL,
			rs->input->w/HISTOGRAM_DATASET_WIDTH);
		for(n=0;n<4;n++)
			rs->pre_mul[n] = 1.0;
		rs->in_use=TRUE;
		rs->filename = filename;
	}
	return;
}

gchar *
rs_dotdir_get(const gchar *filename)
{
	gchar *ret;
	gchar *directory;
	GString *dotdir;

	directory = g_path_get_dirname(filename);

	dotdir = g_string_new(directory);
	dotdir = g_string_append(dotdir, "/");
	dotdir = g_string_append(dotdir, DOTDIR);

	if (!g_file_test(dotdir->str, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
	{
		if (g_mkdir(dotdir->str, 0700) != 0)
			ret = NULL;
		else
			ret = dotdir->str;
	}
	else
		ret = dotdir->str;
	g_free(directory);
	g_string_free(dotdir, FALSE);
	return (ret);
}

gchar *
rs_thumb_get_name(const gchar *src)
{
	gchar *ret=NULL;
	gchar *dotdir, *filename;
	GString *out;
	dotdir = rs_dotdir_get(src);
	filename = g_path_get_basename(src);
	if (dotdir)
	{
		out = g_string_new(dotdir);
		out = g_string_append(out, "/");
		out = g_string_append(out, filename);
		out = g_string_append(out, ".thumb.png");
		ret = out->str;
		g_string_free(out, FALSE);
		g_free(dotdir);
	}
	g_free(filename);
	return(ret);
}

GdkPixbuf *
rs_thumb_grt(const gchar *src)
{
	GdkPixbuf *pixbuf=NULL;
	gchar *in, *argv[6];
	gchar *tmp=NULL;
	gint tmpfd;
	gchar *thumbname;
	static gboolean grt_warning_shown = FALSE;

	thumbname = rs_thumb_get_name(src);

	if (thumbname)
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
			return(pixbuf);
		}

	if (!thumbname)
	{
		tmpfd = g_file_open_tmp("XXXXXX", &tmp, NULL);
		thumbname = tmp;
		close(tmpfd);
	}

	if (g_file_test("/usr/bin/gnome-raw-thumbnailer", G_FILE_TEST_IS_EXECUTABLE))
	{
		in = g_filename_to_uri(src, NULL, NULL);

		if (in)
		{
			argv[0] = "/usr/bin/gnome-raw-thumbnailer";
			argv[1] = "-s";
			argv[2] = "128";
			argv[3] = in;
			argv[4] = thumbname;
			argv[5] = NULL;
			g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(in);
		}
	}
	else if (!grt_warning_shown)
	{
		gui_dialog_simple("Warning", "gnome-raw-thumbnailer needed for RAW thumbnails.");
		grt_warning_shown = TRUE;
	}

	if (tmp)
		g_unlink(tmp);
	g_free(thumbname);
	return(pixbuf);
}

GdkPixbuf *
rs_thumb_gdk(const gchar *src)
{
	GdkPixbuf *pixbuf=NULL;
	gchar *thumbname;

	thumbname = rs_thumb_get_name(src);

	if (thumbname)
	{
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
		}
		else
		{
			pixbuf = gdk_pixbuf_new_from_file_at_size(src, 128, 128, NULL);
			gdk_pixbuf_save(pixbuf, thumbname, "png", NULL, NULL);
			g_free(thumbname);
		}
	}
	else
		pixbuf = gdk_pixbuf_new_from_file_at_size(src, 128, 128, NULL);
	return(pixbuf);
}

void
rs_set_warmth_from_color(RS_BLOB *rs, gint x, gint y)
{
	gint offset, row, col;
	gdouble r=0.0, g=0.0, b=0.0;
	gdouble warmth, tint;

	for(row=0; row<3; row++)
	{
		for(col=0; col<3; col++)
		{
			offset = (y+row-1)*rs->scaled->rowstride
				+ (x+col-1)*rs->scaled->channels;
			r += ((gdouble) rs->scaled->pixels[offset+R])/65535.0;
			g += ((gdouble) rs->scaled->pixels[offset+G])/65535.0;
			b += ((gdouble) rs->scaled->pixels[offset+B])/65535.0;
			if (rs->scaled->channels==4)
				g += ((gdouble) rs->scaled->pixels[offset+G2])/65535.0;
				
		}
	}
	r /= 9;
	g /= 9;
	b /= 9;
	if (rs->scaled->channels==4)
		g /= 2;
	warmth = (b-r)/(r+b); /* r*(1+warmth) = b*(1-warmth) */
	tint = g/(r+r*warmth)-1.0; /* magic */
	SETVAL(rs->settings[rs->current_setting]->warmth, warmth);
	SETVAL(rs->settings[rs->current_setting]->tint, tint);
	return;
}

int
main(int argc, char **argv)
{
	guint a,b,c,d;
#ifdef __i386__
	asm(
		"pushfl\n\t"
		"popl %%eax\n\t"
		"movl %%eax, %%ebx\n\t"
		"xorl $0x00200000, %%eax\n\t"
		"pushl %%eax\n\t"
		"popfl\n\t"
		"pushfl\n\t"
		"popl %%eax\n\t"
		"cmpl %%eax, %%ebx\n\t"
		"je notfound\n\t"
		"movl $1, %0\n\t"
		"notfound:\n\t"
		: "=r" (a)
		:
		: "eax", "ebx"
		);
	if (a)
	{
		cpuid(0x1);
		if(d&0x00800000) cpuflags |= _MMX;
		if(d&0x2000000) cpuflags |= _SSE;
		if(d&0x8000) cpuflags |= _CMOV;
		cpuid(0x80000001);
		if(d&0x80000000) cpuflags |= _3DNOW;
	}
#endif
	gui_init(argc, argv);
	return(0);
}

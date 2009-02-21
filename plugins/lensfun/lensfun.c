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
#include <lensfun.h>

#define RS_TYPE_LENSFUN (rs_lensfun_type)
#define RS_LENSFUN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_LENSFUN, RSLensfun))
#define RS_LENSFUN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_LENSFUN, RSLensfunClass))
#define RS_IS_LENSFUN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_LENSFUN))

typedef struct _RSLensfun RSLensfun;
typedef struct _RSLensfunClass RSLensfunClass;

struct _RSLensfun {
	RSFilter parent;

	gchar *make;
	gchar *model;
	gchar *lens;
	gfloat focal;
	gfloat aperture;
};

struct _RSLensfunClass {
	RSFilterClass parent_class;
};

RS_DEFINE_FILTER(rs_lensfun, RSLensfun)

enum {
	PROP_0,
	PROP_MAKE,
	PROP_MODEL,
	PROP_LENS,
	PROP_FOCAL,
	PROP_APERTURE,
};

static void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static RS_IMAGE16 *get_image(RSFilter *filter);
static void inline rs_image16_nearest_full(RS_IMAGE16 *in, gushort *out, gfloat *pos);

static RSFilterClass *rs_lensfun_parent_class = NULL;

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_lensfun_get_type(G_TYPE_MODULE(plugin));
}

static void
rs_lensfun_class_init(RSLensfunClass *klass)
{
	RSFilterClass *filter_class = RS_FILTER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	rs_lensfun_parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	g_object_class_install_property(object_class,
		PROP_MAKE, g_param_spec_string(
			"make", "make", "The make of the camera (ie. \"Canon\")",
			NULL, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_MODEL, g_param_spec_string(
			"model", "model", "The model of the camera (ie. \"Canon EOS 20D\")",
			NULL, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_LENS, g_param_spec_string(
			"lens", "lens", "The lens of the camera (ie. \"Canon EF-S 18-55mm f/3.5-5.6\")",
			NULL, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_FOCAL, g_param_spec_float(
			"focal", "focal", "focal",
			0.0, G_MAXFLOAT, 50.0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_APERTURE, g_param_spec_float(
			"aperture", "aperture", "aperture",
			1.0, G_MAXFLOAT, 5.6, G_PARAM_READWRITE)
	);

	filter_class->name = "Lensfun filter";
	filter_class->get_image = get_image;
}

static void
rs_lensfun_init(RSLensfun *lensfun)
{
	lensfun->make = NULL;
	lensfun->model = NULL;
	lensfun->lens = NULL;
	lensfun->focal = 50.0; /* Well... */
	lensfun->aperture = 5.6;
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RSLensfun *lensfun = RS_LENSFUN(object);

	switch (property_id)
	{
		case PROP_MAKE:
			g_value_set_string(value, lensfun->make);
			break;
		case PROP_MODEL:
			g_value_set_string(value, lensfun->model);
			break;
		case PROP_LENS:
			g_value_set_string(value, lensfun->lens);
			break;
		case PROP_FOCAL:
			g_value_set_float(value, lensfun->focal);
			break;
		case PROP_APERTURE:
			g_value_set_float(value, lensfun->aperture);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RSLensfun *lensfun = RS_LENSFUN(object);

	switch (property_id)
	{
		case PROP_MAKE:
			g_free(lensfun->make);
			lensfun->make = g_value_dup_string(value);
			break;
		case PROP_MODEL:
			g_free(lensfun->model);
			lensfun->model = g_value_dup_string(value);
			break;
		case PROP_LENS:
			g_free(lensfun->lens);
			lensfun->lens = g_value_dup_string(value);
			break;
		case PROP_FOCAL:
			lensfun->focal = g_value_get_float(value);
			break;
		case PROP_APERTURE:
			lensfun->aperture = g_value_get_float(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static RS_IMAGE16 *
get_image(RSFilter *filter)
{
	RSLensfun *lensfun = RS_LENSFUN(filter);
	RS_IMAGE16 *input;
	RS_IMAGE16 *output = NULL;

	input = rs_filter_get_image(filter->previous);

	gint i, row, col;
	lfDatabase *ldb = lf_db_new ();

	if (!ldb)
	{
		g_warning ("Failed to create database");
		return input;
	}

	lf_db_load (ldb);

	const lfCamera **cameras = NULL;
	if (lensfun->make && lensfun->model)
		cameras = lf_db_find_cameras(ldb, lensfun->make, lensfun->model);

	if (!cameras)
	{
		g_warning("cameras not found...");
		return input;
	}

	for (i = 0; cameras [i]; i++)
	{
		g_print ("Camera: %s / %s %s%s%s\n",
		lf_mlstr_get (cameras [i]->Maker),
		lf_mlstr_get (cameras [i]->Model),
		cameras [i]->Variant ? "(" : "",
		cameras [i]->Variant ? lf_mlstr_get (cameras [i]->Variant) : "",
		cameras [i]->Variant ? ")" : "");
		g_print ("\tMount: %s\n", lf_db_mount_name (ldb, cameras [i]->Mount));
		g_print ("\tCrop factor: %g\n", cameras [i]->CropFactor);
	}

	const lfLens **lenses = NULL;
	if (lensfun->lens)
		lenses = lf_db_find_lenses_hd(ldb, cameras[0], lensfun->make, lensfun->lens, 0);

	if (!lenses)
	{
		g_warning("lenses not found...");
		return NULL;
	}

	const lfLens *lens = lenses [0];
	lf_free (lenses);

	/* Procedd if we got everything */
	if (lf_lens_check((lfLens *) lens))
	{
		gfloat *pos = g_new0(gfloat, input->w*10);

		output = rs_image16_new(input->w, input->h, input->channels, input->pixelsize);

		lfModifier *mod = lf_modifier_new (lens, cameras[0]->CropFactor, input->w, input->h);
		lf_modifier_initialize (mod, lens,
			LF_PF_U16, /* lfPixelFormat */
			lensfun->focal, /* focal */
			lensfun->aperture, /* aperture */
			0.0, /* distance */
			1.0, /* scale */
			LF_FISHEYE, /* lfLensType targeom, */ /* FIXME: ? */
			LF_MODIFY_DISTORTION, /* flags */ /* FIXME: ? */
			FALSE); /* reverse */
			for(row=0;row<input->h;row++)
			{
				gushort *target;
				lf_modifier_apply_subpixel_geometry_distortion(mod, 0.0, (gfloat) row, input->w, 1, pos);

				for(col=0;col<input->w;col++)
				{
					target = GET_PIXEL(output, col, row);
					rs_image16_nearest_full(input, target, &pos[col*6]);
					//				rs_image16_bilinear(input, target, pos[col*6], pos[col*6+1]);
				}
				//			printf("%.0f/%d\n", pos[(input->w-1)*2], input->w);
			}
	}
	else
		g_warning("lf_lens_check() failed");
//	lfModifier *mod = lfModifier::Create (lens, opts.Crop, img->width, img->height);

	g_object_unref(input);
	return output;
}

static void inline
rs_image16_nearest_full(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
	out[R] = GET_PIXEL(in, (gint)(pos[0]), (gint)(pos[1]))[R];
	out[G] = GET_PIXEL(in, (gint)(pos[2]), (gint)(pos[3]))[G];
	out[B] = GET_PIXEL(in, (gint)(pos[4]), (gint)(pos[5]))[B];
}

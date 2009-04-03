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
	RSLens *lens;
	gchar *lens_make;
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
	PROP_LENS_MAKE,
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
		PROP_LENS, g_param_spec_object(
			"lens", "lens", "A RSLens object describing the lens",
			RS_TYPE_LENS, G_PARAM_READWRITE)
	);
	g_object_class_install_property(object_class,
		PROP_LENS_MAKE, g_param_spec_string(
			"lens_make", "lens_make", "The make of the lens (ie. \"Canon\")",
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
	lensfun->lens_make = NULL;
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
			g_value_set_object(value, lensfun->lens);
			break;
		case PROP_LENS_MAKE:
			g_value_set_string(value, lensfun->lens_make);
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
			if (lensfun->lens)
				g_object_unref(lensfun->lens);
			lensfun->lens = g_value_dup_object(value);
			break;
		case PROP_LENS_MAKE:
			g_free(lensfun->lens_make);
			lensfun->lens_make = g_value_dup_string(value);
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
	const gchar *make = NULL;
	const gchar *model = NULL;

	input = rs_filter_get_image(filter->previous);

	gint i, j, row, col;
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
		g_warning("camera not found (make: \"%s\" model: \"%s\")", lensfun->make, lensfun->model);
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

	const lfLens **lenses;

	if (lensfun->lens)
	{
		model = rs_lens_get_lensfun_model(lensfun->lens);
		if (!model)
			model = rs_lens_get_description(lensfun->lens);
		make = rs_lens_get_lensfun_make(lensfun->lens);
	}

	lenses = lf_db_find_lenses_hd(ldb, cameras[0], make, model, 0);
	if (!lenses)
	{
		g_warning("lens not found (make: \"%s\" model: \"%s\")", lensfun->lens_make, model);
		return input;
	}

	for (i = 0; lenses [i]; i++)
	{
		g_print ("Lens: %s / %s\n",
			 lf_mlstr_get (lenses [i]->Maker),
			 lf_mlstr_get (lenses [i]->Model));
		g_print ("\tCrop factor: %g\n", lenses [i]->CropFactor);
		g_print ("\tFocal: %g-%g\n", lenses [i]->MinFocal, lenses [i]->MaxFocal);
		g_print ("\tAperture: %g-%g\n", lenses [i]->MinAperture, lenses [i]->MaxAperture);
		g_print ("\tCenter: %g,%g\n", lenses [i]->CenterX, lenses [i]->CenterY);
		g_print ("\tCCI: %g/%g/%g\n", lenses [i]->RedCCI, lenses [i]->GreenCCI, lenses [i]->BlueCCI);
		if (lenses [i]->Mounts)
			for (j = 0; lenses [i]->Mounts [j]; j++)
				g_print ("\tMount: %s\n", lf_db_mount_name (ldb, lenses [i]->Mounts [j]));
	}

	const lfLens *lens = lenses [0];
	lf_free (lenses);

	/* Procedd if we got everything */
	if (lf_lens_check((lfLens *) lens))
	{
		gint effective_flags;
		gfloat *pos = g_new0(gfloat, input->w*10);

		lfModifier *mod = lf_modifier_new (lens, cameras[0]->CropFactor, input->w, input->h);
		effective_flags = lf_modifier_initialize (mod, lens,
			LF_PF_U16, /* lfPixelFormat */
			lensfun->focal, /* focal */
			lensfun->aperture, /* aperture */
			0.0, /* distance */
			1.0, /* scale */
			LF_UNKNOWN, /* lfLensType targeom, */ /* FIXME: ? */
			LF_MODIFY_ALL, /* flags */ /* FIXME: ? */
			FALSE); /* reverse */

		/* Print flags used */
		GString *flags = g_string_new("");
		if (effective_flags & LF_MODIFY_TCA)
			g_string_append(flags, " LF_MODIFY_TCA");
		if (effective_flags & LF_MODIFY_VIGNETTING)
			g_string_append(flags, " LF_MODIFY_VIGNETTING");
		if (effective_flags & LF_MODIFY_CCI)
			g_string_append(flags, " LF_MODIFY_CCI");
		if (effective_flags & LF_MODIFY_DISTORTION)
			g_string_append(flags, " LF_MODIFY_DISTORTION");
		if (effective_flags & LF_MODIFY_GEOMETRY)
			g_string_append(flags, " LF_MODIFY_GEOMETRY");
		if (effective_flags & LF_MODIFY_SCALE)
			g_string_append(flags, " LF_MODIFY_SCALE");
		g_debug("Effective flags:%s", flags->str);
		g_string_free(flags, TRUE);

		if (effective_flags > 0)
		{
			output = rs_image16_copy(input, FALSE);
			for(row=0;row<input->h;row++)
			{
				gushort *target;
				lf_modifier_apply_subpixel_geometry_distortion(mod, 0.0, (gfloat) row, input->w, 1, pos);

				for(col=0;col<input->w;col++)
				{
					target = GET_PIXEL(output, col, row);
					rs_image16_nearest_full(input, target, &pos[col*6]);
				}
			}
		}
		else
			output = g_object_ref(input);
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

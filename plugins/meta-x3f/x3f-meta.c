/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
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

#include <rawstudio.h>
#include <glib.h>
#include <stdlib.h> /* atoi() */

/* http://www.x3f.info/technotes/FileDocs/X3F_Format.pdf */

typedef enum x3f_extended_data_types {
	X3F_EXTENDED_DATA_UNUSED = 0,
	X3F_EXTENDED_DATA_EXPOSURE_ADJUST = 1,
	X3F_EXTENDED_DATA_CONTRAST_ADJUST = 2,
	X3F_EXTENDED_DATA_SHADOW_ADJUST = 3,
	X3F_EXTENDED_DATA_HIGHLIGHT_ADJUST = 4,
	X3F_EXTENDED_DATA_SATURATION_ADJUST = 5,
	X3F_EXTENDED_DATA_SHARPNESS_ADJUST = 6,
	X3F_EXTENDED_DATA_COLOR_ADJUST_RED = 7,
	X3F_EXTENDED_DATA_COLOR_ADJUST_GREEN = 8,
	X3F_EXTENDED_DATA_COLOR_ADJUST_BLUE = 9,
	X3F_EXTENDED_DATA_FILL_LIGHT_ADJUST = 10,
} X3F_EXTENDED_DATA_TYPES;

typedef enum x3f_data_format {
	X3F_DATA_FORMAT_UNCOMPRESSED = 3,
	X3F_DATA_FORMAT_HUFFMAN = 11,
	X3F_DATA_FORMAT_JPEG = 18,
} X3F_DATA_FORMAT;

/*
 * These structs is mostly used to define the file format - they can not
 * be directly mapped to file because of endian differences on some platforms
 */

typedef struct x3f_file {
	gchar identifier[4]; /* Contains FOVb */
	gushort version_major;
	gushort version_minor;
	guchar unique_identifier[16];
	guint mark_bits;
	guint image_columns;
	guint image_rows;
	guint rotation;

	/* The following was added in version 2.1 */
	gchar wb_label[32];
	X3F_EXTENDED_DATA_TYPES extended_data_types[32];
	gfloat extended_data[32];

	guint directory_start; /* Last 4 bytes of file */
} __attribute__ ((packed)) X3F_FILE;

typedef struct x3f_directory_section {
	gchar section_identifier[4]; /* Contains SECd */
	gushort section_version_major;
	gushort section_version_minor;
	guint number_of_entries;
} __attribute__ ((packed)) X3F_DIRECTORY_SECTION;

typedef struct x3f_directory_entry {
	guint offset;
	guint length;
	guchar type[4];
} __attribute__ ((packed)) X3F_DIRECTORY_ENTRY;

typedef struct x3f_image_data {
	gchar section_identifier[4]; /* Contains SECi */
	gushort version_major;
	gushort version_minor;
	guint type_of_image_data;
	X3F_DATA_FORMAT data_format;
	guint columns;
	guint rows;
	guint rowstride; /* row size in bytes, a value of zero means variable */
	void *image_data;
	/* Followed by image-data */
} __attribute__ ((packed)) X3F_IMAGE_DATA;

typedef struct x3f_property_list {
	gchar section_identifier[4]; /* Contains SECp */
	gushort version_major;
	gushort version_minor;
	guint number_of_entries;
	guint character_format; /* 0 = unicode16 */
	guint __RESERVED1;
	guint total_length;
	/* followed by number_of_entries * X3F_PROPERTY_ENTRY */
} __attribute__ ((packed)) X3F_PROPERTY_LIST;

typedef struct x3f_property {
	guint name_offset; /* offset from start of CHARACTER data */
	guint value_offset; /* offset from start of CHARACTER data */
} __attribute__ ((packed)) X3F_PROPERTY_ENTRY;

static void
x3f_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gint i;
	X3F_FILE file;
	X3F_DIRECTORY_SECTION directory;
	X3F_DIRECTORY_ENTRY directory_entry;
	X3F_IMAGE_DATA image_data;
	guint start=0, width=0, height=0, rowstride=0;
	GdkPixbuf *pixbuf = NULL, *pixbuf2 = NULL;
	gdouble ratio=1.0;

	/* Check if this is infact a Sigma-file */
	if (!raw_strcmp(rawfile, G_STRUCT_OFFSET(X3F_FILE, identifier), "FOVb", 4))
	{
		raw_close_file(rawfile);
		return;
	}

	raw_set_byteorder(rawfile, 0x4949); /* x3f is always little endian */

	/* Fill X3F_FILE with needed values */
	raw_get_ushort(rawfile, G_STRUCT_OFFSET(X3F_FILE, version_major), &file.version_major);
	raw_get_ushort(rawfile, G_STRUCT_OFFSET(X3F_FILE, version_minor), &file.version_minor);
	raw_get_uint(rawfile, G_STRUCT_OFFSET(X3F_FILE, rotation), &file.rotation);
	raw_get_uint(rawfile, raw_get_filesize(rawfile)-4, &file.directory_start);

	meta->orientation=file.rotation;

	if ((file.version_major == 2) && (file.version_minor == 2))
	{
		/* Copy all data types in one go */
		raw_strcpy(rawfile, G_STRUCT_OFFSET(X3F_FILE, extended_data_types), file.extended_data_types, 32);
		for(i=0;i<32;i++)
		{
			/* This could have endianness problems! */
			raw_get_float(rawfile, G_STRUCT_OFFSET(X3F_FILE, extended_data)+i*4, &file.extended_data[i]);

			switch (file.extended_data_types[i])
			{
				case X3F_EXTENDED_DATA_COLOR_ADJUST_RED:
					meta->cam_mul[0] = file.extended_data[i];
					break;
				case X3F_EXTENDED_DATA_COLOR_ADJUST_GREEN:
					meta->cam_mul[1] = file.extended_data[i];
					meta->cam_mul[3] = file.extended_data[i];
					break;
				case X3F_EXTENDED_DATA_COLOR_ADJUST_BLUE:
					meta->cam_mul[2] = file.extended_data[i];
					break;
				default:
					break;
			}
		}
	}
	
	if (file.directory_start < raw_get_filesize(rawfile))
	{
		if (raw_strcmp(rawfile, file.directory_start, "SECd", 4))
		{
			/* Fill X3F_DIRECTORY_SECTION with needed values */
			raw_get_ushort(rawfile,
				file.directory_start+G_STRUCT_OFFSET(X3F_DIRECTORY_SECTION, section_version_major),
				&directory.section_version_major);
			raw_get_ushort(rawfile,
				file.directory_start+G_STRUCT_OFFSET(X3F_DIRECTORY_SECTION, section_version_minor),
				&directory.section_version_minor);
			raw_get_uint(rawfile,
				file.directory_start+G_STRUCT_OFFSET(X3F_DIRECTORY_SECTION, number_of_entries),
				&directory.number_of_entries);

			for(i=0;i<directory.number_of_entries;i++)
			{
				gint offset = file.directory_start + sizeof(X3F_DIRECTORY_SECTION) + i * sizeof(X3F_DIRECTORY_ENTRY);
				raw_get_uint(rawfile, offset+G_STRUCT_OFFSET(X3F_DIRECTORY_ENTRY, offset), &directory_entry.offset);
				raw_get_uint(rawfile, offset+G_STRUCT_OFFSET(X3F_DIRECTORY_ENTRY, length), &directory_entry.length);

				if (raw_strcmp(rawfile, offset+G_STRUCT_OFFSET(X3F_DIRECTORY_ENTRY, type), "IMA", 3))
				{
					/* Image Data */
					raw_get_uint(rawfile, directory_entry.offset+G_STRUCT_OFFSET(X3F_IMAGE_DATA, data_format), &image_data.data_format);
					if (image_data.data_format == X3F_DATA_FORMAT_UNCOMPRESSED)
					{
						start = directory_entry.offset+G_STRUCT_OFFSET(X3F_IMAGE_DATA, image_data);
						raw_get_uint(rawfile, directory_entry.offset+G_STRUCT_OFFSET(X3F_IMAGE_DATA, columns), &width);
						raw_get_uint(rawfile, directory_entry.offset+G_STRUCT_OFFSET(X3F_IMAGE_DATA, rows), &height);
						raw_get_uint(rawfile, directory_entry.offset+G_STRUCT_OFFSET(X3F_IMAGE_DATA, rowstride), &rowstride);
					}
				}
				else if (raw_strcmp(rawfile, offset+G_STRUCT_OFFSET(X3F_DIRECTORY_ENTRY, type), "PROP", 4))
				{
					/* Property List */
					/* Properties is stored as 16 bit unicode (sigh) */
					gint current_entry;
					guint number_of_entries = 0;
					X3F_PROPERTY_ENTRY entry;
					raw_get_uint(rawfile, directory_entry.offset+G_STRUCT_OFFSET(X3F_PROPERTY_LIST, number_of_entries), &number_of_entries);

					offset = directory_entry.offset + sizeof(X3F_PROPERTY_LIST) + number_of_entries * sizeof(X3F_PROPERTY_ENTRY);

					for(current_entry = 0; current_entry < number_of_entries; current_entry++)
					{
						gchar *name;
						gchar *value;

						/* Get location of name */
						raw_get_uint(rawfile, directory_entry.offset
							+ sizeof(X3F_PROPERTY_LIST)
							+ current_entry * sizeof(X3F_PROPERTY_ENTRY)
							+ G_STRUCT_OFFSET(X3F_PROPERTY_ENTRY, name_offset),
							&entry.name_offset);
						entry.name_offset = offset + 2 * entry.name_offset; /* 2 for the 16 bit unicode */

						/* Get location of value */
						raw_get_uint(rawfile, directory_entry.offset
							+ sizeof(X3F_PROPERTY_LIST)
							+ current_entry * sizeof(X3F_PROPERTY_ENTRY)
							+ G_STRUCT_OFFSET(X3F_PROPERTY_ENTRY, value_offset),
							&entry.value_offset);
						entry.value_offset = offset + 2 * entry.value_offset;

						/* Try to convert name and value to UTF8 */
						name = g_utf16_to_utf8(raw_get_map(rawfile)+entry.name_offset, -1, NULL, NULL, NULL);
						value = g_utf16_to_utf8(raw_get_map(rawfile)+entry.value_offset, -1, NULL, NULL, NULL);

						if (g_str_equal(name, "ISO"))
							meta->iso = atoi(value);
						else if (g_str_equal(name, "CAMMANUF"))
						{
							meta->make_ascii = g_strdup(value);
							if (g_str_equal(meta->make_ascii, "SIGMA"))
								meta->make = MAKE_SIGMA;
							else if (g_str_equal(meta->make_ascii, "Polaroid"))
								meta->make = MAKE_POLAROID;
						}
						else if (g_str_equal(name, "CAMMODEL"))
							meta->model_ascii = g_strdup(value);
						else if (g_str_equal(name, "APERTURE")) /* Example: 8.000 */
							meta->aperture = rs_atof(value);
						else if (g_str_equal(name, "SH_DESC")) /* Example: 1/60 */
						{
							gchar *ptr = value;
							while (*ptr++ != '/');
							meta->shutterspeed = atoi(ptr);
						}
						else if (g_str_equal(name, "FLENGTH"))
							meta->focallength = rs_atof(value);
						else if (g_str_equal(name, "TIME"))
						{
							meta->timestamp = (GTime) atoi(value);
							meta->time_ascii = rs_unixtime_to_exiftime(meta->timestamp);
						}

						if (name)
							g_free(name);
						if (value)
							g_free(value);
					}
				}
			}
		}
	}

	if (width > 0)
		pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+start, GDK_COLORSPACE_RGB, FALSE, 8,
			width, height, rowstride, NULL, NULL);

	if (pixbuf)
	{
		if (file.rotation > 0)
		{
			pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf,360-file.rotation);
			g_object_unref(pixbuf);
			pixbuf = pixbuf2;
		}
		ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
		if (ratio>1.0)
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
		else
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		meta->thumbnail = pixbuf2;
	}
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_meta_loader(".x3f", "Sigma", x3f_load_meta, 10);
}

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

#include <rawstudio.h>
#include <gtk/gtk.h>
#include <math.h>
#ifdef WIN32
#include <Winsock2.h> /* ntohl() */
#else
#include <arpa/inet.h> /* sony_decrypt(): htonl() */
#endif
#include <string.h> /* memcpy() */
#include <stdlib.h>
#include "rs-utils.h"

/* It is required having some arbitrary maximum exposure time to prevent borked
 * shutter speed values being interpreted from the tiff.
 * 8h seems to be reasonable, even for astronomists with extra battery packs */
#define EXPO_TIME_MAXVAL (8*60.0*60.0)

typedef struct {
	RSMetadata meta;
	gint sony_offset;
	gint sony_length;
	gint sony_key;
	guint pad[128];
	guint p;
} SonyMeta;

struct IFD {
	gushort tag;
	gushort type;
	guint count;
	guint value_offset;
	guchar value_uchar;
	gushort value_ushort;
	guint value_uint;
	gdouble value_rational;
	guint offset;
	gdouble value;
};

static gfloat get_rational(RAWFILE *rawfile, guint offset);
inline static void read_ifd(RAWFILE *rawfile, guint offset, struct IFD *ifd);
static gboolean makernote_canon(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean makernote_leica(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean makernote_minolta(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean makernote_nikon(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean makernote_olympus(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta);
static gboolean makernote_olympus_camerasettings(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta);
static gboolean makernote_olympus_imageprocessing(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta);
static gboolean makernote_olympus_equipment(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta);
static gboolean ifd_panasonic(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean makernote_pentax(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean makernote_sony(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static void sony_decrypt(SonyMeta *sony, guint *data, gint len);
static gboolean private_sony(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean exif_reader(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean ifd_reader(RAWFILE *rawfile, guint offset, RSMetadata *meta);
static gboolean thumbnail_reader(const gchar *service, RAWFILE *rawfile, guint offset, guint length, RSMetadata *meta);
static gboolean thumbnail_store(GdkPixbuf *pixbuf, RSMetadata *meta);
static GdkPixbuf* raw_thumbnail_reader(const gchar *service, RSMetadata *meta);

typedef enum tiff_field_type
{
	TIFF_FIELD_TYPE_UNDEF__ = 0,
	TIFF_FIELD_TYPE_BYTE = 1,
	TIFF_FIELD_TYPE_ASCII = 2,
	TIFF_FIELD_TYPE_SHORT = 3,
	TIFF_FIELD_TYPE_LONG = 4,
	TIFF_FIELD_TYPE_RATIONAL = 5,

	/* Added in TIFF 6.0 */
	TIFF_FIELD_TYPE_SBYTE = 6,
	TIFF_FIELD_TYPE_UNDEFINED = 7,
	TIFF_FIELD_TYPE_SSHORT = 8,
	TIFF_FIELD_TYPE_SLONG = 9,
	TIFF_FIELD_TYPE_SRATIONAL = 10,
	TIFF_FIELD_TYPE_FLOAT = 11,
	TIFF_FIELD_TYPE_DOUBLE = 12,

	/* Just for convenience */
	TIFF_FIELD_TYPE_MAX = 12,
} TIFF_FIELD_TYPE;

guint tiff_field_size[TIFF_FIELD_TYPE_MAX+1] = {1, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};

/**
 * Get a TIFF_FIELD_TYPE_RATIONAL value from a TIFF file
 */
static gfloat
get_rational(RAWFILE *rawfile, guint offset)
{
	guint uint1=0, uint2=1;
	if (!raw_get_uint(rawfile, offset, &uint1))
		return 0;
	if (!raw_get_uint(rawfile, offset+4, &uint2))
		return 0;

	if (uint2 == 0)
		return 0;
	return ((gdouble) uint1) / ((gdouble) uint2);
}

inline static void
read_ifd(RAWFILE *rawfile, guint offset, struct IFD *ifd)
{
/*	guint size = 0; */

	raw_get_ushort(rawfile, offset, &ifd->tag);
	raw_get_ushort(rawfile, offset+2, &ifd->type);
	raw_get_uint(rawfile, offset+4, &ifd->count);
	raw_get_uint(rawfile, offset+8, &ifd->value_offset);

	ifd->value = 0;

	if (ifd->type > 0 && ifd->type <= TIFF_FIELD_TYPE_MAX)
	{
		if ((ifd->count * tiff_field_size[ifd->type]) < 5)
			ifd->offset = offset+8;
		else
			ifd->offset = ifd->value_offset;
	}

	if (ifd->count == 1)
		switch (ifd->type)
		{
			case TIFF_FIELD_TYPE_BYTE:
				raw_get_uchar(rawfile, offset+8, &ifd->value_uchar);
				ifd->value = ifd->value_uchar;
				break;
			case TIFF_FIELD_TYPE_SHORT:
				raw_get_ushort(rawfile, offset+8, &ifd->value_ushort);
				ifd->value = ifd->value_ushort;
				break;
			case TIFF_FIELD_TYPE_LONG:
				raw_get_uint(rawfile, offset+8, &ifd->value_uint);
				ifd->value = ifd->value_uint;
				break;
			case TIFF_FIELD_TYPE_RATIONAL:
				ifd->value_rational = get_rational(rawfile,  ifd->value_offset);
				ifd->value = ifd->value_rational;
				break;
			default:
				/* FIXME: Implement types from TIFF 6.0 */
				break;
		}
}

#if 0
static void
print_ifd(RAWFILE *rawfile, struct IFD *ifd)
{
	gchar *tmp;
	printf("tag: %04x ", ifd->tag);
	printf("%8u ", ifd->type);
	printf("%8u * ", ifd->count);
	switch (ifd->type)
	{
		case TIFF_FIELD_TYPE_ASCII:
			tmp = raw_strdup (rawfile, ifd->value_offset, ifd->count);
			printf("[%-30s] ", tmp);
			g_free(tmp);
			break;
		case TIFF_FIELD_TYPE_SHORT:
			printf("[%8u] ", ifd->value_ushort);
			break;
		case TIFF_FIELD_TYPE_LONG:
			printf("[%8u] ", ifd->value_offset);
			break;
		case TIFF_FIELD_TYPE_RATIONAL:
			printf("[%.03f] ", ifd->value_rational);
			break;
		default:
			printf("[0x%08x] ", ifd->value_offset);
			break;
	}
	printf("@ %d ", ifd->offset);
	printf("{ %.05f }", ifd->value);
	printf("\n");
}
#endif

static gboolean
makernote_canon(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries = 0;
	gushort ushort_temp1;
	guint uint_temp1;
	gushort wb_index = 0;
	gchar* lens_name;

	struct IFD ifd;

	/* get number of entries */
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch (ifd.tag)
		{

		case 0x0001: /* CanonCameraSettings */
			if (meta->make == MAKE_CANON)
			{
				gshort temp, focalunits;

				/* Lens ID */
				raw_get_short(rawfile, ifd.value_offset+44, &temp);
				meta->lens_id = temp;

				/* Focalunits */
				raw_get_short(rawfile, ifd.value_offset+50, &focalunits);

				/* Max Focal */
				raw_get_short(rawfile, ifd.value_offset+46, &temp);
				meta->lens_max_focal = (gfloat) temp * (gfloat) focalunits;

				/* Min Focal */
				raw_get_short(rawfile, ifd.value_offset+48, &temp);
				meta->lens_min_focal = (gfloat) temp * (gfloat) focalunits;

				/* Max Aperture */
				raw_get_short(rawfile, ifd.value_offset+52, &temp);
				meta->lens_max_aperture = (gfloat) exp(CanonEv(temp)*log(2)/2);

				/* Min Aperture */
				raw_get_short(rawfile, ifd.value_offset+54, &temp);
				meta->lens_min_aperture = (gfloat) exp(CanonEv(temp)*log(2)/2);
			}
			break;
		case 0x0004: /* CanonShotInfo */
			raw_get_ushort(rawfile, ifd.value_offset+14, &wb_index);
			break;
		case 0x0029: /* White Balance for G9 */
			if (g_str_equal(meta->model_ascii, "Canon PowerShot G9"))
			{
				gint wb_offset = (wb_index < 18) ? "012347800000005896"[wb_index]-'0' : 0;
				wb_offset = ifd.value_offset + wb_offset*32 + 8;

				raw_get_uint(rawfile, wb_offset, &uint_temp1);
				meta->cam_mul[1] = (gdouble) uint_temp1;
				raw_get_uint(rawfile, wb_offset+4, &uint_temp1);
				meta->cam_mul[0] = (gdouble) uint_temp1;
				raw_get_uint(rawfile, wb_offset+8, &uint_temp1);
				meta->cam_mul[2] = (gdouble) uint_temp1;
				raw_get_uint(rawfile, wb_offset+12, &uint_temp1);
				meta->cam_mul[3] = (gdouble) uint_temp1;
				rs_metadata_normalize_wb(meta);
			}
			break;
		case 0x0095: /* Lens Name */
			 lens_name = raw_strdup(rawfile, ifd.value_offset, ifd.count);
			/* We only add Canon lenses, since others are simply registered as "30mm", etc. */
			if (lens_name[0] == 'E' && lens_name[1] == 'F')
				meta->fixed_lens_identifier = g_strconcat("Canon ", lens_name, NULL);
			g_free(lens_name);
			break;
		case 0x00a4: /* WhiteBalanceTable */
			raw_get_ushort(rawfile, ifd.value_offset+wb_index*48+0, &ushort_temp1);
			meta->cam_mul[0] = (gdouble) ushort_temp1;
			raw_get_ushort(rawfile, ifd.value_offset+wb_index*48+2, &ushort_temp1);
			meta->cam_mul[1] = (gdouble) ushort_temp1;
			raw_get_ushort(rawfile, ifd.value_offset+wb_index*48+4, &ushort_temp1);
			meta->cam_mul[2] = (gdouble) ushort_temp1;
			meta->cam_mul[3] = meta->cam_mul[1];
			rs_metadata_normalize_wb(meta);
			break;
		case 0x4001: /* white balance for mulpiple Canon cameras */
			switch (ifd.count)
			{
				case 582: /* Canon 20D, 350D */
					ifd.value_offset += 50;
					break;
				case 653: /* Canon EOS 1D Mk II, Canon 1Ds Mk2 */
					ifd.value_offset += 68;
					break;
				case 674: /* Canon EOS 1D Mk III */
				case 692: /* Canon EOS 40D */
				case 702: /* Canon EOS 1Ds Mk III */
				case 796: /* Canon EOS 5D, Canon EOS 30D, Canon EOS 400D */
				case 1227: /* Canon EOS 450D */
				case 1250: /* Canon EOS 5D Mk II */
				case 1337: /* Canon 1D Mk IV */
				case 1251: /* Canon EOS 500D - is this really correct? */
				case 1338: /* Canon EOS 550D */
				case 1346: /* Canon EOS 60D */
					ifd.value_offset += 126;
					break;
				case 5120: /* Canon PowerShot G10 */
					ifd.value_offset += 142;
					break;
			}
			/* RGGB-format! */
			raw_get_ushort(rawfile, ifd.value_offset, &ushort_temp1);
			meta->cam_mul[0] = (gdouble) ushort_temp1;
			raw_get_ushort(rawfile, ifd.value_offset+2, &ushort_temp1);
			meta->cam_mul[1] = (gdouble) ushort_temp1;
			raw_get_ushort(rawfile, ifd.value_offset+4, &ushort_temp1);
			meta->cam_mul[3] = (gdouble) ushort_temp1;
			raw_get_ushort(rawfile, ifd.value_offset+6, &ushort_temp1);
			meta->cam_mul[2] = (gdouble) ushort_temp1;
			rs_metadata_normalize_wb(meta);
			break;
		}
	}

	return TRUE;
}

static gboolean
makernote_leica(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gboolean ret = FALSE;
	gushort number_of_entries = 0;
	guint version = 0;

	struct IFD ifd;

	/* get number of entries */
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch (version)
		{
			case 100:
				switch(ifd.tag)
				{
					case 0x0011: /* Red balance */
						meta->cam_mul[1] = 1.0;
						meta->cam_mul[3] = 1.0;
						meta->cam_mul[0] = ifd.value / 256.0;
						break;
					case 0x0012: /* Blue balance */
						meta->cam_mul[2] = ifd.value / 256.0;
						break;
					case 0x8769: /* ExifIFDPointer */
						exif_reader(rawfile, ifd.value_offset, meta);
						break;
				}
				break;
			case 200:
				switch(ifd.tag)
				{
					case 0x0024: /* WB Red Level */
						meta->cam_mul[0] = ifd.value;
						break;
					case 0x0025: /* WB Green Level */
						meta->cam_mul[1] = ifd.value;
						meta->cam_mul[3] = ifd.value;
						break;
					case 0x0026: /* WB Blue Level */
						meta->cam_mul[2] = ifd.value;
						break;
					case 0x8769: /* ExifIFDPointer */
						exif_reader(rawfile, ifd.value_offset, meta);
						break;
				}
				break;
			default:
				switch(ifd.tag)
				{
					case 0x0001: /* Raw version */
						switch (ifd.value_offset)
						{
							case 0x30303130: /* Leica */
								version = 100;
								ret = TRUE;
								break;
							case 0x30303230: /* Leica / Panasonic */
								version = 200;
								ret = FALSE;
								break;
							case 0x30313230: /* Panasonic */
								version = 210;
								ret = FALSE;
								break;
							case 0x31303230: /* Panasonic */
								version = 201;
								ret = FALSE;
								break;
							case 0x32303230: /* Panasonic */
								version = 202;
								ret = FALSE;
								break;
							default:
								ret = FALSE;
								break;
						}
						break;
				}
				break;
		}
	}
	rs_metadata_normalize_wb(meta);

	return ret;
}

static gboolean
makernote_minolta(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries = 0;

	struct IFD ifd;

	/* get number of entries */
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch (ifd.tag)
		{
			case 0x0088: /* Minolta */
				meta->preview_start = ifd.value_offset + raw_get_base(rawfile);
				break;
			case 0x0081: /* Minolta DiMAGE 5 */
				meta->thumbnail_start = ifd.value_offset + raw_get_base(rawfile);
				meta->thumbnail_length = ifd.count;
				break;
			case 0x0089: /* Minolta */
				meta->preview_length = ifd.value_offset;
				break;
		}
	}

	return TRUE;
}

static gboolean
makernote_nikon(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	static const guchar xlat[2][256] = {
	{ 0xc1,0xbf,0x6d,0x0d,0x59,0xc5,0x13,0x9d,0x83,0x61,0x6b,0x4f,0xc7,0x7f,0x3d,0x3d,
	0x53,0x59,0xe3,0xc7,0xe9,0x2f,0x95,0xa7,0x95,0x1f,0xdf,0x7f,0x2b,0x29,0xc7,0x0d,
	0xdf,0x07,0xef,0x71,0x89,0x3d,0x13,0x3d,0x3b,0x13,0xfb,0x0d,0x89,0xc1,0x65,0x1f,
	0xb3,0x0d,0x6b,0x29,0xe3,0xfb,0xef,0xa3,0x6b,0x47,0x7f,0x95,0x35,0xa7,0x47,0x4f,
	0xc7,0xf1,0x59,0x95,0x35,0x11,0x29,0x61,0xf1,0x3d,0xb3,0x2b,0x0d,0x43,0x89,0xc1,
	0x9d,0x9d,0x89,0x65,0xf1,0xe9,0xdf,0xbf,0x3d,0x7f,0x53,0x97,0xe5,0xe9,0x95,0x17,
	0x1d,0x3d,0x8b,0xfb,0xc7,0xe3,0x67,0xa7,0x07,0xf1,0x71,0xa7,0x53,0xb5,0x29,0x89,
	0xe5,0x2b,0xa7,0x17,0x29,0xe9,0x4f,0xc5,0x65,0x6d,0x6b,0xef,0x0d,0x89,0x49,0x2f,
	0xb3,0x43,0x53,0x65,0x1d,0x49,0xa3,0x13,0x89,0x59,0xef,0x6b,0xef,0x65,0x1d,0x0b,
	0x59,0x13,0xe3,0x4f,0x9d,0xb3,0x29,0x43,0x2b,0x07,0x1d,0x95,0x59,0x59,0x47,0xfb,
	0xe5,0xe9,0x61,0x47,0x2f,0x35,0x7f,0x17,0x7f,0xef,0x7f,0x95,0x95,0x71,0xd3,0xa3,
	0x0b,0x71,0xa3,0xad,0x0b,0x3b,0xb5,0xfb,0xa3,0xbf,0x4f,0x83,0x1d,0xad,0xe9,0x2f,
	0x71,0x65,0xa3,0xe5,0x07,0x35,0x3d,0x0d,0xb5,0xe9,0xe5,0x47,0x3b,0x9d,0xef,0x35,
	0xa3,0xbf,0xb3,0xdf,0x53,0xd3,0x97,0x53,0x49,0x71,0x07,0x35,0x61,0x71,0x2f,0x43,
	0x2f,0x11,0xdf,0x17,0x97,0xfb,0x95,0x3b,0x7f,0x6b,0xd3,0x25,0xbf,0xad,0xc7,0xc5,
	0xc5,0xb5,0x8b,0xef,0x2f,0xd3,0x07,0x6b,0x25,0x49,0x95,0x25,0x49,0x6d,0x71,0xc7 },
	{ 0xa7,0xbc,0xc9,0xad,0x91,0xdf,0x85,0xe5,0xd4,0x78,0xd5,0x17,0x46,0x7c,0x29,0x4c,
	0x4d,0x03,0xe9,0x25,0x68,0x11,0x86,0xb3,0xbd,0xf7,0x6f,0x61,0x22,0xa2,0x26,0x34,
	0x2a,0xbe,0x1e,0x46,0x14,0x68,0x9d,0x44,0x18,0xc2,0x40,0xf4,0x7e,0x5f,0x1b,0xad,
	0x0b,0x94,0xb6,0x67,0xb4,0x0b,0xe1,0xea,0x95,0x9c,0x66,0xdc,0xe7,0x5d,0x6c,0x05,
	0xda,0xd5,0xdf,0x7a,0xef,0xf6,0xdb,0x1f,0x82,0x4c,0xc0,0x68,0x47,0xa1,0xbd,0xee,
	0x39,0x50,0x56,0x4a,0xdd,0xdf,0xa5,0xf8,0xc6,0xda,0xca,0x90,0xca,0x01,0x42,0x9d,
	0x8b,0x0c,0x73,0x43,0x75,0x05,0x94,0xde,0x24,0xb3,0x80,0x34,0xe5,0x2c,0xdc,0x9b,
	0x3f,0xca,0x33,0x45,0xd0,0xdb,0x5f,0xf5,0x52,0xc3,0x21,0xda,0xe2,0x22,0x72,0x6b,
	0x3e,0xd0,0x5b,0xa8,0x87,0x8c,0x06,0x5d,0x0f,0xdd,0x09,0x19,0x93,0xd0,0xb9,0xfc,
	0x8b,0x0f,0x84,0x60,0x33,0x1c,0x9b,0x45,0xf1,0xf0,0xa3,0x94,0x3a,0x12,0x77,0x33,
	0x4d,0x44,0x78,0x28,0x3c,0x9e,0xfd,0x65,0x57,0x16,0x94,0x6b,0xfb,0x59,0xd0,0xc8,
	0x22,0x36,0xdb,0xd2,0x63,0x98,0x43,0xa1,0x04,0x87,0x86,0xf7,0xa6,0x26,0xbb,0xd6,
	0x59,0x4d,0xbf,0x6a,0x2e,0xaa,0x2b,0xef,0xe6,0x78,0xb6,0x4e,0xe0,0x2f,0xdc,0x7c,
	0xbe,0x57,0x19,0x32,0x7e,0x2a,0xd0,0xb8,0xba,0x29,0x00,0x3c,0x52,0x7d,0xa8,0x49,
	0x3b,0x2d,0xeb,0x25,0x49,0xfa,0xa3,0xaa,0x39,0xa7,0xc5,0xa7,0x50,0x11,0x36,0xfb,
	0xc6,0x67,0x4a,0xf5,0xa5,0x12,0x65,0x7e,0xb0,0xdf,0xaf,0x4e,0xb3,0x61,0x7f,0x2f } };
	gint i;
	guint tmp;
	gushort number_of_entries;
	gushort fieldtag=0;
	gushort fieldtype;
	gushort ushort_temp1=0;
	gfloat float_temp1=0.0, float_temp2=0.0;
	guint valuecount;
	guint uint_temp1=0;
	guchar char_tmp='\0';
	guint base;
	guint save;
	gint serial = 0;
	gint key = 0;
	guint ver97 = 0;
	guchar buf97[324], ci, cj, ck;
	guchar buf98[33] = "";
	gushort lensdata = 0;
	gboolean magic; /* Nikon's makernote type */
	gboolean got_wb = FALSE;

	if (raw_strcmp(rawfile, offset, "Nikon", 5))
	{
		base = offset +=10;
		raw_get_uint(rawfile, offset+4, &tmp);
		offset += tmp;
		magic = TRUE;
	}
	else
	{
		magic = FALSE;
		base = offset;
	}

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	if (number_of_entries>5000)
		return FALSE;
	offset += 2;

	save = offset;
	while(number_of_entries--)
	{
		/* FIXME: Port to read_ifd() */
		offset = save;
		raw_get_ushort(rawfile, offset, &fieldtag);
		raw_get_ushort(rawfile, offset+2, &fieldtype);
		raw_get_uint(rawfile, offset+4, &valuecount);
		offset += 8;

		save = offset + 4;
		if ((valuecount * ("1112481124848"[fieldtype < 13 ? fieldtype:0]-'0') > 4) && magic)
		{
			raw_get_uint(rawfile, offset, &uint_temp1);
			offset = base + uint_temp1;
		}
		switch(fieldtag)
		{
			case 0x0002: /* ISO */
				raw_get_ushort(rawfile, offset+2, &meta->iso);
				break;
			case 0x000c: /* D1 White Balance */
				if (g_str_equal(meta->model_ascii, "NIKON D1X")
				    || g_str_equal(meta->model_ascii, "NIKON D90")
					|| g_str_equal(meta->model_ascii, "NIKON D3S")
				    || g_str_equal(meta->model_ascii, "NIKON D300S")
				    || g_str_equal(meta->model_ascii, "NIKON D3000")
				    || g_str_equal(meta->model_ascii, "NIKON D3100")
				    || g_str_equal(meta->model_ascii, "NIKON D5000")
				    || g_str_equal(meta->model_ascii, "NIKON D7000"))
				{
					meta->cam_mul[0] = get_rational(rawfile, offset);
					meta->cam_mul[2] = get_rational(rawfile, offset+8);
					meta->cam_mul[1] = get_rational(rawfile, offset+16);
					meta->cam_mul[3] = get_rational(rawfile, offset+24);
					rs_metadata_normalize_wb(meta);
					got_wb = TRUE;
				}
				else
				{
					/* This is fucked, where did these two magic constants come from? */
					raw_get_float(rawfile, uint_temp1, &float_temp1);
					raw_get_float(rawfile, uint_temp1+4, &float_temp2);
					meta->cam_mul[0] = (gdouble) (float_temp1/float_temp2)/1.0;//2.218750;

					raw_get_float(rawfile, uint_temp1+8, &float_temp1);
					raw_get_float(rawfile, uint_temp1+12, &float_temp2);
					meta->cam_mul[2] = (gdouble) (float_temp1/float_temp2)/1.0;//1.148438;

					raw_get_float(rawfile, uint_temp1+16, &float_temp1);
					raw_get_float(rawfile, uint_temp1+20, &float_temp2);
					meta->cam_mul[1] = (gdouble) (float_temp1/float_temp2);

					raw_get_float(rawfile, uint_temp1+24, &float_temp1);
					raw_get_float(rawfile, uint_temp1+28, &float_temp2);
					meta->cam_mul[3] = (gdouble) (float_temp1/float_temp2);

					rs_metadata_normalize_wb(meta);
				}
				break;
			case 0x0011: /* NikonPreview */
				raw_get_uint(rawfile, offset, &uint_temp1);
				ifd_reader(rawfile, uint_temp1+base, meta);
				meta->thumbnail_start += base;
				break;
			case 0x0084: /* Lens - rational64u[4] */
				raw_get_rational(rawfile, offset, &float_temp1);
				meta->lens_min_focal = float_temp1;
				raw_get_rational(rawfile, offset+8, &float_temp1);
				meta->lens_max_focal = float_temp1;
				raw_get_rational(rawfile, offset+16, &float_temp1);
				meta->lens_max_aperture = float_temp1;
				raw_get_rational(rawfile, offset+24, &float_temp1);
				meta->lens_min_aperture = float_temp1;
				break;
			case 0x0097: /* white balance */
				if (g_str_equal(meta->model_ascii, "NIKON D90")
					|| g_str_equal(meta->model_ascii, "NIKON D3S")
					|| g_str_equal(meta->model_ascii, "NIKON D300S"))
					break;

				for(i=0;i<4;i++)
				{
					raw_get_uchar(rawfile, offset+i, &char_tmp);
					ver97 = (ver97 << 4) + char_tmp-'0';
				}
				offset += 4;
				switch (ver97)
				{
					case 0x100:
						/* RBGG-format */
						offset += 68;
						raw_get_ushort(rawfile, offset, &ushort_temp1);
						meta->cam_mul[0] = (gdouble) ushort_temp1;
						raw_get_ushort(rawfile, offset+2, &ushort_temp1);
						meta->cam_mul[2] = (gdouble) ushort_temp1;
						raw_get_ushort(rawfile, offset+4, &ushort_temp1);
						meta->cam_mul[1] = (gdouble) ushort_temp1;
						raw_get_ushort(rawfile, offset+6, &ushort_temp1);
						meta->cam_mul[3] = (gdouble) ushort_temp1;
						rs_metadata_normalize_wb(meta);
						break;
					case 0x102:
						/* RGGB-format */
						offset += 6;
						raw_get_ushort(rawfile, offset, &ushort_temp1);
						meta->cam_mul[0] = (gdouble) ushort_temp1;
						raw_get_ushort(rawfile, offset+2, &ushort_temp1);
						meta->cam_mul[1] = (gdouble) ushort_temp1;
						raw_get_ushort(rawfile, offset+4, &ushort_temp1);
						meta->cam_mul[3] = (gdouble) ushort_temp1;
						raw_get_ushort(rawfile, offset+6, &ushort_temp1);
						meta->cam_mul[2] = (gdouble) ushort_temp1;
						rs_metadata_normalize_wb(meta);
						break;
					case 0x103:
						offset += 16;
						for(i=0;i<4;i++)
						{
							raw_get_ushort(rawfile, offset+2*i, &ushort_temp1);
							meta->cam_mul[i] = ushort_temp1;
						}
						rs_metadata_normalize_wb(meta);
						break;
				}
				if (ver97 >> 8 == 2)
				{
					if (ver97 != 0x205)
						offset += 280;
					raw_strcpy(rawfile, offset, buf97, 324);
				}
				break;
			case 0x0098: /* LensData - LensData0100 | LensData0101 | LensData0201 | LensData0204 | LensDataUnknown */
				/* Will be used in 0x00a7 */
				raw_strcpy(rawfile, offset, &buf98, 33);
				gchar *str = raw_strdup(rawfile, offset, 4);
				lensdata = atoi(str);
				g_free(str);

				/* Unencrypted LensIDNumber */
				if (lensdata == 100)
					meta->lens_id = buf98[0x06];
				break;
			case 0x001d: /* serial */
				raw_get_uchar(rawfile, offset++, &char_tmp);
				while(char_tmp)
				{
					serial = serial*10 + (g_ascii_isdigit(char_tmp) ? char_tmp - '0' : char_tmp % 10);
					raw_get_uchar(rawfile, offset++, &char_tmp);
				}
				break;
			case 0x00a7: /* white balance */
			{
				guchar ctmp[4];
				raw_get_uchar(rawfile, offset++, ctmp);
				raw_get_uchar(rawfile, offset++, ctmp+1);
				raw_get_uchar(rawfile, offset++, ctmp+2);
				raw_get_uchar(rawfile, offset, ctmp+3);
				key = ctmp[0]^ctmp[1]^ctmp[2]^ctmp[3];

				/* data from 0x0098 */
				if (strlen((const gchar *) buf98))
				{
					ci = xlat[0][serial & 0xff];
					cj = xlat[1][key];
					ck = 0x60;

					for (i=4; i < sizeof(buf98); i++)
						buf98[i] = buf98[i] ^ (cj += ci * ck++);

					/* Finding LensIDNumber - 101 untested */
					if (lensdata == 101 || lensdata == 201 || lensdata == 202 || lensdata == 203)
						meta->lens_id = buf98[0x0b];
					else if (lensdata == 204)
						meta->lens_id = buf98[0x0c];
				}

				if (got_wb)
					break;

				if (ver97 >> 8 == 2)
				{
					ci = xlat[0][serial & 0xff];
					cj = xlat[1][key];
					ck = 0x60;
					for (i=0; i < 324; i++)
						buf97[i] ^= (cj += ci * ck++);

					for (i=0; i<4; i++)
						meta->cam_mul[i ^ (i >> 1)] = raw_get_ushort_from_string(
							rawfile, (gchar *)(buf97 + (ver97 == 0x205 ? 14:6) + i*2));
					if (ver97 == 0x209) /* D300 */
						for(i=0; i<4; i++)
							meta->cam_mul[i ^ (i >> 1) ^ 1] = raw_get_ushort_from_string(rawfile, (gchar *)(buf97 + 10 + i*2));
					rs_metadata_normalize_wb(meta);
				}
				break;
			}
			case 0x00aa: /* Nikon Saturation */
				if (meta->make == MAKE_NIKON)
				{
					if (raw_strcmp(rawfile, offset, "ENHANCED", 8))
						meta->saturation = 1.5;
					else if (raw_strcmp(rawfile, offset, "MODERATE", 8))
						meta->saturation = 0.5;
					else
						meta->saturation = 1.0;
				}
				break;
			case 0x0081: /* Nikon ToneComp (contrast)*/
				if (meta->make == MAKE_NIKON)
				{
					if (raw_strcmp(rawfile, offset, "HIGH", 4))
						meta->contrast = 1.2;
					else if (raw_strcmp(rawfile, offset, "LOW", 3))
						meta->contrast= 0.8;
					else
						meta->contrast = 1.0;
				}
				break;

		}
	}
	return TRUE;
}

static gboolean
makernote_olympus_camerasettings(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta)
{
	/* NOTE! At least on E-410 the offsets in this section is relative to
	   the base of the MakerNotes! */

	gushort number_of_entries;
	gushort fieldtag=0;
	gushort fieldtype;
	guint valuecount;
	guint uint_temp1=0;
	guint save;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	if (number_of_entries>5000)
		return FALSE;
	offset += 2;

	save = offset;
	while(number_of_entries--)
	{
		/* FIXME: Port to read_ifd() */
		offset = save;
		raw_get_ushort(rawfile, offset, &fieldtag);
		raw_get_ushort(rawfile, offset+2, &fieldtype);
		raw_get_uint(rawfile, offset+4, &valuecount);
		offset += 8;

		save = offset + 4;
		if ((valuecount * ("1112481124848"[fieldtype < 13 ? fieldtype:0]-'0') > 4))
		{
			raw_get_uint(rawfile, offset, &uint_temp1);
			offset = base + uint_temp1;
		}
		raw_get_uint(rawfile, offset, &uint_temp1);
		switch(fieldtag)
		{
			case 0x0101: /* PreviewImageStart */
				raw_get_uint(rawfile, offset, &meta->preview_start);
				meta->preview_start += raw_get_base(rawfile);
				break;
			case 0x0102: /* PreviewImageLength */
				raw_get_uint(rawfile, offset, &meta->preview_length);
				break;
		}
	}
	return TRUE;
}

static gboolean
makernote_olympus_imageprocessing(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	struct IFD ifd;
	gushort ushort_temp1, ushort_temp2;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;

	if (number_of_entries>5000)
		return FALSE;

	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch(ifd.tag)
		{
			case 0x0100: /* WB on E-510 */
				if (ifd.count == 2)
				{
					raw_get_ushort(rawfile, ifd.offset, &ushort_temp1);
					raw_get_ushort(rawfile, ifd.offset+2, &ushort_temp2);
				}
				else if (ifd.count == 4)
				{
					raw_get_ushort(rawfile, ifd.offset+base, &ushort_temp1);
					raw_get_ushort(rawfile, ifd.offset+base+2, &ushort_temp2);
				}
				meta->cam_mul[0] = (gdouble) ushort_temp1 / 256.0;
				meta->cam_mul[2] = (gdouble) ushort_temp2 / 256.0;
				rs_metadata_normalize_wb(meta);
				break;
		}
	}
	return TRUE;
}

static gboolean
makernote_olympus_equipment(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	struct IFD ifd;
	gushort ushort_temp1;
	gchar *str = NULL;
	gint total;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;

	if (number_of_entries>5000)
		return FALSE;

	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch(ifd.tag)
		{
			case 0x0202: /* LensSerialNumber */
				str = raw_strdup(rawfile, base + ifd.value_offset, 32);

				/* Make a number from the string we just got */
				gint i = 0;
				while(str[i])
					total += str[i++];

				meta->lens_id = total;
				break;
			case 0x0205: /* MinApertureAtMinFocal */
				raw_get_ushort(rawfile, offset-4, &ushort_temp1);
				meta->lens_min_aperture = (gfloat) pow(sqrt(2),(ushort_temp1/256));
				break;
			case 0x0206: /* MaxApertureAtMaxFocal */
				raw_get_ushort(rawfile, offset-4, &ushort_temp1);
				meta->lens_max_aperture = (gfloat) pow(sqrt(2),(ushort_temp1/256));
				break;
			case 0x0207: /* MinFocalLength */
				raw_get_ushort(rawfile, offset-4, &ushort_temp1);
				meta->lens_min_focal = ushort_temp1;
				break;
			case 0x0208: /* MaxFocalLength */
				raw_get_ushort(rawfile, offset-4, &ushort_temp1);
				meta->lens_max_focal = ushort_temp1;
				break;
		}
	}
	return TRUE;
}


static gboolean
makernote_olympus(RAWFILE *rawfile, guint base, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	gushort fieldtag=0;
	gushort fieldtype;
	gushort ushort_temp1=0;
	guint valuecount;
	guint uint_temp1=0;
	guint save;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	if (number_of_entries>5000)
		return FALSE;
	offset += 2;

	save = offset;
	while(number_of_entries--)
	{
		/* FIXME: Port to read_ifd() */
		offset = save;
		raw_get_ushort(rawfile, offset, &fieldtag);
		raw_get_ushort(rawfile, offset+2, &fieldtype);
		raw_get_uint(rawfile, offset+4, &valuecount);
		offset += 8;

		save = offset + 4;
		if ((valuecount * ("1112481124848"[fieldtype < 13 ? fieldtype:0]-'0') > 4))
		{
			raw_get_uint(rawfile, offset, &uint_temp1);
			offset = base + uint_temp1;
		}
		raw_get_uint(rawfile, offset, &uint_temp1);
		switch(fieldtag)
		{
			case 0x0100: /* Thumbnail */
				raw_get_ushort(rawfile, save-4, &ushort_temp1);
				meta->thumbnail_start = ushort_temp1;
				meta->thumbnail_length = valuecount;
				break;
			case 0x1017: /* Red multiplier on many Olympus's (E-10, E-300, E-330, E-400, E-500) */
				raw_get_ushort(rawfile, offset, &ushort_temp1);
				meta->cam_mul[0] = (gdouble) ushort_temp1 / 256.0;
				break;
			case 0x1018: /* Blue multiplier on many Olympus's (E-10, E-300, E-330, E-400, E-500) */
				raw_get_ushort(rawfile, offset, &ushort_temp1);
				meta->cam_mul[2] = (gdouble) ushort_temp1 / 256.0;
				break;
			case 0x2010: /* Equipment2 */
				raw_get_uint(rawfile, offset, &uint_temp1);
				makernote_olympus_equipment(rawfile, base, base+uint_temp1, meta);
				break;
			case 0x2020: /* Olympus CameraSettings Tags */
				raw_get_uint(rawfile, offset, &uint_temp1);
				makernote_olympus_camerasettings(rawfile, base+uint_temp1, base+uint_temp1, meta);
				meta->preview_start += base; /* Stupid hack! */
				break;
			case 0x2040: /* Olympus ImageProcessing  */
				raw_get_uint(rawfile, offset, &uint_temp1);
				makernote_olympus_imageprocessing(rawfile, base, base+uint_temp1, meta);
				break;
		}
	}
	return TRUE;
}

static gboolean
makernote_panasonic(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	struct IFD ifd;
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
	return FALSE;

	if (number_of_entries>5000)
		return FALSE;

	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;
		switch(ifd.tag)
		{
			case 81: /* Lens type */
				meta->fixed_lens_identifier = raw_strdup(rawfile, ifd.value_offset, ifd.count);
				break;
			case 82: /* Lens serial number */
				if (!meta->fixed_lens_identifier)
					meta->fixed_lens_identifier = raw_strdup(rawfile, ifd.value_offset, ifd.count);
				break;
			case 0x8769: /* ExifIFDPointer */
				exif_reader(rawfile, ifd.value_offset, meta);
				break;
			case 0x0111: /* StripOffsets - may be jpeg data */
				if (ifd.count == 1 && raw_get_uint(rawfile, ifd.offset, &meta->thumbnail_start))
					meta->thumbnail_start += raw_get_base(rawfile);
				else
					meta->thumbnail_start = 0;
				break;
			case 0x0117: /* StripByteCounts */
				if (ifd.value_offset)
					meta->thumbnail_length = ifd.value_offset;
				break;
		}
	}
	return TRUE;
}

static gboolean
likely_jpeg_at(RAWFILE *rawfile, guint offset)
{
	guchar thumb_test_1 = 0;
	guchar thumb_test_2 = 0;
	guchar thumb_test_comps = 0;

	if (raw_get_uchar(rawfile, offset, &thumb_test_1))
	{
		/* If SOI marker, it is likely valid JPEG*/
		if (thumb_test_1 == 0xff && raw_get_uchar(rawfile, offset+1, &thumb_test_2))
			if (thumb_test_2 == 0xd8)
				/* We read the "number of components", any value > 4 doesn't make sense */
				if (raw_get_uchar(rawfile, offset+6, &thumb_test_comps))
					if (thumb_test_comps <= 4)
						return TRUE;
	}
	return FALSE;
}


static gboolean
ifd_panasonic(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	struct IFD ifd;
	RAWFILE *internal_file;
	guint off_first;
	gushort byteorder;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;

	if (number_of_entries>5000)
		return FALSE;

	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;
		switch(ifd.tag)
		{
			case 0x0017: /* ISO */
				meta->iso = ifd.value_offset;
				break;
			case 0x0024: /* WBRedLevel */
				meta->cam_mul[0] = ifd.value_offset;
				break;
			case 0x0025: /* WBGreenLevel */
				meta->cam_mul[1] = ifd.value_offset;
				meta->cam_mul[3] = ifd.value_offset;
				break;
			case 0x0026: /* WBBlueLevel */
				meta->cam_mul[2] = ifd.value_offset;
				break;
			case 0x002e: /* Data Dump */
				if (ifd.type != 7 || ifd.count <= 16)
					break;
				if (!raw_get_uint(rawfile, ifd.value_offset+12+4, &off_first))
					break;
				if (!raw_get_ushort(rawfile,ifd.value_offset+12, &byteorder))
					break;
				meta->thumbnail_length = ifd.count;
				internal_file = raw_create_from_memory(raw_get_map(rawfile)+ifd.value_offset+12, ifd.count, off_first, byteorder);
				ifd_reader(internal_file, off_first, meta);
				raw_close_file(internal_file);
				break;
		}
	}
	if (meta->thumbnail_start > raw_get_filesize(rawfile))
		meta->thumbnail_start = 0;
	if (!likely_jpeg_at(rawfile, meta->thumbnail_start))
		meta->thumbnail_start = 0;

	guint thumb_guess =0x1000;
	/* Thumbnails in Panasonic are always at 0x100 byte boundaries, and never observed start after byte 0x3000, so we play safe */
	while (meta->thumbnail_start == 0 && thumb_guess < 0x8000)
	{
		if (likely_jpeg_at(rawfile, thumb_guess))
			meta->thumbnail_start = thumb_guess;
		thumb_guess += 0x100;
	}
	/* Find end of image */
	if (meta->thumbnail_start)
	{
		thumb_guess = 0;
		guchar* image = raw_get_map(rawfile) + meta->thumbnail_start;
		do {
			if (image[thumb_guess] == 0xff && image[thumb_guess] == 0xd9)
				meta->thumbnail_length = thumb_guess+2;
		} while (thumb_guess++ < raw_get_filesize(rawfile) - meta->thumbnail_start);
	}
	return TRUE;
}

static gboolean
makernote_pentax(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	gushort ushort_temp1=0;
	struct IFD ifd;

	if (raw_strcmp(rawfile, offset, "AOC", 3))
		offset += 6;
	else
		return FALSE;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;

	if (number_of_entries>5000)
		return FALSE;

	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;
		switch(ifd.tag)
		{
			case 0x0201: /* White balance */
				raw_get_ushort(rawfile, ifd.value_offset, &ushort_temp1);
				meta->cam_mul[0] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, ifd.value_offset+2, &ushort_temp1);
				meta->cam_mul[1] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, ifd.value_offset+4, &ushort_temp1);
				meta->cam_mul[3] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, ifd.value_offset+6, &ushort_temp1);
				meta->cam_mul[2] = (gdouble) ushort_temp1;
				break;
		}
	}

	return TRUE;
}

static gboolean
makernote_sony(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries = 0;
	guint uint_temp1;

	struct IFD ifd;

	/* get number of entries */
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch (ifd.tag)
		{
		case 0xb027: /* LensType */
			raw_get_uint(rawfile, offset-4, &uint_temp1);
			meta->lens_id = uint_temp1;
			break;
		}
	}

	return TRUE;
}

static void
sony_decrypt(SonyMeta *sony, guint *data, gint len)
{
	while (len--)
		*data++ ^= sony->pad[sony->p++ & 127] = sony->pad[(sony->p+1) & 127] ^ sony->pad[(sony->p+65) & 127];
}

static gboolean
private_sony(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries;
	struct IFD ifd;
	gint i;
	gint key;
	SonyMeta *sony = (SonyMeta *) meta;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;

	if (number_of_entries>5000)
		return FALSE;

	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch(ifd.tag)
		{
			case 0x7200: /* SR2SubIFDOffset */
				sony->sony_offset = ifd.value_offset;
				break;
			case 0x7201: /* SR2SubIFDLength */
				sony->sony_length = ifd.value_offset;
				break;
			case 0x7221: /* SR2SubIFDKey */
				sony->sony_key = ifd.value_offset;

				/* Initialize decrypter */
				key = sony->sony_key;
				for (sony->p=0; sony->p < 4; sony->p++)
					sony->pad[sony->p] = key = key * 48828125 + 1;
				sony->pad[3] = sony->pad[3] << 1 | (sony->pad[0]^sony->pad[2]) >> 31;
				for (sony->p=4; sony->p < 127; sony->p++)
					sony->pad[sony->p] = (sony->pad[sony->p-4]^sony->pad[sony->p-2]) << 1 | (sony->pad[sony->p-3]^sony->pad[sony->p-1]) >> 31;
				for (sony->p=0; sony->p < 127; sony->p++)
					sony->pad[sony->p] = htonl(sony->pad[sony->p]);
				break;
		}
	}

	if ((sony->sony_offset > 0) && (sony->sony_length > 0) && (sony->sony_key != 0))
	{
		gpointer buf = g_new0(guchar, sony->sony_length);
		if (raw_strcpy(rawfile, sony->sony_offset, buf, sony->sony_length))
		{
			sony_decrypt(sony, buf, sony->sony_length/4);
			gushort *sbuf = (gushort *)(buf);
			gushort tag_count = sbuf[0];
			struct IFD *private_ifd;

			for(i=0;i<tag_count;i++)
			{
#if BYTE_ORDER == BIG_ENDIAN
#warning FIXME: This will NOT work as expected on a big endian host
#endif
				private_ifd = (struct IFD *) (buf+2+i*12);

				switch (private_ifd->tag)
				{
					case 0x7303: /* WB_GRBGLevels */
						sbuf = (gushort *)(buf + private_ifd->value_offset - sony->sony_offset);
						meta->cam_mul[1] = (gdouble) sbuf[0];
						meta->cam_mul[0] = (gdouble) sbuf[1];
						meta->cam_mul[2] = (gdouble) sbuf[2];
						meta->cam_mul[3] = (gdouble) sbuf[3];

						rs_metadata_normalize_wb(meta);
						break;
					case 0x7313: /* WB_RGGBLevels */
						sbuf = (gushort *)(buf + private_ifd->value_offset - sony->sony_offset);
						meta->cam_mul[0] = (gdouble) sbuf[0];
						meta->cam_mul[1] = (gdouble) sbuf[1];
						meta->cam_mul[3] = (gdouble) sbuf[2];
						meta->cam_mul[2] = (gdouble) sbuf[3];

						rs_metadata_normalize_wb(meta);
						break;
				}
			}
		}
	}

	return TRUE;
}

static gboolean
exif_reader(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries = 0;
	
	struct IFD ifd;

	/* get number of entries */
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch (ifd.tag)
		{
			case 0x010f: /* Make */
				if (!meta->make_ascii)
					meta->make_ascii = rs_remove_tailing_spaces(raw_strdup(rawfile, ifd.offset, ifd.count), TRUE);
				break;
			case 0x0110: /* Model */
				if (!meta->model_ascii)
					meta->model_ascii = rs_remove_tailing_spaces(raw_strdup(rawfile, ifd.offset, ifd.count), TRUE);
				break;
			case 0x9003: /* DateTime */
			case 0x9004: /* DateTime */
				if (!meta->time_ascii)
				{
					meta->time_ascii = raw_strdup(rawfile, ifd.value_offset, ifd.count);
					meta->timestamp = rs_exiftime_to_unixtime(meta->time_ascii);
				}
				break;
			case 0x829A: /* ExposureTime */
				if (ifd.count == 1 && ifd.value_rational < EXPO_TIME_MAXVAL)
					meta->shutterspeed = 1.0 / ifd.value_rational;
				break;
			case 0x829D: /* FNumber */
				if (ifd.count == 1)
					meta->aperture = ifd.value_rational;
				break;
			case 0x8827: /* ISOSpeedRatings */
				if (ifd.count == 1)
					meta->iso = ifd.value_ushort;
				break;
			case 0x920A: /* Focal length */
					meta->focallength = ifd.value_rational;
				break;
			case 0x927c: /* MakerNote */
				switch (meta->make)
				{
					case MAKE_CANON:
						makernote_canon(rawfile, ifd.value_offset, meta);
						break;
					case MAKE_MINOLTA:
						makernote_minolta(rawfile, ifd.value_offset, meta);
						break;
					case MAKE_NIKON:
						makernote_nikon(rawfile, ifd.value_offset, meta);
						break;
					case MAKE_PENTAX:
						makernote_pentax(rawfile, ifd.value_offset, meta);
						break;
					case MAKE_OLYMPUS:
						if (raw_strcmp(rawfile, ifd.value_offset, "OLYMPUS", 7))
							makernote_olympus(rawfile, ifd.value_offset, ifd.value_offset+12, meta);
						else if (raw_strcmp(rawfile, ifd.value_offset, "OLYMP", 5))
							makernote_olympus(rawfile, ifd.value_offset+8, ifd.value_offset+8, meta);
						break;
					case MAKE_SONY:
						makernote_sony(rawfile, ifd.value_offset, meta);
						break;
					case MAKE_PANASONIC:
						if (raw_strcmp(rawfile, ifd.value_offset, "Panasonic", 9))
							makernote_panasonic(rawfile, ifd.value_offset+12, meta);
						break;
					default:
						break;
				}
				break;
		}
	}

	return TRUE;
}

static gboolean 
parse_dng_private_data(RAWFILE *rawfile, guint offset, RSMetadata *meta) 
{
  /*
  1. Six bytes containing the zero-terminated string "Adobe". (The DNG specification calls for the DNGPrivateData tag to start with an ASCII string identifying the creator/format).
  2. 4 bytes: an ASCII string ("MakN" for a Makernote),  indicating what sort of data is being stored here. Note that this is not zero-terminated.
  3. A four-byte count (number of data bytes following); this is the length of the original MakerNote data. (This is always in "most significant byte first" format).
  4. 2 bytes: the byte-order indicator from the original file (the usual 'MM'/4D4D or 'II'/4949).
  5. 4 bytes: the original file offset for the MakerNote tag data (stored according to the byte order given above).
  6. The contents of the MakerNote tag. This is a simple byte-for-byte copy, with no modification.
  */
	gushort tiff_byteorder = raw_get_byteorder(rawfile);

	/* Check if first string is "Adobe" */
	if (!raw_strcmp(rawfile, offset, "Adobe", 5))
		return FALSE;
	offset+=6;

	/* Check if type is Adobe Makernote */
	if (!raw_strcmp(rawfile, offset, "MakN", 4))
		return FALSE;
	offset+=4;

	/* Read makernote info */
	raw_set_byteorder(rawfile, 0x4D4D);
	guint org_size;
	if (!raw_get_uint(rawfile, offset, &org_size))
	{
		raw_set_byteorder(rawfile, tiff_byteorder);
		return FALSE;
	}
	if (org_size > raw_get_filesize(rawfile)-offset)
	{
		raw_set_byteorder(rawfile, tiff_byteorder);
		return FALSE;
	}
	offset+=4;

	gushort byteorder = 0;
	raw_get_ushort(rawfile, offset, &byteorder);
	offset+=2;
	if (byteorder != 0x4D4D && byteorder != 0x4949)
	{
		raw_set_byteorder(rawfile, tiff_byteorder);
		return FALSE;
	}

	guint org_offset;
	if (!raw_get_uint(rawfile, offset, &org_offset))
	{
		raw_set_byteorder(rawfile, tiff_byteorder);
		return FALSE;
	}
	offset+=4;

	/* Create memory mapped TIFF */
	const gchar* data = raw_get_map(rawfile);
	gchar* maker_data = g_malloc(org_offset + org_size);
	memcpy(&maker_data[org_offset],&data[offset], org_size);
	RAWFILE *maker_raw = raw_create_from_memory(maker_data, org_offset + org_size, org_offset, byteorder);

	/* Read makernote, as if this was the original file */
	/* We preserve WB since it might have changed - converted Canon sRAW for instance */
	gdouble stored_mul[4];
	memcpy(stored_mul, meta->cam_mul,4*sizeof(gdouble));
	switch (meta->make)
	{
		case MAKE_CANON:
			makernote_canon(maker_raw, org_offset, meta);
			break;
		case MAKE_MINOLTA:
			makernote_minolta(maker_raw, org_offset, meta);
			break;
		case MAKE_NIKON:
			makernote_nikon(maker_raw, org_offset, meta);
			break;
		case MAKE_PENTAX:
			makernote_pentax(maker_raw, org_offset, meta);
			break;
		case MAKE_OLYMPUS:
			if (raw_strcmp(maker_raw, org_offset, "OLYMPUS", 7))
				makernote_olympus(maker_raw, org_offset, org_offset+12, meta);
			else if (raw_strcmp(maker_raw,org_offset, "OLYMP", 5))
				makernote_olympus(maker_raw, org_offset+8, org_offset+8, meta);
			break;
		case MAKE_SONY:
			makernote_sony(maker_raw, org_offset, meta);
			break;
		case MAKE_PANASONIC:
			if (raw_strcmp(maker_raw, org_offset, "Panasonic", 9))
				makernote_panasonic(maker_raw, org_offset+12, meta);
			break;
		default:
			break;
	}
	/* Restore WB */
	memcpy(meta->cam_mul, stored_mul, 4*sizeof(gdouble));
	raw_close_file(maker_raw);
	g_free(maker_data);
	raw_set_byteorder(rawfile, tiff_byteorder);
	return TRUE;
}


static gboolean
ifd_reader(RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	gushort number_of_entries = 0;
	gboolean is_preview = FALSE;
	guint uint_temp1;
	gfloat float_temp;

	struct IFD ifd;

	/* get number of entries */
	if(!raw_get_ushort(rawfile, offset, &number_of_entries))
		return FALSE;
	offset += 2;

	while(number_of_entries--)
	{
		read_ifd(rawfile, offset, &ifd);
		offset += 12;

		switch (ifd.tag)
		{
			case 0x00fe: /* Subfile type */
				is_preview = (((gint)ifd.value) & 1) != 0;
				break;
			case 0x0100: /* Image width */
				if (is_preview)
					meta->preview_width = ifd.value;
				break;
			case 0x0101: /* Image length (aka height in human language) */
				if (is_preview)
					meta->preview_height = ifd.value;
				break;
			case 0x0102: /* Bits per sample */
				if (is_preview)
				{
					raw_get_ushort (rawfile, ifd.value_offset + 0, &meta->preview_bits [0]);
					raw_get_ushort (rawfile, ifd.value_offset + 2, &meta->preview_bits [1]);
					raw_get_ushort (rawfile, ifd.value_offset + 4, &meta->preview_bits [2]);
				}
				break;
			case 0x0103: /* Compression */
				break;
			case 0x010f: /* Make */
				if (!meta->make_ascii)
				{
					meta->make_ascii = rs_remove_tailing_spaces(raw_strdup(rawfile, ifd.offset, ifd.count), TRUE);
					if (raw_strcmp(rawfile, ifd.value_offset, "Canon", 5))
						meta->make = MAKE_CANON;
					else if (raw_strcmp(rawfile, ifd.value_offset, "CASIO", 5))
						meta->make = MAKE_CASIO;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Hasselblad", 10))
						meta->make = MAKE_HASSELBLAD;
					else if (raw_strcmp(rawfile, ifd.value_offset, "KODAK", 5))
						meta->make = MAKE_KODAK;
					else if (raw_strcmp(rawfile, ifd.value_offset, "EASTMAN KODAK", 13))
						meta->make = MAKE_KODAK;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Leica", 5))
						meta->make = MAKE_LEICA;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Minolta", 7))
						meta->make = MAKE_MINOLTA;
					else if (raw_strcmp(rawfile, ifd.value_offset, "KONICA MINOLTA", 14))
						meta->make = MAKE_MINOLTA;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Mamiya", 6))
						meta->make = MAKE_MAMIYA;
					else if (raw_strcmp(rawfile, ifd.value_offset, "NIKON", 5))
						meta->make = MAKE_NIKON;
					else if (raw_strcmp(rawfile, ifd.value_offset, "OLYMPUS", 7))
						meta->make = MAKE_OLYMPUS;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Panasonic", 9))
						meta->make = MAKE_PANASONIC;
					else if (raw_strcmp(rawfile, ifd.value_offset, "PENTAX", 6))
						meta->make = MAKE_PENTAX;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Phase One", 9))
						meta->make = MAKE_PHASEONE;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Ricoh", 5))
						meta->make = MAKE_RICOH;
					else if (raw_strcmp(rawfile, ifd.value_offset, "SAMSUNG", 7))
						meta->make = MAKE_SAMSUNG;
					/* Do not detect SONY, we don't want to call private_sony() unless
					   we're sure we have a hidden SonyMeta */
					else if (raw_strcmp(rawfile, ifd.value_offset, "FUJIFILM", 4))
						meta->make = MAKE_FUJIFILM;
					else if (raw_strcmp(rawfile, ifd.value_offset, "SEIKO EPSON", 11))
						meta->make = MAKE_EPSON;
				}
				break;
			case 0x0110: /* Model */
				if (!meta->model_ascii)
					meta->model_ascii = rs_remove_tailing_spaces(raw_strdup(rawfile, ifd.offset, ifd.count), TRUE);
				break;
			case 0x0111: /* StripOffsets */
				if (meta->preview_start==0 || is_preview)
					meta->preview_start = ifd.value + raw_get_base(rawfile);
				break;
			case 0x0112: /* Orientation */
				if (ifd.count == 1)
				{
					meta->orientation = ifd.value_ushort;
					switch (meta->orientation)
					{
						case 6: meta->orientation = 90;
							break;
						case 8: meta->orientation = 270;
							break;
					}
				}
				break;
			case 0x0117: /* StripByteCounts */
				if (meta->preview_length==0 || is_preview)
					meta->preview_length = ifd.value;
				break;
			case 0x011c: /* Planar configuration */
				if (is_preview)
					meta->preview_planar_config = ifd.value;
				break;
			case 0x0132: /* DateTime */
				break;
			case 0x014a: /* SubIFD */
				if (ifd.count == 1)
					ifd_reader(rawfile, ifd.value_offset, meta);
				else
				{
					raw_get_uint(rawfile, ifd.value_offset, &uint_temp1);
					ifd_reader(rawfile, uint_temp1, meta);
				}
				break;
			case 0x0201: /* JPEGInterchangeFormat */
				meta->thumbnail_start = ifd.value_uint + raw_get_base(rawfile);
				break;
			case 0x0202: /* JPEGInterchangeFormatLength */
				meta->thumbnail_length = ifd.value_uint;
				break;
			case 0x8769: /* ExifIFDPointer */
				exif_reader(rawfile, ifd.value_offset, meta);
				break;

			/* The following tags are from the DNG spec, they should be safe */
			case 0xc628: /* DNG: AsShotNeutral */
				if (((ifd.type == TIFF_FIELD_TYPE_RATIONAL)||(ifd.type == TIFF_FIELD_TYPE_SRATIONAL)) && ifd.count == 3)
				{
					meta->cam_mul[0] = 1.0/get_rational(rawfile, ifd.value_offset);
					meta->cam_mul[1] = 1.0/get_rational(rawfile, ifd.value_offset+8);
					meta->cam_mul[2] = 1.0/get_rational(rawfile, ifd.value_offset+16);
					meta->cam_mul[3] = meta->cam_mul[1];
					rs_metadata_normalize_wb(meta);
				}
				break;
			case 0xc634: /* DNG: PrivateData */
				if (meta->make == MAKE_SONY)
					private_sony(rawfile, ifd.value_offset, meta);
				parse_dng_private_data(rawfile, ifd.value_offset, meta);
				break;
			case 0xc630: /* DNG LensInfo */
				if (raw_get_rational(rawfile, ifd.value_offset, &float_temp))
					meta->lens_min_focal = float_temp;
				if (raw_get_rational(rawfile, ifd.value_offset+8, &float_temp))
					meta->lens_max_focal = float_temp;
				if (raw_get_rational(rawfile, ifd.value_offset+16, &float_temp))
					meta->lens_min_aperture = float_temp;
				if (raw_get_rational(rawfile, ifd.value_offset+24, &float_temp))
					meta->lens_max_aperture = float_temp;
				break;
		}
	}

	return TRUE;
}

/**
 * Generic TIFF reader
 */
static void
tiff_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	guint next = 0;
	gushort ifd_num = 0;
	guchar version;

	version = raw_init_file_tiff(rawfile, offset);

	if (version == 0x55)
		meta->make = MAKE_LEICA;

	offset = get_first_ifd_offset(rawfile);
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break; /* used for calculating next IFD */
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break; /* 2: offset+short(ifd_num), 12: length of ifd-entry */

		switch (meta->make)
		{
			case MAKE_LEICA:
				if (!makernote_leica(rawfile, offset, meta))
					ifd_reader(rawfile, offset, meta);
				break;
			default:
				ifd_reader(rawfile, offset, meta);
				break;
		}

		/* Hack to support a few cameras that embeds EXIF-info or Makernotes in IFD 0 */
		if (meta->make == MAKE_CANON && g_str_equal(meta->model_ascii, "EOS D2000C"))
			exif_reader(rawfile, offset, meta);
		if (meta->make == MAKE_KODAK && g_str_equal(meta->model_ascii, "DCS520C"))
			exif_reader(rawfile, offset, meta);
		if (meta->make == MAKE_KODAK && g_str_equal(meta->model_ascii, "DCS Pro 14N"))
			exif_reader(rawfile, offset, meta);
		if (meta->make == MAKE_PANASONIC)
			ifd_panasonic(rawfile, offset, meta);

		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

	rs_metadata_normalize_wb(meta);
}

/**
 * .TIF reader
 */
static void
tif_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{

	tiff_load_meta(service, rawfile, offset, meta);

	/* Phase One and Samsung doesn't set this */
	if ((meta->make == MAKE_PHASEONE) || (meta->make == MAKE_SAMSUNG))
		meta->preview_planar_config = 1;

	/* Load thumbnail - try thumbnail first - then preview image - then decode the RAW image*/
	if (!thumbnail_reader(service, rawfile, meta->thumbnail_start, meta->thumbnail_length, meta))
		if (!thumbnail_reader(service, rawfile, meta->preview_start, meta->preview_length, meta))
			thumbnail_store(raw_thumbnail_reader(service, meta), meta);
}

static gboolean
thumbnail_reader(const gchar *service, RAWFILE *rawfile, guint offset, guint length, RSMetadata *meta)
{
	GdkPixbuf *pixbuf=NULL;

	if ((offset>0) && (length>0) && (length<5000000))
	{
		if ((length==165888) && (meta->make == MAKE_CANON))
			pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+offset, GDK_COLORSPACE_RGB, FALSE, 8, 288, 192, 288*3, NULL, NULL);
		else if (length==57600) /* Multiple Nikon, Pentax and Samsung cameras */
			pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+offset, GDK_COLORSPACE_RGB, FALSE, 8, 160, 120, 160*3, NULL, NULL);
		else if (length==48672)
			pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+offset, GDK_COLORSPACE_RGB, FALSE, 8, 156, 104, 156*3, NULL, NULL);
		else
			/* Many RAW files are based on TIFF and include the preview image
			 * as the "main" image in the TIFF so that "normal" image viewing
			 * programs can display at least the thumbnail. So we will
			 * check if the TIFF contains such a thumbnail in the simplest
			 * possible format (e.g. uncompressed R8G8B8) and use it
			 * if it is present.
			 */
			if (offset == meta->preview_start && /* if we're using the preview image */
				meta->preview_planar_config == 1 && /* uncompressed */
				meta->preview_bits [0] == 8 &&
				meta->preview_bits [1] == 8 &&
				meta->preview_bits [2] == 8 && /* R8G8B8 */
				meta->preview_width * meta->preview_height * 3 == length &&
				meta->preview_width > 16 &&
				meta->preview_width < 1024 &&
				meta->preview_height > 16 &&
				meta->preview_height < 1024)    /* Some arbitrary sane limit */
				pixbuf = gdk_pixbuf_new_from_data(
					raw_get_map(rawfile)+offset, GDK_COLORSPACE_RGB, FALSE, 8,
					meta->preview_width, meta->preview_height,
					meta->preview_width * 3, NULL, NULL);
			else
				/* Try to guess file format based on contents (JPEG previews) */
			pixbuf = raw_get_pixbuf(rawfile, offset, length);
	}
	if ( pixbuf && (gdk_pixbuf_get_width(pixbuf) < 10 || gdk_pixbuf_get_height(pixbuf) < 10))
		pixbuf = NULL;

	return thumbnail_store(pixbuf, meta);
}

static gboolean
thumbnail_store(GdkPixbuf *pixbuf, RSMetadata *meta)
{
	GdkPixbuf *pixbuf2=NULL;
	if (pixbuf)
	{
		gdouble ratio;
		/* Handle Canon/Nikon cropping */
		if ((gdk_pixbuf_get_width(pixbuf) == 160) && (gdk_pixbuf_get_height(pixbuf)==120))
		{
			pixbuf2 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 160, 106);
			gdk_pixbuf_copy_area(pixbuf, 0, 7, 160, 106, pixbuf2, 0, 0);
			g_object_unref(pixbuf);
			pixbuf = pixbuf2;
		}

		/* Scale to a bounding box of 128x128 pixels */
		ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
		if (ratio>1.0)
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
		else
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = pixbuf2;

		/* Rotate thumbnail in place */
		switch (meta->orientation)
		{
			/* this is very COUNTER-intuitive - gdk_pixbuf_rotate_simple() is wierd */
			case 90:
				pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
				break;
			case 270:
				pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
				break;
		}
		meta->thumbnail = pixbuf;
		return TRUE;
	}
	return FALSE;	
}

		
static GdkPixbuf*
raw_thumbnail_reader(const gchar *service, RSMetadata *meta)
{
	GdkPixbuf* pixbuf = 0;
	gint c;
	gfloat pre_mul[4];

	RSFilter *finput = rs_filter_new("RSInputFile", NULL);
	RSFilter *fdemosaic = rs_filter_new("RSDemosaic", finput);
	RSFilter *fresample = rs_filter_new("RSResample", fdemosaic);
	RSFilter *fcst = rs_filter_new("RSColorspaceTransform", fresample);
	
	g_object_set(fresample, "width", 256,
				 "height", 256, 
				"bounding-box", TRUE, NULL);

	g_object_set(finput, "filename", service, 
				 "color-space", rs_color_space_new_singleton("RSSrgb"), NULL);

	rs_filter_set_recursive(RS_FILTER(fdemosaic), "demosaic-allow-downscale",  TRUE, NULL);
	
	RSFilterRequest *request = rs_filter_request_new();
	rs_filter_request_set_roi(request, FALSE);
	rs_filter_request_set_quick(request, TRUE);

	for(c=0;c<4;c++)
		pre_mul[c] = (gfloat) meta->cam_mul[c] * 1.5f;

	rs_filter_param_set_float4(RS_FILTER_PARAM(request), "premul", pre_mul);
	rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", rs_color_space_new_singleton("RSSrgb"));	

	RSFilterResponse *response = rs_filter_get_image8(fcst, request);
	pixbuf = rs_filter_response_get_image8(response);

	g_object_unref(finput);
	g_object_unref(fdemosaic);
	g_object_unref(fresample);
	g_object_unref(fcst);
	g_object_unref(request);
	g_object_unref(response);

	return pixbuf;
}

static void
sony_load_meta(const gchar *service, RAWFILE *rawfile, guint offset, RSMetadata *meta)
{
	SonyMeta sony;
	sony.sony_offset = 0;
	sony.sony_length = 0;
	sony.sony_key = 0;
	meta->make = MAKE_SONY;

	memcpy(&sony, meta, sizeof(RSMetadata));
	tif_load_meta(service, rawfile, offset, RS_METADATA(&sony));
	memcpy(meta, &sony, sizeof(RSMetadata));
}

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_meta_loader(".cr2", "Canon CR2", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".nef", "Nikon NEF", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".nrw", "Nikon NEF 2", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".tif", "Canon TIFF", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".rwl", "Leica", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".arw", "Sony", sony_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".sr2", "Sony", sony_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".srf", "Sony", sony_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".kdc", "Kodak", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".dcr", "Kodak", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".orf", "Olympus", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".raw", "Panasonic raw", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".rw2", "Panasonic raw 2", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".pef", "Pentax raw", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".dng", "Adobe Digital negative", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".mef", "Mamiya", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".3fr", "Hasselblad", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);
	rs_filetype_register_meta_loader(".erf", "Epson", tif_load_meta, 10, RS_LOADER_FLAGS_RAW);

	rs_filetype_register_meta_loader(".tiff", "Generic TIFF meta loader", tiff_load_meta, 10, RS_LOADER_FLAGS_RAW);
}

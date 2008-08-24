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

#include <gtk/gtk.h>
#include <math.h>
#include "rawstudio.h"
#include "rawfile.h"
#include "tiff-meta.h"
#include "adobe-coeff.h"
#include "rs-image.h"
#include "rs-color-transform.h"
#include "rs-utils.h"
#include "rs-photo.h"

/* It is required having some arbitrary maximum exposure time to prevent borked
 * shutter speed values being interpreted from the tiff.
 * 8h seems to be reasonable, even for astronomists with extra battery packs */
#define EXPO_TIME_MAXVAL (8*60.0*60.0)

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
};

static gfloat get_rational(RAWFILE *rawfile, guint offset);
inline static void read_ifd(RAWFILE *rawfile, guint offset, struct IFD *ifd);
static gboolean makernote_canon(RAWFILE *rawfile, guint offset, RS_METADATA *meta);
static gboolean makernote_minolta(RAWFILE *rawfile, guint offset, RS_METADATA *meta);
static gboolean makernote_nikon(RAWFILE *rawfile, guint offset, RS_METADATA *meta);
static gboolean makernote_olympus(RAWFILE *rawfile, guint base, guint offset, RS_METADATA *meta);
static gboolean makernote_olympus_camerasettings(RAWFILE *rawfile, guint base, guint offset, RS_METADATA *meta);
static gboolean makernote_olympus_imageprocessing(RAWFILE *rawfile, guint base, guint offset, RS_METADATA *meta);
static gboolean makernote_panasonic(RAWFILE *rawfile, guint offset, RS_METADATA *meta);
static gboolean makernote_pentax(RAWFILE *rawfile, guint offset, RS_METADATA *meta);
static gboolean ifd_reader(RAWFILE *rawfile, guint offset, RS_METADATA *meta);

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
	raw_get_uint(rawfile, offset, &uint1);
	raw_get_uint(rawfile, offset+4, &uint2);

	return ((gdouble) uint1) / ((gdouble) uint2);
}

inline static void
read_ifd(RAWFILE *rawfile, guint offset, struct IFD *ifd)
{
/*	guint size = 0; */
	guint uint1, uint2;

	raw_get_ushort(rawfile, offset, &ifd->tag);
	raw_get_ushort(rawfile, offset+2, &ifd->type);
	raw_get_uint(rawfile, offset+4, &ifd->count);
	raw_get_uint(rawfile, offset+8, &ifd->value_offset);

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
				break;
			case TIFF_FIELD_TYPE_SHORT:
				raw_get_ushort(rawfile, offset+8, &ifd->value_ushort);
				break;
			case TIFF_FIELD_TYPE_LONG:
				raw_get_uint(rawfile, offset+8, &ifd->value_uint);
				break;
			case TIFF_FIELD_TYPE_RATIONAL:
				raw_get_uint(rawfile, ifd->value_offset, &uint1);
				raw_get_uint(rawfile, ifd->value_offset+4, &uint2);
				ifd->value_rational = ((gdouble) uint1) / ((gdouble) uint2);
			default:
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
	printf("@ %d\n", ifd->offset);
	printf("\n");
}
#endif

static gboolean
makernote_canon(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	gushort number_of_entries = 0;
	gushort ushort_temp1;

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
						ifd.value_offset += 126;
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
makernote_minolta(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
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
makernote_nikon(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
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
	guint ver97 = 0;
	guchar buf97[324], ci, cj, ck;
	gboolean magic; /* Nikon's makernote type */

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
				raw_get_uint(rawfile, offset, &uint_temp1);

				/* This is fucked, where did these two magic constants come from? */
				raw_get_float(rawfile, uint_temp1, &float_temp1);
				raw_get_float(rawfile, uint_temp1+4, &float_temp2);
				meta->cam_mul[0] = (gdouble) (float_temp1/float_temp2)/2.218750;

				raw_get_float(rawfile, uint_temp1+8, &float_temp1);
				raw_get_float(rawfile, uint_temp1+12, &float_temp2);
				meta->cam_mul[2] = (gdouble) (float_temp1/float_temp2)/1.148438;

				raw_get_float(rawfile, uint_temp1+16, &float_temp1);
				raw_get_float(rawfile, uint_temp1+20, &float_temp2);
				meta->cam_mul[1] = (gdouble) (float_temp1/float_temp2);

				raw_get_float(rawfile, uint_temp1+24, &float_temp1);
				raw_get_float(rawfile, uint_temp1+28, &float_temp2);
				meta->cam_mul[3] = (gdouble) (float_temp1/float_temp2);
				rs_metadata_normalize_wb(meta);
				break;
			case 0x0011: /* NikonPreview */
				raw_get_uint(rawfile, offset, &uint_temp1);
				ifd_reader(rawfile, uint_temp1+base, meta);
				meta->thumbnail_start += base;
				break;
			case 0x0097: /* white balance */
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
			case 0x001d: /* serial */
				raw_get_uchar(rawfile, offset++, &char_tmp);
				while(char_tmp)
				{
					serial = serial*10 + (g_ascii_isdigit(char_tmp) ? char_tmp - '0' : char_tmp % 10);
					raw_get_uchar(rawfile, offset++, &char_tmp);
				}
				break;
			case 0x00a7: /* white balance */
				if (ver97 >> 8 == 2)
				{
					guchar ctmp[4];
					raw_get_uchar(rawfile, offset++, ctmp);
					raw_get_uchar(rawfile, offset++, ctmp+1);
					raw_get_uchar(rawfile, offset++, ctmp+2);
					raw_get_uchar(rawfile, offset, ctmp+3);
					ci = xlat[0][serial & 0xff];
					cj = xlat[1][ctmp[0]^ctmp[1]^ctmp[2]^ctmp[3]];
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
makernote_olympus_camerasettings(RAWFILE *rawfile, guint base, guint offset, RS_METADATA *meta)
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
makernote_olympus_imageprocessing(RAWFILE *rawfile, guint base, guint offset, RS_METADATA *meta)
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
makernote_olympus(RAWFILE *rawfile, guint base, guint offset, RS_METADATA *meta)
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
			case 0x1017: /* Red multiplier on many Olympus's (E-10, E-300, E-330, E-400, E-500) */
				raw_get_ushort(rawfile, offset, &ushort_temp1);
				meta->cam_mul[0] = (gdouble) ushort_temp1 / 256.0;
				break;
			case 0x1018: /* Blue multiplier on many Olympus's (E-10, E-300, E-330, E-400, E-500) */
				raw_get_ushort(rawfile, offset, &ushort_temp1);
				meta->cam_mul[2] = (gdouble) ushort_temp1 / 256.0;
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
makernote_panasonic(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
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
			case 0x002e: /* PreviewImage */
				meta->preview_start = ifd.value_offset;
				meta->preview_length = ifd.count;
				break;
			case 0x8769: /* ExifIFDPointer */
				exif_reader(rawfile, ifd.value_offset, meta);
				break;
		}
	}

	return TRUE;
}

static gboolean
makernote_pentax(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
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

gboolean
exif_reader(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
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
					meta->make_ascii = raw_strdup(rawfile, ifd.value_offset, ifd.count);
				break;
			case 0x0110: /* Model */
				if (!meta->model_ascii)
					meta->model_ascii = raw_strdup(rawfile, ifd.value_offset, ifd.count);
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
					default:
						break;
				}
				break;
		}
	}

	return TRUE;
}

static gboolean
ifd_reader(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	gushort number_of_entries = 0;
	gboolean is_preview = FALSE;
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
			case 0x00fe: /* Subfile type */
				is_preview = (ifd.value_offset & 1) != 0;
				break;
			case 0x0100: /* Image width */
				if (is_preview)
					meta->preview_width = ifd.value_ushort;
				break;
			case 0x0101: /* Image length (aka height in human language) */
				if (is_preview)
					meta->preview_height = ifd.value_ushort;
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
					meta->make_ascii = raw_strdup(rawfile, ifd.value_offset, ifd.count);
					if (raw_strcmp(rawfile, ifd.value_offset, "Canon", 5))
						meta->make = MAKE_CANON;
					else if (raw_strcmp(rawfile, ifd.value_offset, "KODAK", 5))
						meta->make = MAKE_KODAK;
					else if (raw_strcmp(rawfile, ifd.value_offset, "EASTMAN KODAK", 13))
						meta->make = MAKE_KODAK;
					else if (raw_strcmp(rawfile, ifd.value_offset, "Minolta", 7))
						meta->make = MAKE_MINOLTA;
					else if (raw_strcmp(rawfile, ifd.value_offset, "KONICA MINOLTA", 14))
						meta->make = MAKE_MINOLTA;
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
					else if (raw_strcmp(rawfile, ifd.value_offset, "SAMSUNG", 7))
						meta->make = MAKE_SAMSUNG;
					else if (raw_strcmp(rawfile, ifd.value_offset, "SONY", 4))
						meta->make = MAKE_SONY;
				}
				break;
			case 0x0110: /* Model */
				if (!meta->model_ascii)
					meta->model_ascii = raw_strdup(rawfile, ifd.value_offset, ifd.count);
				break;
			case 0x0111: /* StripOffsets */
				if (meta->preview_start==0 || is_preview)
					meta->preview_start = ifd.value_offset + raw_get_base(rawfile);
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
					meta->preview_length = ifd.value_offset;
				break;
			case 0x011c: /* Planar configuration */
				if (is_preview)
					meta->preview_planar_config = ifd.value_offset;
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
				if (ifd.type == TIFF_FIELD_TYPE_RATIONAL && ifd.count == 3)
				{
					meta->cam_mul[0] = 1.0/get_rational(rawfile, ifd.value_offset);
					meta->cam_mul[1] = 1.0/get_rational(rawfile, ifd.value_offset+8);
					meta->cam_mul[2] = 1.0/get_rational(rawfile, ifd.value_offset+16);
					meta->cam_mul[3] = meta->cam_mul[1];
				}
				break;
		}
	}

	return TRUE;
}

void
rs_tiff_load_meta_from_rawfile(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	guint next = 0;
	gushort ifd_num = 0;

	raw_init_file_tiff(rawfile, offset);

	offset = get_first_ifd_offset(rawfile);
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break; /* used for calculating next IFD */
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break; /* 2: offset+short(ifd_num), 12: length of ifd-entry */
		ifd_reader(rawfile, offset, meta);

		/* Hack to support a few cameras that embeds EXIF-info or Makernotes in IFD 0 */
		if (meta->make == MAKE_CANON && g_str_equal(meta->model_ascii, "EOS D2000C"))
			exif_reader(rawfile, offset, meta);
		if (meta->make == MAKE_KODAK && g_str_equal(meta->model_ascii, "DCS520C"))
			exif_reader(rawfile, offset, meta);
		if (meta->make == MAKE_KODAK && g_str_equal(meta->model_ascii, "DCS Pro 14N"))
			exif_reader(rawfile, offset, meta);
		if (meta->make == MAKE_PANASONIC)
			makernote_panasonic(rawfile, offset, meta);

		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

	rs_metadata_normalize_wb(meta);
	adobe_coeff_set(&meta->adobe_coeff, meta->make_ascii, meta->model_ascii);
}

void
rs_tiff_load_meta(const gchar *filename, RS_METADATA *meta)
{
	RAWFILE *rawfile;

	raw_init();

	if(!(rawfile = raw_open_file(filename)))
		return;

	rs_tiff_load_meta_from_rawfile(rawfile, 0, meta);

	raw_close_file(rawfile);
}

GdkPixbuf *
rs_tiff_load_thumb(const gchar *src)
{
	RAWFILE *rawfile;
	GdkPixbuf *pixbuf=NULL, *pixbuf2=NULL;
	guint start=0, length=0;
	RS_METADATA *meta = rs_metadata_new();

	if (!(rawfile = raw_open_file(src)))
		return(NULL);

	rs_tiff_load_meta_from_rawfile(rawfile, 0, meta);

	if ((meta->thumbnail_start>0) && (meta->thumbnail_length>0))
	{
		start = meta->thumbnail_start;
		length = meta->thumbnail_length;
	}

	else if ((meta->preview_start>0) && (meta->preview_length>0))
	{
		start = meta->preview_start;
		length = meta->preview_length;
	}

	/* Phase One doesn't set this */
	if (meta->make == MAKE_PHASEONE)
		meta->preview_planar_config = 1;

	if ((start>0) && (length>0) && (length<5000000))
	{
		if ((length==165888) && (meta->make == MAKE_CANON))
			pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+start, GDK_COLORSPACE_RGB, FALSE, 8, 288, 192, 288*3, NULL, NULL);
		else if (length==57600) /* Multiple Nikon, Pentax and Samsung cameras */
			pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+start, GDK_COLORSPACE_RGB, FALSE, 8, 160, 120, 160*3, NULL, NULL);
		else if (length==48672)
			pixbuf = gdk_pixbuf_new_from_data(raw_get_map(rawfile)+start, GDK_COLORSPACE_RGB, FALSE, 8, 156, 104, 156*3, NULL, NULL);
		else
			/* Many RAW files are based on TIFF and include the preview image
			 * as the "main" image in the TIFF so that "normal" image viewing
			 * programs can display at least the thumbnail. So we will
			 * check if the TIFF contains such a thumbnail in the simplest
			 * possible format (e.g. uncompressed R8G8B8) and use it
			 * if it is present.
			 */
			if (start == meta->preview_start && /* if we're using the preview image */
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
					raw_get_map(rawfile)+start, GDK_COLORSPACE_RGB, FALSE, 8,
					meta->preview_width, meta->preview_height,
					meta->preview_width * 3, NULL, NULL);
			else
				/* Try to guess file format based on contents (JPEG previews) */
			pixbuf = raw_get_pixbuf(rawfile, start, length);
	}
	/* Special case for Panasonic - most have no embedded thumbnail */
	else if (meta->make == MAKE_PANASONIC)
	{
		RS_PHOTO *photo;
		if ((photo = rs_photo_load_from_file(src, TRUE)))
		{
			gint c;
			gfloat pre_mul[4];
			RS_IMAGE16 *image;
			RSColorTransform *rct = rs_color_transform_new();
			image = rs_image16_transform(photo->input, NULL,
				NULL, NULL, photo->crop, 128, 128, TRUE, -1.0,
				photo->angle, photo->orientation, NULL);
			pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, image->w, image->h);

			for(c=0;c<4;c++)
				pre_mul[c] = (gfloat) meta->cam_mul[c];

			rs_color_transform_set_premul(rct, pre_mul);
			rs_color_transform_transform(rct, image->w, image->h, image->pixels,
					image->rowstride, gdk_pixbuf_get_pixels(pixbuf),
					gdk_pixbuf_get_rowstride(pixbuf));

			g_object_unref(photo);
			g_object_unref(image);
			g_object_unref(rct);
		}
	}

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
	}

	rs_metadata_free(meta);
	raw_close_file(rawfile);

	return(pixbuf);
}

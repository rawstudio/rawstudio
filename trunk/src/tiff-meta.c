/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
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

/* It is required having some arbitrary maximum exposure time to prevent borked
 * shutter speed values being interpreted from the tiff.
 * 8h seems to be reasonable, even for astronomists with extra battery packs */
#define EXPO_TIME_MAXVAL (8*60.0*60.0)

static void raw_nikon_makernote(RAWFILE *rawfile, guint offset, RS_METADATA *meta);

static void
raw_nikon_makernote(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
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
		return;
	if (number_of_entries>5000)
		return;
	offset += 2;

	save = offset;
	while(number_of_entries--)
	{
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
				raw_ifd_walker(rawfile, uint_temp1+base, meta);
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
	return;
}

gboolean
raw_ifd_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	gushort number_of_entries;
	gushort fieldtag=0;
/*	gushort fieldtype; */
	gushort ushort_temp1=0;
	guint valuecount;
	guint uint_temp1=0;
	gfloat float_temp1=0.0, float_temp2=0.0;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries)) return(FALSE);
	if (number_of_entries>5000)
		return(FALSE);
	offset += 2;

	while(number_of_entries--)
	{
		raw_get_ushort(rawfile, offset, &fieldtag);
/*		raw_get_ushort(rawfile, offset+2, &fieldtype); */
		raw_get_uint(rawfile, offset+4, &valuecount);
		offset += 8;
		switch(fieldtag)
		{
			case 0x0001: /* CanonCameraSettings */
				if (meta->make == MAKE_CANON)
				{
					gshort contrast;
					gshort saturation;
					gshort sharpness;
					gshort color_tone;
					raw_get_uint(rawfile, offset, &uint_temp1);
					raw_get_short(rawfile, uint_temp1+26, &contrast);
					raw_get_short(rawfile, uint_temp1+28, &saturation);
					raw_get_short(rawfile, uint_temp1+30, &sharpness);
					raw_get_short(rawfile, uint_temp1+84, &color_tone);
					switch(contrast)
					{
						case -2:
							meta->contrast = 0.8;
							break;
						case -1:
							meta->contrast = 0.9;
							break;
						case 0:
							meta->contrast = 1.0;
							break;
						case 1:
							meta->contrast = 1.1;
							break;
						case 2:
							meta->contrast = 1.2;
							break;
						default:
							meta->contrast = 1.0;
							break;
					}
					switch(saturation)
					{
						case -2:
							meta->saturation = 0.4;
							break;
						case -1:
							meta->saturation = 0.7;
							break;
						case 0:
							meta->saturation = 1.0;
							break;
						case 1:
							meta->saturation = 1.3;
							break;
						case 2:
							meta->saturation = 1.6;
							break;
						default:
							meta->saturation = 1.0;
							break;
					}
				}
				break;
			case 0x0002: /* CanonFocalLength */
				if (meta->make == MAKE_CANON)
				{
					raw_get_uint(rawfile, offset, &uint_temp1);
					raw_get_short(rawfile, uint_temp1+2, &meta->focallength);
				}
				break;
			case 0x0110: /* Model */
				raw_get_uint(rawfile, offset, &uint_temp1);
				if (!meta->model_ascii)
					meta->model_ascii = raw_strdup(rawfile, uint_temp1, 32);
				break;
			case 0x010f: /* Make */
				raw_get_uint(rawfile, offset, &uint_temp1);
				if (!meta->make_ascii)
					meta->make_ascii = raw_strdup(rawfile, uint_temp1, 32);
				if (raw_strcmp(rawfile, uint_temp1, "Canon", 5))
					meta->make = MAKE_CANON;
				else if (raw_strcmp(rawfile, uint_temp1, "NIKON", 5))
					meta->make = MAKE_NIKON;
				break;
			case 0x0088: /* Minolta */
			case 0x0111: /* PreviewImageStart */
				if (meta->preview_start==0)
				{
					raw_get_uint(rawfile, offset, &meta->preview_start);
					meta->preview_start += rawfile->base;
				}
				break;
			case 0x0081: /* Minolta DiMAGE 5 */
				if (meta->make == MAKE_MINOLTA)
				{
					raw_get_uint(rawfile, offset, &meta->thumbnail_start);
					meta->thumbnail_start += rawfile->base;
					meta->thumbnail_length = valuecount;
				}
				break;
			case 0x0089: /* Minolta */
			case 0x0117: /* PreviewImageLength */
				if (meta->preview_length==0)
					raw_get_uint(rawfile, offset, &meta->preview_length);
				break;
			case 0x0112: /* Orientation */
				raw_get_ushort(rawfile, offset, &meta->orientation);
				switch (meta->orientation)
				{
					case 6: meta->orientation = 90;
						break;
					case 8: meta->orientation = 270;
						break;
				}
				break;
			case 0x0201: /* jpeg start */
				raw_get_uint(rawfile, offset, &meta->thumbnail_start);
				meta->thumbnail_start += rawfile->base;
				break;
			case 0x0202: /* jpeg length */
				raw_get_uint(rawfile, offset, &meta->thumbnail_length);
				break;
			case 0x829A: /* Exposure time */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_float(rawfile, uint_temp1, &float_temp1);
				raw_get_float(rawfile, uint_temp1+4, &float_temp2);
				float_temp1 /= float_temp2;
				if (float_temp1 < EXPO_TIME_MAXVAL)
					meta->shutterspeed = 1/float_temp1;
				break;
			case 0x829D: /* FNumber */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_float(rawfile, uint_temp1, &float_temp1);
				raw_get_float(rawfile, uint_temp1+4, &float_temp2);
				meta->aperture = float_temp1/float_temp2;
				break;
			case 0x8827: /* ISOSpeedRatings */
				raw_get_ushort(rawfile, offset, &meta->iso);
				break;
			case 0x014a: /* SubIFD */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_uint(rawfile, uint_temp1, &uint_temp1);
				raw_ifd_walker(rawfile, uint_temp1, meta);
				break;
			case 0x927c: /* MakerNote */
				raw_get_uint(rawfile, offset, &uint_temp1);
				if (meta->make == MAKE_CANON)
					raw_ifd_walker(rawfile, uint_temp1, meta);
				else if (meta->make == MAKE_NIKON)
					raw_nikon_makernote(rawfile, uint_temp1, meta);
				else if (meta->make == MAKE_MINOLTA)
					raw_ifd_walker(rawfile, uint_temp1, meta);
				break;
			case 0x8769: /* ExifIFDPointer */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_ifd_walker(rawfile, uint_temp1, meta);
				break;
			case 0x9201: /* ShutterSpeedValue */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_float(rawfile, uint_temp1, &float_temp1);
				raw_get_float(rawfile, uint_temp1+4, &float_temp2);
				float_temp1 /= -float_temp2;
				if (float_temp1 < EXPO_TIME_MAXVAL)
					meta->shutterspeed = 1.0/pow(2.0, float_temp1);
				break;
			case 0x4001: /* white balance for Canon 20D & 350D */
				raw_get_uint(rawfile, offset, &uint_temp1);
				switch (valuecount)
				{
					case 582:
						uint_temp1 += 50;
						break;
					case 653:
						uint_temp1 += 68;
						break;
					case 796:
						uint_temp1 += 126;
						break;
				}
				/* RGGB-format! */
				raw_get_ushort(rawfile, uint_temp1, &ushort_temp1);
				meta->cam_mul[0] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, uint_temp1+2, &ushort_temp1);
				meta->cam_mul[1] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, uint_temp1+4, &ushort_temp1);
				meta->cam_mul[3] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, uint_temp1+6, &ushort_temp1);
				meta->cam_mul[2] = (gdouble) ushort_temp1;
				rs_metadata_normalize_wb(meta);
				break;
		}
		offset += 4;
	}
	return(TRUE);
}

void
rs_tiff_load_meta(const gchar *filename, RS_METADATA *meta)
{
	RAWFILE *rawfile;
	guint next, offset;
	gushort ifd_num;

	raw_init();

	meta->make = MAKE_UNKNOWN;
	meta->aperture = 0.0;
	meta->iso = 0;
	meta->shutterspeed = 0.0;
	meta->thumbnail_start = 0;
	meta->thumbnail_length = 0;
	meta->preview_start = 0;
	meta->preview_length = 0;
	meta->cam_mul[0] = -1.0;
	meta->cam_mul[1] = 1.0;
	meta->cam_mul[2] = 1.0;
	meta->cam_mul[3] = 1.0;

	if(!(rawfile = raw_open_file(filename)))
		return;
	raw_init_file_tiff(rawfile, 0);

	offset = rawfile->first_ifd_offset;
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break;
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break;
		raw_ifd_walker(rawfile, offset, meta);
		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

	adobe_coeff_set(meta);

	raw_close_file(rawfile);
}

GdkPixbuf *
rs_tiff_load_thumb(const gchar *src)
{
	RAWFILE *rawfile;
	guint next, offset;
	gushort ifd_num;
	GdkPixbuf *pixbuf=NULL, *pixbuf2=NULL;
	RS_METADATA *meta = NULL;
	gchar *thumbname;
	guint start=0, length=0;

	raw_init();

	thumbname = rs_thumb_get_name(src);
	if (thumbname)
	{
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
			if (pixbuf) return(pixbuf);
		}
	}

	if (!(rawfile = raw_open_file(src)))
		return(NULL);
	raw_init_file_tiff(rawfile, 0);

	meta = rs_metadata_new();
	offset = rawfile->first_ifd_offset;
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break;
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break;
		raw_ifd_walker(rawfile, offset, meta);
		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

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

	if ((start>0) && (length>0))
	{
		gdouble ratio;

		if ((length==165888) && (meta->make == MAKE_CANON))
			pixbuf = gdk_pixbuf_new_from_data(rawfile->map+start, GDK_COLORSPACE_RGB, FALSE, 8, 288, 192, 288*3, NULL, NULL);
		else if ((length==57600) && (meta->make == MAKE_NIKON))
			pixbuf = gdk_pixbuf_new_from_data(rawfile->map+start, GDK_COLORSPACE_RGB, FALSE, 8, 160, 120, 160*3, NULL, NULL);
		else
			pixbuf = raw_get_pixbuf(rawfile, start, length);
		if (pixbuf)
		{
			if ((gdk_pixbuf_get_width(pixbuf) == 160) && (gdk_pixbuf_get_height(pixbuf)==120))
			{
				pixbuf2 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 160, 106);
				gdk_pixbuf_copy_area(pixbuf, 0, 7, 160, 106, pixbuf2, 0, 0);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
			}
			ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
			if (ratio>1.0)
				pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
			else
				pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
			g_object_unref(pixbuf);
			pixbuf = pixbuf2;
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
			if (thumbname)
				gdk_pixbuf_save(pixbuf, thumbname, "png", NULL, NULL);
		}
	}

	rs_metadata_free(meta);
	raw_close_file(rawfile);

	return(pixbuf);
}

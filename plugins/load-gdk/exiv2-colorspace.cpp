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

#include <gtk/gtk.h>
#include <iostream>
#include <iomanip>
#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <assert.h>
#include "exiv2-colorspace.h"
#include <math.h>
#include <png.h>
#include <jpeglib.h>

#ifndef EXIV2_TEST_VERSION
# define EXIV2_TEST_VERSION(major,minor,patch) \
	( EXIV2_VERSION >= EXIV2_MAKE_VERSION((major),(minor),(patch)) )
#endif

#if EXIV2_TEST_VERSION(0,17,0)
#include <exiv2/convert.hpp>
#endif


extern "C" {

/** INTERFACE **/

using namespace Exiv2;
static void setup_read_icc_profile (j_decompress_ptr cinfo);
static boolean read_icc_profile (j_decompress_ptr cinfo, JOCTET **icc_data_ptr, unsigned int *icc_data_len);

struct error_mgr
{
	jpeg_error_mgr pub;
	jmp_buf buf;
};

static void 
my_error_exit (j_common_ptr cinfo)
{
	error_mgr * error = reinterpret_cast< error_mgr * >(cinfo->err);
	longjmp( error->buf, 1 );
}

RSColorSpace*
exiv2_get_colorspace(const gchar *filename, gfloat *gamma_guess)
{
	struct jpeg_decompress_struct info;
	jpeg_create_decompress(&info);
	error_mgr err;
	info.err = jpeg_std_error(( jpeg_error_mgr * ) &err);
	err.pub.error_exit = &my_error_exit;
	FILE* fp = fopen(filename, "rb");
	if (fp)
	{
		RSColorSpace* profile = NULL;
		
		if (setjmp(err.buf))
		{
			goto jpeg_fail;
		}
		jpeg_stdio_src(&info, fp);
		setup_read_icc_profile(&info);
		jpeg_read_header( &info, TRUE );

		/* extract ICC profile */
		JOCTET *iccBuf;
		unsigned int iccLen;
		if (read_icc_profile(&info, &iccBuf, &iccLen)) 
		{
			RSIccProfile *icc = rs_icc_profile_new_from_memory((gchar*)iccBuf, iccLen, TRUE);
			free(iccBuf);
			profile = rs_color_space_icc_new_from_icc(icc);
		}
jpeg_fail:
		jpeg_destroy_decompress(&info);
		fclose(fp);	
		if (profile)
			return profile;
	}
	jpeg_destroy_decompress(&info);

#ifdef PNG_iCCP_SUPPORTED
	if (1)
	{
		*gamma_guess = 2.2f;
		RSColorSpace* profile = NULL;
		const gchar *icc_profile_title;
		const gchar *icc_profile;
		guint icc_profile_size;
		png_structp png_ptr = png_create_read_struct(
				PNG_LIBPNG_VER_STRING,
				NULL,
				NULL,
				NULL);
		fp = fopen(filename, "rb");
		if (fp)
		{
			png_byte sig[8];
      
			if (fread(sig, 1, 8, fp) && 0 == fseek(fp,0,SEEK_SET) && png_check_sig(sig, 8))
			{
				png_init_io(png_ptr, fp);
				png_infop info_ptr = png_create_info_struct(png_ptr);
				if (info_ptr)
				{
					png_read_info( png_ptr, info_ptr );

					int compression_type;
					/* Extract embedded ICC profile */
					if (info_ptr->valid & PNG_INFO_iCCP)
					{
						png_uint_32 retval = png_get_iCCP (png_ptr, info_ptr,
													(png_charpp) &icc_profile_title, &compression_type,
													(png_charpp) &icc_profile, (png_uint_32*) &icc_profile_size);
						if (retval != 0)
						{
							RSIccProfile *icc = rs_icc_profile_new_from_memory((gchar*)icc_profile, icc_profile_size, TRUE);
							profile = rs_color_space_icc_new_from_icc(icc);
						}
					}
					gdouble gamma = 2.2;
					if (png_get_gAMA(png_ptr, info_ptr, &gamma))
						*gamma_guess = gamma;
				}
				png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			}
			fclose(fp);	
		}
		if (profile)
			return profile;
	}
#endif

	try {
		Image::AutoPtr img = ImageFactory::open(filename);
		img->readMetadata();
		ExifData &exifData = img->exifData();
		*gamma_guess = 2.2f;

#if EXIV2_TEST_VERSION(0,17,0)
		if (exifData.empty() && !img->xmpData().empty())
		{
			copyXmpToExif(img->xmpData(), exifData);
		}
#endif

		/* Parse Exif Data */
		if (!exifData.empty())
		{
			ExifData::const_iterator i;
			i = exifData.findKey(ExifKey("Exif.Image.BitsPerSample"));
			if (i != exifData.end())
				if (i->toLong() == 16)
					*gamma_guess = 1.0f;
			
			i = exifData.findKey(ExifKey("Exif.Photo.ColorSpace"));
			if (i != exifData.end())
			{
				if (i->toLong() == 1)
					return rs_color_space_new_singleton("RSSrgb");
			}

			/* Attempt to find ICC profile */
			i = exifData.findKey(ExifKey("Exif.Image.InterColorProfile"));
			if (i != exifData.end())
			{
				DataBuf buf(i->size());
				i->copy(buf.pData_, Exiv2::invalidByteOrder);
				if (buf.pData_ && buf.size_)
				{
					RSIccProfile *icc = rs_icc_profile_new_from_memory((gchar*)buf.pData_, buf.size_, TRUE);
					return rs_color_space_icc_new_from_icc(icc);
				}
			}

			i = exifData.findKey(ExifKey("Exif.Iop.InteroperabilityIndex"));
			if (i != exifData.end())
				if (0 == i->toString().compare("R03"))
				{
					*gamma_guess = 2.2f;
					return rs_color_space_new_singleton("RSAdobeRGB");
				}
		}
	} catch (Exiv2::Error& e) {
		g_debug("Exiv2 ColorSpace Loader:'%s", e.what());
	}
	g_debug("Exiv2 ColorSpace Loader: Could not determine colorspace, assume sRGB.");
	return rs_color_space_new_singleton("RSSrgb");
}


/*
 * This file provides code to read and write International Color Consortium
 * (ICC) device profiles embedded in JFIF JPEG image files.  The ICC has
 * defined a standard format for including such data in JPEG "APP2" markers.
 * The code given here does not know anything about the internal structure
 * of the ICC profile data; it just knows how to put the profile data into
 * a JPEG file being written, or get it back out when reading.
 *
 * This code depends on new features added to the IJG JPEG library as of
 * IJG release 6b; it will not compile or work with older IJG versions.
 *
 * NOTE: this code would need surgery to work on 16-bit-int machines
 * with ICC profiles exceeding 64K bytes in size.  If you need to do that,
 * change all the "unsigned int" variables to "INT32".  You'll also need
 * to find a malloc() replacement that can allocate more than 64K.
 */


/*
 * Since an ICC profile can be larger than the maximum size of a JPEG marker
 * (64K), we need provisions to split it into multiple markers.  The format
 * defined by the ICC specifies one or more APP2 markers containing the
 * following data:
 *      Identifying string      ASCII "ICC_PROFILE\0"  (12 bytes)
 *      Marker sequence number  1 for first APP2, 2 for next, etc (1 byte)
 *      Number of markers       Total number of APP2's used (1 byte)
 *      Profile data            (remainder of APP2 data)
 * Decoders should use the marker sequence numbers to reassemble the profile,
 * rather than assuming that the APP2 markers appear in the correct sequence.
 */

#define ICC_MARKER  (JPEG_APP0 + 2)     /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN  14            /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER  65533      /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER  (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)


/*
 * This routine writes the given ICC profile data into a JPEG file.
 * It *must* be called AFTER calling jpeg_start_compress() and BEFORE
 * the first call to jpeg_write_scanlines().
 * (This ordering ensures that the APP2 marker(s) will appear after the
 * SOI and JFIF or Adobe markers, but before all else.)
 */


/*
 * Prepare for reading an ICC profile
 */

static void
setup_read_icc_profile (j_decompress_ptr cinfo)
{
  /* Tell the library to keep any APP2 data it may find */
  jpeg_save_markers(cinfo, ICC_MARKER, 0xFFFF);
}


/*
 * Handy subroutine to test whether a saved marker is an ICC profile marker.
 */

static boolean
marker_is_icc (jpeg_saved_marker_ptr marker)
{
  return
    marker->marker == ICC_MARKER &&
    marker->data_length >= ICC_OVERHEAD_LEN &&
    /* verify the identifying string */
    GETJOCTET(marker->data[0]) == 0x49 &&
    GETJOCTET(marker->data[1]) == 0x43 &&
    GETJOCTET(marker->data[2]) == 0x43 &&
    GETJOCTET(marker->data[3]) == 0x5F &&
    GETJOCTET(marker->data[4]) == 0x50 &&
    GETJOCTET(marker->data[5]) == 0x52 &&
    GETJOCTET(marker->data[6]) == 0x4F &&
    GETJOCTET(marker->data[7]) == 0x46 &&
    GETJOCTET(marker->data[8]) == 0x49 &&
    GETJOCTET(marker->data[9]) == 0x4C &&
    GETJOCTET(marker->data[10]) == 0x45 &&
    GETJOCTET(marker->data[11]) == 0x0;
}


/*
 * See if there was an ICC profile in the JPEG file being read;
 * if so, reassemble and return the profile data.
 *
 * TRUE is returned if an ICC profile was found, FALSE if not.
 * If TRUE is returned, *icc_data_ptr is set to point to the
 * returned data, and *icc_data_len is set to its length.
 *
 * IMPORTANT: the data at **icc_data_ptr has been allocated with malloc()
 * and must be freed by the caller with free() when the caller no longer
 * needs it.  (Alternatively, we could write this routine to use the
 * IJG library's memory allocator, so that the data would be freed implicitly
 * at jpeg_finish_decompress() time.  But it seems likely that many apps
 * will prefer to have the data stick around after decompression finishes.)
 *
 * NOTE: if the file contains invalid ICC APP2 markers, we just silently
 * return FALSE.  You might want to issue an error message instead.
 */

static boolean
read_icc_profile (j_decompress_ptr cinfo,
                  JOCTET **icc_data_ptr,
                  unsigned int *icc_data_len)
{
  jpeg_saved_marker_ptr marker;
  int num_markers = 0;
  int seq_no;
  JOCTET *icc_data;
  unsigned int total_length;
#define MAX_SEQ_NO  255         /* sufficient since marker numbers are bytes */
  char marker_present[MAX_SEQ_NO+1];      /* 1 if marker found */
  unsigned int data_length[MAX_SEQ_NO+1]; /* size of profile data in marker */
  unsigned int data_offset[MAX_SEQ_NO+1]; /* offset for data in marker */

  *icc_data_ptr = NULL;         /* avoid confusion if FALSE return */
  *icc_data_len = 0;

  /* This first pass over the saved markers discovers whether there are
   * any ICC markers and verifies the consistency of the marker numbering.
   */

  for (seq_no = 1; seq_no <= MAX_SEQ_NO; seq_no++)
    marker_present[seq_no] = 0;

  for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
    if (marker_is_icc(marker)) {
      if (num_markers == 0)
        num_markers = GETJOCTET(marker->data[13]);
      else if (num_markers != GETJOCTET(marker->data[13]))
        return FALSE;           /* inconsistent num_markers fields */
      seq_no = GETJOCTET(marker->data[12]);
      if (seq_no <= 0 || seq_no > num_markers)
        return FALSE;           /* bogus sequence number */
      if (marker_present[seq_no])
        return FALSE;           /* duplicate sequence numbers */
      marker_present[seq_no] = 1;
      data_length[seq_no] = marker->data_length - ICC_OVERHEAD_LEN;
    }
  }

  if (num_markers == 0)
    return FALSE;

  /* Check for missing markers, count total space needed,
   * compute offset of each marker's part of the data.
   */

  total_length = 0;
  for (seq_no = 1; seq_no <= num_markers; seq_no++) {
    if (marker_present[seq_no] == 0)
      return FALSE;             /* missing sequence number */
    data_offset[seq_no] = total_length;
    total_length += data_length[seq_no];
  }

  if (total_length <= 0)
    return FALSE;               /* found only empty markers? */

  /* Allocate space for assembled data */
  icc_data = (JOCTET *) malloc(total_length * sizeof(JOCTET));
  if (icc_data == NULL)
    return FALSE;               /* oops, out of memory */

  /* and fill it in */
  for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
    if (marker_is_icc(marker)) {
      JOCTET FAR *src_ptr;
      JOCTET *dst_ptr;
      unsigned int length;
      seq_no = GETJOCTET(marker->data[12]);
      dst_ptr = icc_data + data_offset[seq_no];
      src_ptr = marker->data + ICC_OVERHEAD_LEN;
      length = data_length[seq_no];
      while (length--) {
        *dst_ptr++ = *src_ptr++;
      }
    }
  }

  *icc_data_ptr = icc_data;
  *icc_data_len = total_length;

  return TRUE;
}

/** END INTERFACE **/
} // extern "C"

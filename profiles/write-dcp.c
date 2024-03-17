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

#include <glib.h>
#include <tiffio.h>
#include <libxml/parser.h>
#include <libxml/encoding.h>

#define DCPTAG_PROFILE_NAME 0xc6f8
#define DCPTAG_PROFILE_EMBED_POLICY 0xc6fd
#define DCPTAG_PROFILE_COPYRIGHT 0xc6fe
#define DCPTAG_FORWARD_MATRIX1 0xc714
#define DCPTAG_FORWARD_MATRIX2 0xc715

/* Additional TIFF tags needed for writing DCP files */
static const TIFFFieldInfo xtiffFieldInfo[] = {
	{ DCPTAG_PROFILE_NAME,         -1, -1, TIFF_ASCII,     FIELD_CUSTOM,  TRUE, FALSE, "ProfileName" },
	{ DCPTAG_PROFILE_EMBED_POLICY,  1,  1, TIFF_LONG,      FIELD_CUSTOM, FALSE, FALSE, "ProfileEmbedPolicy" },
	{ DCPTAG_PROFILE_COPYRIGHT,    -1, -1, TIFF_ASCII,     FIELD_CUSTOM,  TRUE, FALSE, "ProfileCopyright" },
	{ DCPTAG_FORWARD_MATRIX1,      -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, FALSE,  TRUE, "ForwardMatrix1" },
	{ DCPTAG_FORWARD_MATRIX2,      -1, -1, TIFF_SRATIONAL, FIELD_CUSTOM, FALSE,  TRUE, "ForwardMatrix2" }
};

/**
 * A version of atof() that isn't locale specific
 * @note This doesn't do any error checking!
 * @param str A NULL terminated string representing a number
 * @return The number represented by str or 0.0 if str is NULL
 * Liftet from librawstudio/rs-utils.h
 */
static gdouble
rs_atof(const gchar *str)
{
	gdouble result = 0.0f;
	gdouble div = 1.0f;
	gboolean point_passed = FALSE;

	gchar *ptr = (gchar *) str;

	while(str && *ptr)
	{
		if (g_ascii_isdigit(*ptr))
		{
			result = result * 10.0f + g_ascii_digit_value(*ptr);
			if (point_passed)
				div *= 10.0f;
		}
		else if (*ptr == '-')
			div *= -1.0f;
		else if (g_ascii_ispunct(*ptr))
			point_passed = TRUE;
		ptr++;
	}

	return result / div;
}

/**
 * A version of atoi() that handles empty string and NULL arguments
 * @param str A NULL terminated string representing an integer
 * @return The integer represented by str or 0 if str is NULL or empty
 */
static gint
rs_atoi(const gchar *str)
{
	if (!str)
		return 0;

	if (!str[0])
		return 0;

	return atoi(str);
}

/**
 * Read a 3x3 matrix from a XML file
 * @param doc The XML document pointer
 * @param cur Current node pointer
 * @param A pointer to a gfloat[9] array
 * @return TRUE if successfull read, FALSE otherwise.
 */
static gboolean
read3x3matrix(xmlDocPtr doc, xmlNodePtr cur, gfloat *matrix)
{
	gint i;
	gint row, col;
	gint rows, cols;

	rows = rs_atoi(xmlGetProp(cur, BAD_CAST "Rows"));
	cols = rs_atoi(xmlGetProp(cur, BAD_CAST "Cols"));

	for(i=0; i<9; i++)
		matrix[i] = 0.0;

	i = 0x0;
	if (rows==3 && cols==3)
	{
		xmlNodePtr element = cur->xmlChildrenNode;

		while(element)
		{
			if (!xmlStrcmp(element->name, BAD_CAST "Element"))
			{
				row = rs_atoi(xmlGetProp(element, BAD_CAST "Row"));
				col = rs_atoi(xmlGetProp(element, BAD_CAST "Col"));
				matrix[col + 3*row] = rs_atof(xmlNodeListGetString(doc, element->xmlChildrenNode, 1));

				// Set a bit in i for each found member, we check before returning
				i |= 1<<(col + 3*row);
			}

			element = element->next;
		}
	}

	/* 0x1ff = 0b111111111 */

	if (i != 0x1ff && i != 0x0)
		g_warning("partial matrix read: %x", i);

	return (i == 0x1ff);
}

int
main(gint argc, gchar **argv)
{
	gfloat colormatrix[9];
	TIFF *tiff;
	xmlDocPtr doc;
	xmlNodePtr cur, element;
	xmlChar *val;
	gint row, col;
	gint rows, cols;

	if (argc != 3)
		return 1;

	tiff = TIFFOpen(argv[2], "w");
	if (!tiff)
		g_error("Failed to open \"%s\" for writing", argv[2]);

	/* Add our custom DCP fields to TIFF */
	TIFFMergeFieldInfo(tiff, xtiffFieldInfo, G_N_ELEMENTS(xtiffFieldInfo));

	doc = xmlParseFile(argv[1]);
	if (!doc)
		g_error("Failed to open \"%s\" for reading", argv[1]);

	cur = xmlDocGetRootElement(doc);

	if (xmlStrcmp(cur->name, BAD_CAST "dcpData"))
		g_error("Unexpected root tag [%s] in \"%s\", aborting", cur->name, argv[1]);

	cur = cur->xmlChildrenNode;

	while(cur)
	{
		if ((!xmlStrcmp(cur->name, BAD_CAST "text")))
		{
			/* ignore this */
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ProfileName")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (val && *val)
				TIFFSetField(tiff, DCPTAG_PROFILE_NAME, val);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "CalibrationIlluminant1")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (rs_atoi(val) > 0)
				TIFFSetField(tiff, TIFFTAG_CALIBRATIONILLUMINANT1, rs_atoi(val));
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "CalibrationIlluminant2")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (rs_atoi(val) > 0)
				TIFFSetField(tiff, TIFFTAG_CALIBRATIONILLUMINANT2, rs_atoi(val));
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ColorMatrix1")))
		{
			if (read3x3matrix(doc, cur, colormatrix))
				TIFFSetField(tiff, TIFFTAG_COLORMATRIX1, 9, colormatrix);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ColorMatrix2")))
		{
			if (read3x3matrix(doc, cur, colormatrix))
				TIFFSetField(tiff, TIFFTAG_COLORMATRIX2, 9, colormatrix);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ForwardMatrix1")))
		{
			if (read3x3matrix(doc, cur, colormatrix))
				TIFFSetField(tiff, DCPTAG_FORWARD_MATRIX1, 9, colormatrix);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ForwardMatrix2")))
		{
			if (read3x3matrix(doc, cur, colormatrix))
				TIFFSetField(tiff, DCPTAG_FORWARD_MATRIX2, 9, colormatrix);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ReductionMatrix1")))
		{
			if (read3x3matrix(doc, cur, colormatrix))
				TIFFSetField(tiff, TIFFTAG_REDUCTIONMATRIX1, 9, colormatrix);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "ReductionMatrix2")))
		{
			if (read3x3matrix(doc, cur, colormatrix))
				TIFFSetField(tiff, TIFFTAG_REDUCTIONMATRIX2, 9, colormatrix);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "EmbedPolicy")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (rs_atoi(val) > 0)
				TIFFSetField(tiff, DCPTAG_PROFILE_EMBED_POLICY, rs_atoi(val));
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "UniqueCameraModelRestriction")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (val)
				TIFFSetField(tiff, TIFFTAG_UNIQUECAMERAMODEL, val);
		}
		else if ((!xmlStrcmp(cur->name, BAD_CAST "Copyright")))
		{
			val = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (val)
				TIFFSetField(tiff, DCPTAG_PROFILE_COPYRIGHT, val);
		}
		else
			g_warning("[%s] tag in \"%s\" is unsupported", cur->name, argv[1]);

		cur = cur->next;
	}

	/* Close up shop */
	xmlFreeDoc(doc);
	TIFFClose(tiff);

	return 0;
}

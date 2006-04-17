#include <gtk/gtk.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"

gchar *
rs_cache_get_name(const gchar *src)
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
		out = g_string_append(out, ".cache.xml");
		ret = out->str;
		g_string_free(out, FALSE);
		g_free(dotdir);
	}
	g_free(filename);
	return(ret);
}

void
rs_cache_init()
{
	static gboolean init=FALSE;
	if (!init)
		LIBXML_TEST_VERSION /* yep, it should look like this */
	init = TRUE;
	return;
}

void
rs_cache_save(RS_BLOB *rs)
{
	gint id;
	xmlTextWriterPtr writer;
	gchar *cachename;

	if(!rs->in_use) return;
	cachename = rs_cache_get_name(rs->filename);
	writer = xmlNewTextWriterFilename(cachename, 0); /* fixme, check for errors */
	xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	xmlTextWriterStartElement(writer, BAD_CAST "rawstudio-cache");
	for(id=0;id<3;id++)
	{
		xmlTextWriterStartElement(writer, BAD_CAST "settings");
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "id", "%d", id);
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "exposure", "%f",
			GETVAL(rs->settings[id]->exposure));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "gamma", "%f",
			GETVAL(rs->settings[id]->gamma));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "saturation", "%f",
			GETVAL(rs->settings[id]->saturation));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "hue", "%f",
			GETVAL(rs->settings[id]->hue));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "contrast", "%f",
			GETVAL(rs->settings[id]->contrast));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "warmth", "%f",
			GETVAL(rs->settings[id]->warmth));
		xmlTextWriterWriteFormatElement(writer, BAD_CAST "tint", "%f",
			GETVAL(rs->settings[id]->tint));
		xmlTextWriterEndElement(writer);
	}
	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);
	g_free(cachename);
	return;
}

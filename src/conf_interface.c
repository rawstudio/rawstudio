/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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
#include <glib.h>
#include <stdio.h>
#include "matrix.h"
#include "rs-batch.h"
#include "rawstudio.h"
#include "conf_interface.h"

#ifdef G_OS_WIN32
 #define WITH_REGISTRY
 #undef WITH_GCONF
#endif

#ifdef WITH_GCONF
 #include <gconf/gconf.h>
 #define GCONF_PATH "/apps/rawstudio/"
static GConfEngine *
get_gconf_engine(void)
{
	/* Initialize *engine first time we're called. Otherwise just return the one we've got. */
	static GConfEngine *engine = NULL;
	if (!engine)
		engine = gconf_engine_get_default();
	return engine;
}
#else
 #ifdef G_OS_WIN32
  #include <windows.h>
  #define WITH_REGISTRY
  #define REGISTRY_KEY "Software\\Rawstudio"
 #endif
#endif

gboolean
rs_conf_get_boolean(const gchar *name, gboolean *boolean_value)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_engine_get(engine, fullname->str, NULL);
		if (gvalue)
		{
			if (gvalue->type == GCONF_VALUE_BOOL)
			{
				ret = TRUE;
				*boolean_value = gconf_value_get_bool(gvalue);
			}
			gconf_value_free(gvalue);
		}
	}
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

gboolean
rs_conf_set_boolean(const gchar *name, gboolean bool_value)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
		ret = gconf_engine_set_bool(engine, fullname->str, bool_value, NULL);
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

gchar *
rs_conf_get_string(const gchar *name)
{
	gchar *ret=NULL;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_engine_get(engine, fullname->str, NULL);
		if (gvalue)
		{
			if (gvalue->type == GCONF_VALUE_STRING)
				ret = g_strdup(gconf_value_get_string(gvalue));
			gconf_value_free(gvalue);
		}
	}
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
    HKEY hKey;
	char *szLwd;
    DWORD dwBufLen;
    LONG lRet;

	if (RegOpenKeyEx( HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_QUERY_VALUE, &hKey ) == ERROR_SUCCESS)
	{
	    lRet = RegQueryValueEx( hKey, CONF_LWD, NULL, NULL, NULL, &dwBufLen);
		if (dwBufLen > 0)
		{
	    	szLwd = g_malloc(dwBufLen);
	    	lRet = RegQueryValueEx( hKey, CONF_LWD, NULL, NULL, (LPBYTE) szLwd, &dwBufLen);
	    	RegCloseKey( hKey );
	    	if (lRet == ERROR_SUCCESS)
				ret = szLwd;
			else
				g_free(szLwd);
		}
	}
#endif
	return(ret);
}

gboolean
rs_conf_set_string(const gchar *name, const gchar *string_value)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_value_new(GCONF_VALUE_STRING);
		gconf_value_set_string(gvalue, string_value);
		ret = gconf_engine_set(engine, fullname->str, gvalue, NULL);
		gconf_value_free(gvalue);
	}
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
    HKEY hKey;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		if (RegSetValueEx(hKey, name, 0, REG_SZ, (LPBYTE) string_value, (DWORD) (lstrlen(string_value)+1))==ERROR_SUCCESS)
			ret = TRUE;
	}
    RegCloseKey(hKey);
#endif
	return(ret);
}

gboolean
rs_conf_get_integer(const gchar *name, gint *integer_value)
{
	gboolean ret=FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_engine_get(engine, fullname->str, NULL);
		if (gvalue)
		{
			if (gvalue->type == GCONF_VALUE_INT)
			{
				ret = TRUE;
				*integer_value = gconf_value_get_int(gvalue);
			}
			gconf_value_free(gvalue);
		}
	}
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

gboolean
rs_conf_set_integer(const gchar *name, const gint integer_value)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
		ret = gconf_engine_set_int(engine, fullname->str, integer_value, NULL);
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

gboolean
rs_conf_get_color(const gchar *name, GdkColor *color)
{
	gchar *str;
	gboolean ret = FALSE;

	str = rs_conf_get_string(name);
	if (str)
	{
		ret = gdk_color_parse(str, color);
		g_free(str);
	}
	return(ret);
}

gboolean
rs_conf_set_color(const gchar *name, GdkColor *color)
{
	gchar *str;
	gboolean ret = FALSE;

	str = g_strdup_printf ("#%02x%02x%02x", color->red>>8, color->green>>8, color->blue>>8);
	if (str)
	{
		ret = rs_conf_set_string(name, str);
		g_free(str);
	}
	return(ret);
}

gboolean
rs_conf_get_filetype(const gchar *name, gint *filetype)
{
	gchar *str;
	str = rs_conf_get_string(name);
	if (str)
	{
		if (0==g_ascii_strcasecmp(str, "jpeg"))
			*filetype = FILETYPE_JPEG;
		else if (0==g_ascii_strcasecmp(str, "jpg"))
			*filetype = FILETYPE_JPEG;
		else if (0==g_ascii_strcasecmp(str, "png"))
			*filetype = FILETYPE_PNG;
		g_free(str);
		return(TRUE);
	}
	else
		return(FALSE);
}

gboolean
rs_conf_set_filetype(const gchar *name, gint filetype)
{
	gchar *str = NULL;
	gboolean ret = FALSE;
	switch (filetype)
	{
		case FILETYPE_JPEG:
			str = "jpeg";
			break;
		case FILETYPE_PNG:
			str = "png";
			break;
	}
	if (str)
		ret = rs_conf_set_string(name, str);
	return(ret);
}

gboolean
rs_conf_get_double(const gchar *name, gdouble *float_value)
{
	gboolean ret=FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_engine_get(engine, fullname->str, NULL);
		if (gvalue)
		{
			if (gvalue->type == GCONF_VALUE_FLOAT)
			{
				ret = TRUE;
				*float_value = gconf_value_get_float(gvalue);
			}
			gconf_value_free(gvalue);
		}
	}
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

gboolean
rs_conf_set_double(const gchar *name, const gdouble float_value)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
		ret = gconf_engine_set_float(engine, fullname->str, float_value, NULL);
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

GSList *
rs_conf_get_list_string(const gchar *name)
{
#ifdef WITH_GCONF
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	GSList *list = NULL;

	g_string_append(fullname, name);
	if (engine)
		list = gconf_engine_get_list(engine, fullname->str, GCONF_VALUE_STRING, NULL);
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return list;
}

gboolean
rs_conf_set_list_string(const gchar *name, GSList *list)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);

	g_string_append(fullname, name);
	if (engine)
		ret = gconf_engine_set_list(engine, fullname->str, GCONF_VALUE_STRING, list, NULL);
	g_string_free(fullname, TRUE);
#endif
#ifdef WITH_REGISTRY
	/* FIXME: stub */
#endif
	return(ret);
}

gboolean
rs_conf_add_string_to_list_string(const gchar *name, gchar *value)
{
	gboolean ret = FALSE;

	GSList *newlist = NULL;
	GSList *oldlist = rs_conf_get_list_string(name);

	while (oldlist)
	{
		newlist = g_slist_append(newlist, oldlist->data);
		oldlist = oldlist->next;
	}

	newlist = g_slist_append(newlist, value);
	ret = rs_conf_set_list_string(name, newlist);

	return(ret);
}

gchar *
rs_conf_get_nth_string_from_list_string(const gchar *name, gint num)
{
	GSList *list = rs_conf_get_list_string(name);
	gint i;
	gchar *value = NULL;

	if (list)
	{
		for (i = 0; i < num; i++)
			list = list->next;
		if (list)
			value = (gchar *) list->data;
		else
			value = NULL;
	}
	return value;
}

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
#include <glib.h>
#include <stdio.h>
#include "rawstudio.h"
#include "conf_interface.h"
#include "lcms.h"

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
rs_conf_get_boolean_with_default(const gchar *name, gboolean *boolean_value, gboolean default_value)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	*boolean_value = default_value;
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
	    lRet = RegQueryValueEx( hKey, name, NULL, NULL, NULL, &dwBufLen);
		if (dwBufLen > 0)
		{
	    	szLwd = g_malloc(dwBufLen);
	    	lRet = RegQueryValueEx( hKey, name, NULL, NULL, (LPBYTE) szLwd, &dwBufLen);
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

static const struct {
	gint intent;
	gchar *name;
} intents[] =  {
	{INTENT_PERCEPTUAL, "perceptual"},
	{INTENT_RELATIVE_COLORIMETRIC, "relative"},
	{INTENT_SATURATION, "saturation"},
	{INTENT_ABSOLUTE_COLORIMETRIC, "absolute"},
	{-1, NULL}
};

gboolean
rs_conf_get_cms_intent(const gchar *name, gint *intent)
{
	gchar *str;
	gboolean ret = FALSE;
	gint n = 0;

	str = rs_conf_get_string(name);
	if (str)
	{
		while(intents[n].name)
		{
			if (g_ascii_strncasecmp(intents[n].name, str, 15) == 0)
			{
				*intent = intents[n].intent;
				ret = TRUE;
				break;
			}
			n++;
		}
		g_free(str);
	}

	return(ret);
}

gboolean
rs_conf_set_cms_intent(const gchar *name, gint *intent)
{
	gboolean ret = FALSE;
	gchar *str = NULL;
	gint n = 0;

	while(intents[n].name)
	{
		if (*intent == intents[n].intent)
		{
			str = intents[n].name;
			ret = TRUE;
			break;
		}
		n++;
	}

	if (str)
		ret = rs_conf_set_string(name, str);

	return(ret);
}

gchar *
rs_conf_get_cms_profile(gint type)
{
	gchar *ret = NULL;
	gint selected = 0;
	if (type == RS_CMS_PROFILE_IN)
	{
		rs_conf_get_integer(CONF_CMS_IN_PROFILE_SELECTED, &selected);
		if (selected > 0)
			ret = rs_conf_get_nth_string_from_list_string(CONF_CMS_IN_PROFILE_LIST, --selected);
	}
	else if (type == RS_CMS_PROFILE_DISPLAY)
	{
		rs_conf_get_integer(CONF_CMS_DI_PROFILE_SELECTED, &selected);
		if (selected > 0)
			ret = rs_conf_get_nth_string_from_list_string(CONF_CMS_DI_PROFILE_LIST, --selected);
	} 
	else if (type == RS_CMS_PROFILE_EXPORT)
	{
		rs_conf_get_integer(CONF_CMS_EX_PROFILE_SELECTED, &selected);
		if (selected > 0)
			ret = rs_conf_get_nth_string_from_list_string(CONF_CMS_EX_PROFILE_LIST, --selected);
	}

	return ret;
}

gboolean
rs_conf_get_filetype(const gchar *name, RS_FILETYPE **target)
{
	extern RS_FILETYPE *filetypes;
	RS_FILETYPE *filetype = filetypes, *def=NULL;
	gchar *str;
	str = rs_conf_get_string(name);
	while(filetype)
	{
		if (0==g_ascii_strcasecmp(filetype->id, DEFAULT_CONF_EXPORT_FILETYPE))
			def = filetype;
		if (str)
			if (0==g_ascii_strcasecmp(filetype->id, str))
			{
				*target = filetype;
				g_free(str);
				return(TRUE);
			}
		filetype = filetype->next;
	}
	if (str)
		g_free(str);
	*target = def;
	return(FALSE);
}

gboolean
rs_conf_set_filetype(const gchar *name, const RS_FILETYPE *filetype)
{
	gchar *str;
	gboolean ret = FALSE;
	if (filetype)
	{
		str = filetype->id;
		if (str)
			ret = rs_conf_set_string(name, str);
	}
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
	GSList *list = NULL;
#ifdef WITH_GCONF
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);

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

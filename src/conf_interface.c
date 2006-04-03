#include <glib.h>
#include <stdio.h>
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
rs_conf_get_boolean(const char *name)
{
	gboolean ret = FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_engine_get(engine, name, NULL);
		if (gvalue)
		{
			if (gvalue->type == GCONF_VALUE_BOOL)
				ret = gconf_value_get_bool(gvalue);
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
rs_conf_set_boolean(const char *name, gboolean bool_value)
{
	gint ret = FALSE;
#ifdef WITH_GCONF
	GConfValue *gvalue;
	GConfEngine *engine = get_gconf_engine();
	GString *fullname = g_string_new(GCONF_PATH);
	g_string_append(fullname, name);
	if (engine)
	{
		gvalue = gconf_value_new(GCONF_VALUE_STRING);
		gconf_value_set_bool(gvalue, bool_value);
		ret = gconf_engine_set(engine, name, gvalue, NULL);
		gconf_value_free(gvalue);
	}
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

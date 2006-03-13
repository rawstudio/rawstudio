#include <glib.h>
#include <stdio.h>
#ifndef G_OS_WIN32
#include <gconf/gconf.h>

#define GCONF_LWD_PATH "/apps/rawstudio/last_working_directory"
#else
#include <windows.h>
#define WIN_REG_KEY "Software\\Rawstudio"
#define WIN_REG_VALUE_LWD "LastWorkingDir"
#define BUFFERSIZE 150
#endif

#ifndef G_OS_WIN32
static GConfEngine *get_gconf_engine(void) {
	/* Initialize *engine first time we're called. Otherwise just return the one we've got. */
	static GConfEngine *engine = NULL;
	if (!engine)
		engine = gconf_engine_get_default();
	return engine;
}

void rs_set_last_working_directory(const char *lwd)
{
	GConfValue *gconf_lwd = gconf_value_new(GCONF_VALUE_STRING);
	gconf_value_set_string(gconf_lwd, lwd);
	gconf_engine_set(get_gconf_engine(), GCONF_LWD_PATH, gconf_lwd, NULL); 
}

const gchar *rs_get_last_working_directory(void)
{
	GConfValue *gconf_lwd = gconf_engine_get(get_gconf_engine(), GCONF_LWD_PATH, NULL);
	const gchar *lwd;

	if (!gconf_lwd)
		return NULL;

	if (gconf_lwd->type != GCONF_VALUE_STRING) {
		g_free(gconf_lwd);
		return NULL;
	}

	lwd = gconf_value_get_string(gconf_lwd);
	g_free(gconf_lwd);
	return(lwd);
}

#else

void rs_set_last_working_directory(const char *lwd)
{
    HKEY hKey;

	if (RegCreateKeyEx( HKEY_CURRENT_USER, WIN_REG_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
		return;
		
	RegSetValueEx(hKey, WIN_REG_VALUE_LWD, 0, REG_SZ, (LPBYTE) lwd, (DWORD) (lstrlen(lwd)+1));
    RegCloseKey(hKey);

}

const gchar *rs_get_last_working_directory(void)
{
    HKEY hKey;
	char *szLwd;
    DWORD dwBufLen;
    LONG lRet;

	if (RegOpenKeyEx( HKEY_CURRENT_USER, WIN_REG_KEY, 0, KEY_QUERY_VALUE, &hKey ) != ERROR_SUCCESS)
        return NULL;

    lRet = RegQueryValueEx( hKey, WIN_REG_VALUE_LWD, NULL, NULL, NULL, &dwBufLen);
    szLwd = g_malloc(dwBufLen);
    lRet = RegQueryValueEx( hKey, WIN_REG_VALUE_LWD, NULL, NULL, (LPBYTE) szLwd, &dwBufLen);
    RegCloseKey( hKey );
    if (lRet != ERROR_SUCCESS ) {
        return NULL;
	}
    
    return szLwd;
}

#endif

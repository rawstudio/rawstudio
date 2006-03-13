#include <gconf/gconf.h>
#include <glib.h>
#include <stdio.h>

#define GCONF_LWD_PATH "/apps/rawstudio/last_working_directory"

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

#define CONF_LWD "last_working_directory"

// get the last working directory from gconf
void rs_set_last_working_directory(const char *lwd);

// save the current working directory to gconf
gchar *rs_get_last_working_directory(void);

gchar *rs_conf_get_string(const gchar *path);
gboolean rs_conf_set_string(const gchar *path, const gchar *string);
gboolean rs_conf_get_color(const gchar *name, GdkColor *color);
gboolean rs_conf_set_color(const gchar *name, GdkColor *color);

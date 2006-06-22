#define CONF_LWD "last_working_directory"
#define CONF_PREBGCOLOR "preview_background_color"
#define CONF_HISTHEIGHT "histogram_height"
#define CONF_GAMMAVALUE "gamma"
#define CONF_PASTE_MASK "paste_mask"

// get the last working directory from gconf
void rs_set_last_working_directory(const char *lwd);

// save the current working directory to gconf
gchar *rs_get_last_working_directory(void);

gchar *rs_conf_get_string(const gchar *path);
gboolean rs_conf_set_string(const gchar *path, const gchar *string);
gboolean rs_conf_get_integer(const gchar *name, gint *integer_value);
gboolean rs_conf_set_integer(const gchar *name, const gint integer_value);
gboolean rs_conf_get_color(const gchar *name, GdkColor *color);
gboolean rs_conf_set_color(const gchar *name, GdkColor *color);
gboolean rs_conf_get_double(const gchar *name, gdouble *float_value);
gboolean rs_conf_set_double(const gchar *name, const gdouble float_value);

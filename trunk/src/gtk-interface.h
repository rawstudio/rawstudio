enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN,
	FULLNAME_COLUMN,
	PRIORITY_COLUMN,
	NUM_COLUMNS
};

enum {
	PRIO_U = 0,
	PRIO_D = 51,
	PRIO_1 = 1,
	PRIO_2 = 2,
	PRIO_3 = 3,
	PRIO_ALL = 255
};

enum {
	OP_NONE = 0,
	OP_MOVE
};

#define GUI_CATCHUP() while (gtk_events_pending()) gtk_main_iteration()

void gui_status_push(const char *text);
void gui_status_pop();
void update_histogram(RS_BLOB *rs);
gboolean update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs);
gint fill_model_compare_func (GtkTreeModel *model, GtkTreeIter *tia,
	GtkTreeIter *tib, gpointer userdata);
void fill_model(GtkListStore *store, const char *path);
void icon_activated_helper(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
void icon_activated(GtkIconView *iconview, RS_BLOB *rs);
GtkWidget *make_iconbox(RS_BLOB *rs, GtkListStore *store);
void gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs);
gboolean gui_fullscreen_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox);
void gui_menu_setprio_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_widget_visible_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_iconbar_previous_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_iconbar_next_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_preference_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_about();
void gui_dialog_simple(gchar *title, gchar *message);
void gui_menu_auto_wb_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
GtkWidget *gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkListStore *store, GtkWidget *iconbox, GtkWidget *toolbox);
GtkWidget *gui_window_make();
int gui_init(int argc, char **argv);

#define PITCH(width) ((((width)+31)/32)*32)

enum {
	FILE_UNKN,
	FILE_RAW
};

typedef struct {
	gboolean in_use;
	guint w;
	guint h;
	gint pitch;
	guint channels;
	gushort *pixels;
	dcraw_data *raw;
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *scale;
	guint vis_scale;
	guint vis_w;
	guint vis_h;
	guint vis_pitch;
	gushort *vis_pixels;
	guint vis_histogram[3][256];
	GtkWidget *histogram;
	guint hist_w;
	guint hist_h;
	GdkPixbuf *vis_pixbuf;
	GtkWidget *vis_image;
	GtkWidget *files; /* ugly hack */
} RS_BLOB;

void update_gammatable(const double g);
void update_previewtable(RS_BLOB *rs, const double gamma, const double contrast);
void rs_debug(RS_BLOB *rs);
void update_scaled(RS_BLOB *rs);
void update_preview(RS_BLOB *rs);
void rs_reset(RS_BLOB *rs);
void rs_free_raw(RS_BLOB *rs);
void rs_free(RS_BLOB *rs);
void rs_alloc(RS_BLOB *rs, const guint width, const guint height, const guint channels);
RS_BLOB *rs_new();
void rs_load_raw_from_memory(RS_BLOB *rs);
void rs_load_raw_from_file(RS_BLOB *rs, const gchar *filename);

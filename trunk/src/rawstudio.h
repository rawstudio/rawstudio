#define PITCH(width) ((((width)+31)/32)*32)

enum {
	FILE_UNKN,
	FILE_RAW
};

typedef struct {
	guint w;
	guint h;
	guint pitch;
	guint channels;
	gushort *pixels;
} RS_IMAGE;

typedef struct {
	gboolean in_use;
	RS_IMAGE *input;
	RS_IMAGE *preview;
	dcraw_data *raw;
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *scale;
	guint preview_scale;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	guint histogram_w;
	guint histogram_h;
	GdkPixbuf *preview_pixbuf;
	GtkImage *preview_image;
	GtkFileSelection *files; /* ugly hack */
} RS_BLOB;

void update_gammatable(const double g);
void update_previewtable(RS_BLOB *rs, const double gamma, const double contrast);
void rs_debug(RS_BLOB *rs);
void update_scaled(RS_BLOB *rs);
void update_preview(RS_BLOB *rs);
void rs_reset(RS_BLOB *rs);
void rs_free_raw(RS_BLOB *rs);
void rs_free(RS_BLOB *rs);
RS_IMAGE *rs_image_new(const guint width, const guint height, const guint channels);
void rs_image_free(RS_IMAGE *rsi);
RS_BLOB *rs_new();
void rs_load_raw_from_memory(RS_BLOB *rs);
void rs_load_raw_from_file(RS_BLOB *rs, const gchar *filename);

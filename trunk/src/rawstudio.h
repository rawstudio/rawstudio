#define PITCH(width) ((((width)+31)/32)*32)

#define SWAP( a, b ) a ^= b ^= a ^= b

#define DOTDIR ".rawstudio"

enum {
	FILE_UNKN,
	FILE_RAW
};

typedef struct {
	guint w;
	guint h;
	gint pitch;
	guint channels;
	guchar *pixels;
} RS_IMAGE8;

typedef struct {
	guint w;
	guint h;
	gint pitch;
	guint channels;
	gushort *pixels;
} RS_IMAGE16;

typedef struct {
	gboolean in_use;
	const gchar *filename;
	RS_IMAGE16 *input;
	RS_IMAGE16 *preview;
	RS_IMAGE8 *preview8;
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
void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
void rs_image16_mirror(RS_IMAGE16 *rsi);
void rs_image16_flip(RS_IMAGE16 *rsi);
RS_IMAGE16 *rs_image16_new(const guint width, const guint height, const guint channels);
void rs_image16_free(RS_IMAGE16 *rsi);
RS_IMAGE8 *rs_image8_new(const guint width, const guint height, const guint channels);
void rs_image8_free(RS_IMAGE8 *rsi);
RS_BLOB *rs_new();
void rs_load_raw_from_memory(RS_BLOB *rs);
void rs_load_raw_from_file(RS_BLOB *rs, const gchar *filename);

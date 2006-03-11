#define PITCH(width) ((((width)+31)/32)*32)

#define SWAP( a, b ) a ^= b ^= a ^= b

#define DOTDIR ".rawstudio"

#define DIRECTION_RESET(direction) direction = 0
#define DIRECTION_90(direction) direction = (direction&4) | ((direction+1)&3)
#define DIRECTION_180(direction) direction = (direction^2)
#define DIRECTION_270(direction) direction = (direction&4) | ((direction+3)&3)
#define DIRECTION_FLIP(direction) direction = (direction^4)
#define DIRECTION_MIRROR(direction) direction = ((direction&4)^4) | ((direction+2)&3)

enum {
	FILE_UNKN,
	FILE_RAW
};

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint direction;
	guchar *pixels;
} RS_IMAGE8;

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint direction;
	gushort *pixels;
} RS_IMAGE16;

typedef struct {
	gboolean in_use;
	const gchar *filename;
	RS_IMAGE16 *input;
	RS_IMAGE16 *scaled;
	RS_IMAGE8 *preview;
	dcraw_data *raw;
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *scale;
	guint direction;
	guint flip;
	guint preview_scale;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	guint histogram_w;
	guint histogram_h;
	GtkWidget *preview_drawingarea;
	GtkFileSelection *files; /* ugly hack */
} RS_BLOB;

void update_gammatable(const double g);
void update_previewtable(RS_BLOB *rs, const double gamma, const double contrast);
void print_debug_line(const char *format, const gint value, const gboolean a);
void rs_image16_debug(RS_IMAGE16 *rsi);
void rs_debug(RS_BLOB *rs);
void update_scaled(RS_BLOB *rs);
void update_preview(RS_BLOB *rs);
void rs_reset(RS_BLOB *rs);
void rs_free_raw(RS_BLOB *rs);
void rs_free(RS_BLOB *rs);
void rs_image16_direction(RS_IMAGE16 *rsi, gint direction);
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

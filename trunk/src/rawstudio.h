#define PITCH(width) ((((width)+31)/32)*32)

#define SWAP( a, b ) a ^= b ^= a ^= b

#define DOTDIR ".rawstudio"
#define HISTOGRAM_DATASET_WIDTH (250)

#define DIRECTION_RESET(direction) direction = 0
#define DIRECTION_90(direction) direction = (direction&4) | ((direction+1)&3)
#define DIRECTION_180(direction) direction = (direction^2)
#define DIRECTION_270(direction) direction = (direction&4) | ((direction+3)&3)
#define DIRECTION_FLIP(direction) direction = (direction^4)
#define DIRECTION_MIRROR(direction) direction = ((direction&4)^4) | ((direction+2)&3)

#define GETVAL(adjustment) \
	gtk_adjustment_get_value((GtkAdjustment *) adjustment)
#define SETVAL(adjustment, value) \
	gtk_adjustment_set_value((GtkAdjustment *) adjustment, value)

enum {
_MMX = 1,
_SSE = 2,
_CMOV = 4,
_3DNOW = 8
};

#if __GNUC__ >= 3
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define align(x) __attribute__ ((aligned (x)))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define align(x)
#endif

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in CHARS */
	guint direction;
	guchar *pixels;
} RS_IMAGE8;

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in SHORTS */
	guint direction;
	gushort *pixels;
} RS_IMAGE16;

typedef struct {
	gint x1;
	gint y1;
	gint x2;
	gint y2;
} RS_RECT;

typedef struct {
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *warmth;
	GtkObject *tint;
} RS_SETTINGS;

typedef struct _metadata {
	gushort orientation;
	gfloat aperture;
	gushort iso;
	gfloat shutterspeed;
	guint jpeg_start;
	guint jpeg_length;
} RS_METADATA;

typedef struct {
	gboolean in_use;
	const gchar *filename;
	RS_IMAGE16 *input;
	RS_IMAGE16 *scaled;
	RS_IMAGE8 *preview;
	RS_SETTINGS *settings[3];
	gint current_setting;
	gint priority;
	dcraw_data *raw;
	GtkObject *scale;
	gfloat pre_mul[4];
	guint direction;
	guint preview_scale;
	RS_RECT *preview_exposed;
	RS_IMAGE16 *histogram_dataset;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	GtkWidget *preview_drawingarea;
	gboolean preview_idle_render;
	gboolean preview_done;
	GdkPixmap *preview_backing;
	RS_METADATA *metadata;
	gint preview_idle_render_lastrow;
	RS_MATRIX4Int mati;
	RS_MATRIX4 mat;
} RS_BLOB;

typedef struct {
	const gchar *ext;
	void (*load)(RS_BLOB *, const gchar *);
	GdkPixbuf *(*thumb)(const gchar *);
	void (*load_meta)(const gchar *, RS_METADATA *);
} RS_FILETYPE;

void update_gammatable(const double g);
void update_previewtable(RS_BLOB *rs, const double gamma, const double contrast);
void print_debug_line(const char *format, const gint value, const gboolean a);
void rs_image16_debug(RS_IMAGE16 *rsi);
void rs_debug(RS_BLOB *rs);
void update_scaled(RS_BLOB *rs);
void update_preview(RS_BLOB *rs);
void update_preview_region(RS_BLOB *rs, gint rx, gint ry, gint rw, gint rh);
gboolean rs_render_idle(RS_BLOB *rs);
inline void rs_render(RS_BLOB *rs, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride);
inline void rs_histogram_update_table(RS_MATRIX4Int mati, RS_IMAGE16 *input, guint *table);
void rs_reset(RS_BLOB *rs);
void rs_free_raw(RS_BLOB *rs);
void rs_free(RS_BLOB *rs);
void rs_image16_direction(RS_IMAGE16 *rsi, gint direction);
void rs_image16_rotate(RS_IMAGE16 *rsi, gint quarterturns);
void rs_image16_mirror(RS_IMAGE16 *rsi);
void rs_image16_flip(RS_IMAGE16 *rsi);
RS_IMAGE16 *rs_image16_scale(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale);
RS_IMAGE16 *rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image16_free(RS_IMAGE16 *rsi);
RS_IMAGE8 *rs_image8_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image8_free(RS_IMAGE8 *rsi);
void rs_settings_reset(RS_SETTINGS *rss);
RS_SETTINGS *rs_settings_new();
void rs_settings_free(RS_SETTINGS *rss);
RS_BLOB *rs_new();
void rs_load_raw_from_memory(RS_BLOB *rs);
void rs_load_raw_from_file(RS_BLOB *rs, const gchar *filename);
RS_FILETYPE *rs_filetype_get(const gchar *filename, gboolean load);
void rs_load_gdk(RS_BLOB *rs, const gchar *filename);
gchar *rs_dotdir_get(const gchar *filename);
gchar *rs_thumb_get_name(const gchar *src);
GdkPixbuf *rs_thumb_grt(const gchar *src);
GdkPixbuf *rs_thumb_gdk(const gchar *src);
void rs_set_warmth_from_color(RS_BLOB *rs, gint x, gint y);
guint _have_mmx(void);

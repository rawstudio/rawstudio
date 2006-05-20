#define ENDIANSWAP4(a) (((a) & 0x000000FF) << 24 | ((a) & 0x0000FF00) << 8 | ((a) & 0x00FF0000) >> 8) | (((a) & 0xFF000000) >> 24)
#define ENDIANSWAP2(a) (((a) & 0x00FF) << 8) | (((a) & 0xFF00) >> 8)

void rs_tiff_load_meta(const gchar *filename, RS_METADATA *meta);

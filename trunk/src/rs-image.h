RS_IMAGE16 *rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image16_free(RS_IMAGE16 *rsi);
RS_IMAGE8 *rs_image8_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image8_free(RS_IMAGE8 *rsi);
void rs_image16_orientation(RS_IMAGE16 *rsi, gint orientation);
RS_IMAGE16 *rs_image16_scale(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale);
RS_IMAGE16 *rs_image16_copy(RS_IMAGE16 *rsi);

/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * rs-image.h - image functions
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

RS_IMAGE16 *rs_image16_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image16_free(RS_IMAGE16 *rsi);
RS_IMAGE8 *rs_image8_new(const guint width, const guint height, const guint channels, const guint pixelsize);
void rs_image8_free(RS_IMAGE8 *rsi);
void rs_image16_orientation(RS_IMAGE16 *rsi, gint orientation);
RS_IMAGE16 *rs_image16_scale(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble scale);
RS_IMAGE16 *rs_image16_copy(RS_IMAGE16 *rsi);

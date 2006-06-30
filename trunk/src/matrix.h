/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * matrix.h - matrix functions
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

#define MATRIX_RESOLUTION (8) /* defined in bits! */

typedef struct {double coeff[4][4]; } RS_MATRIX4;
typedef struct {int coeff[4][4]; } RS_MATRIX4Int;

void printmat(RS_MATRIX4 *mat);
void matrix4_identity (RS_MATRIX4 *matrix);
void matrix4_to_matrix4int(RS_MATRIX4 *matrix, RS_MATRIX4Int *matrixi);
void matrix4_color_saturate(RS_MATRIX4 *mat, double sat);
void matrix4_color_hue(RS_MATRIX4 *mat, double rot);
void matrix4_color_exposure(RS_MATRIX4 *mat, double exp);
void matrix4_color_mixer(RS_MATRIX4 *mat, double r, double g, double b);

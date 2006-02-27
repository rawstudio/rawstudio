#define MATRIX_RESOLUTION (8) /* defined in bits! */

typedef struct {double coeff[4][4]; } RS_MATRIX4;
typedef struct {int coeff[4][4]; } RS_MATRIX4Int;

void printmat(RS_MATRIX4 *mat);
void matrix4_identity (RS_MATRIX4 *matrix);
void matrix4_mult(const RS_MATRIX4 *matrix1, RS_MATRIX4 *matrix2);
void matrix4_zshear (RS_MATRIX4 *matrix, double dx, double dy);
void matrix4_to_matrix4int(RS_MATRIX4 *matrix, RS_MATRIX4Int *matrixi);
void matrix4_xrotate(RS_MATRIX4 *matrix, double rs, double rc);
void matrix4_yrotate(RS_MATRIX4 *matrix, double rs, double rc);
void matrix4_zrotate(RS_MATRIX4 *matrix, double rs, double rc);
void matrix4_color_saturate(RS_MATRIX4 *mat, double sat);
void xformpnt(RS_MATRIX4 *matrix, double x, double y, double z, double *tx, double *ty, double *tz);
void matrix4_color_hue(RS_MATRIX4 *mat, double rot);
void matrix4_color_exposure(RS_MATRIX4 *mat, double exp);
void matrix4_color_mixer(RS_MATRIX4 *mat, double r, double g, double b);

/* luminance weight, notice that these is used for linear data */

#define RLUM (0.3086)
#define GLUM (0.6094)
#define BLUM (0.0820)

#define CLAMP65535(a) MAX(MIN(65535,a),0)
#define CLAMP255(a) MAX(MIN(255,a),0)

enum {
	R=0,
	G,
	B,
	G2
};

/* SSE2 Polynomial pow function from Mesa3d (MIT License) */

#define EXP_POLY_DEGREE 3

#define POLY0(x, c0) _mm_load_ps(c0)
#define POLY1(x, c0, c1) _mm_add_ps(_mm_mul_ps(POLY0(x, c1), x), _mm_load_ps(c0))
#define POLY2(x, c0, c1, c2) _mm_add_ps(_mm_mul_ps(POLY1(x, c1, c2), x), _mm_load_ps(c0))
#define POLY3(x, c0, c1, c2, c3) _mm_add_ps(_mm_mul_ps(POLY2(x, c1, c2, c3), x), _mm_load_ps(c0))
#define POLY4(x, c0, c1, c2, c3, c4) _mm_add_ps(_mm_mul_ps(POLY3(x, c1, c2, c3, c4), x), _mm_load_ps(c0))
#define POLY5(x, c0, c1, c2, c3, c4, c5) _mm_add_ps(_mm_mul_ps(POLY4(x, c1, c2, c3, c4, c5), x), _mm_load_ps(c0))

static const gfloat exp_p5_0[4] __attribute__ ((aligned (16))) = {9.9999994e-1f, 9.9999994e-1f, 9.9999994e-1f, 9.9999994e-1f};
static const gfloat exp_p5_1[4] __attribute__ ((aligned (16))) = {6.9315308e-1f, 6.9315308e-1f, 6.9315308e-1f, 6.9315308e-1f};
static const gfloat exp_p5_2[4] __attribute__ ((aligned (16))) = {2.4015361e-1f,  2.4015361e-1f,  2.4015361e-1f,  2.4015361e-1f};
static const gfloat exp_p5_3[4] __attribute__ ((aligned (16))) = {5.5826318e-2f, 5.5826318e-2f, 5.5826318e-2f, 5.5826318e-2f};
static const gfloat exp_p5_4[4] __attribute__ ((aligned (16))) = {8.9893397e-3f, 8.9893397e-3f, 8.9893397e-3f, 8.9893397e-3f};
static const gfloat exp_p5_5[4] __attribute__ ((aligned (16))) = {1.8775767e-3f, 1.8775767e-3f, 1.8775767e-3f, 1.8775767e-3f};

static const gfloat exp_p4_0[4] __attribute__ ((aligned (16))) = {1.0000026f, 1.0000026f, 1.0000026f, 1.0000026f};
static const gfloat exp_p4_1[4] __attribute__ ((aligned (16))) = {6.9300383e-1f,  6.9300383e-1f,  6.9300383e-1f,  6.9300383e-1f};
static const gfloat exp_p4_2[4] __attribute__ ((aligned (16))) = {2.4144275e-1f, 2.4144275e-1f, 2.4144275e-1f, 2.4144275e-1f};
static const gfloat exp_p4_3[4] __attribute__ ((aligned (16))) = {5.2011464e-2f, 5.2011464e-2f, 5.2011464e-2f, 5.2011464e-2f};
static const gfloat exp_p4_4[4] __attribute__ ((aligned (16))) = {1.3534167e-2f, 1.3534167e-2f, 1.3534167e-2f, 1.3534167e-2f};

static const gfloat exp_p3_0[4] __attribute__ ((aligned (16))) = {9.9992520e-1f, 9.9992520e-1f, 9.9992520e-1f, 9.9992520e-1f};
static const gfloat exp_p3_1[4] __attribute__ ((aligned (16))) = {6.9583356e-1f, 6.9583356e-1f, 6.9583356e-1f, 6.9583356e-1f};
static const gfloat exp_p3_2[4] __attribute__ ((aligned (16))) = {2.2606716e-1f, 2.2606716e-1f, 2.2606716e-1f, 2.2606716e-1f};
static const gfloat exp_p3_3[4] __attribute__ ((aligned (16))) = {7.8024521e-2f, 7.8024521e-2f, 7.8024521e-2f, 7.8024521e-2f};

static const gfloat exp_p2_0[4] __attribute__ ((aligned (16))) = {1.0017247f, 1.0017247f, 1.0017247f, 1.0017247f};
static const gfloat exp_p2_1[4] __attribute__ ((aligned (16))) = {6.5763628e-1f, 6.5763628e-1f, 6.5763628e-1f, 6.5763628e-1f};
static const gfloat exp_p2_2[4] __attribute__ ((aligned (16))) = {3.3718944e-1f, 3.3718944e-1f, 3.3718944e-1f, 3.3718944e-1f};

static const gfloat _ones_ps[4] __attribute__ ((aligned (16))) = {1.0f, 1.0f, 1.0f, 1.0f};
static const gfloat _one29_ps[4] __attribute__ ((aligned (16))) = {129.00000f, 129.00000f, 129.00000f, 129.00000f};
static const gfloat _minusone27_ps[4] __attribute__ ((aligned (16))) = {-126.99999f, -126.99999f, -126.99999f, -126.99999f};
static const gfloat _half_ps[4] __attribute__ ((aligned (16))) = {0.5f, 0.5f, 0.5f, 0.5f};
static const guint _one27[4] __attribute__ ((aligned (16))) = {127,127,127,127};
	
static inline __m128 
exp2f4(__m128 x)
{
	__m128i ipart;
	__m128 fpart, expipart, expfpart;

	x = _mm_min_ps(x, _mm_load_ps(_one29_ps));
	x = _mm_max_ps(x, _mm_load_ps(_minusone27_ps));

	/* ipart = int(x - 0.5) */
	ipart = _mm_cvtps_epi32(_mm_sub_ps(x, _mm_load_ps(_half_ps)));

	/* fpart = x - ipart */
	fpart = _mm_sub_ps(x, _mm_cvtepi32_ps(ipart));

	/* expipart = (float) (1 << ipart) */
	expipart = _mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(ipart, _mm_load_si128((__m128i*)_one27)), 23));

	/* minimax polynomial fit of 2**x, in range [-0.5, 0.5[ */
#if EXP_POLY_DEGREE == 5
	expfpart = POLY5(fpart, exp_p5_0, exp_p5_1, exp_p5_2, exp_p5_3, exp_p5_4, exp_p5_5);
#elif EXP_POLY_DEGREE == 4
	expfpart = POLY4(fpart, exp_p4_0, exp_p4_1, exp_p4_2, exp_p4_3, exp_p4_4);
#elif EXP_POLY_DEGREE == 3
	expfpart = POLY3(fpart, exp_p3_0, exp_p3_1, exp_p3_2, exp_p3_3);
#elif EXP_POLY_DEGREE == 2
	expfpart = POLY2(fpart, exp_p2_0, exp_p2_1, exp_p2_2);
#else
#error
#endif

	return _mm_mul_ps(expipart, expfpart);
}


#define LOG_POLY_DEGREE 5

static const gfloat log_p5_0[4] __attribute__ ((aligned (16))) = {3.1157899f, 3.1157899f, 3.1157899f, 3.1157899f};
static const gfloat log_p5_1[4] __attribute__ ((aligned (16))) = {-3.3241990f, -3.3241990f, -3.3241990f, -3.3241990f};
static const gfloat log_p5_2[4] __attribute__ ((aligned (16))) = {2.5988452f, 2.5988452f, 2.5988452f, 2.5988452f};
static const gfloat log_p5_3[4] __attribute__ ((aligned (16))) = {-1.2315303f, -1.2315303f, -1.2315303f, -1.2315303f};
static const gfloat log_p5_4[4] __attribute__ ((aligned (16))) = {3.1821337e-1f, 3.1821337e-1f, 3.1821337e-1f, 3.1821337e-1f};
static const gfloat log_p5_5[4] __attribute__ ((aligned (16))) = {-3.4436006e-2f, -3.4436006e-2f, -3.4436006e-2f, -3.4436006e-2f};

static const gfloat log_p4_0[4] __attribute__ ((aligned (16))) = {2.8882704548164776201f, 2.8882704548164776201f, 2.8882704548164776201f, 2.8882704548164776201f};
static const gfloat log_p4_1[4] __attribute__ ((aligned (16))) = {-2.52074962577807006663f, -2.52074962577807006663f, -2.52074962577807006663f, -2.52074962577807006663f};
static const gfloat log_p4_2[4] __attribute__ ((aligned (16))) = {1.48116647521213171641f, 1.48116647521213171641f, 1.48116647521213171641f, 1.48116647521213171641f};
static const gfloat log_p4_3[4] __attribute__ ((aligned (16))) = {-0.465725644288844778798f, -0.465725644288844778798f,-0.465725644288844778798f, -0.465725644288844778798f};
static const gfloat log_p4_4[4] __attribute__ ((aligned (16))) = {0.0596515482674574969533f, 0.0596515482674574969533f, 0.0596515482674574969533f, 0.0596515482674574969533f};

static const gfloat log_p3_0[4] __attribute__ ((aligned (16))) = {2.61761038894603480148f, 2.61761038894603480148f, 2.61761038894603480148f, 2.61761038894603480148f};
static const gfloat log_p3_1[4] __attribute__ ((aligned (16))) = {-1.75647175389045657003f, -1.75647175389045657003f, -1.75647175389045657003f, -1.75647175389045657003f};
static const gfloat log_p3_2[4] __attribute__ ((aligned (16))) = {0.688243882994381274313f, 0.688243882994381274313f, 0.688243882994381274313f, 0.688243882994381274313f};
static const gfloat log_p3_3[4] __attribute__ ((aligned (16))) = {-0.107254423828329604454f, -0.107254423828329604454f, -0.107254423828329604454f, -0.107254423828329604454f};

static const gfloat log_p2_0[4] __attribute__ ((aligned (16))) = {2.28330284476918490682f, 2.28330284476918490682f, 2.28330284476918490682f, 2.28330284476918490682f};
static const gfloat log_p2_1[4] __attribute__ ((aligned (16))) = {-1.04913055217340124191f, -1.04913055217340124191f, -1.04913055217340124191f, -1.04913055217340124191f};
static const gfloat log_p2_2[4] __attribute__ ((aligned (16))) = {0.204446009836232697516f, 0.204446009836232697516f, 0.204446009836232697516f, 0.204446009836232697516f};

static const guint _exp_mask[4] __attribute__ ((aligned (16))) = {0x7F800000,0x7F800000,0x7F800000,0x7F800000};
static const guint _mantissa_mask[4] __attribute__ ((aligned (16))) = {0x007FFFFF,0x007FFFFF,0x007FFFFF,0x007FFFFF};

static inline __m128 
log2f4(__m128 x)
{
	__m128i exp = _mm_load_si128((__m128i*)_exp_mask);
	__m128i mant = _mm_load_si128((__m128i*)_mantissa_mask);
	__m128 one = _mm_load_ps(_ones_ps);
	__m128i i = _mm_castps_si128(x);
	__m128 e = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_srli_epi32(_mm_and_si128(i, exp), 23), _mm_load_si128((__m128i*)_one27)));
	__m128 m = _mm_or_ps(_mm_castsi128_ps(_mm_and_si128(i, mant)), one);
	__m128 p;

	/* Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[ */
#if LOG_POLY_DEGREE == 6
	p = POLY5( m, log_p5_0, log_p5_1, log_p5_2, log_p5_3, log_p5_4, log_p5_5);
#elif LOG_POLY_DEGREE == 5
	p = POLY4(m, log_p4_0, log_p4_1, log_p4_2, log_p4_3, log_p4_4);
#elif LOG_POLY_DEGREE == 4
	p = POLY3(m, log_p3_0, log_p3_1, log_p3_2, log_p3_3);
#elif LOG_POLY_DEGREE == 3
	p = POLY2(m, log_p2_0, log_p2_1, log_p2_2);
#else
#error
#endif

	/* This effectively increases the polynomial degree by one, but ensures that log2(1) == 0*/
	p = _mm_mul_ps(p, _mm_sub_ps(m, one));

	return _mm_add_ps(p, e);
}

static inline __m128
_mm_fastpow_ps(__m128 x, __m128 y)
{
	return exp2f4(_mm_mul_ps(log2f4(x), y));
}

#include <stdio.h>
#include <emscripten/math.h>

int main()
{
	printf("%f\n", emscripten_math_cbrt(8.0));
	printf("%f\n", emscripten_math_pow(2.0, 4.0));
	printf("%d\n", (int)(emscripten_math_random() >= 0 && emscripten_math_random() <= 1));
	printf("%f\n", emscripten_math_sign(-42.0));
	printf("%f\n", emscripten_math_exp(2.0));
	printf("%f\n", emscripten_math_expm1(2.0));
	printf("%f\n", emscripten_math_log(42.0));
	printf("%f\n", emscripten_math_log1p(42.0));
	printf("%f\n", emscripten_math_log10(42.0));
	printf("%f\n", emscripten_math_log2(42.0));
	printf("%f\n", emscripten_math_round(42.5));
	printf("%f\n", emscripten_math_acos(0.5));
	printf("%f\n", emscripten_math_acosh(42.0));
	printf("%f\n", emscripten_math_asin(0.5));
	printf("%f\n", emscripten_math_asinh(42.0));
	printf("%f\n", emscripten_math_atan(42.0));
	printf("%f\n", emscripten_math_atanh(0.9));
	printf("%f\n", emscripten_math_atan2(42.0, 13.0));
	printf("%f\n", emscripten_math_cos(42.0));
	printf("%f\n", emscripten_math_cosh(0.6));
	printf("%f\n", emscripten_math_hypot(3, 3.0, 4.0, 5.0));
	printf("%f\n", emscripten_math_sin(42.0));
	printf("%f\n", emscripten_math_sinh(0.6));
	printf("%f\n", emscripten_math_tan(42.0));
	printf("%f\n", emscripten_math_tanh(42.0));
}

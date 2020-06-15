#include <math.h>

double fmax(double x, double y)
{
	if (isnan(x))
		return y;
	if (isnan(y))
		return x;
// XXX EMSCRIPTEN: use wasm builtins for code size
#ifdef __wasm__
	return __builtin_wasm_max_f64(x, y);
#else
	/* handle signed zeros, see C99 Annex F.9.9.2 */
	if (signbit(x) != signbit(y))
		return signbit(x) ? y : x;
	return x < y ? y : x;
#endif
}

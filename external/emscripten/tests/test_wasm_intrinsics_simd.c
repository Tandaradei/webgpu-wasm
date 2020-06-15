#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <emscripten.h>
#include <wasm_simd128.h>

#define TESTFN EMSCRIPTEN_KEEPALIVE __attribute__((noinline))

v128_t TESTFN i8x16_load(void *ptr) {
  return wasm_v128_load(ptr);
}

#ifdef __wasm_unimplemented_simd128__

v128_t TESTFN v8x16_load_splat(void *ptr) {
  return wasm_v8x16_load_splat(ptr);
}
v128_t TESTFN v16x8_load_splat(void *ptr) {
  return wasm_v16x8_load_splat(ptr);
}
v128_t TESTFN v32x4_load_splat(void *ptr) {
  return wasm_v32x4_load_splat(ptr);
}
v128_t TESTFN v64x2_load_splat(void *ptr) {
  return wasm_v64x2_load_splat(ptr);
}
v128_t TESTFN i16x8_load8x8_s(void *ptr) {
  return wasm_i16x8_load_8x8(ptr);
}
v128_t TESTFN i16x8_load8x8_u(void *ptr) {
  return wasm_u16x8_load_8x8(ptr);
}
v128_t TESTFN i32x4_load16x4_s(void *ptr) {
  return wasm_i32x4_load_16x4(ptr);
}
v128_t TESTFN i32x4_load16x4_u(void *ptr) {
  return wasm_u32x4_load_16x4(ptr);
}
v128_t TESTFN i64x2_load32x2_s(void *ptr) {
  return wasm_i64x2_load_32x2(ptr);
}
v128_t TESTFN i64x2_load32x2_u(void *ptr) {
  return wasm_u64x2_load_32x2(ptr);
}

#endif // __wasm_unimplemented_simd128__

void TESTFN i8x16_store(void *ptr, v128_t vec) {
  wasm_v128_store(ptr, vec);
}
v128_t TESTFN i8x16_const(void) {
  return wasm_i8x16_const(
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
}
v128_t TESTFN i16x8_const(void) {
  return wasm_i16x8_const(1, 2, 3, 4, 5, 6, 7, 8);
}
v128_t TESTFN i32x4_const(void) {
  return wasm_i32x4_const(1, 2, 3, 4);
}
v128_t TESTFN f32x4_const(void) {
  return wasm_f32x4_const(1, 2, 3, 4);
}
v128_t TESTFN i64x2_const(void) {
  return wasm_i64x2_const(1, 2);
}
v128_t TESTFN f64x2_const(void) {
  return wasm_f64x2_const(1, 2);
}
v128_t TESTFN i8x16_make(int8_t first) {
  return wasm_i8x16_make(
      first, (int8_t)2, (int8_t)3, (int8_t)4,
      (int8_t)5, (int8_t)6, (int8_t)7, (int8_t)8,
      (int8_t)9, (int8_t)10, (int8_t)11, (int8_t)12,
      (int8_t)13, (int8_t)14, (int8_t)15, (int8_t)16
  );
}
v128_t TESTFN i16x8_make(int16_t first) {
  return wasm_i16x8_make(
      first, (int16_t)2, (int16_t)3, (int16_t)4,
      (int16_t)5, (int16_t)6, (int16_t)7, (int16_t)8
  );
}
v128_t TESTFN i32x4_make(int32_t first) {
  return wasm_i32x4_make(first, (int32_t)2, (int32_t)3, (int32_t)4);
}
v128_t TESTFN f32x4_make(float first) {
  return wasm_f32x4_make(first, 2.f, 3.f, 4.f);
}
v128_t TESTFN i64x2_make(int64_t first) {
  return wasm_i64x2_make(first, (int64_t)2);
}
v128_t TESTFN f64x2_make(double first) {
  return wasm_f64x2_make(first, 2.);
}
v128_t TESTFN i8x16_shuffle_interleave_bytes(v128_t x, v128_t y) {
  return wasm_v8x16_shuffle(x, y, 0, 17, 2, 19, 4, 21, 6, 23, 8, 25, 10, 27, 12, 29, 14, 31);
}
v128_t TESTFN i16x8_shuffle_reverse(v128_t vec) {
  return wasm_v16x8_shuffle(vec, vec, 7, 6, 5, 4, 3, 2, 1, 0);
}
v128_t TESTFN i32x4_shuffle_reverse(v128_t vec) {
  return wasm_v32x4_shuffle(vec, vec, 3, 2, 1, 0);
}
v128_t TESTFN i64x2_shuffle_reverse(v128_t vec) {
  return wasm_v64x2_shuffle(vec, vec, 1, 0);
}

#ifdef __wasm_unimplemented_simd128__

v128_t TESTFN v8x16_swizzle(v128_t x, v128_t y) {
  return wasm_v8x16_swizzle(x, y);
}

#endif // __wasm_unimplemented_simd128__

v128_t TESTFN i8x16_splat(int32_t x) {
  return wasm_i8x16_splat(x);
}
int32_t TESTFN i8x16_extract_lane_s_first(v128_t vec) {
  return wasm_i8x16_extract_lane(vec, 0);
}
int32_t TESTFN i8x16_extract_lane_s_last(v128_t vec) {
  return wasm_i8x16_extract_lane(vec, 15);
}

uint32_t TESTFN i8x16_extract_lane_u_first(v128_t vec) {
  return wasm_u8x16_extract_lane(vec, 0);
}
uint32_t TESTFN i8x16_extract_lane_u_last(v128_t vec) {
  return wasm_u8x16_extract_lane(vec, 15);
}

v128_t TESTFN i8x16_replace_lane_first(v128_t vec, int32_t val) {
  return wasm_i8x16_replace_lane(vec, 0, val);
}
v128_t TESTFN i8x16_replace_lane_last(v128_t vec, int32_t val) {
  return wasm_i8x16_replace_lane(vec, 15, val);
}
v128_t TESTFN i16x8_splat(int32_t x) {
  return wasm_i16x8_splat(x);
}
int32_t TESTFN i16x8_extract_lane_s_first(v128_t vec) {
  return wasm_i16x8_extract_lane(vec, 0);
}
int32_t TESTFN i16x8_extract_lane_s_last(v128_t vec) {
  return wasm_i16x8_extract_lane(vec, 7);
}

int32_t TESTFN i16x8_extract_lane_u_first(v128_t vec) {
    return wasm_u16x8_extract_lane(vec, 0);
}
int32_t TESTFN i16x8_extract_lane_u_last(v128_t vec) {
  return wasm_u16x8_extract_lane(vec, 7);
}

v128_t TESTFN i16x8_replace_lane_first(v128_t vec, int32_t val) {
  return wasm_i16x8_replace_lane(vec, 0, val);
}
v128_t TESTFN i16x8_replace_lane_last(v128_t vec, int32_t val) {
  return wasm_i16x8_replace_lane(vec, 7, val);
}
v128_t TESTFN i32x4_splat(int32_t x) {
  return wasm_i32x4_splat(x);
}
int32_t TESTFN i32x4_extract_lane_first(v128_t vec) {
  return wasm_i32x4_extract_lane(vec, 0);
}
int32_t TESTFN i32x4_extract_lane_last(v128_t vec) {
  return wasm_i32x4_extract_lane(vec, 3);
}
v128_t TESTFN i32x4_replace_lane_first(v128_t vec, int32_t val) {
  return wasm_i32x4_replace_lane(vec, 0, val);
}
v128_t TESTFN i32x4_replace_lane_last(v128_t vec, int32_t val) {
  return wasm_i32x4_replace_lane(vec, 3, val);
}

v128_t TESTFN i64x2_splat(int64_t x) {
  return wasm_i64x2_splat(x);
}
int64_t TESTFN i64x2_extract_lane_first(v128_t vec) {
  return wasm_i64x2_extract_lane(vec, 0);
}
int64_t TESTFN i64x2_extract_lane_last(v128_t vec) {
  return wasm_i64x2_extract_lane(vec, 1);
}
v128_t TESTFN i64x2_replace_lane_first(v128_t vec, int64_t val) {
  return wasm_i64x2_replace_lane(vec, 0, val);
}
v128_t TESTFN i64x2_replace_lane_last(v128_t vec, int64_t val) {
  return wasm_i64x2_replace_lane(vec, 1, val);
}

v128_t TESTFN f32x4_splat(float x) {
  return wasm_f32x4_splat(x);
}
float TESTFN f32x4_extract_lane_first(v128_t vec) {
  return wasm_f32x4_extract_lane(vec, 0);
}
float TESTFN f32x4_extract_lane_last(v128_t vec) {
  return wasm_f32x4_extract_lane(vec, 3);
}
v128_t TESTFN f32x4_replace_lane_first(v128_t vec, float val) {
  return wasm_f32x4_replace_lane(vec, 0, val);
}
v128_t TESTFN f32x4_replace_lane_last(v128_t vec, float val) {
  return wasm_f32x4_replace_lane(vec, 3, val);
}

v128_t TESTFN f64x2_splat(double x) {
  return wasm_f64x2_splat(x);
}
double TESTFN f64x2_extract_lane_first(v128_t vec) {
    return wasm_f64x2_extract_lane(vec, 0);
}
double TESTFN f64x2_extract_lane_last(v128_t vec) {
  return wasm_f64x2_extract_lane(vec, 1);
}
v128_t TESTFN f64x2_replace_lane_first(v128_t vec, double val) {
  return wasm_f64x2_replace_lane(vec, 0, val);
}
v128_t TESTFN f64x2_replace_lane_last(v128_t vec, double val) {
  return wasm_f64x2_replace_lane(vec, 1, val);
}

v128_t TESTFN i8x16_eq(v128_t x, v128_t y) {
  return wasm_i8x16_eq(x, y);
}
v128_t TESTFN i8x16_ne(v128_t x, v128_t y) {
  return wasm_i8x16_ne(x, y);
}
v128_t TESTFN i8x16_lt_s(v128_t x, v128_t y) {
  return wasm_i8x16_lt(x, y);
}
v128_t TESTFN i8x16_lt_u(v128_t x, v128_t y) {
  return wasm_u8x16_lt(x, y);
}
v128_t TESTFN i8x16_gt_s(v128_t x, v128_t y) {
  return wasm_i8x16_gt(x, y);
}
v128_t TESTFN i8x16_gt_u(v128_t x, v128_t y) {
  return wasm_u8x16_gt(x,y);
}
v128_t TESTFN i8x16_le_s(v128_t x, v128_t y) {
  return wasm_i8x16_le(x,y);
}
v128_t TESTFN i8x16_le_u(v128_t x, v128_t y) {
   return wasm_u8x16_le(x, y);
}
v128_t TESTFN i8x16_ge_s(v128_t x, v128_t y) {
  return wasm_i8x16_ge(x, y);
}
v128_t  TESTFN i8x16_ge_u(v128_t x, v128_t y) {
  return wasm_u8x16_ge(x, y);
}
v128_t TESTFN i16x8_eq(v128_t x, v128_t y) {
  return wasm_i16x8_eq(x,y);
}
v128_t TESTFN i16x8_ne(v128_t x, v128_t y) {
  return wasm_i16x8_ne(x,y);
}
v128_t TESTFN i16x8_lt_s(v128_t x, v128_t y) {
  return wasm_i16x8_lt(x,y);
}
v128_t TESTFN i16x8_lt_u(v128_t x, v128_t y) {
    return wasm_u16x8_lt(x,y);
}
v128_t TESTFN i16x8_gt_s(v128_t x, v128_t y) {
  return wasm_i16x8_gt(x,y);
}
v128_t TESTFN i16x8_gt_u(v128_t x, v128_t y) {
  return wasm_u16x8_gt(x,y);
}
v128_t TESTFN i16x8_le_s(v128_t x, v128_t y) {
  return wasm_i16x8_le(x, y);
}
v128_t TESTFN i16x8_le_u(v128_t x, v128_t y) {
  return wasm_u16x8_le(x, y);
}
v128_t TESTFN i16x8_ge_s(v128_t x, v128_t y) {
  return wasm_i16x8_ge(x, y);
}
v128_t  TESTFN i16x8_ge_u(v128_t x, v128_t y) {
  return wasm_u16x8_ge(x, y);
}
v128_t TESTFN i32x4_eq(v128_t x, v128_t y) {
  return wasm_i32x4_eq(x, y);
}
v128_t TESTFN i32x4_ne(v128_t x, v128_t y) {
  return wasm_i32x4_ne(x, y);
}
v128_t TESTFN i32x4_lt_s(v128_t x, v128_t y) {
  return wasm_i32x4_lt(x, y);
}
v128_t TESTFN i32x4_lt_u(v128_t x, v128_t y) {
  return wasm_u32x4_lt(x, y);
}
v128_t TESTFN i32x4_gt_s(v128_t x, v128_t y) {
  return wasm_i32x4_gt(x, y);
}
v128_t TESTFN i32x4_gt_u(v128_t x, v128_t y) {
  return wasm_u32x4_gt(x, y);
}
v128_t TESTFN i32x4_le_s(v128_t x, v128_t y) {
  return wasm_i32x4_le(x, y);
}
v128_t TESTFN i32x4_le_u(v128_t x, v128_t y) {
  return wasm_u32x4_le(x, y);
}
v128_t TESTFN i32x4_ge_s(v128_t x, v128_t y) {
  return wasm_i32x4_ge(x, y);
}
v128_t  TESTFN i32x4_ge_u(v128_t x, v128_t y) {
  return wasm_u32x4_ge(x, y);
}
v128_t TESTFN f32x4_eq(v128_t x, v128_t y) {
  return wasm_f32x4_eq(x,y);
}
v128_t TESTFN f32x4_ne(v128_t x, v128_t y) {
  return wasm_f32x4_ne(x, y);
}
v128_t TESTFN f32x4_lt(v128_t x, v128_t y) {
  return wasm_f32x4_lt(x, y);
}
v128_t TESTFN f32x4_gt(v128_t x, v128_t y) {
  return wasm_f32x4_gt(x,y);
}
v128_t TESTFN f32x4_le(v128_t x, v128_t y) {
  return wasm_f32x4_le(x, y);
}
v128_t TESTFN f32x4_ge(v128_t x, v128_t y) {
  return wasm_f32x4_ge(x, y);
}
v128_t TESTFN f64x2_eq(v128_t x, v128_t y) {
  return wasm_f64x2_eq(x,y);
}
v128_t TESTFN f64x2_ne(v128_t x, v128_t y) {
  return wasm_f64x2_ne(x,y);
}
v128_t TESTFN f64x2_lt(v128_t x, v128_t y) {
  return wasm_f64x2_lt(x,y);
}
v128_t TESTFN f64x2_gt(v128_t x, v128_t y) {
  return wasm_f64x2_gt(x, y);
}
v128_t TESTFN f64x2_le(v128_t x, v128_t y) {
  return wasm_f64x2_le(x, y);
}
v128_t TESTFN f64x2_ge(v128_t x, v128_t y) {
  return wasm_f64x2_ge(x, y);
}
v128_t TESTFN v128_not(v128_t vec) {
  return wasm_v128_not(vec);
}
v128_t TESTFN v128_and(v128_t x, v128_t y) {
  return wasm_v128_and(x, y);
}
v128_t TESTFN v128_or(v128_t x, v128_t y) {
  return wasm_v128_or(x, y);
}
v128_t TESTFN v128_xor(v128_t x, v128_t y) {
  return wasm_v128_xor(x, y);
}

#ifdef __wasm_unimplemented_simd128__

v128_t TESTFN v128_andnot(v128_t x, v128_t y) {
  return wasm_v128_andnot(x, y);
}

#endif // __wasm_unimplemented_simd128__

v128_t TESTFN v128_bitselect(v128_t x, v128_t y, v128_t cond) {
  return wasm_v128_bitselect(x, y, cond);
}
v128_t TESTFN i8x16_abs(v128_t vec) {
  return wasm_i8x16_abs(vec);
}
v128_t TESTFN i8x16_neg(v128_t vec) {
  return wasm_i8x16_neg(vec);
}
int32_t TESTFN i8x16_any_true(v128_t vec) {
  return wasm_i8x16_any_true(vec);
}
int32_t TESTFN i8x16_all_true(v128_t vec) {
  return wasm_i8x16_all_true(vec);
}
v128_t TESTFN i8x16_shl(v128_t vec, int32_t shift) {
  return wasm_i8x16_shl(vec, shift);
}
v128_t TESTFN i8x16_shr_s(v128_t vec, int32_t shift) {
  return wasm_i8x16_shr(vec, shift);
}
v128_t TESTFN i8x16_shr_u(v128_t vec, int32_t shift) {
  return wasm_u8x16_shr(vec, shift);
}
v128_t TESTFN i8x16_add(v128_t x, v128_t y) {
  return wasm_i8x16_add(x,y);
}
v128_t TESTFN i8x16_add_saturate_s(v128_t x, v128_t y) {
  return wasm_i8x16_add_saturate(x, y);
}
v128_t TESTFN i8x16_add_saturate_u(v128_t x, v128_t y) {
  return wasm_u8x16_add_saturate(x, y);
}
v128_t TESTFN i8x16_sub(v128_t x, v128_t y) {
  return wasm_i8x16_sub(x,y);
}
v128_t TESTFN i8x16_sub_saturate_s(v128_t x, v128_t y) {
  return wasm_i8x16_sub_saturate(x, y);
}
v128_t TESTFN i8x16_sub_saturate_u(v128_t x, v128_t y) {
  return wasm_u8x16_sub_saturate(x, y);
}

#ifdef __wasm_unimplemented_simd128__

v128_t TESTFN i8x16_mul(v128_t x, v128_t y) {
  return wasm_i8x16_mul(x, y);
}

#endif // __wasm_unimplemented_simd128__

v128_t TESTFN i8x16_min_s(v128_t x, v128_t y) {
  return wasm_i8x16_min(x, y);
}
v128_t TESTFN i8x16_min_u(v128_t x, v128_t y) {
  return wasm_u8x16_min(x, y);
}
v128_t TESTFN i8x16_max_s(v128_t x, v128_t y) {
  return wasm_i8x16_max(x, y);
}
v128_t TESTFN i8x16_max_u(v128_t x, v128_t y) {
  return wasm_u8x16_max(x, y);
}
v128_t TESTFN i8x16_avgr_u(v128_t x, v128_t y) {
  return wasm_u8x16_avgr(x, y);
}
v128_t TESTFN i16x8_abs(v128_t vec) {
  return wasm_i16x8_abs(vec);
}
v128_t TESTFN i16x8_neg(v128_t vec) {
  return wasm_i16x8_neg(vec);
}
bool TESTFN i16x8_any_true(v128_t vec) {
  return wasm_i16x8_any_true(vec);
}
bool TESTFN i16x8_all_true(v128_t vec) {
  return wasm_i16x8_all_true(vec);
}
v128_t TESTFN i16x8_shl(v128_t vec, int32_t shift) {
  return wasm_i16x8_shl(vec, shift);
}
v128_t TESTFN i16x8_shr_s(v128_t vec, int32_t shift) {
  return wasm_i16x8_shr(vec, shift);
}
v128_t TESTFN i16x8_shr_u(v128_t vec, int32_t shift) {
  return wasm_u16x8_shr(vec, shift);
}
v128_t TESTFN i16x8_add(v128_t x, v128_t y) {
  return wasm_i16x8_add(x, y);
}
v128_t TESTFN i16x8_add_saturate_s(v128_t x, v128_t y) {
  return wasm_i16x8_add_saturate(x, y);
}
v128_t TESTFN i16x8_add_saturate_u(v128_t x, v128_t y) {
  return wasm_u16x8_add_saturate(x, y);
}
v128_t TESTFN i16x8_sub(v128_t x, v128_t y) {
  return wasm_i16x8_sub(x, y);
}
v128_t TESTFN i16x8_sub_saturate_s(v128_t x, v128_t y) {
  return wasm_i16x8_sub_saturate(x,y);
}
v128_t TESTFN i16x8_sub_saturate_u(v128_t x, v128_t y) {
  return wasm_u16x8_sub_saturate(x, y);
}
v128_t TESTFN i16x8_min_s(v128_t x, v128_t y) {
  return wasm_i16x8_min(x, y);
}
v128_t TESTFN i16x8_min_u(v128_t x, v128_t y) {
  return wasm_u16x8_min(x, y);
}
v128_t TESTFN i16x8_max_s(v128_t x, v128_t y) {
  return wasm_i16x8_max(x, y);
}
v128_t TESTFN i16x8_max_u(v128_t x, v128_t y) {
  return wasm_u16x8_max(x, y);
}
v128_t TESTFN i16x8_avgr_u(v128_t x, v128_t y) {
  return wasm_u16x8_avgr(x, y);
}
v128_t TESTFN i16x8_mul(v128_t x, v128_t y) {
  return wasm_i16x8_mul(x, y);
}
v128_t TESTFN i32x4_abs(v128_t vec) {
  return wasm_i32x4_abs(vec);
}
v128_t TESTFN i32x4_neg(v128_t vec) {
  return wasm_i32x4_neg(vec);
}
int32_t TESTFN i32x4_any_true(v128_t vec) {
  return wasm_i32x4_any_true(vec);
}
int32_t TESTFN i32x4_all_true(v128_t vec) {
  return wasm_i32x4_all_true(vec);
}
v128_t TESTFN i32x4_shl(v128_t vec, int32_t shift) {
  return wasm_i32x4_shl(vec, shift);
}
v128_t TESTFN i32x4_shr_s(v128_t vec, int32_t shift) {
  return wasm_i32x4_shr(vec, shift);
}
v128_t TESTFN i32x4_shr_u(v128_t vec, int32_t shift) {
  return wasm_u32x4_shr(vec, shift);
}
v128_t TESTFN i32x4_add(v128_t x, v128_t y) {
  return wasm_i32x4_add(x, y);
}
v128_t TESTFN i32x4_sub(v128_t x, v128_t y) {
  return wasm_i32x4_sub(x, y);
}
v128_t TESTFN i32x4_min_s(v128_t x, v128_t y) {
  return wasm_i32x4_min(x, y);
}
v128_t TESTFN i32x4_min_u(v128_t x, v128_t y) {
  return wasm_u32x4_min(x, y);
}
v128_t TESTFN i32x4_max_s(v128_t x, v128_t y) {
  return wasm_i32x4_max(x, y);
}
v128_t TESTFN i32x4_max_u(v128_t x, v128_t y) {
  return wasm_u32x4_max(x, y);
}
v128_t TESTFN i32x4_mul(v128_t x, v128_t y) {
  return wasm_i32x4_mul(x, y);
}
v128_t TESTFN i64x2_neg(v128_t vec) {
  return wasm_i64x2_neg(vec);
}

#ifdef __wasm_unimplemented_simd128__

bool TESTFN i64x2_any_true(v128_t vec) {
  return wasm_i64x2_any_true(vec);
}
bool TESTFN i64x2_all_true(v128_t vec) {
  return wasm_i64x2_all_true(vec);
}

#endif // __wasm_unimplemented_simd128__

v128_t TESTFN i64x2_shl(v128_t vec, int32_t shift) {
  return wasm_i64x2_shl(vec, shift);
}
v128_t TESTFN i64x2_shr_s(v128_t vec, int32_t shift) {
  return wasm_i64x2_shr(vec, shift);
}
v128_t TESTFN i64x2_shr_u(v128_t vec, int32_t shift) {
  return wasm_u64x2_shr(vec, shift);
}
v128_t TESTFN i64x2_add(v128_t x, v128_t y) {
  return wasm_i64x2_add(x, y);
}
v128_t TESTFN i64x2_sub(v128_t x, v128_t y) {
  return wasm_i64x2_sub(x, y);
}
v128_t TESTFN f32x4_abs(v128_t vec) {
  return wasm_f32x4_abs(vec);
}
v128_t TESTFN f32x4_neg(v128_t vec) {
  return wasm_f32x4_neg(vec);
}
v128_t TESTFN f32x4_sqrt(v128_t vec) {
  return wasm_f32x4_sqrt(vec);
}

#ifdef __wasm_unimplemented_simd128__

v128_t TESTFN f32x4_qfma(v128_t a, v128_t b, v128_t c) {
  return wasm_f32x4_qfma(a, b, c);
}
v128_t TESTFN f32x4_qfms(v128_t a, v128_t b, v128_t c) {
  return wasm_f32x4_qfms(a, b, c);
}

#endif // __wasm_unimplemented_simd128__

v128_t TESTFN f32x4_add(v128_t x, v128_t y) {
  return wasm_f32x4_add(x, y);
}
v128_t TESTFN f32x4_sub(v128_t x, v128_t y) {
  return wasm_f32x4_sub(x, y);
}
v128_t TESTFN f32x4_mul(v128_t x, v128_t y) {
  return wasm_f32x4_mul(x, y);
}
v128_t TESTFN f32x4_div(v128_t x, v128_t y) {
  return wasm_f32x4_div(x, y);
}
v128_t TESTFN f32x4_min(v128_t x, v128_t y) {
  return wasm_f32x4_min(x, y);
}
v128_t TESTFN f32x4_max(v128_t x, v128_t y) {
  return wasm_f32x4_max(x, y);
}
v128_t TESTFN f64x2_abs(v128_t vec) {
  return wasm_f64x2_abs(vec);
}
v128_t TESTFN f64x2_neg(v128_t vec) {
  return wasm_f64x2_neg(vec);
}
v128_t TESTFN f64x2_sqrt(v128_t vec) {
  return wasm_f64x2_sqrt(vec);
}

#ifdef __wasm_unimplemented_simd128__

v128_t TESTFN f64x2_qfma(v128_t a, v128_t b, v128_t c) {
  return wasm_f64x2_qfma(a, b, c);
}
v128_t TESTFN f64x2_qfms(v128_t a, v128_t b, v128_t c) {
  return wasm_f64x2_qfms(a, b, c);
}

#endif // __wasm_unimplemented_simd128__

v128_t TESTFN f64x2_add(v128_t x, v128_t y) {
  return wasm_f64x2_add(x, y);
}
v128_t TESTFN f64x2_sub(v128_t x, v128_t y) {
  return wasm_f64x2_sub(x, y);
}
v128_t TESTFN f64x2_mul(v128_t x, v128_t y) {
  return wasm_f64x2_mul(x, y);
}
v128_t TESTFN f64x2_div(v128_t x, v128_t y) {
  return wasm_f64x2_div(x, y);
}
v128_t TESTFN f64x2_min(v128_t x, v128_t y) {
  return wasm_f64x2_min(x, y);
}
v128_t TESTFN f64x2_max(v128_t x, v128_t y) {
  return wasm_f64x2_max(x, y);
}
v128_t TESTFN i32x4_trunc_s_f32x4_sat(v128_t vec) {
  return wasm_i32x4_trunc_saturate_f32x4(vec);
}
v128_t TESTFN i32x4_trunc_u_f32x4_sat(v128_t vec) {
  return wasm_u32x4_trunc_saturate_f32x4(vec);
}

v128_t TESTFN f32x4_convert_s_i32x4(v128_t vec) {
  return wasm_f32x4_convert_i32x4(vec);
}
v128_t TESTFN f32x4_convert_u_i32x4(v128_t vec) {
  return wasm_f32x4_convert_u32x4(vec);
}

v128_t TESTFN i8x16_narrow_i16x8_s(v128_t a, v128_t b) {
  return wasm_i8x16_narrow_i16x8(a, b);
}
v128_t TESTFN i8x16_narrow_i16x8_u(v128_t a, v128_t b) {
  return wasm_u8x16_narrow_i16x8(a, b);
}
v128_t TESTFN i16x8_narrow_i32x4_s(v128_t a, v128_t b) {
  return wasm_i16x8_narrow_i32x4(a, b);
}
v128_t TESTFN i16x8_narrow_i32x4_u(v128_t a, v128_t b) {
  return wasm_u16x8_narrow_i32x4(a, b);
}
v128_t TESTFN i16x8_widen_low_i8x16_s(v128_t a) {
  return wasm_i16x8_widen_low_i8x16(a);
}
v128_t TESTFN i16x8_widen_high_i8x16_s(v128_t a) {
  return wasm_i16x8_widen_high_i8x16(a);
}
v128_t TESTFN i16x8_widen_low_i8x16_u(v128_t a) {
  return wasm_i16x8_widen_low_u8x16(a);
}
v128_t TESTFN i16x8_widen_high_i8x16_u(v128_t a) {
  return wasm_i16x8_widen_high_u8x16(a);
}
v128_t TESTFN i32x4_widen_low_i16x8_s(v128_t a) {
  return wasm_i32x4_widen_low_i16x8(a);
}
v128_t TESTFN i32x4_widen_high_i16x8_s(v128_t a) {
  return wasm_i32x4_widen_high_i16x8(a);
}
v128_t TESTFN i32x4_widen_low_i16x8_u(v128_t a) {
  return wasm_i32x4_widen_low_u16x8(a);
}
v128_t TESTFN i32x4_widen_high_i16x8_u(v128_t a) {
  return wasm_i32x4_widen_high_u16x8(a);
}

static int failures = 0;

#define formatter(x) _Generic((x),                              \
                              char: "%d",                       \
                              unsigned char: "%d",              \
                              short: "%d",                      \
                              unsigned short: "%d",             \
                              int: "%d",                        \
                              unsigned int: "%d",               \
                              long long: "%lld",                \
                              unsigned long long: "%lld",       \
                              bool: "%d",                       \
                              float: "%f",                      \
                              double: "%f"                      \
  )

#define err(x) fprintf(stderr, formatter(x), x)

#define eq(a, b) ({                             \
      bool anan = _Generic((a),                 \
                           float: isnan(a),     \
                           double: isnan(a),    \
                           default: false);     \
      bool bnan = _Generic((b),                 \
                           float: isnan(b),     \
                           double: isnan(b),    \
                           default: false);     \
      ((anan && bnan) || (!anan && a == b));    \
    })

#define expect_eq(_a, _b) __extension__({                       \
      __typeof__(_a) a = (_a), b = (_b);                        \
      if (!eq(a, b)) {                                          \
        failures++;                                             \
        fprintf(stderr, "line %d: expected ", __LINE__);        \
        err(b);                                                 \
        fprintf(stderr, ", got ");                              \
        err(a);                                                 \
        fprintf(stderr, "\n");                                  \
      }                                                         \
    })

#define expect_vec(_a, _b) __extension__({                      \
      __typeof__(_b) a = (__typeof__(_b))(_a), b = (_b);        \
      bool err = false;                                         \
      size_t lanes = sizeof(a) / sizeof(a[0]);                  \
      for (size_t i = 0; i < lanes; i++) {                      \
        if (!eq(a[i], b[i])) {                                  \
          err = true;                                           \
          break;                                                \
        }                                                       \
      }                                                         \
      if (err) {                                                \
        failures++;                                             \
        fprintf(stderr, "line %d: expected {", __LINE__);       \
        for (size_t i = 0; i < lanes - 1; i++) {                \
          err(b[i]);                                            \
          fprintf(stderr, ", ");                                \
        }                                                       \
        err(b[lanes - 1]);                                      \
        fprintf(stderr, "}, got {");                            \
        for (size_t i = 0; i < lanes - 1; i++) {                \
          err(a[i]);                                            \
          fprintf(stderr, ", ");                                \
        }                                                       \
        err(a[lanes - 1]);                                      \
        fprintf(stderr, "}\n");                                 \
      }                                                         \
    })

#define i8x16(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16) \
  (__extension__(char __attribute__((__vector_size__(16))))             \
  {c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16})

#define u8x16(c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16) \
  (__extension__(unsigned char __attribute__((__vector_size__(16))))    \
  {c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16})

#define i16x8(c1, c2, c3, c4, c5, c6, c7, c8)                   \
  (__extension__(short __attribute__((__vector_size__(16))))    \
  {c1, c2, c3, c4, c5, c6, c7, c8})

#define u16x8(c1, c2, c3, c4, c5, c6, c7, c8)                           \
  (__extension__(unsigned short __attribute__((__vector_size__(16))))   \
  {c1, c2, c3, c4, c5, c6, c7, c8})

#define i32x4(c1, c2, c3, c4)                                           \
  (__extension__(int __attribute__((__vector_size__(16)))){c1, c2, c3, c4})

#define u32x4(c1, c2, c3, c4)                                           \
  (__extension__(unsigned int __attribute__((__vector_size__(16)))){c1, c2, c3, c4})

#define i64x2(c1, c2)                                                   \
  (__extension__(long long __attribute__((__vector_size__(16)))) {c1, c2})

#define u64x2(c1, c2)                                                   \
  (__extension__(unsigned long long __attribute__((__vector_size__(16)))){c1, c2})

#define f32x4(c1, c2, c3, c4)                                           \
  (__extension__(float __attribute__((__vector_size__(16)))){c1, c2, c3, c4})

#define f64x2(c1, c2)                                                   \
  (__extension__(double __attribute__((__vector_size__(16)))) {c1, c2})


int EMSCRIPTEN_KEEPALIVE __attribute__((__optnone__)) main(int argc, char** argv) {
  {
    v128_t vec = (v128_t)u8x16(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3);
    expect_vec(i8x16_load(&vec),
               i8x16(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3));
    i8x16_store(&vec, (v128_t)i8x16(7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7));
    expect_vec(i8x16_load(&vec),
               i8x16(7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7));
  }

#ifdef __wasm_unimplemented_simd128__

  {
    char vec[8] = {0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0};
    void *ptr = &vec;
    expect_vec(
      v8x16_load_splat(ptr),
      i8x16(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80)
    );
    expect_vec(
      v16x8_load_splat(ptr),
      i16x8(0x9080, 0x9080, 0x9080, 0x9080, 0x9080, 0x9080, 0x9080, 0x9080)
    );
    expect_vec(v32x4_load_splat(ptr), i32x4(0xb0a09080, 0xb0a09080, 0xb0a09080, 0xb0a09080));
    expect_vec(v64x2_load_splat(ptr), i64x2(0xf0e0d0c0b0a09080, 0xf0e0d0c0b0a09080));
    expect_vec(
      i16x8_load8x8_s(ptr),
      i16x8(0xff80, 0xff90, 0xffa0, 0xffb0, 0xffc0, 0xffd0, 0xffe0, 0xfff0)
    );
    expect_vec(
      i16x8_load8x8_u(ptr),
      i16x8(0x0080, 0x0090, 0x00a0, 0x00b0, 0x00c0, 0x00d0, 0x00e0, 0x00f0)
    );
    expect_vec(
      i32x4_load16x4_s(ptr),
      i32x4(0xffff9080, 0xffffb0a0, 0xffffd0c0, 0xfffff0e0)
    );
    expect_vec(
      i32x4_load16x4_u(ptr),
      i32x4(0x00009080, 0x0000b0a0, 0x0000d0c0, 0x0000f0e0)
    );
    expect_vec(
      i64x2_load32x2_s(ptr),
      i64x2(0xffffffffb0a09080, 0xfffffffff0e0d0c0)
    );
    expect_vec(
      i64x2_load32x2_u(ptr),
      i64x2(0x00000000b0a09080, 0x00000000f0e0d0c0)
    );
  }

#endif // __wasm_unimplemented_simd128__

  expect_vec(i8x16_const(), u8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
  expect_vec(i16x8_const(), u16x8(1, 2, 3, 4, 5, 6, 7, 8));
  expect_vec(i32x4_const(), u32x4(1, 2, 3, 4));
  expect_vec(f32x4_const(), f32x4(1., 2., 3., 4.));
  expect_vec(i64x2_const(), u64x2(1, 2));
  expect_vec(f64x2_const(), f64x2(1., 2.));
  expect_vec(i8x16_make(1), u8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
  expect_vec(i16x8_make(1), u16x8(1, 2, 3, 4, 5, 6, 7, 8));
  expect_vec(i32x4_make(1), u32x4(1, 2, 3, 4));
  expect_vec(f32x4_make(1), f32x4(1., 2., 3., 4.));
  expect_vec(i64x2_make(1), u64x2(1, 2));
  expect_vec(f64x2_make(1), f64x2(1., 2.));
  expect_vec(
    i8x16_shuffle_interleave_bytes(
      (v128_t)i8x16(1, 0, 3, 0, 5, 0, 7, 0, 9, 0, 11, 0, 13, 0, 15, 0),
      (v128_t)i8x16(0, 2, 0, 4, 0, 6, 0, 8, 0, 10, 0, 12, 0, 14, 0, 16)
    ),
    i8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
  );
  expect_vec(
    i16x8_shuffle_reverse(
      (v128_t)i16x8(1, 2, 3, 4, 5, 6, 7, 8)
    ),
    (v128_t)i16x8(8, 7, 6, 5, 4, 3, 2, 1)
  );
  expect_vec(i32x4_shuffle_reverse((v128_t)i32x4(1, 2, 3, 4)), i32x4(4, 3, 2, 1));
  expect_vec(i64x2_shuffle_reverse((v128_t)i64x2(1, 2)), i64x2(2, 1));

#ifdef  __wasm_unimplemented_simd128__

  expect_vec(
    v8x16_swizzle(
      (v128_t)i8x16(0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff),
      (v128_t)i8x16(0, 4, 8, 12, 16, 255, 129, 128, 127, 17, 15, 13, 12, 8, 4, 0)
    ),
    i8x16(0xf0, 0xf4, 0xf8, 0xfc, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0xff, 0xfd, 0xfc, 0xf8, 0xf4, 0xf0)
  );

#endif // __wasm_unimplemented_simd128__

  // i8x16 lane accesses
  expect_vec(i8x16_splat(5), i8x16(5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5));
  expect_vec(i8x16_splat(257), i8x16(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1));
  expect_eq(
    i8x16_extract_lane_s_first(
      (v128_t)i8x16(-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    ),
    -1
  );
  expect_eq(
    i8x16_extract_lane_s_last(
      (v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1)
    ),
    -1
  );
  expect_eq(
    i8x16_extract_lane_u_first(
      (v128_t)i8x16(-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    ),
    255
  );
  expect_eq(
    i8x16_extract_lane_u_last(
      (v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1)
    ),
    255
  );
  expect_vec(
    i8x16_replace_lane_first(
      (v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
      7
    ),
    i8x16(7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
  );
  expect_vec(
    i8x16_replace_lane_last(
      (v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
      7
    ),
    i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7)
  );

  // i16x8 lane accesses
  expect_vec(i16x8_splat(5), i16x8(5, 5, 5, 5, 5, 5, 5, 5));
  expect_vec(i16x8_splat(65537), i16x8(1, 1, 1, 1, 1, 1, 1, 1));
  expect_eq(i16x8_extract_lane_s_first((v128_t)i16x8(-1, 0, 0, 0, 0, 0, 0, 0)), -1);
  expect_eq(i16x8_extract_lane_s_last((v128_t)i16x8(0, 0, 0, 0, 0, 0, 0, -1)), -1);
  expect_eq(i16x8_extract_lane_u_first((v128_t)i16x8(-1, 0, 0, 0, 0, 0, 0, 0)), 65535);
  expect_eq(i16x8_extract_lane_u_last((v128_t)i16x8(0, 0, 0, 0, 0, 0, 0, -1)), 65535);
  expect_vec(
    i16x8_replace_lane_first((v128_t)i16x8(0, 0, 0, 0, 0, 0, 0, 0), 7),
    i16x8(7, 0, 0, 0, 0, 0, 0, 0)
  );
  expect_vec(
    i16x8_replace_lane_last((v128_t)i16x8(0, 0, 0, 0, 0, 0, 0, 0), 7),
    i16x8(0, 0, 0, 0, 0, 0, 0, 7)
  );

  // i32x4 lane accesses
  expect_vec(i32x4_splat(-5), i32x4(-5, -5, -5, -5));
  expect_eq(i32x4_extract_lane_first((v128_t)i32x4(-5, 0, 0, 0)), -5);
  expect_eq(i32x4_extract_lane_last((v128_t)i32x4(0, 0, 0, -5)), -5);
  expect_vec(
    i32x4_replace_lane_first((v128_t)i32x4(0, 0, 0, 0), 53),
    i32x4(53, 0, 0, 0)
  );
  expect_vec(
    i32x4_replace_lane_last((v128_t)i32x4(0, 0, 0, 0), 53),
    i32x4(0, 0, 0, 53)
  );
  // i64x2 lane accesses
  expect_vec(i64x2_splat(-5), i64x2(-5, -5));
  expect_eq(i64x2_extract_lane_first((v128_t)i64x2(-5, 0)), -5);
  expect_eq(i64x2_extract_lane_last((v128_t)i64x2(0, -5)), -5);
  expect_vec(i64x2_replace_lane_first((v128_t)i64x2(0, 0), 53), i64x2(53, 0));
  expect_vec(i64x2_replace_lane_last((v128_t)i64x2(0, 0), 53), i64x2(0, 53));

  // f32x4 lane accesses
  expect_vec(f32x4_splat(-5), f32x4(-5, -5, -5, -5));
  expect_eq(f32x4_extract_lane_first((v128_t)f32x4(-5, 0, 0, 0)), -5);
  expect_eq(f32x4_extract_lane_last((v128_t)f32x4(0, 0, 0, -5)), -5);
  expect_vec(f32x4_replace_lane_first((v128_t)f32x4(0, 0, 0, 0), 53), f32x4(53, 0, 0, 0));
  expect_vec(f32x4_replace_lane_last((v128_t)f32x4(0, 0, 0, 0), 53), f32x4(0, 0, 0, 53));

  // f64x2 lane accesses
  expect_vec(f64x2_splat(-5), f64x2(-5, -5));
  expect_eq(f64x2_extract_lane_first((v128_t)f64x2(-5, 0)), -5);
  expect_eq(f64x2_extract_lane_last((v128_t)f64x2(0, -5)), -5);
  expect_vec(f64x2_replace_lane_first((v128_t)f64x2(0, 0), 53), f64x2(53, 0));
  expect_vec(f64x2_replace_lane_last((v128_t)f64x2(0, 0), 53), f64x2(0, 53));

  // i8x16 comparisons
  expect_vec(
    i8x16_eq(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(-1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0)
  );
  expect_vec(
    i8x16_ne(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(0, -1, 0, -1, -1, -1, -1, -1, 0, -1, -1, 0, -1, -1, -1, -1)
  );
  expect_vec(
    i8x16_lt_s(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(0, 0, 0, -1, 0, -1, -1, 0, 0, 0, -1, 0, 0, -1, -1, 0)
  );
  expect_vec(
    i8x16_lt_u(
      (v128_t)u8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)u8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(0, -1, 0, 0, -1, -1, 0, -1, 0, -1, 0, 0, -1, -1, 0, -1)
  );
  expect_vec(
    i8x16_gt_s(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(0, -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, -1)
  );
  expect_vec(
    i8x16_gt_u(
      (v128_t)u8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)u8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(0, 0, 0, -1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0)
  );
  expect_vec(
    i8x16_le_s(
        (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
        (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(-1, 0, -1, -1, 0, -1, -1, 0, -1, 0, -1, -1, 0, -1, -1, 0)
  );
  expect_vec(
    i8x16_le_u(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    i8x16(-1, -1, -1, 0, -1, -1, 0, -1, -1, -1, 0, -1, -1, -1, 0, -1)
  );
  expect_vec(
    i8x16_ge_s(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    u8x16(-1, -1, -1, 0, -1, 0, 0, -1, -1, -1, 0, -1, -1, 0, 0, -1)
  );
  expect_vec(
    i8x16_ge_u(
      (v128_t)i8x16(0, 127, 13, 128,  1,  13, 129,  42, 0, 127, 255, 42,   1,  13, 129,  42),
      (v128_t)i8x16(0, 255, 13, 42, 129, 127,   0, 128, 0, 255,  13, 42, 129, 127,   0, 128)
    ),
    i8x16(-1, 0, -1, -1, 0, 0, -1, 0, -1, 0, -1, -1, 0, 0, -1, 0)
  );

  // i16x8 comparisons
  expect_vec(
    i16x8_eq(
      (v128_t)i16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)i16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(-1, 0, 0, 0, 0, 0, 0, 0)
  );
  expect_vec(
    i16x8_ne(
      (v128_t)i16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)i16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(0, -1, -1, -1, -1, -1, -1, -1)
  );
  expect_vec(
    i16x8_lt_s(
      (v128_t)i16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)i16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(0, 0, 0, -1, 0, -1, 0, -1)
  );
  expect_vec(
    i16x8_lt_u(
      (v128_t)u16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)u16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(0, 0, 0, 0, -1, 0, -1, 0)
  );
  expect_vec(
    i16x8_gt_s(
      (v128_t)i16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)i16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(0, -1, -1, 0, -1, 0, -1, 0)
  );
  expect_vec(
    i16x8_gt_u(
      (v128_t)u16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)u16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(0, -1, -1, -1, 0, -1, 0, -1)
  );
  expect_vec(
    i16x8_le_s(
      (v128_t)i16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)i16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(-1, 0, 0, -1, 0, -1, 0, -1)
  );
  expect_vec(
    i16x8_le_u(
      (v128_t)u16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)u16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(-1, 0, 0, 0, -1, 0, -1, 0)
  );
  expect_vec(
    i16x8_ge_s(
      (v128_t)i16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)i16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(-1, -1, -1, 0, -1, 0, -1, 0)
  );
  expect_vec(
    i16x8_ge_u(
      (v128_t)u16x8(0, 32767, 13, -32768,      1, -32767,     42, -25536),
      (v128_t)u16x8(0,    13,  1,  32767, -32767,     42, -25536,  32767)
    ),
    u16x8(-1, -1, -1, -1, 0, -1, 0, -1)
  );

  // i342x4 comparisons
  expect_vec(
    i32x4_eq((v128_t)i32x4(0, -1, 53, -7), (v128_t)i32x4(0, 53, -7, -1)),
    u32x4(-1, 0, 0, 0)
  );
  expect_vec(
    i32x4_ne((v128_t)i32x4(0, -1, 53, -7), (v128_t)i32x4(0, 53, -7, -1)),
    u32x4(0, -1, -1, -1)
  );
  expect_vec(
      i32x4_lt_s((v128_t)i32x4(0, -1, 53, -7), (v128_t)i32x4(0, 53, -7, -1)),
      u32x4(0, -1, 0, -1)
  );
  expect_vec(
    i32x4_lt_u((v128_t)u32x4(0, -1, 53, -7), (v128_t)u32x4(0, 53, -7, -1)),
    u32x4(0, 0, -1, -1)
  );
  expect_vec(
    i32x4_gt_s((v128_t)i32x4(0, -1, 53, -7), (v128_t)i32x4(0, 53, -7, -1)),
    u32x4(0, 0, -1, 0)
  );
  expect_vec(
    i32x4_gt_u((v128_t)u32x4(0, -1, 53, -7), (v128_t)u32x4(0, 53, -7, -1)),
    u32x4(0, -1, 0, 0)
  );
  expect_vec(
      i32x4_le_s((v128_t)i32x4(0, -1, 53, -7), (v128_t)i32x4(0, 53, -7, -1)),
      u32x4(-1, -1, 0, -1)
  );
  expect_vec(
    i32x4_le_u((v128_t)u32x4(0, -1, 53, -7), (v128_t)u32x4(0, 53, -7, -1)),
    u32x4(-1, 0, -1, -1)
  );
  expect_vec(
    i32x4_ge_s((v128_t)i32x4(0, -1, 53, -7), (v128_t)i32x4(0, 53, -7, -1)),
    u32x4(-1, 0, -1, 0)
  );
  expect_vec(
    i32x4_ge_u((v128_t)u32x4(0, -1, 53, -7), (v128_t)u32x4(0, 53, -7, -1)),
    u32x4(-1, -1, 0, 0)
  );

  // f32x4 comparisons
  expect_vec(
    f32x4_eq((v128_t)f32x4(0, -1, 1, 0), (v128_t)f32x4(0, 0, -1, 1)),
    u32x4(-1, 0, 0, 0)
  );
  expect_vec(
    f32x4_ne((v128_t)f32x4(0, -1, 1, 0), (v128_t)f32x4(0, 0, -1, 1)),
    u32x4(0, -1, -1, -1)
  );
  expect_vec(
    f32x4_lt((v128_t)f32x4(0, -1, 1, 0), (v128_t)f32x4(0, 0, -1, 1)),
    u32x4(0, -1, 0, -1)
  );
  expect_vec(
    f32x4_gt((v128_t)f32x4(0, -1, 1, 0), (v128_t)f32x4(0, 0, -1, 1)),
    u32x4(0, 0, -1, 0)
  );
  expect_vec(
    f32x4_le((v128_t)f32x4(0, -1, 1, 0), (v128_t)f32x4(0, 0, -1, 1)),
    u32x4(-1, -1, 0, -1)
  );
  expect_vec(
    f32x4_ge((v128_t)i32x4(0, -1, 1, 0), (v128_t)f32x4(0, 0, -1, 1)),
    u32x4(-1, 0, -1, 0)
  );
  expect_vec(
    f32x4_eq((v128_t)f32x4(NAN, 0, NAN, INFINITY), (v128_t)f32x4(0, NAN, NAN, INFINITY)),
    u32x4(0, 0, 0, -1)
  );
  expect_vec(
    f32x4_ne((v128_t)f32x4(NAN, 0, NAN, INFINITY), (v128_t)f32x4(0, NAN, NAN, INFINITY)),
    u32x4(-1, -1, -1, 0)
  );
  expect_vec(
    f32x4_lt((v128_t)f32x4(NAN, 0, NAN, INFINITY), (v128_t)f32x4(0, NAN, NAN, INFINITY)),
    u32x4(0, 0, 0, 0)
  );
  expect_vec(
    f32x4_gt((v128_t)f32x4(NAN, 0, NAN, INFINITY), (v128_t)f32x4(0, NAN, NAN, INFINITY)),
    u32x4(0, 0, 0, 0)
  );
  expect_vec(
    f32x4_le((v128_t)f32x4(NAN, 0, NAN, INFINITY), (v128_t)f32x4(0, NAN, NAN, INFINITY)),
    u32x4(0, 0, 0, -1)
  );
  expect_vec(
    f32x4_ge((v128_t)f32x4(NAN, 0, NAN, INFINITY), (v128_t)f32x4(0, NAN, NAN, INFINITY)),
    u32x4(0, 0, 0, -1)
  );
  expect_vec(
    f32x4_eq((v128_t)f32x4(-INFINITY, 0, NAN, -INFINITY), (v128_t)f32x4(0, INFINITY, INFINITY, NAN)),
    u32x4(0, 0, 0, 0)
  );
  expect_vec(
    f32x4_ne((v128_t)f32x4(-INFINITY, 0, NAN, -INFINITY), (v128_t)f32x4(0, INFINITY, INFINITY, NAN)),
    u32x4(-1, -1, -1, -1)
  );
  expect_vec(
    f32x4_lt((v128_t)f32x4(-INFINITY, 0, NAN, -INFINITY), (v128_t)f32x4(0, INFINITY, INFINITY, NAN)),
    u32x4(-1, -1, 0, 0)
  );
  expect_vec(
    f32x4_gt((v128_t)f32x4(-INFINITY, 0, NAN, -INFINITY), (v128_t)f32x4(0, INFINITY, INFINITY, NAN)),
    u32x4(0, 0, 0, 0)
  );
  expect_vec(
    f32x4_le((v128_t)f32x4(-INFINITY, 0, NAN, -INFINITY), (v128_t)f32x4(0, INFINITY, INFINITY, NAN)),
    u32x4(-1, -1, 0, 0)
  );
  expect_vec(
    f32x4_ge((v128_t)f32x4(-INFINITY, 0, NAN, -INFINITY), (v128_t)f32x4(0, INFINITY, INFINITY, NAN)),
    u32x4(0, 0, 0, 0)
  );

  // f64x2 comparisons
  expect_vec(f64x2_eq((v128_t)f64x2(0, 1), (v128_t)f64x2(0, 0)), u64x2(-1, 0));
  expect_vec(f64x2_ne((v128_t)f64x2(0, 1), (v128_t)f64x2(0, 0)), u64x2(0, -1));
  expect_vec(f64x2_lt((v128_t)f64x2(0, 1), (v128_t)f64x2(0, 0)), u64x2(0, 0));
  expect_vec(f64x2_gt((v128_t)f64x2(0, 1), (v128_t)f64x2(0, 0)), u64x2(0, -1));
  expect_vec(f64x2_le((v128_t)f64x2(0, 1), (v128_t)f64x2(0, 0)), u64x2(-1, 0));
  expect_vec(f64x2_ge((v128_t)f64x2(0, 1), (v128_t)f64x2(0, 0)), u64x2(-1, -1));
  expect_vec(f64x2_eq((v128_t)f64x2(NAN, 0), (v128_t)f64x2(INFINITY, INFINITY)), u64x2(0, 0));
  expect_vec(f64x2_ne((v128_t)f64x2(NAN, 0), (v128_t)f64x2(INFINITY, INFINITY)), u64x2(-1, -1));
  expect_vec(f64x2_lt((v128_t)f64x2(NAN, 0), (v128_t)f64x2(INFINITY, INFINITY)), u64x2(0, -1));
  expect_vec(f64x2_gt((v128_t)f64x2(NAN, 0), (v128_t)f64x2(INFINITY, INFINITY)), u64x2(0, 0));
  expect_vec(f64x2_le((v128_t)f64x2(NAN, 0), (v128_t)f64x2(INFINITY, INFINITY)), u64x2(0, -1));
  expect_vec(f64x2_ge((v128_t)f64x2(NAN, 0), (v128_t)f64x2(INFINITY, INFINITY)), u64x2(0, 0));

  // bitwise operations
  expect_vec(v128_not((v128_t)i32x4(0, -1, 0, -1)), (v128_t)i32x4(-1, 0, -1, 0));
  expect_vec(
    v128_and((v128_t)i32x4(0, 0, -1, -1), (v128_t)i32x4(0, -1, 0, -1)),
    i32x4(0, 0, 0, -1)
  );
  expect_vec(
    v128_or((v128_t)i32x4(0, 0, -1, -1), (v128_t)i32x4(0, -1, 0, -1)),
    i32x4(0, -1, -1, -1)
  );
  expect_vec(
    v128_xor((v128_t)i32x4(0, 0, -1, -1), (v128_t)i32x4(0, -1, 0, -1)),
    i32x4(0, -1, -1, 0)
  );

#ifdef __wasm_unimplemented_simd128__

  expect_vec(
    v128_andnot((v128_t)i32x4(0, 0, -1, -1), (v128_t)i32x4(0, -1, 0, -1)),
    i32x4(0, -1, -1, 0)
  );

#endif // __wasm_unimplemented_simd128__

  expect_vec(
    v128_bitselect(
      (v128_t)i32x4(0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA),
      (v128_t)i32x4(0xBBBBBBBB, 0xBBBBBBBB, 0xBBBBBBBB, 0xBBBBBBBB),
      (v128_t)i32x4(0xF0F0F0F0, 0xFFFFFFFF, 0x00000000, 0xFF00FF00)
    ),
    i32x4(0xABABABAB, 0xAAAAAAAA, 0xBBBBBBBB, 0xAABBAABB)
  );

  // i8x16 arithmetic
  expect_vec(
    i8x16_abs((v128_t)i8x16(0, 1, 42, -3, -56, 127, -128, -126, 0, -1, -42, 3, 56, -127, -128, 126)),
    i8x16(0, 1, 42, 3, 56, 127, -128, 126, 0, 1, 42, 3, 56, 127, -128, 126)
  );
  expect_vec(
    i8x16_neg((v128_t)i8x16(0, 1, 42, -3, -56, 127, -128, -126, 0, -1, -42, 3, 56, -127, -128, 126)),
    i8x16(0, -1, -42, 3, 56, -127, -128, 126, 0, 1, 42, -3, -56, 127, -128, -126)
  );
  expect_eq(i8x16_any_true((v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)), 0);
  expect_eq(i8x16_any_true((v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0)), 1);
  expect_eq(i8x16_any_true((v128_t)i8x16(1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)), 1);
  expect_eq(i8x16_any_true((v128_t)i8x16(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)), 1);
  expect_eq(i8x16_all_true((v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)), 0);
  expect_eq(i8x16_all_true((v128_t)i8x16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0)), 0);
  expect_eq(i8x16_all_true((v128_t)i8x16(1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)), 0);
  expect_eq(i8x16_all_true((v128_t)i8x16(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)), 1);
  expect_vec(
    i8x16_shl((v128_t)i8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64), 1),
    i8x16(0, 2, 4, 8, 16, 32, 64, -128, 0, 6, 12, 24, 48, 96, -64, -128)
  );
  expect_vec(
    i8x16_shl((v128_t)i8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64), 8),
    i8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64)
  );
  expect_vec(
    i8x16_shr_u((v128_t)u8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64), 1),
    u8x16(0, 0, 1, 2, 4, 8, 16, 32, 64, 1, 3, 6, 12, 24, 48, 96)
  );
  expect_vec(
    i8x16_shr_u((v128_t)u8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64), 8),
    u8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64)
  );
  expect_vec(
    i8x16_shr_s((v128_t)i8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64), 1),
    i8x16(0, 0, 1, 2, 4, 8, 16, 32, -64, 1, 3, 6, 12, 24, 48, -32)
  );
  expect_vec(
    i8x16_shr_s((v128_t)i8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64), 8),
    i8x16(0, 1, 2, 4, 8, 16, 32, 64, -128, 3, 6, 12, 24, 48, 96, -64)
  );
  expect_vec(
    i8x16_add(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(3, 17, 0, 0, 0, 135, 109, 46, 145, 225, 48, 184, 17, 249, 128, 215)
  );
  expect_vec(
    i8x16_add_saturate_s(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(3, 17, 0, 128, 0, 135, 109, 46, 127, 225, 48, 184, 17, 249, 127, 215)
  );
  expect_vec(
    i8x16_add_saturate_u(
      (v128_t)u8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)u8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    u8x16(3, 255, 255, 255, 255, 135, 109, 46, 145, 225, 255, 184, 17, 255, 128, 215)
  );
  expect_vec(
    i8x16_sub(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(253, 67, 254, 0, 254, 123, 159, 12, 61, 167, 158, 100, 17, 251, 130, 187)
  );
  expect_vec(
    i8x16_sub_saturate_s(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(253, 67, 254, 0, 127, 128, 159, 12, 61, 167, 158, 128, 17, 251, 130, 127)
  );
  expect_vec(
    i8x16_sub_saturate_u(
      (v128_t)u8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)u8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    u8x16(0, 0, 254, 0, 0, 123, 0, 12, 61, 167, 158, 100, 17, 0, 0, 0)
  );

#ifdef __wasm_unimplemented_simd128__

  expect_vec(
    i8x16_mul(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(0, 230, 255, 0, 255, 6, 106, 237, 230, 52, 223, 76, 0, 6, 127, 126)
  );

#endif // __wasm_unimplemented_simd128__

  expect_vec(
    i8x16_min_s(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(0, 231, 255, 128, 129, 129, 6, 17, 42, 196, 231, 142, 0, 250, 1, 142)
  );
  expect_vec(
    i8x16_min_u(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(0, 42, 1, 128, 127, 6, 6, 17, 42, 29, 73, 42, 0, 250, 1, 73)
  );
  expect_vec(
    i8x16_max_s(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(3, 42, 1, 128, 127, 6, 103, 29, 103, 29, 73, 42, 17, 255, 127, 73)
  );
  expect_vec(
    i8x16_max_u(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(3, 231, 255, 128, 129, 129, 103, 29, 103, 196, 231, 142, 17, 255, 127, 142)
  );
  expect_vec(
    i8x16_avgr_u(
      (v128_t)i8x16(0, 42, 255, 128, 127, 129, 6, 29, 103, 196, 231, 142, 17, 250, 1, 73),
      (v128_t)i8x16(3, 231, 1, 128, 129, 6, 103, 17, 42, 29, 73, 42, 0, 255, 127, 142)
    ),
    i8x16(2, 137, 128, 128, 128, 68, 55, 23, 73, 113, 152, 92, 9, 253, 64, 108)
  );

  // i16x8 arithmetic
  expect_vec(
    i16x8_abs((v128_t)i16x8(0, 1, 42, -3, -56, 32767, -32768, 32766)),
    i16x8(0, 1, 42, 3, 56, 32767, -32768, 32766)
  );
  expect_vec(
    i16x8_neg((v128_t)i16x8(0, 1, 42, -3, -56, 32767, -32768, 32766)),
    i16x8(0, -1, -42, 3, 56, -32767, -32768, -32766)
  );
  expect_eq(i16x8_any_true((v128_t)i16x8(0, 0, 0, 0, 0, 0, 0, 0)), 0);
  expect_eq(i16x8_any_true((v128_t)i16x8(0, 0, 1, 0, 0, 0, 0, 0)), 1);
  expect_eq(i16x8_any_true((v128_t)i16x8(1, 1, 1, 1, 1, 0, 1, 1)), 1);
  expect_eq(i16x8_any_true((v128_t)i16x8(1, 1, 1, 1, 1, 1, 1, 1)), 1);
  expect_eq(i16x8_all_true((v128_t)i16x8(0, 0, 0, 0, 0, 0, 0, 0)), 0);
  expect_eq(i16x8_all_true((v128_t)i16x8(0, 0, 1, 0, 0, 0, 0, 0)), 0);
  expect_eq(i16x8_all_true((v128_t)i16x8(1, 1, 1, 1, 1, 0, 1, 1)), 0);
  expect_eq(i16x8_all_true((v128_t)i16x8(1, 1, 1, 1, 1, 1, 1, 1)), 1);
  expect_vec(
    i16x8_shl((v128_t)i16x8(0, 8, 16, 128, 256, 2048, 4096, -32768), 1),
    i16x8(0, 16, 32, 256, 512, 4096, 8192, 0)
  );
  expect_vec(
    i16x8_shl((v128_t)i16x8(0, 8, 16, 128, 256, 2048, 4096, -32768), 16),
    i16x8(0, 8, 16, 128, 256, 2048, 4096, -32768)
  );
  expect_vec(
    i16x8_shr_u((v128_t)u16x8(0, 8, 16, 128, 256, 2048, 4096, -32768), 1),
    u16x8(0, 4, 8, 64, 128, 1024, 2048, 16384)
  );
  expect_vec(
    i16x8_shr_u((v128_t)u16x8(0, 8, 16, 128, 256, 2048, 4096, -32768), 16),
    u16x8(0, 8, 16, 128, 256, 2048, 4096, -32768)
  );
  expect_vec(
    i16x8_shr_s((v128_t)i16x8(0, 8, 16, 128, 256, 2048, 4096, -32768), 1),
    i16x8(0, 4, 8, 64, 128, 1024, 2048, -16384)
  );
  expect_vec(
    i16x8_shr_s((v128_t)i16x8(0, 8, 16, 128, 256, 2048, 4096, -32768), 16),
    i16x8(0, 8, 16, 128, 256, 2048, 4096, -32768)
  );
  expect_vec(
    i16x8_add(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(768, -255, 0, 0, -30976, 12288, -1792, -32768)
  );
  expect_vec(
    i16x8_add_saturate_s(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(768, -255, -32768, 0, -30976, 12288, -1792, 32767)
  );
  expect_vec(
    i16x8_add_saturate_u(
      (v128_t)u16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)u16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    u16x8(768, -255, -1, -1, -30976, -1, -1, -32768)
  );
  expect_vec(
    i16x8_sub(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(-768, -257, 0, -512, 31488, -25088, -1280, 32764)
  );
  expect_vec(
    i16x8_sub_saturate_s(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(-768, -257, 0, 32767, -32768, -25088, -1280, 32764)
  );
  expect_vec(
    i16x8_sub_saturate_u(
      (v128_t)u16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)u16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    u16x8(0, -257, 0, 0, 31488, -25088, 0, 32764)
  );
  expect_vec(
    i16x8_mul(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(0, -256, 0, 0, 0, 0, 0, -4)
  );
  expect_vec(
    i16x8_min_s(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(0, -256, -32768, -32512, -32512, -6400, -1536, 2)
  );
  expect_vec(
    i16x8_min_u(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(0, 1, -32768, 32512, 1536, 18688, -1536, 2)
  );
  expect_vec(
    i16x8_max_s(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(768, 1, -32768, 32512, 1536, 18688, -256, 32766)
  );
  expect_vec(
    i16x8_max_u(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(768, -256, -32768, -32512, -32512, -6400, -256, 32766)
  );
  expect_vec(
    i16x8_max_u(
      (v128_t)i16x8(0, -256, -32768, 32512, -32512, -6400, -1536, 32766),
      (v128_t)i16x8(768, 1, -32768, -32512, 1536, 18688, -256, 2)
    ),
    i16x8(768, -256, -32768, -32512, -32512, -6400, -256, 32766)
  );

  // i32x4 arithmetic
  expect_vec(
    i32x4_abs((v128_t)i32x4(0, 1, 0x80000000, 0x80000001)),
    i32x4(0, 1, 0x80000000, 0x7fffffff)
  );
  expect_vec(
    i32x4_neg((v128_t)i32x4(0, 1, 0x80000000, 0x80000001)),
    i32x4(0, -1, 0x80000000, 0x7fffffff)
  );
  expect_eq(i32x4_any_true((v128_t)i32x4(0, 0, 0, 0)), 0);
  expect_eq(i32x4_any_true((v128_t)i32x4(0, 0, 1, 0)), 1);
  expect_eq(i32x4_any_true((v128_t)i32x4(1, 0, 1, 1)), 1);
  expect_eq(i32x4_any_true((v128_t)i32x4(1, 1, 1, 1)), 1);
  expect_eq(i32x4_all_true((v128_t)i32x4(0, 0, 0, 0)), 0);
  expect_eq(i32x4_all_true((v128_t)i32x4(0, 0, 1, 0)), 0);
  expect_eq(i32x4_all_true((v128_t)i32x4(1, 0, 1, 1)), 0);
  expect_eq(i32x4_all_true((v128_t)i32x4(1, 1, 1, 1)), 1);
  expect_vec(
    i32x4_shl((v128_t)i32x4(1, 0x40000000, 0x80000000, -1), 1),
    i32x4(2, 0x80000000, 0, -2)
  );
  expect_vec(
    i32x4_shl((v128_t)i32x4(1, 0x40000000, 0x80000000, -1), 32),
    i32x4(1, 0x40000000, 0x80000000, -1)
  );
  expect_vec(
    i32x4_shr_s((v128_t)i32x4(1, 0x40000000, 0x80000000, -1), 1),
    i32x4(0, 0x20000000, 0xc0000000, -1)
  );
  expect_vec(
    i32x4_shr_s((v128_t)i32x4(1, 0x40000000, 0x80000000, -1), 32),
    i32x4(1, 0x40000000, 0x80000000, -1)
  );
  expect_vec(
    i32x4_shr_u((v128_t)u32x4(1, 0x40000000, 0x80000000, -1), 1),
    u32x4(0, 0x20000000, 0x40000000, 0x7fffffff)
  );
  expect_vec(
    i32x4_shr_u((v128_t)u32x4(1, 0x40000000, 0x80000000, -1), 32),
    u32x4(1, 0x40000000, 0x80000000, -1)
  );
  expect_vec(
    i32x4_add((v128_t)i32x4(0, 0x80000001, 42, 5), (v128_t)i32x4(0, 0x80000001, 5, 42)),
    i32x4(0, 2, 47, 47)
  );
  expect_vec(
    i32x4_sub((v128_t)i32x4(0, 2, 47, 47), (v128_t)i32x4(0, 0x80000001, 42, 5)),
    i32x4(0, 0x80000001, 5, 42)
  );
  expect_vec(
    i32x4_mul((v128_t)i32x4(0, 0x80000001, 42, 5), (v128_t)i32x4(0, 0x80000001, 42, 5)),
    i32x4(0, 1, 1764, 25)
  );
  expect_vec(
      i32x4_min_s((v128_t)i32x4(0, 0x80000001, 42, -5), (v128_t)i32x4(0x80000001, -42, 5, 0)),
      i32x4(0x80000001, 0x80000001, 5, -5)
  );
  expect_vec(
      i32x4_min_u((v128_t)i32x4(0, 0x80000001, 42, -5), (v128_t)i32x4(0x80000001, -42, 5, 0)),
      i32x4(0, 0x80000001, 5, 0)
  );
  expect_vec(
      i32x4_max_s((v128_t)i32x4(0, 0x80000001, 42, -5), (v128_t)i32x4(0x80000001, -42, 5, 0)),
      i32x4(0, -42, 42, 0)
  );
  expect_vec(
      i32x4_max_u((v128_t)i32x4(0, 0x80000001, 42, -5), (v128_t)i32x4(0x80000001, -42, 5, 0)),
      i32x4(0x80000001, -42, 42, -5)
  );

  // i64x2 arithmetic
  expect_vec(i64x2_neg((v128_t)i64x2(0x8000000000000000, 42)), i64x2(0x8000000000000000, -42));
  /* expect_eq(i64x2_any_true((v128_t)i64x2(0, 0)), 0); */
  /* expect_eq(i64x2_any_true((v128_t)i64x2(1, 0)), 1); */
  /* expect_eq(i64x2_any_true((v128_t)i64x2(1, 1)), 1); */
  /* expect_eq(i64x2_all_true((v128_t)i64x2(0, 0)), 0); */
  /* expect_eq(i64x2_all_true((v128_t)i64x2(1, 0)), 0); */
  /* expect_eq(i64x2_all_true((v128_t)i64x2(1, 1)), 1); */

  expect_vec(i64x2_shl((v128_t)i64x2(1, 0x8000000000000000), 1), i64x2(2, 0));
  expect_vec(i64x2_shl((v128_t)i64x2(1, 0x8000000000000000), 64), i64x2(1, 0x8000000000000000));
  expect_vec(i64x2_shr_s((v128_t)i64x2(1, 0x8000000000000000), 1), i64x2(0, 0xc000000000000000));
  expect_vec(i64x2_shr_s((v128_t)i64x2(1, 0x8000000000000000), 64), i64x2(1, 0x8000000000000000));
  expect_vec(i64x2_shr_u((v128_t)u64x2(1, 0x8000000000000000), 1), u64x2(0, 0x4000000000000000));
  expect_vec(i64x2_shr_u((v128_t)u64x2(1, 0x8000000000000000), 64), u64x2(1, 0x8000000000000000));
  expect_vec(
    i64x2_add((v128_t)i64x2(0x8000000000000001, 42), (v128_t)i64x2(0x8000000000000001, 0)),
    i64x2(2, 42)
  );
  expect_vec(
    i64x2_sub((v128_t)i64x2(2, 42), (v128_t)i64x2(0x8000000000000001, 0)),
    i64x2(0x8000000000000001, 42)
  );

  // f32x4 arithmetic
  expect_vec(f32x4_abs((v128_t)f32x4(-0., NAN, -INFINITY, 5)), f32x4(0, NAN, INFINITY, 5));
  expect_vec(f32x4_neg((v128_t)f32x4(-0., NAN, -INFINITY, 5)), f32x4(0, -NAN, INFINITY, -5));
  expect_vec(f32x4_sqrt((v128_t)f32x4(0., NAN, INFINITY, 4)), f32x4(-0., NAN, INFINITY, 2));
  // TODO: test QFMA/QFMS
  expect_vec(
    f32x4_add((v128_t)f32x4(NAN, -NAN, INFINITY, 42), (v128_t)f32x4(42, INFINITY, INFINITY, 1)),
    f32x4(NAN, -NAN, INFINITY, 43)
  );
  expect_vec(
    f32x4_sub((v128_t)f32x4(NAN, -NAN, INFINITY, 42), (v128_t)f32x4(42, INFINITY, -INFINITY, 1)),
    f32x4(NAN, -NAN, INFINITY, 41)
  );
  expect_vec(
    f32x4_mul((v128_t)f32x4(NAN, -NAN, INFINITY, 42), (v128_t)f32x4(42, INFINITY, INFINITY, 2)),
    f32x4(NAN, -NAN, INFINITY, 84)
  );
  expect_vec(
    f32x4_div((v128_t)f32x4(NAN, -NAN, INFINITY, 42), (v128_t)f32x4(42, INFINITY, 2, 2)),
    f32x4(NAN, -NAN, INFINITY, 21)
  );
  expect_vec(
    f32x4_min((v128_t)f32x4(-0., 0, NAN, 5), (v128_t)f32x4(0, -0., 5, NAN)),
    f32x4(-0., -0., NAN, NAN)
  );
  expect_vec(
    f32x4_max((v128_t)f32x4(-0., 0, NAN, 5), (v128_t)f32x4(0, -0., 5, NAN)),
    f32x4(0, 0, NAN, NAN)
  );

  // f64x2 arithmetic
  expect_vec(f64x2_abs((v128_t)f64x2(-0., NAN)), f64x2(0, NAN));
  expect_vec(f64x2_abs((v128_t)f64x2(-INFINITY, 5)), f64x2(INFINITY, 5));
  expect_vec(f64x2_neg((v128_t)f64x2(-0., NAN)), f64x2(0, -NAN));
  expect_vec(f64x2_neg((v128_t)f64x2(-INFINITY, 5)), f64x2(INFINITY, -5));
  // TODO: test QFMA/QFMS
  // https://bugs.chromium.org/p/v8/issues/detail?id=10170
  //  expect_vec(f64x2_sqrt((v128_t)f64x2(-0., NAN)), f64x2(-0., NAN));
  //expect_vec(f64x2_sqrt((v128_t)f64x2(INFINITY, 4)), f64x2(INFINITY, 2));
  expect_vec(
    f64x2_add((v128_t)f64x2(NAN, -NAN), (v128_t)f64x2(42, INFINITY)),
    f64x2(NAN, -NAN)
  );
  expect_vec(
    f64x2_add((v128_t)f64x2(INFINITY, 42), (v128_t)f64x2(INFINITY, 1)),
    f64x2(INFINITY, 43)
  );
  expect_vec(
    f64x2_sub((v128_t)f64x2(NAN, -NAN), (v128_t)f64x2(42, INFINITY)),
    f64x2(NAN, -NAN)
  );
  expect_vec(
    f64x2_sub((v128_t)f64x2(INFINITY, 42), (v128_t)f64x2(-INFINITY, 1)),
    f64x2(INFINITY, 41)
  );
  expect_vec(
    f64x2_mul((v128_t)f64x2(NAN, -NAN), (v128_t)f64x2(42, INFINITY)),
    f64x2(NAN, -NAN)
  );
  expect_vec(
    f64x2_mul((v128_t)f64x2(INFINITY, 42), (v128_t)f64x2(INFINITY, 2)),
    f64x2(INFINITY, 84)
  );
  expect_vec(
    f64x2_div((v128_t)f64x2(NAN, -NAN), (v128_t)f64x2(42, INFINITY)),
    f64x2(NAN, -NAN)
  );
  expect_vec(
    f64x2_div((v128_t)f64x2(INFINITY, 42), (v128_t)f64x2(2, 2)),
    f64x2(INFINITY, 21)
  );
  expect_vec(f64x2_min((v128_t)f64x2(-0., 0), (v128_t)f64x2(0, -0)), f64x2(-0., -0));
  expect_vec(f64x2_min((v128_t)f64x2(NAN, 5), (v128_t)f64x2(5, NAN)), f64x2(NAN, NAN));
  expect_vec(f64x2_max((v128_t)f64x2(-0., 0), (v128_t)f64x2(0, -0)), f64x2(0, 0));
  expect_vec(f64x2_max((v128_t)f64x2(NAN, 5), (v128_t)f64x2(5, NAN)), f64x2(NAN, NAN));

  // conversions
  expect_vec(
    i32x4_trunc_s_f32x4_sat((v128_t)f32x4(42, NAN, INFINITY, -INFINITY)),
    i32x4(42, 0, 2147483647, -2147483648ll)
  );
  expect_vec(
    i32x4_trunc_u_f32x4_sat((v128_t)f32x4(42, NAN, INFINITY, -INFINITY)),
    u32x4(42, 0, 4294967295ull, 0)
  );
  expect_vec(
    f32x4_convert_s_i32x4((v128_t)i32x4(0, -1, 2147483647, -2147483647 - 1)),
    f32x4(0, -1, 2147483648., -2147483648.)
  );
  expect_vec(
    f32x4_convert_u_i32x4((v128_t)u32x4(0, -1, 2147483647, -2147483647 - 1)),
    f32x4(0, 4294967296., 2147483648., 2147483648.)
  );
  expect_vec(
    i8x16_narrow_i16x8_s(
      (v128_t)i16x8(129, 127, -32767, 32767, -32768, -1, 1, 0),
      (v128_t)i16x8(0, 1, -1, -32768, 32767, -32767, 127, 129)
    ),
    i8x16(127, 127, -128, 127, -128, -1, 1, 0, 0, 1, -1, -128, 127, -128, 127, 127)
  );
  expect_vec(
    i8x16_narrow_i16x8_u(
      (v128_t)i16x8(129, 127, -32767, 32767, -32768, -1, 1, 0),
      (v128_t)i16x8(0, 1, -1, -32768, 32767, -32767, 127, 129)
    ),
    i8x16(129, 127, 0, 255, 0, 0, 1, 0, 0, 1, 0, 0, 255, 0, 127, 129)
  );
  expect_vec(
    i16x8_narrow_i32x4_s(
      (v128_t)i32x4(32769, 32767, -2147483647, 2147483647),
      (v128_t)i32x4(0, 1, -1, -2147483647 - 1)
    ),
    i16x8(32767, 32767, -32768, 32767, 0, 1, -1, -32768)
  );
  expect_vec(
    i16x8_narrow_i32x4_u(
      (v128_t)i32x4(32769, 32767, -2147483647, 2147483647),
      (v128_t)i32x4(0, 1, -1, -2147483647 - 1)
    ),
    i16x8(-32767, 32767, 0, -1, 0, 1, 0, 0)
  );
  expect_vec(
    i16x8_widen_low_i8x16_s(
      (v128_t)i8x16(0, 1, -1, -128, 127, 129, 64, -64, -64, 64, 129, 127, -128, -1, 1, 0)
    ),
    i16x8(0, 1, -1, -128, 127, -127, 64, -64)
  );
  expect_vec(
    i16x8_widen_high_i8x16_s(
      (v128_t)i8x16(0, 1, -1, -128, 127, 129, 64, -64, -64, 64, 129, 127, -128, -1, 1, 0)
    ),
    i16x8(-64, 64, -127, 127, -128, -1, 1, 0)
  );
  expect_vec(
    i16x8_widen_low_i8x16_u(
      (v128_t)i8x16(0, 1, -1, -128, 127, 129, 64, -64, -64, 64, 129, 127, -128, -1, 1, 0)
    ),
    i16x8(0, 1, 255, 128, 127, 129, 64, 192)
  );
  expect_vec(
    i16x8_widen_high_i8x16_u(
      (v128_t)i8x16(0, 1, -1, -128, 127, 129, 64, -64, -64, 64, 129, 127, -128, -1, 1, 0)
    ),
    i16x8(192, 64, 129, 127, 128, 255, 1, 0)
  );
  expect_vec(i32x4_widen_low_i16x8_s((v128_t)i16x8(0, 1, -1, -32768, 32767, -32767, 16384, -16384)), i32x4(0, 1, -1, -32768));
  expect_vec(i32x4_widen_high_i16x8_s((v128_t)i16x8(0, 1, -1, -32768, 32767, -32767, 16384, -16384)), i32x4(32767, -32767, 16384, -16384));
  expect_vec(i32x4_widen_low_i16x8_u((v128_t)i16x8(0, 1, -1, -32768, 32767, -32767, 16384, -16384)), i32x4(0, 1, 65535, 32768));
  expect_vec(i32x4_widen_high_i16x8_u((v128_t)i16x8(0, 1, -1, -32768, 32767, -32767, 16384, -16384)), i32x4(32767, 32769, 16384, 49152));

  if (failures == 0) {
    printf("Success!\n");
  } else {
    printf("Failed :(\n");
  }
}

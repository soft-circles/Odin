package main

import "base:intrinsics"

@(export) mat3_mul     :: proc "c" (a, b: ^matrix[3, 3]f32, out: ^matrix[3, 3]f32) { out^ = a^ * b^ }
@(export) mat4_mul     :: proc "c" (a, b: ^matrix[4, 4]f32, out: ^matrix[4, 4]f32) { out^ = a^ * b^ }
@(export) mat3_mul_vec :: proc "c" (a: ^matrix[3, 3]f32, v: ^[3]f32, out: ^[3]f32) { out^ = a^ * v^ }
@(export) vec_mul_mat3 :: proc "c" (v: ^[3]f32, a: ^matrix[3, 3]f32, out: ^[3]f32) { out^ = v^ * a^ }
@(export) mat2_mul_f64 :: proc "c" (a, b: ^matrix[2, 2]f64, out: ^matrix[2, 2]f64) { out^ = a^ * b^ }

// An explicit fused multiply-add is still a request for fused rounding.
@(export) explicit_fma :: proc "c" (a, b, c: f32) -> f32 { return intrinsics.fused_mul_add(a, b, c) }

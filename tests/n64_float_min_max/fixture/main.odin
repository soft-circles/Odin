package main

@(export) min_f32   :: proc "c" (a, b: f32) -> f32 { return min(a, b) }
@(export) max_f32   :: proc "c" (a, b: f32) -> f32 { return max(a, b) }
@(export) min_f64   :: proc "c" (a, b: f64) -> f64 { return min(a, b) }
@(export) max_f64   :: proc "c" (a, b: f64) -> f64 { return max(a, b) }
@(export) min3_f32  :: proc "c" (a, b, c: f32) -> f32 { return min(a, b, c) }
@(export) clamp_f32 :: proc "c" (x, lo, hi: f32) -> f32 { return clamp(x, lo, hi) }

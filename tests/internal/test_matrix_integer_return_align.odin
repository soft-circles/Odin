package test_internal

import "core:testing"

// Matrix returns can be coerced to integers such as i24, i40, i48, and i56.
// Keep the call boundary and consume every element: zero/discarded returns
// cannot detect corruption when the ABI temporary has incorrect alignment.
@test
test_matrix_integer_return_align :: proc(t: ^testing.T) {
	check_return :: proc(t: ^testing.T, $T: typeid, $R: u32, $C: u32) {
		return_matrix :: #force_no_inline proc(seed: i32) -> matrix[R, C]T {
			result: matrix[R, C]T
			for row in 0..<R {
				for col in 0..<C {
					result[row, col] = T(seed + i32(row*C + col))
				}
			}
			return result
		}
		seeds := [?]i32{-31, 17}
		for seed in seeds {
			result := return_matrix(seed)
			for row in 0..<R {
				for col in 0..<C {
					testing.expect_value(t, result[row, col], T(seed + i32(row*C + col)))
				}
			}
		}
	}
	check_return(t, i8, 1, 3)
	check_return(t, i8, 1, 5)
	check_return(t, i8, 1, 6)
	check_return(t, i8, 6, 1)
	check_return(t, i8, 1, 7)
	check_return(t, i16, 3, 1)
	check_return(t, i16, 1, 3)
	// Preserve all lanes of the half-vector return signatures used by the
	// existing matrix-alignment sweep, including two-register aggregates.
	check_return(t, f16, 1, 6)
	check_return(t, f16, 2, 3)
	check_return(t, f16, 4, 2)
}

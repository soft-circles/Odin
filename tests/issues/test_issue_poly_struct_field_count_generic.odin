// Tests a struct intrinsic on a polymorphic instantiation whose parameter is still generic
package test_issues

import "base:intrinsics"
import "core:testing"

Poly_Pair :: struct($T: typeid) {
	a, b: T,
}

first_of_pair :: proc($T: typeid, x: [intrinsics.type_struct_field_count(Poly_Pair(T))]T) -> T {
	return x[0]
}

@(test)
test_field_count_of_generic_instantiation :: proc(t: ^testing.T) {
	testing.expect_value(t, first_of_pair(int, {1, 2}), 1)
}

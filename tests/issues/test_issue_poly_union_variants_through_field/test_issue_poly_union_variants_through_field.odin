// Tests reading a polymorphic union's variants reached through another record's field, not through the
// union's own instantiations: `Poly_Union_Holder_N` instantiates `Poly_Union_N(int)`, which builds
// `Poly_Back_N(int)` first, and `POLY_UNION_COUNT_N` must wait for the union's remaining variants.
package test_issue_poly_union_variants_through_field

import "base:intrinsics"

Poly_Union_0 :: union($T: typeid) {
	Poly_Back_0(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_0 :: struct($T: typeid) { p: ^Poly_Union_0(T) }
Poly_Union_Holder_0 :: struct { u: Poly_Union_0(int) }
POLY_UNION_COUNT_0 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_0(int), "p")))
#assert(POLY_UNION_COUNT_0 == 9)

Poly_Union_1 :: union($T: typeid) {
	Poly_Back_1(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_1 :: struct($T: typeid) { p: ^Poly_Union_1(T) }
Poly_Union_Holder_1 :: struct { u: Poly_Union_1(int) }
POLY_UNION_COUNT_1 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_1(int), "p")))
#assert(POLY_UNION_COUNT_1 == 9)

Poly_Union_2 :: union($T: typeid) {
	Poly_Back_2(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_2 :: struct($T: typeid) { p: ^Poly_Union_2(T) }
Poly_Union_Holder_2 :: struct { u: Poly_Union_2(int) }
POLY_UNION_COUNT_2 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_2(int), "p")))
#assert(POLY_UNION_COUNT_2 == 9)

Poly_Union_3 :: union($T: typeid) {
	Poly_Back_3(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_3 :: struct($T: typeid) { p: ^Poly_Union_3(T) }
Poly_Union_Holder_3 :: struct { u: Poly_Union_3(int) }
POLY_UNION_COUNT_3 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_3(int), "p")))
#assert(POLY_UNION_COUNT_3 == 9)

Poly_Union_4 :: union($T: typeid) {
	Poly_Back_4(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_4 :: struct($T: typeid) { p: ^Poly_Union_4(T) }
Poly_Union_Holder_4 :: struct { u: Poly_Union_4(int) }
POLY_UNION_COUNT_4 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_4(int), "p")))
#assert(POLY_UNION_COUNT_4 == 9)

Poly_Union_5 :: union($T: typeid) {
	Poly_Back_5(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_5 :: struct($T: typeid) { p: ^Poly_Union_5(T) }
Poly_Union_Holder_5 :: struct { u: Poly_Union_5(int) }
POLY_UNION_COUNT_5 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_5(int), "p")))
#assert(POLY_UNION_COUNT_5 == 9)

Poly_Union_6 :: union($T: typeid) {
	Poly_Back_6(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_6 :: struct($T: typeid) { p: ^Poly_Union_6(T) }
Poly_Union_Holder_6 :: struct { u: Poly_Union_6(int) }
POLY_UNION_COUNT_6 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_6(int), "p")))
#assert(POLY_UNION_COUNT_6 == 9)

Poly_Union_7 :: union($T: typeid) {
	Poly_Back_7(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_7 :: struct($T: typeid) { p: ^Poly_Union_7(T) }
Poly_Union_Holder_7 :: struct { u: Poly_Union_7(int) }
POLY_UNION_COUNT_7 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_7(int), "p")))
#assert(POLY_UNION_COUNT_7 == 9)

Poly_Union_8 :: union($T: typeid) {
	Poly_Back_8(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_8 :: struct($T: typeid) { p: ^Poly_Union_8(T) }
Poly_Union_Holder_8 :: struct { u: Poly_Union_8(int) }
POLY_UNION_COUNT_8 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_8(int), "p")))
#assert(POLY_UNION_COUNT_8 == 9)

Poly_Union_9 :: union($T: typeid) {
	Poly_Back_9(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_9 :: struct($T: typeid) { p: ^Poly_Union_9(T) }
Poly_Union_Holder_9 :: struct { u: Poly_Union_9(int) }
POLY_UNION_COUNT_9 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_9(int), "p")))
#assert(POLY_UNION_COUNT_9 == 9)

Poly_Union_10 :: union($T: typeid) {
	Poly_Back_10(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_10 :: struct($T: typeid) { p: ^Poly_Union_10(T) }
Poly_Union_Holder_10 :: struct { u: Poly_Union_10(int) }
POLY_UNION_COUNT_10 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_10(int), "p")))
#assert(POLY_UNION_COUNT_10 == 9)

Poly_Union_11 :: union($T: typeid) {
	Poly_Back_11(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_11 :: struct($T: typeid) { p: ^Poly_Union_11(T) }
Poly_Union_Holder_11 :: struct { u: Poly_Union_11(int) }
POLY_UNION_COUNT_11 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_11(int), "p")))
#assert(POLY_UNION_COUNT_11 == 9)

Poly_Union_12 :: union($T: typeid) {
	Poly_Back_12(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_12 :: struct($T: typeid) { p: ^Poly_Union_12(T) }
Poly_Union_Holder_12 :: struct { u: Poly_Union_12(int) }
POLY_UNION_COUNT_12 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_12(int), "p")))
#assert(POLY_UNION_COUNT_12 == 9)

Poly_Union_13 :: union($T: typeid) {
	Poly_Back_13(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_13 :: struct($T: typeid) { p: ^Poly_Union_13(T) }
Poly_Union_Holder_13 :: struct { u: Poly_Union_13(int) }
POLY_UNION_COUNT_13 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_13(int), "p")))
#assert(POLY_UNION_COUNT_13 == 9)

Poly_Union_14 :: union($T: typeid) {
	Poly_Back_14(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_14 :: struct($T: typeid) { p: ^Poly_Union_14(T) }
Poly_Union_Holder_14 :: struct { u: Poly_Union_14(int) }
POLY_UNION_COUNT_14 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_14(int), "p")))
#assert(POLY_UNION_COUNT_14 == 9)

Poly_Union_15 :: union($T: typeid) {
	Poly_Back_15(T),
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
	proc(self: T, x: [3]T) -> T,
	proc(self: T, x: [4]T) -> T,
	proc(self: T, x: [5]T) -> T,
	proc(self: T, x: [6]T) -> T,
	proc(self: T, x: [7]T) -> T,
	proc(self: T, x: [8]T) -> T,
}
Poly_Back_15 :: struct($T: typeid) { p: ^Poly_Union_15(T) }
Poly_Union_Holder_15 :: struct { u: Poly_Union_15(int) }
POLY_UNION_COUNT_15 :: intrinsics.type_union_variant_count(intrinsics.type_elem_type(intrinsics.type_field_type(Poly_Back_15(int), "p")))
#assert(POLY_UNION_COUNT_15 == 9)

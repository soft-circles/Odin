// Tests reading a polymorphic struct's field count while another global group instantiates it:
// `Poly_Fields_Holder_N` instantiates `Poly_Fields` and `POLY_FIELDS_COUNT_N` must wait for its fields.
package test_issue_poly_struct_field_count_race

import "base:intrinsics"

Poly_Fields :: struct($T: typeid) {
	a: proc(self: T, x: [1]T) -> T,
	b: proc(self: T, x: [2]T) -> T,
}

Poly_Fields_Holder_0 :: struct { f: Poly_Fields([0]u8) }
POLY_FIELDS_COUNT_0 :: intrinsics.type_struct_field_count(Poly_Fields([0]u8))
#assert(POLY_FIELDS_COUNT_0 == 2)

Poly_Fields_Holder_1 :: struct { f: Poly_Fields([1]u8) }
POLY_FIELDS_COUNT_1 :: intrinsics.type_struct_field_count(Poly_Fields([1]u8))
#assert(POLY_FIELDS_COUNT_1 == 2)

Poly_Fields_Holder_2 :: struct { f: Poly_Fields([2]u8) }
POLY_FIELDS_COUNT_2 :: intrinsics.type_struct_field_count(Poly_Fields([2]u8))
#assert(POLY_FIELDS_COUNT_2 == 2)

Poly_Fields_Holder_3 :: struct { f: Poly_Fields([3]u8) }
POLY_FIELDS_COUNT_3 :: intrinsics.type_struct_field_count(Poly_Fields([3]u8))
#assert(POLY_FIELDS_COUNT_3 == 2)

Poly_Fields_Holder_4 :: struct { f: Poly_Fields([4]u8) }
POLY_FIELDS_COUNT_4 :: intrinsics.type_struct_field_count(Poly_Fields([4]u8))
#assert(POLY_FIELDS_COUNT_4 == 2)

Poly_Fields_Holder_5 :: struct { f: Poly_Fields([5]u8) }
POLY_FIELDS_COUNT_5 :: intrinsics.type_struct_field_count(Poly_Fields([5]u8))
#assert(POLY_FIELDS_COUNT_5 == 2)

Poly_Fields_Holder_6 :: struct { f: Poly_Fields([6]u8) }
POLY_FIELDS_COUNT_6 :: intrinsics.type_struct_field_count(Poly_Fields([6]u8))
#assert(POLY_FIELDS_COUNT_6 == 2)

Poly_Fields_Holder_7 :: struct { f: Poly_Fields([7]u8) }
POLY_FIELDS_COUNT_7 :: intrinsics.type_struct_field_count(Poly_Fields([7]u8))
#assert(POLY_FIELDS_COUNT_7 == 2)

Poly_Fields_Holder_8 :: struct { f: Poly_Fields([8]u8) }
POLY_FIELDS_COUNT_8 :: intrinsics.type_struct_field_count(Poly_Fields([8]u8))
#assert(POLY_FIELDS_COUNT_8 == 2)

Poly_Fields_Holder_9 :: struct { f: Poly_Fields([9]u8) }
POLY_FIELDS_COUNT_9 :: intrinsics.type_struct_field_count(Poly_Fields([9]u8))
#assert(POLY_FIELDS_COUNT_9 == 2)

Poly_Fields_Holder_10 :: struct { f: Poly_Fields([10]u8) }
POLY_FIELDS_COUNT_10 :: intrinsics.type_struct_field_count(Poly_Fields([10]u8))
#assert(POLY_FIELDS_COUNT_10 == 2)

Poly_Fields_Holder_11 :: struct { f: Poly_Fields([11]u8) }
POLY_FIELDS_COUNT_11 :: intrinsics.type_struct_field_count(Poly_Fields([11]u8))
#assert(POLY_FIELDS_COUNT_11 == 2)

Poly_Fields_Holder_12 :: struct { f: Poly_Fields([12]u8) }
POLY_FIELDS_COUNT_12 :: intrinsics.type_struct_field_count(Poly_Fields([12]u8))
#assert(POLY_FIELDS_COUNT_12 == 2)

Poly_Fields_Holder_13 :: struct { f: Poly_Fields([13]u8) }
POLY_FIELDS_COUNT_13 :: intrinsics.type_struct_field_count(Poly_Fields([13]u8))
#assert(POLY_FIELDS_COUNT_13 == 2)

Poly_Fields_Holder_14 :: struct { f: Poly_Fields([14]u8) }
POLY_FIELDS_COUNT_14 :: intrinsics.type_struct_field_count(Poly_Fields([14]u8))
#assert(POLY_FIELDS_COUNT_14 == 2)

Poly_Fields_Holder_15 :: struct { f: Poly_Fields([15]u8) }
POLY_FIELDS_COUNT_15 :: intrinsics.type_struct_field_count(Poly_Fields([15]u8))
#assert(POLY_FIELDS_COUNT_15 == 2)

Poly_Fields_Holder_16 :: struct { f: Poly_Fields([16]u8) }
POLY_FIELDS_COUNT_16 :: intrinsics.type_struct_field_count(Poly_Fields([16]u8))
#assert(POLY_FIELDS_COUNT_16 == 2)

Poly_Fields_Holder_17 :: struct { f: Poly_Fields([17]u8) }
POLY_FIELDS_COUNT_17 :: intrinsics.type_struct_field_count(Poly_Fields([17]u8))
#assert(POLY_FIELDS_COUNT_17 == 2)

Poly_Fields_Holder_18 :: struct { f: Poly_Fields([18]u8) }
POLY_FIELDS_COUNT_18 :: intrinsics.type_struct_field_count(Poly_Fields([18]u8))
#assert(POLY_FIELDS_COUNT_18 == 2)

Poly_Fields_Holder_19 :: struct { f: Poly_Fields([19]u8) }
POLY_FIELDS_COUNT_19 :: intrinsics.type_struct_field_count(Poly_Fields([19]u8))
#assert(POLY_FIELDS_COUNT_19 == 2)

Poly_Fields_Holder_20 :: struct { f: Poly_Fields([20]u8) }
POLY_FIELDS_COUNT_20 :: intrinsics.type_struct_field_count(Poly_Fields([20]u8))
#assert(POLY_FIELDS_COUNT_20 == 2)

Poly_Fields_Holder_21 :: struct { f: Poly_Fields([21]u8) }
POLY_FIELDS_COUNT_21 :: intrinsics.type_struct_field_count(Poly_Fields([21]u8))
#assert(POLY_FIELDS_COUNT_21 == 2)

Poly_Fields_Holder_22 :: struct { f: Poly_Fields([22]u8) }
POLY_FIELDS_COUNT_22 :: intrinsics.type_struct_field_count(Poly_Fields([22]u8))
#assert(POLY_FIELDS_COUNT_22 == 2)

Poly_Fields_Holder_23 :: struct { f: Poly_Fields([23]u8) }
POLY_FIELDS_COUNT_23 :: intrinsics.type_struct_field_count(Poly_Fields([23]u8))
#assert(POLY_FIELDS_COUNT_23 == 2)

Poly_Fields_Holder_24 :: struct { f: Poly_Fields([24]u8) }
POLY_FIELDS_COUNT_24 :: intrinsics.type_struct_field_count(Poly_Fields([24]u8))
#assert(POLY_FIELDS_COUNT_24 == 2)

Poly_Fields_Holder_25 :: struct { f: Poly_Fields([25]u8) }
POLY_FIELDS_COUNT_25 :: intrinsics.type_struct_field_count(Poly_Fields([25]u8))
#assert(POLY_FIELDS_COUNT_25 == 2)

Poly_Fields_Holder_26 :: struct { f: Poly_Fields([26]u8) }
POLY_FIELDS_COUNT_26 :: intrinsics.type_struct_field_count(Poly_Fields([26]u8))
#assert(POLY_FIELDS_COUNT_26 == 2)

Poly_Fields_Holder_27 :: struct { f: Poly_Fields([27]u8) }
POLY_FIELDS_COUNT_27 :: intrinsics.type_struct_field_count(Poly_Fields([27]u8))
#assert(POLY_FIELDS_COUNT_27 == 2)

Poly_Fields_Holder_28 :: struct { f: Poly_Fields([28]u8) }
POLY_FIELDS_COUNT_28 :: intrinsics.type_struct_field_count(Poly_Fields([28]u8))
#assert(POLY_FIELDS_COUNT_28 == 2)

Poly_Fields_Holder_29 :: struct { f: Poly_Fields([29]u8) }
POLY_FIELDS_COUNT_29 :: intrinsics.type_struct_field_count(Poly_Fields([29]u8))
#assert(POLY_FIELDS_COUNT_29 == 2)

Poly_Fields_Holder_30 :: struct { f: Poly_Fields([30]u8) }
POLY_FIELDS_COUNT_30 :: intrinsics.type_struct_field_count(Poly_Fields([30]u8))
#assert(POLY_FIELDS_COUNT_30 == 2)

Poly_Fields_Holder_31 :: struct { f: Poly_Fields([31]u8) }
POLY_FIELDS_COUNT_31 :: intrinsics.type_struct_field_count(Poly_Fields([31]u8))
#assert(POLY_FIELDS_COUNT_31 == 2)

Poly_Fields_Holder_32 :: struct { f: Poly_Fields([32]u8) }
POLY_FIELDS_COUNT_32 :: intrinsics.type_struct_field_count(Poly_Fields([32]u8))
#assert(POLY_FIELDS_COUNT_32 == 2)

Poly_Fields_Holder_33 :: struct { f: Poly_Fields([33]u8) }
POLY_FIELDS_COUNT_33 :: intrinsics.type_struct_field_count(Poly_Fields([33]u8))
#assert(POLY_FIELDS_COUNT_33 == 2)

Poly_Fields_Holder_34 :: struct { f: Poly_Fields([34]u8) }
POLY_FIELDS_COUNT_34 :: intrinsics.type_struct_field_count(Poly_Fields([34]u8))
#assert(POLY_FIELDS_COUNT_34 == 2)

Poly_Fields_Holder_35 :: struct { f: Poly_Fields([35]u8) }
POLY_FIELDS_COUNT_35 :: intrinsics.type_struct_field_count(Poly_Fields([35]u8))
#assert(POLY_FIELDS_COUNT_35 == 2)

Poly_Fields_Holder_36 :: struct { f: Poly_Fields([36]u8) }
POLY_FIELDS_COUNT_36 :: intrinsics.type_struct_field_count(Poly_Fields([36]u8))
#assert(POLY_FIELDS_COUNT_36 == 2)

Poly_Fields_Holder_37 :: struct { f: Poly_Fields([37]u8) }
POLY_FIELDS_COUNT_37 :: intrinsics.type_struct_field_count(Poly_Fields([37]u8))
#assert(POLY_FIELDS_COUNT_37 == 2)

Poly_Fields_Holder_38 :: struct { f: Poly_Fields([38]u8) }
POLY_FIELDS_COUNT_38 :: intrinsics.type_struct_field_count(Poly_Fields([38]u8))
#assert(POLY_FIELDS_COUNT_38 == 2)

Poly_Fields_Holder_39 :: struct { f: Poly_Fields([39]u8) }
POLY_FIELDS_COUNT_39 :: intrinsics.type_struct_field_count(Poly_Fields([39]u8))
#assert(POLY_FIELDS_COUNT_39 == 2)

Poly_Fields_Holder_40 :: struct { f: Poly_Fields([40]u8) }
POLY_FIELDS_COUNT_40 :: intrinsics.type_struct_field_count(Poly_Fields([40]u8))
#assert(POLY_FIELDS_COUNT_40 == 2)

Poly_Fields_Holder_41 :: struct { f: Poly_Fields([41]u8) }
POLY_FIELDS_COUNT_41 :: intrinsics.type_struct_field_count(Poly_Fields([41]u8))
#assert(POLY_FIELDS_COUNT_41 == 2)

Poly_Fields_Holder_42 :: struct { f: Poly_Fields([42]u8) }
POLY_FIELDS_COUNT_42 :: intrinsics.type_struct_field_count(Poly_Fields([42]u8))
#assert(POLY_FIELDS_COUNT_42 == 2)

Poly_Fields_Holder_43 :: struct { f: Poly_Fields([43]u8) }
POLY_FIELDS_COUNT_43 :: intrinsics.type_struct_field_count(Poly_Fields([43]u8))
#assert(POLY_FIELDS_COUNT_43 == 2)

Poly_Fields_Holder_44 :: struct { f: Poly_Fields([44]u8) }
POLY_FIELDS_COUNT_44 :: intrinsics.type_struct_field_count(Poly_Fields([44]u8))
#assert(POLY_FIELDS_COUNT_44 == 2)

Poly_Fields_Holder_45 :: struct { f: Poly_Fields([45]u8) }
POLY_FIELDS_COUNT_45 :: intrinsics.type_struct_field_count(Poly_Fields([45]u8))
#assert(POLY_FIELDS_COUNT_45 == 2)

Poly_Fields_Holder_46 :: struct { f: Poly_Fields([46]u8) }
POLY_FIELDS_COUNT_46 :: intrinsics.type_struct_field_count(Poly_Fields([46]u8))
#assert(POLY_FIELDS_COUNT_46 == 2)

Poly_Fields_Holder_47 :: struct { f: Poly_Fields([47]u8) }
POLY_FIELDS_COUNT_47 :: intrinsics.type_struct_field_count(Poly_Fields([47]u8))
#assert(POLY_FIELDS_COUNT_47 == 2)

Poly_Fields_Holder_48 :: struct { f: Poly_Fields([48]u8) }
POLY_FIELDS_COUNT_48 :: intrinsics.type_struct_field_count(Poly_Fields([48]u8))
#assert(POLY_FIELDS_COUNT_48 == 2)

Poly_Fields_Holder_49 :: struct { f: Poly_Fields([49]u8) }
POLY_FIELDS_COUNT_49 :: intrinsics.type_struct_field_count(Poly_Fields([49]u8))
#assert(POLY_FIELDS_COUNT_49 == 2)

Poly_Fields_Holder_50 :: struct { f: Poly_Fields([50]u8) }
POLY_FIELDS_COUNT_50 :: intrinsics.type_struct_field_count(Poly_Fields([50]u8))
#assert(POLY_FIELDS_COUNT_50 == 2)

Poly_Fields_Holder_51 :: struct { f: Poly_Fields([51]u8) }
POLY_FIELDS_COUNT_51 :: intrinsics.type_struct_field_count(Poly_Fields([51]u8))
#assert(POLY_FIELDS_COUNT_51 == 2)

Poly_Fields_Holder_52 :: struct { f: Poly_Fields([52]u8) }
POLY_FIELDS_COUNT_52 :: intrinsics.type_struct_field_count(Poly_Fields([52]u8))
#assert(POLY_FIELDS_COUNT_52 == 2)

Poly_Fields_Holder_53 :: struct { f: Poly_Fields([53]u8) }
POLY_FIELDS_COUNT_53 :: intrinsics.type_struct_field_count(Poly_Fields([53]u8))
#assert(POLY_FIELDS_COUNT_53 == 2)

Poly_Fields_Holder_54 :: struct { f: Poly_Fields([54]u8) }
POLY_FIELDS_COUNT_54 :: intrinsics.type_struct_field_count(Poly_Fields([54]u8))
#assert(POLY_FIELDS_COUNT_54 == 2)

Poly_Fields_Holder_55 :: struct { f: Poly_Fields([55]u8) }
POLY_FIELDS_COUNT_55 :: intrinsics.type_struct_field_count(Poly_Fields([55]u8))
#assert(POLY_FIELDS_COUNT_55 == 2)

Poly_Fields_Holder_56 :: struct { f: Poly_Fields([56]u8) }
POLY_FIELDS_COUNT_56 :: intrinsics.type_struct_field_count(Poly_Fields([56]u8))
#assert(POLY_FIELDS_COUNT_56 == 2)

Poly_Fields_Holder_57 :: struct { f: Poly_Fields([57]u8) }
POLY_FIELDS_COUNT_57 :: intrinsics.type_struct_field_count(Poly_Fields([57]u8))
#assert(POLY_FIELDS_COUNT_57 == 2)

Poly_Fields_Holder_58 :: struct { f: Poly_Fields([58]u8) }
POLY_FIELDS_COUNT_58 :: intrinsics.type_struct_field_count(Poly_Fields([58]u8))
#assert(POLY_FIELDS_COUNT_58 == 2)

Poly_Fields_Holder_59 :: struct { f: Poly_Fields([59]u8) }
POLY_FIELDS_COUNT_59 :: intrinsics.type_struct_field_count(Poly_Fields([59]u8))
#assert(POLY_FIELDS_COUNT_59 == 2)

Poly_Fields_Holder_60 :: struct { f: Poly_Fields([60]u8) }
POLY_FIELDS_COUNT_60 :: intrinsics.type_struct_field_count(Poly_Fields([60]u8))
#assert(POLY_FIELDS_COUNT_60 == 2)

Poly_Fields_Holder_61 :: struct { f: Poly_Fields([61]u8) }
POLY_FIELDS_COUNT_61 :: intrinsics.type_struct_field_count(Poly_Fields([61]u8))
#assert(POLY_FIELDS_COUNT_61 == 2)

Poly_Fields_Holder_62 :: struct { f: Poly_Fields([62]u8) }
POLY_FIELDS_COUNT_62 :: intrinsics.type_struct_field_count(Poly_Fields([62]u8))
#assert(POLY_FIELDS_COUNT_62 == 2)

Poly_Fields_Holder_63 :: struct { f: Poly_Fields([63]u8) }
POLY_FIELDS_COUNT_63 :: intrinsics.type_struct_field_count(Poly_Fields([63]u8))
#assert(POLY_FIELDS_COUNT_63 == 2)

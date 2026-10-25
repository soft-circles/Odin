// Tests reading a polymorphic union's variant count while another global group instantiates it:
// `Poly_Variants_Holder_N` instantiates `Poly_Variants` and `POLY_VARIANTS_COUNT_N` must wait for its variants.
package test_issues

import "base:intrinsics"

Poly_Variants :: union($T: typeid) {
	proc(self: T, x: [1]T) -> T,
	proc(self: T, x: [2]T) -> T,
}

Poly_Variants_Holder_0 :: struct { v: Poly_Variants([0]u8) }
POLY_VARIANTS_COUNT_0 :: intrinsics.type_union_variant_count(Poly_Variants([0]u8))
#assert(POLY_VARIANTS_COUNT_0 == 2)

Poly_Variants_Holder_1 :: struct { v: Poly_Variants([1]u8) }
POLY_VARIANTS_COUNT_1 :: intrinsics.type_union_variant_count(Poly_Variants([1]u8))
#assert(POLY_VARIANTS_COUNT_1 == 2)

Poly_Variants_Holder_2 :: struct { v: Poly_Variants([2]u8) }
POLY_VARIANTS_COUNT_2 :: intrinsics.type_union_variant_count(Poly_Variants([2]u8))
#assert(POLY_VARIANTS_COUNT_2 == 2)

Poly_Variants_Holder_3 :: struct { v: Poly_Variants([3]u8) }
POLY_VARIANTS_COUNT_3 :: intrinsics.type_union_variant_count(Poly_Variants([3]u8))
#assert(POLY_VARIANTS_COUNT_3 == 2)

Poly_Variants_Holder_4 :: struct { v: Poly_Variants([4]u8) }
POLY_VARIANTS_COUNT_4 :: intrinsics.type_union_variant_count(Poly_Variants([4]u8))
#assert(POLY_VARIANTS_COUNT_4 == 2)

Poly_Variants_Holder_5 :: struct { v: Poly_Variants([5]u8) }
POLY_VARIANTS_COUNT_5 :: intrinsics.type_union_variant_count(Poly_Variants([5]u8))
#assert(POLY_VARIANTS_COUNT_5 == 2)

Poly_Variants_Holder_6 :: struct { v: Poly_Variants([6]u8) }
POLY_VARIANTS_COUNT_6 :: intrinsics.type_union_variant_count(Poly_Variants([6]u8))
#assert(POLY_VARIANTS_COUNT_6 == 2)

Poly_Variants_Holder_7 :: struct { v: Poly_Variants([7]u8) }
POLY_VARIANTS_COUNT_7 :: intrinsics.type_union_variant_count(Poly_Variants([7]u8))
#assert(POLY_VARIANTS_COUNT_7 == 2)

Poly_Variants_Holder_8 :: struct { v: Poly_Variants([8]u8) }
POLY_VARIANTS_COUNT_8 :: intrinsics.type_union_variant_count(Poly_Variants([8]u8))
#assert(POLY_VARIANTS_COUNT_8 == 2)

Poly_Variants_Holder_9 :: struct { v: Poly_Variants([9]u8) }
POLY_VARIANTS_COUNT_9 :: intrinsics.type_union_variant_count(Poly_Variants([9]u8))
#assert(POLY_VARIANTS_COUNT_9 == 2)

Poly_Variants_Holder_10 :: struct { v: Poly_Variants([10]u8) }
POLY_VARIANTS_COUNT_10 :: intrinsics.type_union_variant_count(Poly_Variants([10]u8))
#assert(POLY_VARIANTS_COUNT_10 == 2)

Poly_Variants_Holder_11 :: struct { v: Poly_Variants([11]u8) }
POLY_VARIANTS_COUNT_11 :: intrinsics.type_union_variant_count(Poly_Variants([11]u8))
#assert(POLY_VARIANTS_COUNT_11 == 2)

Poly_Variants_Holder_12 :: struct { v: Poly_Variants([12]u8) }
POLY_VARIANTS_COUNT_12 :: intrinsics.type_union_variant_count(Poly_Variants([12]u8))
#assert(POLY_VARIANTS_COUNT_12 == 2)

Poly_Variants_Holder_13 :: struct { v: Poly_Variants([13]u8) }
POLY_VARIANTS_COUNT_13 :: intrinsics.type_union_variant_count(Poly_Variants([13]u8))
#assert(POLY_VARIANTS_COUNT_13 == 2)

Poly_Variants_Holder_14 :: struct { v: Poly_Variants([14]u8) }
POLY_VARIANTS_COUNT_14 :: intrinsics.type_union_variant_count(Poly_Variants([14]u8))
#assert(POLY_VARIANTS_COUNT_14 == 2)

Poly_Variants_Holder_15 :: struct { v: Poly_Variants([15]u8) }
POLY_VARIANTS_COUNT_15 :: intrinsics.type_union_variant_count(Poly_Variants([15]u8))
#assert(POLY_VARIANTS_COUNT_15 == 2)

Poly_Variants_Holder_16 :: struct { v: Poly_Variants([16]u8) }
POLY_VARIANTS_COUNT_16 :: intrinsics.type_union_variant_count(Poly_Variants([16]u8))
#assert(POLY_VARIANTS_COUNT_16 == 2)

Poly_Variants_Holder_17 :: struct { v: Poly_Variants([17]u8) }
POLY_VARIANTS_COUNT_17 :: intrinsics.type_union_variant_count(Poly_Variants([17]u8))
#assert(POLY_VARIANTS_COUNT_17 == 2)

Poly_Variants_Holder_18 :: struct { v: Poly_Variants([18]u8) }
POLY_VARIANTS_COUNT_18 :: intrinsics.type_union_variant_count(Poly_Variants([18]u8))
#assert(POLY_VARIANTS_COUNT_18 == 2)

Poly_Variants_Holder_19 :: struct { v: Poly_Variants([19]u8) }
POLY_VARIANTS_COUNT_19 :: intrinsics.type_union_variant_count(Poly_Variants([19]u8))
#assert(POLY_VARIANTS_COUNT_19 == 2)

Poly_Variants_Holder_20 :: struct { v: Poly_Variants([20]u8) }
POLY_VARIANTS_COUNT_20 :: intrinsics.type_union_variant_count(Poly_Variants([20]u8))
#assert(POLY_VARIANTS_COUNT_20 == 2)

Poly_Variants_Holder_21 :: struct { v: Poly_Variants([21]u8) }
POLY_VARIANTS_COUNT_21 :: intrinsics.type_union_variant_count(Poly_Variants([21]u8))
#assert(POLY_VARIANTS_COUNT_21 == 2)

Poly_Variants_Holder_22 :: struct { v: Poly_Variants([22]u8) }
POLY_VARIANTS_COUNT_22 :: intrinsics.type_union_variant_count(Poly_Variants([22]u8))
#assert(POLY_VARIANTS_COUNT_22 == 2)

Poly_Variants_Holder_23 :: struct { v: Poly_Variants([23]u8) }
POLY_VARIANTS_COUNT_23 :: intrinsics.type_union_variant_count(Poly_Variants([23]u8))
#assert(POLY_VARIANTS_COUNT_23 == 2)

Poly_Variants_Holder_24 :: struct { v: Poly_Variants([24]u8) }
POLY_VARIANTS_COUNT_24 :: intrinsics.type_union_variant_count(Poly_Variants([24]u8))
#assert(POLY_VARIANTS_COUNT_24 == 2)

Poly_Variants_Holder_25 :: struct { v: Poly_Variants([25]u8) }
POLY_VARIANTS_COUNT_25 :: intrinsics.type_union_variant_count(Poly_Variants([25]u8))
#assert(POLY_VARIANTS_COUNT_25 == 2)

Poly_Variants_Holder_26 :: struct { v: Poly_Variants([26]u8) }
POLY_VARIANTS_COUNT_26 :: intrinsics.type_union_variant_count(Poly_Variants([26]u8))
#assert(POLY_VARIANTS_COUNT_26 == 2)

Poly_Variants_Holder_27 :: struct { v: Poly_Variants([27]u8) }
POLY_VARIANTS_COUNT_27 :: intrinsics.type_union_variant_count(Poly_Variants([27]u8))
#assert(POLY_VARIANTS_COUNT_27 == 2)

Poly_Variants_Holder_28 :: struct { v: Poly_Variants([28]u8) }
POLY_VARIANTS_COUNT_28 :: intrinsics.type_union_variant_count(Poly_Variants([28]u8))
#assert(POLY_VARIANTS_COUNT_28 == 2)

Poly_Variants_Holder_29 :: struct { v: Poly_Variants([29]u8) }
POLY_VARIANTS_COUNT_29 :: intrinsics.type_union_variant_count(Poly_Variants([29]u8))
#assert(POLY_VARIANTS_COUNT_29 == 2)

Poly_Variants_Holder_30 :: struct { v: Poly_Variants([30]u8) }
POLY_VARIANTS_COUNT_30 :: intrinsics.type_union_variant_count(Poly_Variants([30]u8))
#assert(POLY_VARIANTS_COUNT_30 == 2)

Poly_Variants_Holder_31 :: struct { v: Poly_Variants([31]u8) }
POLY_VARIANTS_COUNT_31 :: intrinsics.type_union_variant_count(Poly_Variants([31]u8))
#assert(POLY_VARIANTS_COUNT_31 == 2)

Poly_Variants_Holder_32 :: struct { v: Poly_Variants([32]u8) }
POLY_VARIANTS_COUNT_32 :: intrinsics.type_union_variant_count(Poly_Variants([32]u8))
#assert(POLY_VARIANTS_COUNT_32 == 2)

Poly_Variants_Holder_33 :: struct { v: Poly_Variants([33]u8) }
POLY_VARIANTS_COUNT_33 :: intrinsics.type_union_variant_count(Poly_Variants([33]u8))
#assert(POLY_VARIANTS_COUNT_33 == 2)

Poly_Variants_Holder_34 :: struct { v: Poly_Variants([34]u8) }
POLY_VARIANTS_COUNT_34 :: intrinsics.type_union_variant_count(Poly_Variants([34]u8))
#assert(POLY_VARIANTS_COUNT_34 == 2)

Poly_Variants_Holder_35 :: struct { v: Poly_Variants([35]u8) }
POLY_VARIANTS_COUNT_35 :: intrinsics.type_union_variant_count(Poly_Variants([35]u8))
#assert(POLY_VARIANTS_COUNT_35 == 2)

Poly_Variants_Holder_36 :: struct { v: Poly_Variants([36]u8) }
POLY_VARIANTS_COUNT_36 :: intrinsics.type_union_variant_count(Poly_Variants([36]u8))
#assert(POLY_VARIANTS_COUNT_36 == 2)

Poly_Variants_Holder_37 :: struct { v: Poly_Variants([37]u8) }
POLY_VARIANTS_COUNT_37 :: intrinsics.type_union_variant_count(Poly_Variants([37]u8))
#assert(POLY_VARIANTS_COUNT_37 == 2)

Poly_Variants_Holder_38 :: struct { v: Poly_Variants([38]u8) }
POLY_VARIANTS_COUNT_38 :: intrinsics.type_union_variant_count(Poly_Variants([38]u8))
#assert(POLY_VARIANTS_COUNT_38 == 2)

Poly_Variants_Holder_39 :: struct { v: Poly_Variants([39]u8) }
POLY_VARIANTS_COUNT_39 :: intrinsics.type_union_variant_count(Poly_Variants([39]u8))
#assert(POLY_VARIANTS_COUNT_39 == 2)

Poly_Variants_Holder_40 :: struct { v: Poly_Variants([40]u8) }
POLY_VARIANTS_COUNT_40 :: intrinsics.type_union_variant_count(Poly_Variants([40]u8))
#assert(POLY_VARIANTS_COUNT_40 == 2)

Poly_Variants_Holder_41 :: struct { v: Poly_Variants([41]u8) }
POLY_VARIANTS_COUNT_41 :: intrinsics.type_union_variant_count(Poly_Variants([41]u8))
#assert(POLY_VARIANTS_COUNT_41 == 2)

Poly_Variants_Holder_42 :: struct { v: Poly_Variants([42]u8) }
POLY_VARIANTS_COUNT_42 :: intrinsics.type_union_variant_count(Poly_Variants([42]u8))
#assert(POLY_VARIANTS_COUNT_42 == 2)

Poly_Variants_Holder_43 :: struct { v: Poly_Variants([43]u8) }
POLY_VARIANTS_COUNT_43 :: intrinsics.type_union_variant_count(Poly_Variants([43]u8))
#assert(POLY_VARIANTS_COUNT_43 == 2)

Poly_Variants_Holder_44 :: struct { v: Poly_Variants([44]u8) }
POLY_VARIANTS_COUNT_44 :: intrinsics.type_union_variant_count(Poly_Variants([44]u8))
#assert(POLY_VARIANTS_COUNT_44 == 2)

Poly_Variants_Holder_45 :: struct { v: Poly_Variants([45]u8) }
POLY_VARIANTS_COUNT_45 :: intrinsics.type_union_variant_count(Poly_Variants([45]u8))
#assert(POLY_VARIANTS_COUNT_45 == 2)

Poly_Variants_Holder_46 :: struct { v: Poly_Variants([46]u8) }
POLY_VARIANTS_COUNT_46 :: intrinsics.type_union_variant_count(Poly_Variants([46]u8))
#assert(POLY_VARIANTS_COUNT_46 == 2)

Poly_Variants_Holder_47 :: struct { v: Poly_Variants([47]u8) }
POLY_VARIANTS_COUNT_47 :: intrinsics.type_union_variant_count(Poly_Variants([47]u8))
#assert(POLY_VARIANTS_COUNT_47 == 2)

Poly_Variants_Holder_48 :: struct { v: Poly_Variants([48]u8) }
POLY_VARIANTS_COUNT_48 :: intrinsics.type_union_variant_count(Poly_Variants([48]u8))
#assert(POLY_VARIANTS_COUNT_48 == 2)

Poly_Variants_Holder_49 :: struct { v: Poly_Variants([49]u8) }
POLY_VARIANTS_COUNT_49 :: intrinsics.type_union_variant_count(Poly_Variants([49]u8))
#assert(POLY_VARIANTS_COUNT_49 == 2)

Poly_Variants_Holder_50 :: struct { v: Poly_Variants([50]u8) }
POLY_VARIANTS_COUNT_50 :: intrinsics.type_union_variant_count(Poly_Variants([50]u8))
#assert(POLY_VARIANTS_COUNT_50 == 2)

Poly_Variants_Holder_51 :: struct { v: Poly_Variants([51]u8) }
POLY_VARIANTS_COUNT_51 :: intrinsics.type_union_variant_count(Poly_Variants([51]u8))
#assert(POLY_VARIANTS_COUNT_51 == 2)

Poly_Variants_Holder_52 :: struct { v: Poly_Variants([52]u8) }
POLY_VARIANTS_COUNT_52 :: intrinsics.type_union_variant_count(Poly_Variants([52]u8))
#assert(POLY_VARIANTS_COUNT_52 == 2)

Poly_Variants_Holder_53 :: struct { v: Poly_Variants([53]u8) }
POLY_VARIANTS_COUNT_53 :: intrinsics.type_union_variant_count(Poly_Variants([53]u8))
#assert(POLY_VARIANTS_COUNT_53 == 2)

Poly_Variants_Holder_54 :: struct { v: Poly_Variants([54]u8) }
POLY_VARIANTS_COUNT_54 :: intrinsics.type_union_variant_count(Poly_Variants([54]u8))
#assert(POLY_VARIANTS_COUNT_54 == 2)

Poly_Variants_Holder_55 :: struct { v: Poly_Variants([55]u8) }
POLY_VARIANTS_COUNT_55 :: intrinsics.type_union_variant_count(Poly_Variants([55]u8))
#assert(POLY_VARIANTS_COUNT_55 == 2)

Poly_Variants_Holder_56 :: struct { v: Poly_Variants([56]u8) }
POLY_VARIANTS_COUNT_56 :: intrinsics.type_union_variant_count(Poly_Variants([56]u8))
#assert(POLY_VARIANTS_COUNT_56 == 2)

Poly_Variants_Holder_57 :: struct { v: Poly_Variants([57]u8) }
POLY_VARIANTS_COUNT_57 :: intrinsics.type_union_variant_count(Poly_Variants([57]u8))
#assert(POLY_VARIANTS_COUNT_57 == 2)

Poly_Variants_Holder_58 :: struct { v: Poly_Variants([58]u8) }
POLY_VARIANTS_COUNT_58 :: intrinsics.type_union_variant_count(Poly_Variants([58]u8))
#assert(POLY_VARIANTS_COUNT_58 == 2)

Poly_Variants_Holder_59 :: struct { v: Poly_Variants([59]u8) }
POLY_VARIANTS_COUNT_59 :: intrinsics.type_union_variant_count(Poly_Variants([59]u8))
#assert(POLY_VARIANTS_COUNT_59 == 2)

Poly_Variants_Holder_60 :: struct { v: Poly_Variants([60]u8) }
POLY_VARIANTS_COUNT_60 :: intrinsics.type_union_variant_count(Poly_Variants([60]u8))
#assert(POLY_VARIANTS_COUNT_60 == 2)

Poly_Variants_Holder_61 :: struct { v: Poly_Variants([61]u8) }
POLY_VARIANTS_COUNT_61 :: intrinsics.type_union_variant_count(Poly_Variants([61]u8))
#assert(POLY_VARIANTS_COUNT_61 == 2)

Poly_Variants_Holder_62 :: struct { v: Poly_Variants([62]u8) }
POLY_VARIANTS_COUNT_62 :: intrinsics.type_union_variant_count(Poly_Variants([62]u8))
#assert(POLY_VARIANTS_COUNT_62 == 2)

Poly_Variants_Holder_63 :: struct { v: Poly_Variants([63]u8) }
POLY_VARIANTS_COUNT_63 :: intrinsics.type_union_variant_count(Poly_Variants([63]u8))
#assert(POLY_VARIANTS_COUNT_63 == 2)

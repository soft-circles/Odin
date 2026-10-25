// Tests a struct intrinsic in a polymorphic parameter's default, which is checked while the record's
// instantiations are locked: `Poly_Map_N(int)`'s default reads `Poly_Node_N(int)`, whose field points back at
// `Poly_Map_N(int)`. Waiting for `Poly_Node_N(int)`'s fields there deadlocked with the thread checking them.
// `N` is not asserted: the default skips that wait, so it can read the fields while they are being checked.
package test_issues

import "base:intrinsics"

Poly_Map_0 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_0(K))) { k: K, a: [N]u8 }
Poly_Node_0 :: struct($T: typeid) { m: ^Poly_Map_0(T), t: T }
Poly_Map_Holder_0 :: struct { m: Poly_Map_0(int) }
Poly_Node_Holder_0 :: struct { n: Poly_Node_0(int) }

Poly_Map_1 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_1(K))) { k: K, a: [N]u8 }
Poly_Node_1 :: struct($T: typeid) { m: ^Poly_Map_1(T), t: T }
Poly_Map_Holder_1 :: struct { m: Poly_Map_1(int) }
Poly_Node_Holder_1 :: struct { n: Poly_Node_1(int) }

Poly_Map_2 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_2(K))) { k: K, a: [N]u8 }
Poly_Node_2 :: struct($T: typeid) { m: ^Poly_Map_2(T), t: T }
Poly_Map_Holder_2 :: struct { m: Poly_Map_2(int) }
Poly_Node_Holder_2 :: struct { n: Poly_Node_2(int) }

Poly_Map_3 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_3(K))) { k: K, a: [N]u8 }
Poly_Node_3 :: struct($T: typeid) { m: ^Poly_Map_3(T), t: T }
Poly_Map_Holder_3 :: struct { m: Poly_Map_3(int) }
Poly_Node_Holder_3 :: struct { n: Poly_Node_3(int) }

Poly_Map_4 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_4(K))) { k: K, a: [N]u8 }
Poly_Node_4 :: struct($T: typeid) { m: ^Poly_Map_4(T), t: T }
Poly_Map_Holder_4 :: struct { m: Poly_Map_4(int) }
Poly_Node_Holder_4 :: struct { n: Poly_Node_4(int) }

Poly_Map_5 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_5(K))) { k: K, a: [N]u8 }
Poly_Node_5 :: struct($T: typeid) { m: ^Poly_Map_5(T), t: T }
Poly_Map_Holder_5 :: struct { m: Poly_Map_5(int) }
Poly_Node_Holder_5 :: struct { n: Poly_Node_5(int) }

Poly_Map_6 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_6(K))) { k: K, a: [N]u8 }
Poly_Node_6 :: struct($T: typeid) { m: ^Poly_Map_6(T), t: T }
Poly_Map_Holder_6 :: struct { m: Poly_Map_6(int) }
Poly_Node_Holder_6 :: struct { n: Poly_Node_6(int) }

Poly_Map_7 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_7(K))) { k: K, a: [N]u8 }
Poly_Node_7 :: struct($T: typeid) { m: ^Poly_Map_7(T), t: T }
Poly_Map_Holder_7 :: struct { m: Poly_Map_7(int) }
Poly_Node_Holder_7 :: struct { n: Poly_Node_7(int) }

Poly_Map_8 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_8(K))) { k: K, a: [N]u8 }
Poly_Node_8 :: struct($T: typeid) { m: ^Poly_Map_8(T), t: T }
Poly_Map_Holder_8 :: struct { m: Poly_Map_8(int) }
Poly_Node_Holder_8 :: struct { n: Poly_Node_8(int) }

Poly_Map_9 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_9(K))) { k: K, a: [N]u8 }
Poly_Node_9 :: struct($T: typeid) { m: ^Poly_Map_9(T), t: T }
Poly_Map_Holder_9 :: struct { m: Poly_Map_9(int) }
Poly_Node_Holder_9 :: struct { n: Poly_Node_9(int) }

Poly_Map_10 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_10(K))) { k: K, a: [N]u8 }
Poly_Node_10 :: struct($T: typeid) { m: ^Poly_Map_10(T), t: T }
Poly_Map_Holder_10 :: struct { m: Poly_Map_10(int) }
Poly_Node_Holder_10 :: struct { n: Poly_Node_10(int) }

Poly_Map_11 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_11(K))) { k: K, a: [N]u8 }
Poly_Node_11 :: struct($T: typeid) { m: ^Poly_Map_11(T), t: T }
Poly_Map_Holder_11 :: struct { m: Poly_Map_11(int) }
Poly_Node_Holder_11 :: struct { n: Poly_Node_11(int) }

Poly_Map_12 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_12(K))) { k: K, a: [N]u8 }
Poly_Node_12 :: struct($T: typeid) { m: ^Poly_Map_12(T), t: T }
Poly_Map_Holder_12 :: struct { m: Poly_Map_12(int) }
Poly_Node_Holder_12 :: struct { n: Poly_Node_12(int) }

Poly_Map_13 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_13(K))) { k: K, a: [N]u8 }
Poly_Node_13 :: struct($T: typeid) { m: ^Poly_Map_13(T), t: T }
Poly_Map_Holder_13 :: struct { m: Poly_Map_13(int) }
Poly_Node_Holder_13 :: struct { n: Poly_Node_13(int) }

Poly_Map_14 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_14(K))) { k: K, a: [N]u8 }
Poly_Node_14 :: struct($T: typeid) { m: ^Poly_Map_14(T), t: T }
Poly_Map_Holder_14 :: struct { m: Poly_Map_14(int) }
Poly_Node_Holder_14 :: struct { n: Poly_Node_14(int) }

Poly_Map_15 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_15(K))) { k: K, a: [N]u8 }
Poly_Node_15 :: struct($T: typeid) { m: ^Poly_Map_15(T), t: T }
Poly_Map_Holder_15 :: struct { m: Poly_Map_15(int) }
Poly_Node_Holder_15 :: struct { n: Poly_Node_15(int) }

Poly_Map_16 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_16(K))) { k: K, a: [N]u8 }
Poly_Node_16 :: struct($T: typeid) { m: ^Poly_Map_16(T), t: T }
Poly_Map_Holder_16 :: struct { m: Poly_Map_16(int) }
Poly_Node_Holder_16 :: struct { n: Poly_Node_16(int) }

Poly_Map_17 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_17(K))) { k: K, a: [N]u8 }
Poly_Node_17 :: struct($T: typeid) { m: ^Poly_Map_17(T), t: T }
Poly_Map_Holder_17 :: struct { m: Poly_Map_17(int) }
Poly_Node_Holder_17 :: struct { n: Poly_Node_17(int) }

Poly_Map_18 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_18(K))) { k: K, a: [N]u8 }
Poly_Node_18 :: struct($T: typeid) { m: ^Poly_Map_18(T), t: T }
Poly_Map_Holder_18 :: struct { m: Poly_Map_18(int) }
Poly_Node_Holder_18 :: struct { n: Poly_Node_18(int) }

Poly_Map_19 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_19(K))) { k: K, a: [N]u8 }
Poly_Node_19 :: struct($T: typeid) { m: ^Poly_Map_19(T), t: T }
Poly_Map_Holder_19 :: struct { m: Poly_Map_19(int) }
Poly_Node_Holder_19 :: struct { n: Poly_Node_19(int) }

Poly_Map_20 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_20(K))) { k: K, a: [N]u8 }
Poly_Node_20 :: struct($T: typeid) { m: ^Poly_Map_20(T), t: T }
Poly_Map_Holder_20 :: struct { m: Poly_Map_20(int) }
Poly_Node_Holder_20 :: struct { n: Poly_Node_20(int) }

Poly_Map_21 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_21(K))) { k: K, a: [N]u8 }
Poly_Node_21 :: struct($T: typeid) { m: ^Poly_Map_21(T), t: T }
Poly_Map_Holder_21 :: struct { m: Poly_Map_21(int) }
Poly_Node_Holder_21 :: struct { n: Poly_Node_21(int) }

Poly_Map_22 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_22(K))) { k: K, a: [N]u8 }
Poly_Node_22 :: struct($T: typeid) { m: ^Poly_Map_22(T), t: T }
Poly_Map_Holder_22 :: struct { m: Poly_Map_22(int) }
Poly_Node_Holder_22 :: struct { n: Poly_Node_22(int) }

Poly_Map_23 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_23(K))) { k: K, a: [N]u8 }
Poly_Node_23 :: struct($T: typeid) { m: ^Poly_Map_23(T), t: T }
Poly_Map_Holder_23 :: struct { m: Poly_Map_23(int) }
Poly_Node_Holder_23 :: struct { n: Poly_Node_23(int) }

Poly_Map_24 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_24(K))) { k: K, a: [N]u8 }
Poly_Node_24 :: struct($T: typeid) { m: ^Poly_Map_24(T), t: T }
Poly_Map_Holder_24 :: struct { m: Poly_Map_24(int) }
Poly_Node_Holder_24 :: struct { n: Poly_Node_24(int) }

Poly_Map_25 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_25(K))) { k: K, a: [N]u8 }
Poly_Node_25 :: struct($T: typeid) { m: ^Poly_Map_25(T), t: T }
Poly_Map_Holder_25 :: struct { m: Poly_Map_25(int) }
Poly_Node_Holder_25 :: struct { n: Poly_Node_25(int) }

Poly_Map_26 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_26(K))) { k: K, a: [N]u8 }
Poly_Node_26 :: struct($T: typeid) { m: ^Poly_Map_26(T), t: T }
Poly_Map_Holder_26 :: struct { m: Poly_Map_26(int) }
Poly_Node_Holder_26 :: struct { n: Poly_Node_26(int) }

Poly_Map_27 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_27(K))) { k: K, a: [N]u8 }
Poly_Node_27 :: struct($T: typeid) { m: ^Poly_Map_27(T), t: T }
Poly_Map_Holder_27 :: struct { m: Poly_Map_27(int) }
Poly_Node_Holder_27 :: struct { n: Poly_Node_27(int) }

Poly_Map_28 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_28(K))) { k: K, a: [N]u8 }
Poly_Node_28 :: struct($T: typeid) { m: ^Poly_Map_28(T), t: T }
Poly_Map_Holder_28 :: struct { m: Poly_Map_28(int) }
Poly_Node_Holder_28 :: struct { n: Poly_Node_28(int) }

Poly_Map_29 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_29(K))) { k: K, a: [N]u8 }
Poly_Node_29 :: struct($T: typeid) { m: ^Poly_Map_29(T), t: T }
Poly_Map_Holder_29 :: struct { m: Poly_Map_29(int) }
Poly_Node_Holder_29 :: struct { n: Poly_Node_29(int) }

Poly_Map_30 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_30(K))) { k: K, a: [N]u8 }
Poly_Node_30 :: struct($T: typeid) { m: ^Poly_Map_30(T), t: T }
Poly_Map_Holder_30 :: struct { m: Poly_Map_30(int) }
Poly_Node_Holder_30 :: struct { n: Poly_Node_30(int) }

Poly_Map_31 :: struct($K: typeid, $N: int = intrinsics.type_struct_field_count(Poly_Node_31(K))) { k: K, a: [N]u8 }
Poly_Node_31 :: struct($T: typeid) { m: ^Poly_Map_31(T), t: T }
Poly_Map_Holder_31 :: struct { m: Poly_Map_31(int) }
Poly_Node_Holder_31 :: struct { n: Poly_Node_31(int) }

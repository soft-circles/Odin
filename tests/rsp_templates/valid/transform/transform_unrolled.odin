// transform.odin's template written with `#unroll for`, as Odin64's builder body is
// written with loops: its words must equal the flat template's (golden.odin checks).
package transform

@(rsp)
transform_unrolled :: asm(args: ^Args, input: ^Point, output: ^Point, count: i32, scratch: ^[20]u32) [args = %s6, input = %s1, output = %s2, count = %s3, scratch = %s7] {
	// coefficients: a and c, then b and d (4 bytes further into Args), each splatted
	// to the four words at 32*half and 32*half + 16
	#unroll for half in 0..<2 {
		lh    %t0, [args + (offset_of(Args, a) + 4*half)]
		lh    %t1, [args + (offset_of(Args, c) + 4*half)]
		sra   %t4, %t0, 8
		sll   %t4, %t4, 16
		sra   %t5, %t1, 8
		andi  %t5, %t5, 0xffff
		or    %t4, %t4, %t5
		#unroll for lane in 0..<4 {
			sw    %t4, [scratch + (32*half + 4*lane)]
		}
		andi  %t4, %t0, 0xff
		sll   %t4, %t4, 24
		andi  %t5, %t1, 0xff
		sll   %t5, %t5, 8
		or    %t4, %t4, %t5
		#unroll for lane in 0..<4 {
			sw    %t4, [scratch + (32*half + 16 + 4*lane)]
		}
	}
	lw    %t4, [args + (offset_of(Args, tx))]
	#unroll for offset in SPLAT_OFFSETS {
		sw    %t4, [scratch + 64 + offset]
	}
	#unroll for vr, i in {%v10..=%v14} {
		lqv   %vr.e0, [scratch + (16*i)]
	}
.loop:
	lqv   %v01.e0, [input + 0]
	vmudn %v02, %v11, %v01.q0
	vmadn %v02, %v13, %v01.q1
	vmadh %v02, %v10, %v01.q0
	vmadh %v02, %v12, %v01.q1
	vmadh %v02, %v14, %v30.e7
	sqv   %v02.e0, [output + 0]
	addiu count, count, -4
	addiu input, input, 16
	bgtz  count, .loop
	addiu output, output, 16
}

SPLAT_OFFSETS :: []i32{0, 4, 8, 12}

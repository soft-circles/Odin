package n64_bit_field

import "base:runtime"

// Fixture-local logging only; this compiler test has no Odin64 dependency.
foreign import dragon "system:dragon"
@(default_calling_convention="c")
foreign dragon {
	debugf :: proc(msg: cstring, #c_vararg args: ..any) ---
}

#assert(ODIN_OS == .N64)

Buttons :: bit_field u16 {
	c_right: bool | 1,
	c_left:  bool | 1,
	pad:     u8   | 6,
	r:       bool | 1,
	start:   bool | 1,
	_:       u8   | 5,
	a:       bool | 1,
}

Surface_Flags :: bit_field u16 {
	format:      u8   | 5,
	owned:       bool | 1,
	_:           u8   | 2,
	placeholder: u8   | 4,
}

Wide :: bit_field u32 {
	low:    u8  | 4,
	middle: u16 | 16,
	signed: i8  | 5,
	top:    u8  | 7,
}

Split :: bit_field [2]u16 {
	low:   u16 | 12,
	cross: u16 | 8,
	high:  u16 | 12,
}

failures: int

expect :: proc(name: cstring, got, want: u64) {
	if got != want {
		debugf("ODIN_N64_BIT_FIELD_FAIL:%s got 0x%08lx%08lx want 0x%08lx%08lx\n", name,
		       u32(got >> 32), u32(got), u32(want >> 32), u32(want))
		failures += 1
	}
}

check_buttons :: proc() {
	b: Buttons
	b.a = true
	expect("store bool bit 15", u64(transmute(u16)b), 0x8000)
	b.c_right = true
	expect("store bool bit 0", u64(transmute(u16)b), 0x8001)
	b.pad = 0x2a
	expect("store 6-bit field", u64(transmute(u16)b), 0x80a9)
	b.start = true
	expect("store bool bit 9", u64(transmute(u16)b), 0x82a9)
	b.a = false
	expect("clear bool bit 15", u64(transmute(u16)b), 0x02a9)

	c := transmute(Buttons)u16(0x8102)
	expect("load bool bit 15", u64(c.a), 1)
	expect("load bool bit 8", u64(c.r), 1)
	expect("load bool bit 1", u64(c.c_left), 1)
	expect("load bool bit 0", u64(c.c_right), 0)
	expect("load bool bit 9", u64(c.start), 0)

	literal := Buttons{a = true, c_right = true}
	expect("literal matches stores", u64(transmute(u16)literal), 0x8001)
}

check_surface_flags :: proc() {
	f: Surface_Flags
	f.format = 0x13
	f.owned = true
	f.placeholder = 0xb
	expect("surface flags store", u64(transmute(u16)f), 0x0b33)

	g := transmute(Surface_Flags)u16(0x0a24)
	expect("surface format load", u64(g.format), 0x04)
	expect("surface owned load", u64(g.owned), 1)
	expect("surface placeholder load", u64(g.placeholder), 0xa)
}

check_wide :: proc() {
	w: Wide
	w.low = 0x5
	w.middle = 0xbeef
	w.signed = -3
	w.top = 0x55
	expect("u32 store across bytes", u64(transmute(u32)w), 0xabdbeef5)

	v := transmute(Wide)u32(0x7b1234c9)
	expect("u32 low load", u64(v.low), 0x9)
	expect("u16 field load across bytes", u64(v.middle), 0x234c)
	expect("signed field load", u64(v.signed + 16), 1)
	expect("u32 top load", u64(v.top), 0x3d)
}

check_split :: proc() {
	s: Split
	s.low = 0xab
	s.cross = 0xcd
	s.high = 0xef
	arr := transmute([2]u16)s
	expect("array element 0", u64(arr[0]), 0xd0ab)
	expect("array element 1", u64(arr[1]), 0x0efc)

	t := transmute(Split)[2]u16{0x9123, 0x0456}
	expect("array low load", u64(t.low), 0x123)
	expect("array cross load", u64(t.cross), 0x69)
	expect("array high load", u64(t.high), 0x045)
}

main :: proc() {
	runtime.print_string("ODIN_N64_BIT_FIELD_START\n")
	check_buttons()
	check_surface_flags()
	check_wide()
	check_split()
	if failures == 0 {
		debugf("ODIN_N64_BIT_FIELD_PASS\n")
	}
	debugf("ODIN_N64_BIT_FIELD_DONE\n")
}

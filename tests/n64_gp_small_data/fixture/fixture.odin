package n64_gp_small_data

// GCC's -G 8 puts globals of 8 bytes or less in .sdata or .sbss, which libdragon's n64.ld reaches from _gp.
counter: u32
ticks: u64
nine: [9]u8
@(link_section=".data.placed")
placed: u32

foreign {
	c_small: u32
	c_zero: [0]u8
}

@(export)
bump_counter :: proc "c" () -> u32 {
	counter += 1
	return counter
}

@(export)
bump_ticks :: proc "c" () {
	ticks += 1
}

@(export)
bump_nine :: proc "c" (index: u32) {
	nine[index % len(nine)] += 1
}

@(export)
bump_placed :: proc "c" () {
	placed += 1
}

@(export)
read_c_small :: proc "c" () -> u32 {
	return c_small
}

@(export)
address_of_c_zero :: proc "c" () -> rawptr {
	return &c_zero
}

@(export)
scale :: proc "c" (x: f32) -> f32 {
	return x * 1.2345
}

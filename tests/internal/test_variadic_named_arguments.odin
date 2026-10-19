package test_internal

import "core:fmt"
import "core:testing"

@(private="file")
format_with_action :: proc(format: string, args: ..any, action := "") -> string {
	return fmt.tprintf(format, ..args)
}

@(private="file")
last_part :: proc(parts: ..string) -> string {
	return parts[len(parts)-1]
}

@(private="file")
sum_ints :: proc(xs: ..int, extra := 0) -> int {
	total := 0
	for x in xs {
		total += x
	}
	return total*1000 + extra
}

@(private="file")
two_strings :: proc() -> (string, string) {
	return "t1", "t2"
}

@(private="file")
join_any :: proc(args: ..any, sep := "") -> string {
	return fmt.tprint(..args, sep = " ")
}

@(test)
test_variadic_args_survive_variadic_named_argument :: proc(t: ^testing.T) {
	Change :: struct { path: string }
	change := Change{path = "src/assets.odin"}

	testing.expect_value(t, format_with_action("%s", change.path, action = fmt.tprintf("%s", "other")), "src/assets.odin")
	testing.expect_value(t, format_with_action("%s", change.path, action = last_part("Set ", "X")), "src/assets.odin")
	testing.expect_value(t, sum_ints(1, 2, 7, extra = sum_ints(5, 6)), 21000)
	testing.expect_value(t, join_any(two_strings(), sep = fmt.tprint("x", "y")), "t1 t2")
}

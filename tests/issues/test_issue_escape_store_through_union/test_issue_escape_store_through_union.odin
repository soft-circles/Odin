// A call that stores through a pointer loaded via a union field (`&s.log.?`) loses the type of the memory it
// stores into. Escape analysis used to nest the stored value's paths without end, so `odin check` never finished.

package test_issue_escape_store_through_union

Log :: struct {
	entries: [dynamic]State,
}

State :: struct {
	a, b:   [dynamic]int,
	va, vb: []int,
	log:    Maybe(Log),
}

record :: proc(s: ^State) {
	log := &s.log.?
	append(&log.entries, s^)
}

record_in_a_loop :: proc() {
	s: State
	s.va = s.a[:]
	s.vb = s.b[:]
	for _ in 0 ..< 2 {
		record(&s)
	}
}

record_in_a_row :: proc() {
	s: State
	s.va = s.a[:]
	s.vb = s.b[:]
	record(&s); record(&s); record(&s); record(&s); record(&s)
	record(&s); record(&s); record(&s); record(&s); record(&s)
	record(&s); record(&s); record(&s); record(&s); record(&s)
	record(&s); record(&s); record(&s); record(&s); record(&s)
}

main :: proc() {
	record_in_a_loop()
	record_in_a_row()
}

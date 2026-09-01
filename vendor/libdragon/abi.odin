package libdragon

import "core:c"

// C99 _Bool is one byte in the pinned SDK ABI. Use c.bool in every foreign
// signature so the binding states that relationship explicitly.
#assert(size_of(c.bool) == 1)
#assert(align_of(c.bool) == 1)

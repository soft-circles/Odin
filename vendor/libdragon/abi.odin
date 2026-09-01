package libdragon

import "core:c"

// C99 _Bool is one byte in the pinned SDK ABI. Every foreign `_Bool` signature
// uses c.bool so the declaration states that relationship explicitly.
#assert(size_of(c.bool) == 1)
#assert(align_of(c.bool) == 1)

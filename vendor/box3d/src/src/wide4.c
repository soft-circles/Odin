// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#define B3_SIMD_WIDTH 4
#include "core.h"

#if B3_FORCE_OVERFLOW == 0
#include "contact_solver_wide.inl"
#endif
#include "convex_manifold_wide.inl"

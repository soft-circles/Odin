// Package libdragon exposes the narrow raw libdragon surface qualified for the
// Odin N64 target. It does not manage subsystem lifetimes, resource ownership,
// display/console state, joypad polling, or DragonFS handles for the caller.
//
// These declarations are pinned to the installed libdragon SDK at commit
// c79a52b42ac790e06e797aede43914dd8754cd5f. Do not update them from a different
// checkout without re-running the C layout and runtime ABI probes. See
// README.md for supported subsystems, C-to-Odin type rules, and ordering.
package libdragon

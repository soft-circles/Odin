package libdragon

import "core:c"

foreign import lib "system:dragon"

pi_addr_t :: u32

DFS_DEFAULT_LOCATION :: pi_addr_t(0)
DFS_ESUCCESS         :: c.int(0)

@(default_calling_convention="c")
foreign lib {
	dfs_init  :: proc(base_fs_loc: pi_addr_t) -> c.int ---
	dfs_open  :: proc(path: cstring) -> c.int ---
	dfs_read  :: proc(buf: rawptr, size, count: c.int, handle: u32) -> c.int ---
	dfs_close :: proc(handle: u32) -> c.int ---
	dfs_size  :: proc(handle: u32) -> c.int ---
}

#assert(size_of(pi_addr_t) == 4)

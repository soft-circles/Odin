package libdragon

import "core:c"

foreign import lib "system:dragon"

pi_addr_t :: u32

DFS_DEFAULT_LOCATION :: pi_addr_t(0)
DFS_ESUCCESS         :: c.int(0)

@(default_calling_convention="c")
foreign lib {
	// Mount the packaged DragonFS image before opening files.
	dfs_init  :: proc(base_fs_loc: pi_addr_t) -> c.int ---
	// Open a root-relative path. Negative results are errors; convert a
	// successful handle to u32 only after checking.
	dfs_open  :: proc(path: cstring) -> c.int ---
	// Read count elements of size bytes. A negative result is an error.
	dfs_read  :: proc(buf: rawptr, size, count: c.int, handle: u32) -> c.int ---
	// Close every successfully opened handle.
	dfs_close :: proc(handle: u32) -> c.int ---
	// Return the file size or a negative error result.
	dfs_size  :: proc(handle: u32) -> c.int ---
}

#assert(size_of(pi_addr_t) == 4)

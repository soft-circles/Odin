# N64 compiler runtime probe

This standalone compiler-owned fixture tests global initialization, package init,
both context allocators (alignment, zeroing, growth, shrink, OOM, reset and
replacement), built-in maps, main return, and package finalization. Its only local
foreign declarations are the SDK logging functions used as the test oracle. It does
not import Odin64 or require a graphics binding.

```sh
./odin check tests/n64_runtime -target:n64 -vet -warnings-as-errors
./odin build tests/n64_runtime -target:n64 -build-mode:obj -out:runtime-obj
./odin build tests/n64_runtime -target:n64 -out:runtime.z64
"$ARES_TEST" tests/n64_runtime/runtime.test.js runtime.z64 --timeout 30
```

The object build runs code generation without an SDK, so quick validation catches
backend crashes that `odin check` never reaches. Linking a ROM requires an
[installed SDK](../../N64_BUILD.md). Ordered EMUX sentinels verify lifecycle
behavior; they are not a process-exit-status API. Framebuffer, input and binding
integration coverage lives in [Odin64](https://github.com/soft-circles/Odin64).

# Integrated N64 build tests

See [`N64_BUILD.md`](../../N64_BUILD.md) for the project-facing build guide.
This file documents the compiler's automated regression coverage.

This directory tests the integrated N64 build path, including ROM
configuration, extended metadata, and raw DFS assets, only through its public
CLI:

```sh
python3 tests/n64_build/test_n64_build.py
```

For normal contributor confidence, use the repository-level orchestrator:

```sh
python3 tests/n64_validate.py quick
```

The focused command above remains available when this stage fails. The
full layer runs it with the selected SDK and does not accept its
end-to-end skip.

The SDK discovery tests cover required files/tools and custom SDKs with absent,
unrecognized or locally modified version metadata. A stub link driver that
exits with an error proves the compiler reaches packaging and preserves its
failure. The end-to-end
tests use `N64_INST` when it is set, otherwise they use
`~/n64_toolchain` when that directory exists. Set `ODIN` to select a compiler
binary other than the repository's `odin` executable.

The end-to-end runtime probe test deliberately puts the source, SDK alias, output,
and retained intermediates beneath paths containing spaces. It builds the
existing Odin-only runtime probe with a single `odin build . -target:n64` invocation,
checks the requested `.z64` and ROM byte order, and verifies that
`-keep-temp-files` retained the link and packaging intermediates.
When `ARES_TEST` names a compatible `ares-test` executable, the same test also
runs the generated runtime probe ROM through the pinned headless script and checks its
ordered startup/allocator/cleanup sentinels.
An additional link test supplies both a prebuilt foreign object and archive and
checks that the integrated path carries them into libdragon's static link.
Further end-to-end tests check that `-extra-linker-flags` reaches the link, that
an SDK directory whose own name contains spaces packages the same ROM, and that
the ROM title defaults to the package directory name.
The [canonical option table](../../N64_BUILD.md#rom-configuration-options) and
[maintainer intermediates contract](../../N64_MAINTAINERS.md#intermediates-and-cleanup-guarantees)
own the behavior exercised here.

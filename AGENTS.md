# Shaka Packager -- Agent Guide

Shaka Packager is a C++ media packaging SDK and command-line tool for
[DASH](http://dashif.org/) and [HLS](https://developer.apple.com/streaming/)
streaming, supporting DRM encryption and a wide range of codecs and containers.
See [README.md](README.md) for full details.

## Attribution

Read [AGENT-ATTRIBUTION.md](AGENT-ATTRIBUTION.md) for attribution details.

## Build Workflow

**Configure (Debug):**
```shell
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Debug -G Ninja
```

Debug is the default for development. Use `-DCMAKE_BUILD_TYPE=Release` for
distribution builds (as used in CI). Build output lands in `build/`.

**Build:**
```shell
cmake --build build/ --parallel
```

**Run all tests (slow -- includes end-to-end tests):**
```shell
ctest -C Debug -V --test-dir build/
```

**Clean:**
```shell
rm -rf build
```

**Update submodules** (after pulling or switching branches):
```shell
git submodule update --force --recursive --init
```

## Cross-platform Constraints

**Include-what-you-use (IWYU) -- the most common CI failure cause:**
Always explicitly `#include` every header your code uses directly. Never rely
on transitive includes. Different standard library implementations on different
OSes have different implicit inclusion chains -- code that compiles on Linux
x64 may silently fail on macOS arm64 or Windows. Be pedantic about includes.

**Static builds and dependencies:**
All dependencies are built from source via git submodules. Do not introduce
new system library dependencies. Exception: on Windows and macOS, certain
system libraries are permitted as dynamic links. The CI workflows
(`.github/workflows/`) are the authoritative reference for what is allowed.

**Supported platforms:** Linux (x64, arm64), macOS (x64, arm64), Windows (x64 only).

## Code Style

C++ is formatted with `clang-format` using Chromium style. Linter failures
are a common CI failure mode -- always run the check before considering a
change complete.

**Check formatting** (C++ via clang-format, Python via pylint):
```shell
python3 packager/tools/git/check_formatting.py main
```

The `main` argument is a git ref used as a baseline; only files changed since
that ref are checked. `main` is the right default when working from a branch.

**Auto-fix C++ formatting:**
```shell
git clang-format --style Chromium main
```

Same baseline ref semantics as above.

## Testing

Unit tests are colocated with the source files they test. Test executables
follow the naming convention `<subsystem>_unittest` (e.g. `mp4_unittest`,
`hls_unittest`; `<subsystem>_unittest.exe` on Windows). These are defined via
`add_executable` in each subdirectory's `CMakeLists.txt`.

**Run a specific subsystem's tests** (faster when iterating):
```shell
./build/packager/mp4_unittest       # example
./build/packager/hls_unittest       # example
```

**Run all tests** (slow -- includes end-to-end golden output tests):
```shell
ctest -C Debug -V --test-dir build/
```

End-to-end tests run the `packager` binary against real media and compare
output to golden files in `packager/app/test/testdata/`.

**To regenerate goldens:**
```shell
./build/packager/packager_test.py --test_update_golden_files
```

Before committing updated goldens, verify that it makes sense for them to
have changed at all. Changes to binary media outputs are rare and require
clear justification (e.g. packaging structure changes, a dependency like
libwebm was upgraded). Changes to manifest outputs (HLS/DASH) are more
common but still need a clear reason.

Unit tests embed expected output as byte arrays inline in the test source --
not in data files.

Always add or update tests when making a change. If a bug exists without a
corresponding test failure, test coverage is insufficient -- add a regression
test.

## Directory Structure

Shallow overview of top-level directories. **If you add, remove, or rename a
top-level directory as part of a change, update this section.** For deeper
structure within `packager/`, the `CMakeLists.txt` files are the authoritative
source.

```
packager/            C++ sources, tests, and third-party dependencies
  third_party/       One subdirectory per dependency; each contains:
    <dep>/source/    Dependency source, managed as a git submodule
    <dep>/CMakeLists.txt  Integrates that dependency into the larger build
docs/                Project documentation (Sphinx/RST)
include/             Public C++ headers for the libpackager SDK
link-test/           Dummy app that verifies libpackager links correctly and
                     that public headers in include/ are complete and
                     self-contained (shared library builds only)
npm/                 npm package wrapper
```

**Adding new third-party dependencies** is a significant, rarely-approved
change. It requires strong justification and will receive close scrutiny from
maintainers. Do not add new entries under `packager/third_party/` without
prior discussion.

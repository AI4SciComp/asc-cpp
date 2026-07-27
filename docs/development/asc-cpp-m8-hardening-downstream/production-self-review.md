# Milestone 8 Production Hardening Self-Review

Status: Complete; final lead-matrix revalidation required

Date: 2026-07-27

Write scope:

```text
tools/hardening/**
abi/**
docs/development/asc-cpp-m8-hardening-downstream/production-self-review.md
```

No product header, source, CMake/package file, test, benchmark, general
documentation, architecture file, sibling repository, or Git history was
modified by this role.

## Delivered artifacts

### Public surface

`tools/hardening/CheckPublicSurface.cmake` deterministically checks:

- all 49 `include/asc/*.h` paths;
- LF-normalized SHA-256 content for every public header;
- all 15 known components and their exact direct component dependencies; and
- installed header equivalence for either the CUDA-disabled 37-header
  projection or the complete 49-header provider-enabled projection.

The content hashes cover the full public source surface, including templates,
inline definitions, macros, declarations, and comments. This is intentionally
stricter than a declaration-only approximation. Normalizing CRLF avoids a
normal Windows checkout becoming a false API change.

### Configured targets

`tools/hardening/TargetInventory.cmake` checks the actual configured CMake
targets after creation:

- all target and `ASC::` alias identities;
- compiled versus interface target kind;
- `EXPORT_NAME` and `OUTPUT_NAME`;
- strict local and propagated C++20;
- exact direct and public link sets;
- linkage-appropriate static-definition macros; and
- ownership of all public header file sets.

`ProjectTargetInventoryHook.cmake` supplies a read-only integration route
through `CMAKE_PROJECT_INCLUDE` and a deferred end-of-project check. It does
not require a product or root-CMake edit and does not mutate target
properties.

### Shared symbol and ABI observations

`tools/hardening/InspectElfAbi.cmake` uses locally available GNU/LLVM-style
`readelf`, `nm`, and `c++filt` to record:

- ELF class, byte order, OS ABI, and machine;
- SONAME and direct runtime dependencies; and
- every defined dynamic symbol, preserving its mangled ABI key, symbol type,
  and demangled review spelling.

Addresses and build-placement values are excluded. Libraries and symbols are
sorted. A missing tool or a non-ELF input is classified `skipped`; it is not
reported as a pass. Exact full reports can be compared directly. The committed
local baselines use a SHA-256 of the complete deterministic report plus
reviewed per-library counts and limitations.

## Baselines

```text
abi/public-headers.sha256
abi/header-owners.txt
abi/targets-and-components.txt
abi/linux-x86_64-gcc11-cpu-shared.txt
abi/linux-x86_64-clang19-cpu-shared.txt
abi/linux-x86_64-gcc11-cuda12-shared.txt
```

The three ABI observations are deliberately environment-specific:

| Baseline | Configuration | Libraries | Defined symbols |
| --- | --- | ---: | ---: |
| GCC 11 CPU | Ubuntu 22.04 x86-64, GCC 11.4, Debug shared, CUDA off | 5 | 261 |
| Clang 19 CPU | Ubuntu 22.04 x86-64, Clang 19.0, Debug shared, CUDA off | 5 | 483 |
| GCC/NVCC CUDA | Ubuntu 22.04 x86-64, GCC 11.4, NVCC 12.9.86, Release shared, CUDA on | 11 | 264 |

These are pre-1.0 review observations, not cross-minor, compiler, standard
library, build-mode, CUDA-toolkit, driver, platform, or hardware ABI promises.

## Exact focused validation

### Source public surface

```sh
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  -DASC_CPP_HARDENING_OUTPUT=/tmp/asc-cpp-m8-public-surface.txt \
  -P tools/hardening/CheckPublicSurface.cmake
```

Result: **PASS** — 49 public headers and 15 components.

### GCC Release static, CUDA disabled, configured targets

```sh
m8_target_dir=$(mktemp -d /tmp/asc-cpp-m8-target-inventory.XXXXXX)
cmake -S . -B "$m8_target_dir/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DCMAKE_PROJECT_INCLUDE="$PWD/tools/hardening/ProjectTargetInventoryHook.cmake" \
  -DASC_CPP_HARDENING_TARGET_OUTPUT="$m8_target_dir/target-inventory.txt"
```

Result: **PASS** at
`/tmp/asc-cpp-m8-target-inventory.hrRvxV` — nine CPU targets checked; six
CUDA targets truthfully skipped because CUDA was disabled.

### GCC/NVCC Release shared, CUDA enabled, configured targets

```sh
m8_cuda_target_dir=$(mktemp \
  -d /tmp/asc-cpp-m8-cuda-target-inventory.XXXXXX)
cmake -S . -B "$m8_cuda_target_dir/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DCMAKE_PROJECT_INCLUDE="$PWD/tools/hardening/ProjectTargetInventoryHook.cmake" \
  -DASC_CPP_HARDENING_TARGET_OUTPUT="$m8_cuda_target_dir/target-inventory.txt"
```

Result: **PASS** at
`/tmp/asc-cpp-m8-cuda-target-inventory.UqSZF7` — all 15 targets checked,
including the exact CUDA Runtime, cuBLAS, and cuSPARSE implementation edges.
This is target configuration evidence, not a runtime GPU result.

### Clang 19 Debug shared build and installation

```sh
m8_clang_dir=$(mktemp -d /tmp/asc-cpp-m8-clang-shared.XXXXXX)
CC=clang-19 CXX=clang++-19 cmake -S . -B "$m8_clang_dir/build" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build "$m8_clang_dir/build" --parallel 4
cmake --install "$m8_clang_dir/build" --prefix "$m8_clang_dir/prefix"
```

Result: **PASS** at `/tmp/asc-cpp-m8-clang-shared.1A7zRo` — configure,
17 compilation/link steps, and installation completed with Clang 19.0.0.

### Installed public-header equivalence

```sh
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  -DASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR=/tmp/asc-cpp-m8-clang-shared.1A7zRo/prefix/include \
  -DASC_CPP_HARDENING_OUTPUT=/tmp/asc-cpp-m8-clang-shared.1A7zRo/installed-public-surface.txt \
  -P tools/hardening/CheckPublicSurface.cmake

cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  -DASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR=/tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/prefix/include \
  -DASC_CPP_HARDENING_OUTPUT=/tmp/asc-cpp-m8-cuda-installed-public-surface.txt \
  -P tools/hardening/CheckPublicSurface.cmake
```

Results:

- **PASS** — CUDA-disabled installed projection, 37 headers.
- **PASS** — provider-enabled installed projection, all 49 headers.

The provider-enabled installation was the unchanged-product M7 final shared
installation. The lead must repeat this check against the final fresh M8
installation before Checkpoint B.

### ELF observations and baseline comparisons

The common command shape was:

```sh
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  '-DASC_CPP_HARDENING_LIBRARIES=<sorted semicolon-separated shared libraries>' \
  -DASC_CPP_HARDENING_OUTPUT=<external full-report path> \
  -DASC_CPP_HARDENING_BASELINE=<matching abi baseline> \
  -P tools/hardening/InspectElfAbi.cmake
```

Exact checked inputs and results:

```text
/tmp/asc-cpp-m7-final-cpu-shared.nEcEvg/prefix/lib/libasc_*.so
  -> abi/linux-x86_64-gcc11-cpu-shared.txt
  -> PASS, 5 libraries

/tmp/asc-cpp-m8-clang-shared.1A7zRo/prefix/lib/libasc_*.so
  -> abi/linux-x86_64-clang19-cpu-shared.txt
  -> PASS, 5 libraries

/tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/prefix/lib/libasc_*.so
  -> abi/linux-x86_64-gcc11-cuda12-shared.txt
  -> PASS, 11 libraries
```

GNU Binutils 2.38 supplied `readelf`, `nm`, and `c++filt`. Repeated reports
matched their exact committed SHA-256 baselines.

### Structural checks

```sh
git diff --check -- \
  tools/hardening \
  abi \
  docs/development/asc-cpp-m8-hardening-downstream/production-self-review.md

awk -F'|' \
  '/^target/ {if (NF != 10) print "bad target row", NR, NF}
   /^component/ {if (NF != 3) print "bad component row", NR, NF}' \
  abi/targets-and-components.txt

awk -F'|' \
  'NF != 2 {print "bad owner row", NR, NF}' \
  abi/header-owners.txt
```

Result: **PASS** — no whitespace errors or malformed baseline rows.

## Findings

### M8-PROD-001 — compiler-owned weak symbols are observable

The GCC Debug CPU libraries expose 83 weak/unique symbols; the Clang Debug
CPU libraries expose 275. Most are standard-library template instantiations.
The exact set differs materially between GCC and Clang despite the same
libstdc++ and product source.

Resolution: recorded separate exact toolchain baselines and explicitly denied
cross-toolchain ABI stability. This is a remaining portability/ABI risk, not a
reason to add a new visibility mechanism or dependency in M8.

### M8-PROD-002 — intentional internal support symbols are ABI-observable

Public dense and sparse templates dispatch to exported
`internal_dense_linalg` and `internal_sparse_linalg` functions. Core public
execution owners also have ABI spellings containing internal state types.

Resolution: retained and recorded them because the frozen contract explicitly
requires intentional internal support symbols needed by public templates.
They remain implementation-named and pre-1.0; no compatibility promise was
added.

### M8-PROD-003 — local ABI tooling is ELF-specific

`readelf`/`nm` evidence is valid for the local Linux ELF matrix only. PE/COFF,
Mach-O, MSVC, AppleClang, MinGW, 32-bit, and cross-compiled observations were
not available.

Resolution: the tool returns `skipped` when it cannot make an ELF observation.
Unavailable formats remain skips for Checkpoint B and require native future
tooling rather than inference.

### M8-PROD-004 — shared libraries have unversioned SONAMEs

All observed shared libraries use names such as `libasc_core.so` as their
SONAME. No product `SOVERSION` exists.

Resolution: observation only. ADR 0018 and the frozen M8 contract deny a
cross-minor ABI guarantee, and no SOVERSION change was approved. This fact
must remain visible in the final ABI policy and risk list.

### M8-PROD-005 — no production or package defect found

The target inventory matched all 15 approved targets, aliases, public header
owners, C++20 propagation requirements, and exact direct dependency edges in
both the CPU-static and CUDA-shared configurations. No product edit was
requested.

## Lead integration requests

The lead should:

1. register the source/component checker in the M8 hardening test group;
2. use `ProjectTargetInventoryHook.cmake` in fresh static/shared and
   CUDA-disabled/enabled configurations, or call the inventory function after
   all targets exist;
3. run installed-header equivalence against final CPU and CUDA installation
   prefixes;
4. generate ELF reports from the final clean GCC, Clang, and CUDA shared
   builds and compare them with the matching baselines; and
5. classify non-ELF/native-platform ABI evidence as skipped, never passed.

No root-CMake integration is required by the tools themselves. Any CTest
registration remains lead-owned.

## Final scope statement

No dependency, public API, product target, operation, provider, package
component, or later milestone was added. No commit, push, merge, tag, release,
or branch deletion occurred.

# Milestone 8 Revision-2 Production Hardening Self-Review

Status: Complete; final lead-matrix revalidation required

Date: 2026-07-28

Branch: `feature/asc-cpp-m8-hardening-downstream-r2`

Write scope:

```text
tools/hardening/**
abi/**
docs/development/asc-cpp-m8-hardening-downstream/production-self-review.md
```

No product header, source, CMake/package file, test, benchmark, general
documentation, architecture file, sibling repository, or Git history was
modified by this role.

## Revision-2 boundary

The old `feature/asc-cpp-m8-hardening-downstream` branch was inspected only as
project-owned prior evidence. Its baselines were not restored because they
describe an earlier M7 surface. In particular, its
`asc/random/providers/cuda.h` baseline contains the obsolete pointer-plus-count
raw-fill API.

Revision 2 inventories the exact approved cumulative M0--M7 worktree. The
recorded declaration and ELF symbol are:

```text
CudaFillPhilox4x32(const ExecutionContext&, MutableMemoryView,
                   std::uint64_t word_count, RandomStream,
                   RandomSubsequence, RandomOffset)
```

No pointer-plus-count compatibility overload is present or recorded.

## Delivered artifacts

### Public surface

`tools/hardening/CheckPublicSurface.cmake` deterministically checks:

- all 49 `include/asc/*.h` paths;
- LF-normalized SHA-256 content for every public header;
- all 15 known components in repository order and their exact direct component
  dependencies; and
- installed-header equivalence for either the CUDA-disabled 37-header
  projection or the complete 49-header CUDA-enabled projection.

The hashes cover declarations, templates, inline definitions, macros, and
comments. Normalizing CRLF and lone CR prevents normal line-ending conversion
from becoming a false surface change.

### Configured targets

`tools/hardening/TargetInventory.cmake` checks the actual configured CMake
targets after creation:

- all target and `ASC::` alias identities;
- compiled versus interface target kind;
- `EXPORT_NAME` and `OUTPUT_NAME`;
- strict local and propagated C++20;
- exact direct and public link sets;
- linkage-appropriate static-definition macros; and
- ownership of every configured public-header file set.

`ProjectTargetInventoryHook.cmake` supplies a read-only integration path through
`CMAKE_PROJECT_INCLUDE` and a deferred end-of-project check. It does not mutate
target properties or require a product CMake edit.

### Shared symbols and bounded ABI observations

`tools/hardening/InspectElfAbi.cmake` uses locally available GNU/LLVM-style
`readelf`, `nm`, and `c++filt` to record:

- ELF class, byte order, OS ABI, and machine;
- SONAME and direct runtime dependencies; and
- every defined dynamic symbol, preserving its mangled ABI key, type, and
  demangled review spelling.

Addresses and build placement are excluded. Libraries and symbols are sorted.
A missing inspection tool or non-ELF input is classified `skipped`; it is not
reported as a pass. A reviewed baseline may contain the SHA-256 of the complete
deterministic report.

## Baselines

```text
abi/public-headers.sha256
abi/header-owners.txt
abi/targets-and-components.txt
abi/linux-x86_64-gcc11-cpu-shared.txt
abi/linux-x86_64-clang19-cpu-shared.txt
abi/linux-x86_64-gcc11-cuda12-shared.txt
```

The three ABI observations are environment-specific:

| Baseline | Configuration | Libraries | Defined symbols |
| --- | --- | ---: | ---: |
| GCC 11 CPU | Ubuntu 22.04 x86-64, GCC 11.4, Debug shared, CUDA off | 5 | 230 |
| Clang 19 CPU | Ubuntu 22.04 x86-64, Clang 19.0, Debug shared, CUDA off | 5 | 413 |
| GCC/NVCC CUDA | Ubuntu 22.04 x86-64, GCC 11.4, NVCC 12.9.86, Release shared, CUDA on | 11 | 250 |

These are pre-1.0 review observations, not cross-minor, compiler, standard
library, build-mode, CUDA-toolkit, driver, GPU, platform, or hardware ABI
promises.

## Exact focused validation

### Source public surface

```sh
m8r2_tmp=$(mktemp -d /tmp/asc-cpp-m8r2-production.XXXXXX)
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  -DASC_CPP_HARDENING_OUTPUT="$m8r2_tmp/public-surface.txt" \
  -P tools/hardening/CheckPublicSurface.cmake
```

Result: **PASS** -- 49 public headers and 15 components.

### GCC Release static, CUDA disabled, configured targets

```sh
m8r2_target=$(mktemp -d /tmp/asc-cpp-m8r2-target.XXXXXX)
cmake -S . -B "$m8r2_target/build" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DCMAKE_PROJECT_INCLUDE="$PWD/tools/hardening/ProjectTargetInventoryHook.cmake" \
  -DASC_CPP_HARDENING_TARGET_OUTPUT="$m8r2_target/target-inventory.txt"
```

Result: **PASS** at `/tmp/asc-cpp-m8r2-target.R3gRKS` -- nine CPU targets
checked; six CUDA targets truthfully skipped because CUDA was disabled.

### GCC/NVCC Release shared, CUDA enabled, configured targets

```sh
m8r2_cuda_target=$(mktemp \
  -d /tmp/asc-cpp-m8r2-cuda-target.XXXXXX)
cmake -S . -B "$m8r2_cuda_target/build" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_HOST_COMPILER=g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DCMAKE_PROJECT_INCLUDE="$PWD/tools/hardening/ProjectTargetInventoryHook.cmake" \
  -DASC_CPP_HARDENING_TARGET_OUTPUT="$m8r2_cuda_target/target-inventory.txt"
```

Result: **PASS** at `/tmp/asc-cpp-m8r2-cuda-target.yQGu0E` -- all 15 targets
checked, including the exact CUDA Runtime, cuBLAS, and cuSPARSE implementation
edges. This is `configure-tested` evidence, not a runtime GPU result.

### Fresh CPU shared builds and installs

The following shape was run once with `g++` and once with `clang++-19`:

```sh
cmake -S . -B "$build_root/build" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER="$compiler" \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build "$build_root/build" --parallel 4
cmake --install "$build_root/build" --prefix "$build_root/prefix"
```

Results:

- **PASS**, GCC 11 at
  `/tmp/asc-cpp-m8r2-gcc-debug-shared.yPFZXN`;
- **PASS**, Clang 19 at
  `/tmp/asc-cpp-m8r2-clang-debug-shared.8trdbz`; and
- **PASS**, both 37-header installed CPU projections.

### Full installed CUDA header equivalence

The exact current M7 CUDA build was installed to a fresh external prefix before
checking, avoiding an older prefix left from an earlier install:

```sh
m8r2_cuda_prefix=$(mktemp \
  -d /tmp/asc-cpp-m8r2-cuda-install.XXXXXX)
cmake --install \
  build/m7-final-gcc-cuda-release-shared-make \
  --prefix "$m8r2_cuda_prefix"
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  -DASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR="$m8r2_cuda_prefix/include" \
  -DASC_CPP_HARDENING_OUTPUT=/tmp/m8r2-cuda-public-surface.txt \
  -P tools/hardening/CheckPublicSurface.cmake
```

Result: **PASS** at `/tmp/asc-cpp-m8r2-cuda-install.gFtdpj` -- all 49
installed headers equal the approved revision-2 baseline.

### ELF reports and exact baseline comparison

The common command shape was:

```sh
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR="$PWD" \
  "-DASC_CPP_HARDENING_LIBRARIES=<sorted-semicolon-list>" \
  -DASC_CPP_HARDENING_OUTPUT=<external-full-report> \
  -DASC_CPP_HARDENING_BASELINE=<matching-abi-baseline> \
  -P tools/hardening/InspectElfAbi.cmake
```

Results:

| Input | Baseline digest | Result |
| --- | --- | --- |
| fresh GCC 11 Debug CPU shared install | `ff013b28794f21c27f01e58601f4b0026b0dde20df0b54b2afe4e8faf8e2be99` | **PASS**, 5 libraries |
| fresh Clang 19 Debug CPU shared install | `f23f6342e56ea277334b5b03459a7d98304448f632972d00a1de04d0426b67d4` | **PASS**, 5 libraries |
| exact final M7 GCC/NVCC Release CUDA shared build | `0da3f872cb76f1a876567409f8baae09a80198214368faa229c0698e456646b4` | **PASS**, 11 libraries |

Each report was generated twice. The second generation passed the committed
digest comparison and was byte-equal to the first. The CUDA report contains:

```text
asc::CudaFillPhilox4x32(asc::ExecutionContext const&,
                        asc::MutableMemoryView, unsigned long,
                        unsigned long, unsigned long, unsigned long)
```

### Path, skip, and mutation guards

- Public-surface and ELF reports passed with output directories and filenames
  containing spaces.
- A fresh CPU target inventory passed with both build and report paths
  containing spaces.
- Supplying a non-ELF input produced
  `status|skipped|not-elf|README.md`.
- Attempting to write a report beneath the repository source tree failed before
  any file was created.

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

Result: **PASS** -- no whitespace errors or malformed baseline rows.

## Findings

### M8R2-PROD-001 -- prior M8 baselines are stale

The old branch records an earlier M7 header and CUDA ABI, including the rejected
pointer-plus-count raw CUDA signature. Several other public-header hashes and
local symbol counts also differ from the final approved M7 candidate.

Resolution: regenerated every surface hash and all three local ABI observations
from the current worktree or its exact final build. No old digest or product
state was restored.

### M8R2-PROD-002 -- compiler-owned symbols are ABI-observable

The GCC Debug CPU libraries expose 70 weak/unique dynamic symbols; the Clang
Debug CPU libraries expose 238. Most are compiler or standard-library template
artifacts. The exact set differs materially despite using libstdc++ in both
builds.

Resolution: recorded separate toolchain baselines and explicitly denied
cross-toolchain ABI stability. This is a remaining portability/ABI risk, not an
approved reason to add a dependency or visibility mechanism.

### M8R2-PROD-003 -- intentional support symbols are ABI-observable

Public dense and sparse templates dispatch to exported
`internal_dense_linalg` and `internal_sparse_linalg` functions. Core execution
owners also have ABI spellings containing internal state types.

Resolution: retained and recorded them because the frozen contract requires
intentional support symbols needed by public templates. They remain
implementation-named and pre-1.0.

### M8R2-PROD-004 -- local ABI tooling is ELF-specific

The evidence applies only to the local Linux ELF matrix. PE/COFF, Mach-O, MSVC,
AppleClang, MinGW, 32-bit, and cross-compiled observations were unavailable.

Resolution: a non-ELF input is reported as `skipped`. Unavailable formats
remain skips and require native future tooling rather than inferred passes.

### M8R2-PROD-005 -- shared libraries have unversioned SONAMEs

All observed shared libraries use names such as `libasc_core.so` as SONAMEs.
No product `SOVERSION` exists.

Resolution: observation only. ADR 0018 and the frozen contract deny a
cross-minor ABI guarantee, and no SOVERSION change was approved.

### M8R2-PROD-006 -- an old install prefix was not final evidence

An existing M7 CUDA prefix contained headers installed before the final
owner-approved M7 edits, although the final build directory contained the
current libraries. The checker rejected the stale prefix.

Resolution: installed the exact current final M7 build to a fresh prefix and
obtained full 49-header equivalence. This was a validation-artifact issue, not
a product or package defect. The lead must use fresh final M8 prefixes.

### M8R2-PROD-007 -- no product or package defect found

The configured inventory matched all 15 approved targets, aliases, public
header owners, C++20 propagation requirements, and exact direct dependency
edges in CPU-static and CUDA-shared configurations. No product edit is
requested.

## Lead integration requests

The lead should:

1. register the source/component checker in the M8 hardening group;
2. run the target-inventory hook in fresh static/shared and
   CUDA-disabled/enabled configurations;
3. run installed-header equivalence against fresh final CPU and CUDA prefixes;
4. generate ELF reports from the final clean GCC, Clang, and CUDA shared builds
   and compare them with the matching baselines; and
5. classify non-ELF/native-platform ABI evidence as skipped, never passed.

No root-CMake change is required by the tools themselves. Any CTest
registration remains lead-owned.

## Final scope statement

No dependency, public API, product target, operation, provider, package
component, or later milestone was added. No commit, push, merge, tag, release,
pull-request mutation, or branch deletion occurred.

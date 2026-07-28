# Milestone 8 Revision-2 Independent Verification Review

Status: Complete; accepted for lead matrix and Publication Checkpoint B

Date: 2026-07-28

Branch: `feature/asc-cpp-m8-hardening-downstream-r2`

## Review boundary and independence

The verification oracle in `verification-design.md` was frozen before this
role inspected `tools/hardening`, `abi`, or the superseded Milestone 8 branch.
It was derived from the frozen revision-2 contract, ownership ledger, approved
Milestone 7 checkpoint, and current Milestone 7 public/package surface.

This role wrote only:

```text
tests/hardening/** except tests/hardening/CMakeLists.txt
tests/downstream/** except tests/downstream/CMakeLists.txt
benchmarks/hardening/**
docs/development/asc-cpp-m8-hardening-downstream/verification-design.md
docs/development/asc-cpp-m8-hardening-downstream/verification-review.md
```

Lead-owned registration was inspected and tested after the oracle and fixtures
were complete. Product code, package/shared CMake, production tools, ABI
baselines, architecture records, general documentation, sibling repositories,
Git history, branches, remotes, and pull requests remained read-only.

## Delivered independent evidence

### Package metadata and version behavior

`tests/hardening/package_metadata_version_test.cmake` creates isolated
throwaway CMake consumers. It verified:

- exact candidate version `0.9.0`;
- exact ordered 15-component known inventory;
- exact nine-component CUDA-disabled and 15-component CUDA-enabled available
  inventories;
- default provider-free `cpp` closure;
- every explicit component's exact transitive `ASC::` target closure;
- exact direct public ASC link interfaces and C++20 propagation;
- compiled/interface target kinds and linkage-appropriate static macros;
- unknown required, known unavailable required, and quiet optional-miss
  behavior;
- provider-free success with CUDAToolkit discovery disabled;
- accepted `0.9`, accepted exact `0.9.0`, rejected `0.8`, `0.10`, `1.0`, and
  too-new `0.9.1` requests; and
- disabled use of the CMake user package registry.

The probe passed against build-tree, copied build-tree, installed, relocated,
and path-with-spaces package locations, both shared and static CPU packages,
and the fresh CUDA-enabled shared package.

### Public headers and file sets

`tests/hardening/header_manifest_test.cmake` carries an independently written
49-header ownership oracle. It compares:

1. the physical source `include/asc` tree;
2. every requested imported target's exported `HEADERS` file set;
3. the enabled physical installed header tree; and
4. each installed header byte-for-byte with its source.

Results:

- source tree: **PASS**, exactly 49 headers;
- CPU build-tree/install/relocation: **PASS**, exact 37-header provider-free
  projection;
- CUDA build-tree/install: **PASS**, exact 49-header complete projection; and
- copied and relocated paths containing spaces: **PASS**.

Deleted `asc/array.h` and `asc/linalg.h` compatibility headers remain absent.

### Public API and immutable M7 signature

`tests/hardening/public_api_surface_test.cc` uses only public Core, Dense, and
Expression APIs. It performs a Dense explicit-step workflow, verifies all
values and a reduction checksum, and checks move-only owner/event contracts.

When CUDA is enabled, its dependent compile-time oracle requires:

```text
CudaFillPhilox4x32(ExecutionContext, MutableMemoryView, word_count,
                   stream, subsequence, offset)
```

and rejects a `std::uint32_t*` destination. The GCC and Clang CPU builds and
the CUDA-enabled package compile all passed. No pointer-plus-count overload
was found in the header or shared symbol surface.

One GCC/Clang-compatible local observation was:

```text
sizeof(Status)=80
sizeof(MutableMemoryView)=24
sizeof(ExecutionContext)=32
sizeof(CompletionEvent)=16
sizeof(DenseArray<double, Extents<dynamic,dynamic>>)=120
checksum=19.95
```

These values are bounded observations, not cross-toolchain ABI promises.

### Shared symbols

`tests/hardening/shared_symbol_test.cmake` independently inspects defined
dynamic symbols with `nm` and `c++filt`. It requires representative public or
public-template-support definitions in every compiled component, including
`CompletionEvent(bool)`, Dense `Gemv`, Sparse template support, Philox, all
provider contexts, and Random provider erased support. It also rejects a
pointer-form `CudaFillPhilox4x32`.

Results:

- five CPU shared libraries: **PASS**;
- all eleven CUDA-enabled shared libraries: **PASS**; and
- production ELF digest and target/header baselines: **PASS** in the clean
  integrated shared run.

### Compile and object-size observations

`benchmarks/hardening/observe_compile_object.cmake` compiles three installed
public-header translation units:

```text
compile_core.cc
compile_dense.cc
compile_provider_free.cc
```

The record explicitly includes compiler/version/style, C++20, build mode,
host hardware, compile-only provider boundary, workload, zero warmup, one
repetition, compiler-process synchronization, options, source/object SHA-256,
object bytes, elapsed observation, pass result, and known noise. It makes no
threshold or cross-machine claim.

Final clean integrated Debug observations were:

| Compiler | Translation unit | Object bytes | Elapsed observation |
| --- | --- | ---: | ---: |
| GCC 11.4 | Core | 6,576 | 0.795574 s |
| GCC 11.4 | Dense/expression | 271,904 | 0.816891 s |
| GCC 11.4 | provider-free umbrellas | 2,048 | 0.988198 s |
| Clang 19 | Core | 5,552 | 0.986439 s |
| Clang 19 | Dense/expression | 220,600 | 0.864525 s |
| Clang 19 | provider-free umbrellas | 1,328 | 1.05307 s |

Variation from separate Release `-O2` observations confirmed that these
numbers are toolchain/configuration observations rather than ABI or
application-build predictions.

The runner supports GNU-like and MSVC/clang-cl driver syntax. GNU and Clang
execution passed locally. MSVC/clang-cl execution was unavailable and is
skipped.

### asc-xde-shaped downstream trial

`tests/downstream/asc_xde_trial.cc` and
`tests/downstream/run_asc_xde_trial.cmake` request:

```cmake
find_package(ASCCpp 0.9 REQUIRED COMPONENTS dense)
target_link_libraries(asc_xde_trial PRIVATE ASC::dense)
```

The fixture requires the exact imported closure:

```text
ASC::core
ASC::dense
ASC::expression
```

It disables CUDAToolkit discovery, verifies that no CUDA target exists,
rejects deleted compatibility headers, performs a two-dimensional Dense
explicit-Euler step, checks each value, and checks reduction checksum `19.95`.

The trial passed for build-tree, copied build-tree, installed, relocated, and
path-with-spaces use, in shared and static CPU configurations. Scratch remained
outside asc-xde. Before and after every run the real repository was clean at:

```text
abcb29b51f22f40afd7f174707b7ccf83c32d4bf
```

No downstream repository write occurred.

## Exact focused commands and results

The independent external validation root was:

```text
/tmp/asc-cpp-m8r2-verification.8l3y7T
```

### Fresh GCC Release shared CPU producer

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8r2-verification.8l3y7T/build \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m8r2-verification.8l3y7T/prefix \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m8r2-verification.8l3y7T/build --parallel 4
cmake --install /tmp/asc-cpp-m8r2-verification.8l3y7T/build
```

Result: **PASS**.

The package, header, symbol, compile/object, and downstream scripts were run
with the installed/build package directories from that producer. All passed.
A separate GCC Debug static producer passed the metadata/static-macro probe and
installed downstream trial.

### Fresh GCC/NVCC Release shared CUDA producer

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8r2-verification.8l3y7T/cuda-build \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_HOST_COMPILER=g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m8r2-verification.8l3y7T/cuda-prefix \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m8r2-verification.8l3y7T/cuda-build --parallel 4
cmake --install /tmp/asc-cpp-m8r2-verification.8l3y7T/cuda-build
```

Result: **PASS** with GCC 11.4, NVCC 12.9.86, and architecture 86. The
CUDA-enabled package/version/all-component probe, 49-header equivalence,
signature/API compile, and eleven-library symbol probe all passed.

### Clean integrated lead registration

The first fresh shared filtered run found M8R2-VER-008 below: 26/27 applicable
tests passed and the repeated-lookup build failed because its setup fixture was
not selected. After the lead repair, the exact scratch directory was removed,
the tree was reconfigured, and the complete filtered command was rerun:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8r2-verification.8l3y7T/integrated-shared
cmake -E rm -rf \
  "/tmp/asc-cpp-m8r2-verification.8l3y7T/integrated-shared/tests/package/repeated component lookup work"
ctest \
  --test-dir /tmp/asc-cpp-m8r2-verification.8l3y7T/integrated-shared \
  --output-on-failure \
  -L "hardening|downstream" \
  -j1
```

Result: **PASS, 28/28**, real time 91.29 seconds. This included fixture setup,
all package modes, source/installed surface, independent metadata and header
oracles, public API, representative shared symbols, production ELF baseline,
three compile/object probes, and four asc-xde-shaped modes.

The corresponding integrated GCC Debug static run passed **25/25** before the
new setup test became automatically selected; no shared-symbol/ELF test is
claimed for static linkage.

### Real-device provider revalidation

The current product surface is unchanged from the approved final M7 provider
implementation. Its real-device suites were independently rerun:

```sh
ctest \
  --test-dir build/m7-final-gcc-cuda-release-shared-make \
  --output-on-failure \
  -R '^asc_cpp\.(core_cuda\.core_cuda_(native_state|runtime|validation)_test|dense_cuda\.dense_cuda_.*_test|sparse_cuda\.runtime|random_cuda\.runtime|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)$' \
  -j1
```

Result: **PASS, 13/13**, real time 6.84 seconds:

- Core CUDA: 3/3;
- Dense CUDA: 6/6;
- Sparse CUDA: 1/1;
- raw Random CUDA: 1/1;
- Dense Random CUDA: 1/1; and
- Sparse Random CUDA: 1/1.

The lead remains responsible for the final clean M8 CUDA matrix; this focused
run is independent revalidation of the unchanged provider product.

## GPU evidence classification

| Facet | Evidence |
| --- | --- |
| `core_cuda` | configure-tested; compile-tested; runtime-tested |
| `dense_cuda` | configure-tested; compile-tested; runtime-tested; parity-tested |
| `sparse_cuda` | configure-tested; compile-tested; runtime-tested; parity-tested |
| `random_cuda` | configure-tested; compile-tested; runtime-tested; parity-tested |
| `random_dense_cuda` | configure-tested; compile-tested; runtime-tested; parity-tested |
| `random_sparse_cuda` | configure-tested; compile-tested; runtime-tested; parity-tested |

No GPU evidence is inferred from package configuration alone. Multi-GPU,
trusted device CSC, cross-toolkit, cross-driver, non-Linux GPU, other compute
capabilities, hosted GPU CI, and unavailable provider topologies are
`skipped`.

## Findings and resolutions

### M8R2-VER-001 — independent oracle freeze

The production tool and prior M8 branch had to remain unseen until the
verification surface was independently specified.

Resolution: froze `verification-design.md` first, including the exact
component/header/closure/signature/downstream oracles. Production inspection
occurred only afterward.

Status: resolved.

### M8R2-VER-002 — package and header truthfulness

Independent tests could have exposed drift between metadata, target file sets,
physical install contents, version rules, and actual closures.

Resolution: exhaustive isolated probes passed for CPU and CUDA packages,
static/shared linkage, and all required package placements. No product or
package defect was found.

Status: accepted.

### M8R2-VER-003 — obsolete Random CUDA signature risk

The superseded M8 branch used a pointer-plus-count raw destination.

Resolution: dependent compile-time detection, installed header comparison, and
dynamic symbol inspection all find only the approved
`MutableMemoryView + word_count` form.

Status: resolved; no compatibility overload added.

### M8R2-VER-004 — downstream claim boundary

The real asc-xde repository is skeletal and cannot truthfully be described as
an implemented integration.

Resolution: used an asc-cpp-owned isolated fixture, exact minimal `dense`
closure, CUDA-disabled CPU workflow, and before/after repository audit.

Status: accepted as an API/package trial only.

### M8R2-VER-005 — symbol evidence is platform-bounded

ELF symbol spelling/counts contain compiler and standard-library artifacts and
cannot establish cross-platform or cross-minor ABI compatibility.

Resolution: representative independent assertions plus deterministic
production ELF observations passed. The records explicitly deny broader ABI
promises.

Status: accepted with remaining risk.

### M8R2-VER-006 — compile observations are not performance guarantees

Object sizes and compile elapsed observations differed materially with
compiler, flags, and build mode.

Resolution: no threshold was introduced. Reports include the complete
environment/method/noise/checksum fields and are local observations only.

Status: accepted.

### M8R2-VER-007 — GNU-only compile driver

The first compile/object runner unconditionally emitted
`-std`, `-I`, `-c`, and `-o`, so MSVC/clang-cl would fail.

Resolution: the owned runner now supports `COMPILER_STYLE=gnu|msvc`, including
`/std:c++20`, `/I`, `/c`, `/Fo`, and `.obj`. Lead registration selects
MSVC-style `/W4;/WX;/permissive-` for MSVC frontends and GNU-style warnings
otherwise.

Status: resolved by code review and GNU/Clang regression execution; native
MSVC execution skipped because unavailable.

### M8R2-VER-008 — clean filtered CTest fixture omission

The first clean `-L hardening|downstream` shared run selected
`asc_cpp.package.repeated_component_lookup_build` without selecting the test
that creates its work directory. A previously used build tree masked this
failure.

Resolution: the lead made the configure test
`FIXTURES_SETUP M8RepeatedComponentLookup` and the build test
`FIXTURES_REQUIRED M8RepeatedComponentLookup`. After deleting the exact scratch
directory, the clean filtered suite passed 28/28.

Status: resolved and regression-tested.

### M8R2-VER-009 — incomplete compile observation fields

The first report omitted explicit build mode, hardware/provider, workload,
warmup/repetition, synchronization, result, and noise fields.

Resolution: added all fields. Source/object SHA-256 values are the
compile-probe checksums; runtime provider/checksum are not applicable.

Status: resolved and refreshed in the final 28/28 run.

## Remaining risks and skips

- PE/COFF, Mach-O, MSVC, clang-cl, AppleClang, MinGW, non-Linux, 32-bit,
  cross-compilation, and native multi-config execution were unavailable and
  are skipped.
- The MSVC runner path is reviewed and registered but not executed locally.
- ELF symbol observations include compiler/STL artifacts and intentional
  implementation-named support symbols; they are not ABI stability promises.
- Shared objects retain unversioned SONAMEs under the approved pre-1.0 policy.
- Compile timings are one-repetition, cache-sensitive observations.
- The asc-xde fixture is not an asc-xde feature, migration completion, solver,
  or production dependency integration.
- Final sanitizer, full CPU/CUDA configuration matrix, package relocation,
  runtime performance, and final Checkpoint B aggregation remain lead-owned.

## Disposition

Independent verification accepts the Milestone 8 revision-2 hardening
surface. All verification defects found during clean execution were resolved
and rerun. No open product API, dependency, package, header, target, symbol, or
downstream blocker remains.

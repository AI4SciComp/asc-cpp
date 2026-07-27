# Milestone 8 portability, GPU, and performance review

Status: Independent review complete; accepted with bounded skips

Date: 2026-07-27

## Scope and independence

This review covered only the frozen Milestone 8 contract: packaging, API,
symbol/ABI, performance, and downstream hardening of the existing fifteen
targets. I independently inspected the approved ADRs, component graph, public
file sets, package config, hardening tools and baselines, verification
fixtures, general M8 documentation, existing CPU/CUDA tests, and the final
lead-owned integration.

I wrote only this report. I did not edit product code, CMake/package files,
tests, benchmarks, general documentation, either sibling repository, Git
history, branches, or remote state.

## Reviewed environment

```text
branch: feature/asc-cpp-m8-hardening-downstream
baseline HEAD: 33b261ea33616a6395c4ad3b20646093103344f7
host: Linux 6.18.33.2-microsoft-standard-WSL2, x86_64
CPU: Intel Core i7-11800H, 8 cores / 16 logical CPUs
memory: 15 GiB
CMake: 4.1.2
generator used: Unix Makefiles 4.3 (Ninja was unavailable)
GCC: 11.4.0
Clang: 19.0.0 as /usr/bin/clang++-19
CUDA compiler/toolkit: nvcc 12.9.86 / CUDA Toolkit 12.9
Compute Sanitizer: 2025.2.1.0
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
driver: 576.83
compute capability / compiled architecture: 8.6 / 86
asc-cmake: released ASCCMake 0.1.0 package
asc-xde: abcb29b51f22f40afd7f174707b7ccf83c32d4bf, clean
```

The first diagnostic configure requested Ninja and failed because Ninja is not
installed. That generator is `skipped`; the same matrix configuration passed
with locally available Unix Makefiles. This is an environment limitation, not
an ASCCpp failure.

## C++20, compiler, linkage, and public-surface evidence

The following independent configurations/builds passed with warnings as
errors and the released ASCCMake package:

- GCC 11.4, Release, static, CUDA disabled, tests and install enabled;
- Clang 19, Release, shared, CUDA disabled, followed by installation to a
  prefix containing spaces and an installed API compile/link/run; and
- Clang 19, Debug, shared, CUDA disabled, for the reviewed ELF digest; and
- GCC 11.4 plus nvcc 12.9.86, Release, shared, CUDA enabled, for all eleven
  compiled libraries and the reviewed CUDA ELF digest.

The source inventory check passed for 49 headers, 15 components, and the exact
direct component edges. The target inventory passed for all nine
provider-free targets in both static and shared configurations. The
CUDA-disabled installed manifest passed for exactly 37 headers, and the
production normalized-hash check reported `checked-cpu`. Standalone header and
exceptions-disabled targets compiled under the full GCC build. The four new
M8 C++ fixture/probe sources passed:

```sh
clang-format-19 --dry-run --Werror \
  benchmarks/hardening/public_header_compile_probe.cc \
  tests/downstream/asc_xde_trial/asc_xde_trial.cc \
  tests/hardening/installed_api/installed_api_contract.cc \
  tests/hardening/package_metadata/core_smoke.cc
```

`git diff --check` also passed for every M8 production, verification,
documentation, and governance write scope.

The Clang shared installation and isolated public API consumer passed. A
manual ELF inspection passed for all five CPU shared libraries and recorded
ELF64 little-endian x86-64, unversioned SONAMEs, direct runtime dependencies,
and defined dynamic symbols. The exact Clang 19 Debug digest gate also passed.
The exact GNU 11/CUDA 12 Release shared digest gate passed for all eleven
compiled libraries.
The visible `internal_dense_linalg` and `internal_sparse_linalg` symbols are
intentional public-template support, not supported extension points. The
observations are local pre-1.0 baselines, not a cross-compiler, cross-build,
cross-platform, or cross-minor ABI promise.

Windows/MSVC, macOS/AppleClang, libc++, non-x86-64 CPUs, non-ELF binary
formats, other CMake versions, and Ninja are `skipped`.

## Package, relocation, and downstream evidence

A fresh post-integration GCC Release/static M8 slice ran:

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cpu-gcc-make \
  --output-on-failure -R 'asc_cpp\.(hardening|downstream)'
```

Result: 26/26 pass in 129.78 seconds. This covered:

- source and installed public-surface checks;
- copied build-tree package, install, copied/relocated prefix, and paths with
  spaces;
- exact installed headers and installed public API configure/build/run;
- `0.9.0 EXACT`, compatible `0.9`, rejected `0.9.1`, rejected `0.8`, and
  rejected `1.0` requests against build-tree and relocated packages;
- all known/available/component-found variables and exact target closures;
- GCC and Clang compile/object observations; and
- asc-xde-shaped trials against build-tree, copied build-tree, installed, and
  relocated packages.

The separate provider-free component-consumer rerun passed 19/19 in 95.38
seconds after the stale version correction. The real asc-xde repository
remained clean at
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf` before and after the trials.

After CUDA integration was freshly regenerated, the corresponding static
CUDA hardening slice passed 22/22 in 208.67 seconds. It repeated the component
metadata/version matrix against the 15-component package, verified exactly 49
installed headers, compiled and ran the installed API consumer with all six
CUDA facets, and repeated both compiler/object probes.

The final repeated-lookup consumer passed 2/2 for both CPU and CUDA packages.
It proved that a first `core` lookup stayed isolated, a second `dense` or
`dense_cuda` lookup loaded its exact ASC closure and exact imported link
interfaces, and a third `utilities` lookup succeeded in the same CMake
process. The nested executable compiled and linked.

A CPU build cache contained no CUDAToolkit or CUDA compiler discovery.
Provider-free installed lookup created no CUDA imported target. Requesting
CUDA with `/nonexistent/nvcc` failed configuration as expected. No product
dependency, target, operation, or provider was added.

## Sanitizer evidence

GCC ASan+UBSan capability probes, configure, and build passed with the actual
ASCCMake sanitizer API. With leak detection and halt-on-error enabled, the
non-package/non-consumer/non-performance suite passed 134/134 in 23.85
seconds:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m8-portability-asan-ubsan \
  --output-on-failure -LE 'package|consumer|performance'
```

The additional GCC ThreadSanitizer selection configured and compiled through
the same ASCCMake API. A focused sparse-random runtime passed, but the
dense-random runtime could not start under this WSL2 host; a direct rerun
exited 66 with:

```text
FATAL: ThreadSanitizer: unexpected memory mapping ...
```

ThreadSanitizer runtime is therefore an environment `skipped`, not a product
pass or failure. Leak detection is covered by the passing ASan configuration;
standalone LSan was not independently repeated.

Device-memory validation used:

```sh
for m8_binary in \
  tests/core_cuda/asc_core_cuda_test \
  tests/dense_cuda/asc_dense_cuda_storage_evaluate_test \
  tests/dense_cuda/asc_dense_cuda_linalg_test \
  tests/sparse_cuda/asc_sparse_cuda_test \
  tests/random_cuda/asc_random_cuda_test \
  tests/random_dense_cuda/asc_random_dense_cuda_test \
  tests/random_sparse_cuda/asc_random_sparse_cuda_test
do
  compute-sanitizer --tool memcheck --error-exitcode=99 \
    --leak-check=full --report-api-errors=no "$m8_binary"
done
```

Result: 7/7 pass; every executable reported zero leaks and zero errors.
`--report-api-errors=no` is required for this suite because the core negative
contract intentionally requests an impossible allocation and verifies
`cudaErrorMemoryAllocation`. The initial default-reporting diagnostic returned
99 for exactly those two expected API-error observations while still
reporting zero leaks. Suppressing expected API-return diagnostics does not
suppress device memory-access or leak errors.

Compute Sanitizer memcheck is `runtime-tested` device-memory evidence, not a
new numerical oracle. CUDA provider host code was not combined with host
ASan/UBSan; the approved local matrix uses strict host compilation, real-device
tests, and Compute Sanitizer as complementary evidence.

## GPU provider evidence

The CUDA configuration used GNU 11.4 as the nvcc host compiler, CUDA
12.9.86, C++20, architecture 86, Release, static ASC libraries, shared CUDA
Runtime, and warnings as errors.

| Surface | Classification | Result |
| --- | --- | --- |
| CUDA disabled with no provider/toolkit discovery | `configure-tested` | pass |
| CUDA requested with a nonexistent compiler | `configure-tested` | expected failure pass |
| CUDA 12.9.86, CUDAToolkit targets, architecture 86 | `configure-tested` | pass |
| all six CUDA facets, provider headers/contracts, and tests | `compile-tested` | pass |
| `core_cuda` allocation/copy/context/event operations on the real device | `runtime-tested` | pass |
| `core_cuda` host/device round trips against exact host data | `parity-tested` | pass |
| `dense_cuda` storage/evaluation/algebra/concurrency on the real device | `runtime-tested` | 4/4 pass |
| `dense_cuda` hand-computed float/double value and algebra oracles | `parity-tested` | pass |
| `sparse_cuda` clone/SpMV/evaluation on the real device | `runtime-tested` | pass |
| `sparse_cuda` structural and hand-computed numerical oracles | `parity-tested` | pass |
| `random_cuda` raw Philox generation on the real device | `runtime-tested` | pass |
| `random_cuda` independent Philox word oracle | `parity-tested` | pass |
| `random_dense_cuda` float/double fills on the real device | `runtime-tested` | pass |
| `random_dense_cuda` serial CPU bit oracle, including padding | `parity-tested` | pass |
| `random_sparse_cuda` float/double generation on the real device | `runtime-tested` | pass |
| `random_sparse_cuda` independent coordinates/values and CPU bit oracle | `parity-tested` | pass |
| seven provider executables under Compute Sanitizer memcheck | `runtime-tested` | 7/7 pass, zero leaks/errors |
| trusted-device CSC evaluator success path | `skipped` | no approved trusted device CSC producer |
| multi-device restoration, peer access, MIG, and other GPU architectures | `skipped` | unavailable local topology/hardware |
| Clang CUDA-host pairing, other toolkits/drivers, hosted GPU CI | `skipped` | not run |

The focused runtime/benchmark CTest command passed 11/11. Labels are not
inferred from one another: each `parity-tested` row above is supported by an
independent hand, structural, CPU, or bit oracle inspected in the named test.

## Performance evidence

The M8 compile/object script compiled the same public aggregate probe three
times per compiler. It passed correctness and within-command object-size
repeatability:

| Compiler | Elapsed seconds | Object bytes |
| --- | --- | --- |
| GCC 11.4 | 2.84594, 2.75995, 2.68286 | 1864 each |
| Clang 19 | 3.87677, 4.02490, 3.77456 | 1144 each |

These are cold/local wall-time observations under concurrent validation load,
not compiler comparisons or absolute gates.

The GCC Release CPU correctness-checked runtime probes passed:

| Probe | Workload and result |
| --- | --- |
| dense | 32x32, 100 evaluate-add/GEMM iterations, 0 operation allocations, 63465 us, checksum 4.75 |
| sparse | CSR 128x128, 382 nnz, 200 SpMV iterations, 0 operation allocations, 358 us, checksum 382 |
| random dense | 64x64, 500 iterations, 0 operation allocations, 55574 us, checksum 2046.81 |
| random sparse | 32x32, count 64, 20 iterations, 40 matching allocation/deallocation calls, 61725 us, checksum 39724 |

The real-device CUDA probes passed their untimed correctness oracle after
warmup and synchronization:

```text
dense float:  vector pointwise 19994 us, AXPY 4193 us,
              GEMV 1459 us, GEMM 859 us, checksum 349.632
dense double: vector pointwise 26970 us, AXPY 7920 us,
              GEMV 1208 us, GEMM 14666 us, checksum 349.632
sparse float: CSR SpMV 1024x1024, 5120 nnz, 20 repetitions,
              workspace 704 bytes, 1410 us, checksum 10240
sparse double: workspace 752 bytes, 1559 us, checksum 10240
random raw:   1048576 words, 20 repetitions, 1915 us,
              checksum 2253713208691922
random dense: 1024x1024 float, 20 repetitions, 4143 us,
              checksum 524733
random sparse including output allocation: 128x128, count 128,
              5 repetitions, 3750209 us, coordinate checksum 16629,
              value checksum 66.3448
```

These measurements are `parity-tested` observations because the benchmarks
run independent untimed checksum/bit/coordinate oracles. They are not speedup
claims, cross-machine thresholds, or evidence for another workload. Thermal
state, WSL2 scheduling, concurrent builds, provider first use, and frequency
scaling were uncontrolled. Sparse random remains deliberately serial and
non-scalable.

## Exact executed commands

All commands in this section were run from
`/home/yicai/AI4SciComp/asc-cpp`. The `/tmp` paths identify this reviewer's
isolated builds and reports.

### CUDA configure, compile, and real-device runtime

The static CUDA producer and complete test tree were configured and built
with:

```sh
cmake -S . -B /tmp/asc-cpp-m8-portability-cuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.9/bin/nvcc \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build /tmp/asc-cpp-m8-portability-cuda --parallel 6
```

Result: configure and build pass; target inventory pass for all 15 targets.

The exact focused real-device command was:

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  --output-on-failure \
  -R 'asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency|benchmark)|sparse_cuda\.(runtime|benchmark)|random_cuda\.(runtime|benchmark)|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)'
```

Result: 11/11 pass.

After lead integration was regenerated, the CUDA package/API hardening slice
was run with:

```sh
cmake -S . -B /tmp/asc-cpp-m8-portability-cuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.9/bin/nvcc \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build /tmp/asc-cpp-m8-portability-cuda --parallel 6
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  --output-on-failure -R '^asc_cpp\.hardening\.'
```

Result: 22/22 pass in 208.67 seconds.

### Repeated component lookups

The corrected CPU repeated-lookup fixture was freshly registered and run
with:

```sh
cmake -S . -B /tmp/asc-cpp-m8-portability-cpu-gcc-make \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX='/tmp/asc-cpp-m8-portability-prefix with spaces'
ctest --test-dir /tmp/asc-cpp-m8-portability-cpu-gcc-make \
  --output-on-failure \
  -R '^asc_cpp\.package\.repeated_component_lookup'
```

Result: CPU configure/build 2/2 pass in 11.03 seconds.

The corrected CUDA oracle was rerun from an empty nested consumer directory:

```sh
cmake -E remove_directory \
  '/tmp/asc-cpp-m8-portability-cuda/tests/package/repeated component lookup work'
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  --output-on-failure \
  -R '^asc_cpp\.package\.repeated_component_lookup'
```

Result: CUDA configure/build 2/2 pass in 21.37 seconds.

### Shared CUDA ELF baseline

The exact GNU 11/CUDA 12 Release shared-library digest gate used:

```sh
cmake -S . -B /tmp/asc-cpp-m8-portability-cuda-shared \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.9/bin/nvcc \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DBUILD_SHARED_LIBS=ON
cmake --build /tmp/asc-cpp-m8-portability-cuda-shared \
  --parallel 6 \
  --target \
    asc_core \
    asc_utilities \
    asc_random \
    asc_dense \
    asc_sparse \
    asc_core_cuda \
    asc_dense_cuda \
    asc_sparse_cuda \
    asc_random_cuda \
    asc_random_dense_cuda \
    asc_random_sparse_cuda
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda-shared \
  --output-on-failure -R '^asc_cpp\.hardening\.elf_abi$'
```

Result: all eleven shared libraries build; exact ELF digest gate 1/1 pass.

### Compile-time and object-size probes

The two direct observation commands were:

```sh
cmake \
  -DCXX_COMPILER=/usr/bin/g++ \
  -DCOMPILER_ID=GNU \
  -DINCLUDE_DIR=/home/yicai/AI4SciComp/asc-cpp/include \
  -DSOURCE_FILE=/home/yicai/AI4SciComp/asc-cpp/benchmarks/hardening/public_header_compile_probe.cc \
  -DOUTPUT_DIR=/tmp/asc-cpp-m8-portability-compile-gcc \
  -DREPETITIONS=3 \
  -P benchmarks/hardening/run_compile_object_probe.cmake
cmake \
  -DCXX_COMPILER=/usr/bin/clang++-19 \
  -DCOMPILER_ID=Clang \
  -DINCLUDE_DIR=/home/yicai/AI4SciComp/asc-cpp/include \
  -DSOURCE_FILE=/home/yicai/AI4SciComp/asc-cpp/benchmarks/hardening/public_header_compile_probe.cc \
  -DOUTPUT_DIR=/tmp/asc-cpp-m8-portability-compile-clang \
  -DREPETITIONS=3 \
  -P benchmarks/hardening/run_compile_object_probe.cmake
```

Result: both pass for three repetitions with invariant object sizes, 1864
bytes for GCC and 1144 bytes for Clang. The exact timings are recorded in the
performance table above.

### Runtime performance observations

The CPU benchmark output was captured with:

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cpu-gcc-make \
  -L performance -V
```

Result: dense, sparse, and random-storage probes pass 3/3.

The CUDA benchmark output was captured with:

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  -V -R 'asc_cpp\.(dense_cuda|sparse_cuda|random_cuda)\.benchmark'
```

Result: dense, sparse, and random CUDA probes pass 3/3. Their workloads,
warmups, repetitions, synchronization boundaries, elapsed observations, and
checksums are recorded above.

## Findings and resolutions

### PORT-M8-001 — stale consumer package minor

The first fresh GCC run passed production build and tests through the CPU
runtime suite, then isolated consumers rejected the `0.9.0` package because
their fixtures still requested `0.7`.

Resolution: the lead updated every current consumer request to `0.9`.
The fresh consumer rerun passed. Historical M0--M7 evidence was not rewritten.

Status: resolved.

### PORT-M8-002 — hardening tools initially lacked integration

The verifier fixtures were initially registered, but the production public
surface, target inventory, and ELF tools were not.

Resolution: the lead integrated target inventory at configure time, source
and installed surface CTests, and a shared-only ELF CTest with reviewed local
digests. Independent runs passed.

Status: resolved.

### PORT-M8-003 — non-ELF tool skips appeared as CTest passes

`InspectElfAbi.cmake` correctly emitted `skipped` when required tools or an ELF
input were unavailable, but its first CTest registration had no skip
expression.

Resolution: the lead added `SKIP_REGULAR_EXPRESSION` for the tool's explicit
skip diagnostic. An unavailable binary format/tool can no longer be counted
as a CTest pass.

Status: resolved.

### PORT-M8-004 — expected CUDA API errors obscured memcheck

Default Compute Sanitizer API-error reporting treated the intentional
out-of-memory negative contract as an error.

Resolution: the final memcheck command disables API-return reporting while
retaining memory-access, leak, application-exit, and error-exit checks. All
seven provider executables then reported zero memory errors and zero leaks.

Status: resolved; both the diagnostic and final command are retained above.

### PORT-M8-005 — repeated CUDA lookup oracle confused target existence with a link edge

The first CUDA version of the repeated-component fixture rejected
`CUDA::cusparse` target existence after requesting `dense_cuda`. The
configuration failed 0/2 because CMake's `FindCUDAToolkit` may define provider
targets beyond the ones named in an ASCCpp imported target's link interface.
This did not demonstrate an ASCCpp dependency leak.

Resolution: the fixture continues to enforce the exact ASC target closure and
now checks exact static/shared imported link interfaces, including the
`LINK_ONLY` provider dependencies of static packages. It requires Runtime and
cuBLAS for `dense_cuda` without treating unrelated toolkit target definitions
as link edges. The package guide now states this CMake distinction.

Independent rerun: CPU 2/2 pass; CUDA 2/2 pass in 21.37 seconds.

Status: resolved. The superseded failure remains recorded here.

## Remaining risks

- The evidence covers one Linux/WSL2 x86-64 host, libstdc++, one CUDA
  toolkit/driver, one NVIDIA compute-8.6 device, and no multi-device topology.
- Pre-1.0 ELF symbol/layout baselines are toolchain and build-mode
  observations. Unversioned SONAMEs and public C++ standard-library types
  preclude a broad ABI promise.
- Arbitrary external CUDA allocation terminal bounds remain caller supplied;
  CUDA Runtime alone has no approved general allocation-range query.
- Trusted sparse provenance is not an asynchronous readiness event. Storage,
  resources, workspaces, and provider contexts must outlive completion.
- Trusted device CSC evaluation, other platforms/standard libraries, hosted
  CI, other GPU architectures/toolkits, and multi-device behavior are
  `skipped`.
- Absolute performance regressions cannot be decided from these single-host
  smoke observations. A comparison requires the frozen same-environment
  protocol in `docs/performance.md`.
- The complete cumulative M0--M8 implementation remains uncommitted over the
  unchanged historical baseline; publication and release remain separately
  unauthorized.

## Disposition

The portability/GPU/performance review accepts the integrated M8 candidate for
Publication Checkpoint B. No new product dependency, provider, component,
operation, later-milestone feature, or product C++ defect was found. All
findings raised by this reviewer were resolved and independently rechecked.

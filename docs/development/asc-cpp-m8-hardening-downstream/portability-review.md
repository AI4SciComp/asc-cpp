# Milestone 8 Revision-2 Portability, GPU, and Performance Review

Status: Complete; accepted for Publication Checkpoint B with the explicit
skips and residual risks below

Date: 2026-07-28

Branch: `feature/asc-cpp-m8-hardening-downstream-r2`

Role: independent portability/GPU/performance reviewer

## Review boundary and independence

This review is separate from production implementation, independent
verification, documentation/API review, and lead integration. Its exclusive
write scope is this file. Product code, tests, benchmarks, CMake/package files,
hardening tools, ABI baselines, general documentation, sibling repositories,
Git history, branches, remotes, and pull requests were read-only.

The governing authorities are the frozen revision-2 Milestone 8 contract and
ownership ledger, the approved architecture package and ADRs, the dependency
and capability manifests, backend matrix, release roadmap, complete runbook,
and approved Milestone 7 checkpoint. This review did not implement a later
milestone.

No commit, push, merge, tag, release, branch deletion, or remote action was
performed.

## Reviewed environment

The independent final evidence was collected on:

```text
OS/kernel:             Ubuntu 22.04.5 LTS under WSL2
                       Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
CPU:                   Intel Core i7-11800H, 16 logical CPUs
memory:                15 GiB
GPU:                   NVIDIA GeForce RTX 3060 Laptop GPU
GPU memory:            6144 MiB
compute capability:    8.6
driver:                576.83
CMake/CTest:           4.1.2
generator:             Unix Makefiles
GNU Make:              4.3
GCC:                   11.4.0
Clang:                 19.0.0
CUDA compiler/toolkit: 12.9.86
Compute Sanitizer:     2025.2.1.0
binutils:              2.38
glibc:                 2.35
```

Ninja was not installed and therefore was not exercised.

## Findings and resolutions

### M8R2-PORT-001 — compile/object probe used source headers

Severity: high

The initial registered compile/object observations included the repository
source include directory. That did not establish the contract's installed
public-header compile-cost evidence and could hide install/file-set defects.

Resolution: the lead changed the registered probe to use the relocated
installed include directory and added the `M8RelocatedPackage` fixture
dependency. Fresh GCC and Clang observations subsequently compiled only the
relocated installed headers, including a prefix containing spaces.

Status: resolved and independently rerun.

### M8R2-PORT-002 — ELF-only symbol inspection was registered portably

Severity: medium

The initial shared-symbol test used GNU `nm` semantics but was registered for
all shared-library platforms. That would make a non-ELF platform fail for the
wrong reason even though the production ABI inspector correctly treats
non-ELF binaries as outside its evidence boundary.

Resolution: the lead restricted GNU-`nm` shared-symbol registration to
`CMAKE_SYSTEM_NAME STREQUAL "Linux"`. The production inspector retains its
explicit non-ELF skip behavior.

Status: resolved and independently inspected.

### M8R2-PORT-003 — standalone LSan/TSan full test builds

Severity: evidence limitation, not a product defect

The complete standalone LSan and TSan test builds fail to link deliberate test
executables that override global allocation functions, including
`result_fatal_no_allocation_test.cc` and `expression_test.cc`. Those overrides
conflict with the sanitizer runtimes' own allocation interceptors.

Resolution: the sanitizer instrumentation probes passed, and a bounded
sanitizer-safe runtime selection passed 12/12 under LSan and 12/12 under TSan.
The complete LSan/TSan suite is recorded as skipped at build/link time; it is
not promoted to a product pass.

Status: accepted with exact limitation.

### M8R2-PORT-004 — allocation-failure negative under memcheck

Severity: evidence limitation, not a product defect

`asc_core_cuda_native_state_test` intentionally requests an impossible
`cudaMalloc` and asserts the resulting allocation failure. Compute Sanitizer
therefore reports one `cudaErrorMemoryAllocation` API error even though it
reports zero leaks. Ordinary CTest execution of the negative passed.

Resolution: that one negative is skipped under Compute Sanitizer memcheck.
The other 12 applicable CUDA runtime executables passed memcheck with zero
memory errors and zero leaked bytes.

Status: accepted with exact limitation.

### M8R2-PORT-005 — mixed CUDA toolchain identity

Severity: reporting boundary

The second CUDA build used Clang 19 for C++ translation units and NVCC 12.9
with GNU 11 as its actual CUDA host compiler. It is not evidence for Clang as
the CUDA compiler or as NVCC's host compiler.

Resolution: all reports name the exact mixed toolchain. A true Clang CUDA
compiler/host configuration remains skipped.

Status: resolved in the evidence classification.

## CPU compiler, mode, and linkage matrix

The independent matrix root was:

```text
/tmp/asc-cpp-m8r2-port-cpu-final.99ApI7
```

Each of the eight GCC/Clang, Debug/Release, static/shared rows used this exact
command pattern with the shown row values substituted:

```sh
cmake -S . -B <build> -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> \
  -DCMAKE_CXX_COMPILER=</usr/bin/g++-or-/usr/bin/clang++-19> \
  -DBUILD_SHARED_LIBS=<OFF-or-ON> \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build <build> --parallel 4
ctest --test-dir <build> --output-on-failure --parallel 4 \
  --label-exclude 'package|consumer|downstream|performance'
cmake --install <build> --prefix <prefix>
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR=/home/yicai/AI4SciComp/asc-cpp \
  -DASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR=<prefix>/include \
  -DASC_CPP_HARDENING_OUTPUT=<external-report> \
  -P tools/hardening/CheckPublicSurface.cmake
```

Results:

| Compiler | Mode | Linkage | Configure/build/install | Selected tests | Installed headers |
| --- | --- | --- | --- | ---: | --- |
| GCC 11.4 | Debug | static | pass | 137/137 pass | 37/37 equivalent |
| GCC 11.4 | Debug | shared | pass | 139/139 pass | 37/37 equivalent |
| GCC 11.4 | Release | static | pass | 137/137 pass | 37/37 equivalent |
| GCC 11.4 | Release | shared | pass | 139/139 pass | 37/37 equivalent |
| Clang 19 | Debug | static | pass | 137/137 pass | 37/37 equivalent |
| Clang 19 | Debug | shared | pass | 139/139 pass | 37/37 equivalent |
| Clang 19 | Release | static | pass | 137/137 pass | 37/37 equivalent |
| Clang 19 | Release | shared | pass | 139/139 pass | 37/37 equivalent |

Every CPU configure reported exactly nine enabled provider-free product
targets and six truthfully skipped CUDA targets. The shared rows additionally
passed their applicable shared-symbol and ELF observations.

The complete GCC Release/static suite was run with:

```sh
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-cpu-final.99ApI7/gcc-release-static/build \
  --output-on-failure \
  --parallel 4
```

Result: pass, 189/189 in 137.21 seconds. This included package, version,
build-tree, copied-tree, installation, relocation, path-with-spaces,
provider-isolation, repeated-component, isolated-consumer, hardening,
performance-smoke, and asc-xde-shaped downstream evidence.

The integrated Clang Release/shared Milestone 8 selection was:

```sh
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-cpu-final.99ApI7/clang-release-shared/build \
  --output-on-failure \
  --parallel 4 \
  -L milestone-8
```

Result: pass, 54/54 in 152.31 seconds. The total includes fixture setup; the
CTest label summary contains 53 Milestone 8 tests.

No source-tree or independent build-tree absolute path was found in relocated
CPU package metadata.

## Sanitizer evidence

The independent sanitizer root was:

```text
/tmp/asc-cpp-m8r2-port-sanitizers-final.39I6he
```

All three builds used Clang 19, Debug, static libraries, CUDA disabled,
warnings as errors, and the normal ASCCMake integration. The instrumentation
options were:

```text
asan-ubsan:
  ASC_CPP_ENABLE_ADDRESS_SANITIZER=ON
  ASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
lsan:
  ASC_CPP_ENABLE_LEAK_SANITIZER=ON
tsan:
  ASC_CPP_ENABLE_THREAD_SANITIZER=ON
```

The ASan+UBSan command and result were:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-sanitizers-final.39I6he/asan-ubsan/build \
  --output-on-failure \
  --parallel 4 \
  --label-exclude 'package|consumer|downstream|performance|abi'
```

Result: pass, 137/137 in 0.49 seconds, with no ASan, leak, or UBSan
diagnostic.

The LSan and TSan bounded selection comprised these exact 12 tests:

```text
asc_cpp.core.memory_execution_test
asc_cpp.utilities.command_line_test
asc_cpp.utilities.timer_test
asc_cpp.sparse.coordinate_test
asc_cpp.sparse.compressed_conversion_test
asc_cpp.random.distribution_test
asc_cpp.random.engine_test
asc_cpp.random_dense.dense_generation_test
asc_cpp.random_dense.thread_partition_test
asc_cpp.random_sparse.sparse_generation_test
asc_cpp.random_sparse.thread_reproducibility_test
asc_cpp.hardening.public_api_surface
```

Results: LSan pass, 12/12 in 0.10 seconds; TSan pass, 12/12 in 0.34
seconds. The complete LSan/TSan test selections remain skipped for the
test-harness linker conflict recorded in M8R2-PORT-003.

## Full GCC/NVCC CUDA build

The independent CUDA root was:

```text
/tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq
```

The exact configure/build/test commands were:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build \
  --parallel 4
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build \
  --output-on-failure \
  --parallel 4
```

Configure identified GCC 11.4, NVCC 12.9.86 with GNU 11.4 as host,
CUDAToolkit 12.9, architecture 86, and exactly 15/15 enabled product targets.
Build passed. The complete test suite passed 245/245 in 1468.34 seconds.

The full run includes all 12 CUDA component consumers: six CUDA facets
against both the build-tree and installed/relocated package. Package aggregate
tests passed for build-tree, copied build-tree, installed, relocated,
path-with-spaces, registry-disabled, CUDA-disabled isolation, known-unavailable
components, and repeated component requests. All 49 installed CUDA-package
headers were byte-equivalent to their source file-set members. No source or
independent build absolute path was found in relocated CUDA package metadata.

## Mixed Clang C++/NVCC CUDA build

The second CUDA build root was:

```text
/tmp/asc-cpp-m8r2-port-cuda-clang-mixed.rJ9Y9Z
```

It used Clang 19 for C++ translation units, NVCC 12.9 for CUDA translation
units, GNU 11 as the actual NVCC host, Release, static libraries, architecture
86, CUDA enabled, tests enabled, install enabled, and warnings as errors.
Configure found all 15 product targets and the complete build passed.

The focused command was:

```sh
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-cuda-clang-mixed.rJ9Y9Z/build \
  --output-on-failure \
  -R 'asc_cpp\.(core_cuda\.|dense_cuda\.|sparse_cuda\.(runtime|benchmark)|random_cuda\.(runtime|benchmark)|random_dense_cuda\.runtime|random_sparse_cuda\.runtime|consumer\.(core_cuda|dense_cuda|sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.)'
```

Result: pass, 28/28 in 219.40 seconds. This comprises 12 CUDA component
consumers, three Core CUDA tests, six Dense CUDA tests, Sparse and raw-Random
runtime/benchmark pairs, and Dense-Random and Sparse-Random runtimes.

## Compute Sanitizer evidence

Each applicable CUDA runtime executable was run as:

```sh
compute-sanitizer \
  --tool memcheck \
  --leak-check full \
  --error-exitcode=99 \
  <runtime-executable>
```

The 12 applicable executables passed, each with `ERROR SUMMARY: 0 errors` and
zero leaked bytes:

```text
core CUDA runtime and validation
six Dense CUDA runtime tests
Sparse CUDA runtime
raw Random CUDA runtime
Dense Random CUDA runtime
Sparse Random CUDA runtime
```

Logs are under:

```text
/tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/compute-sanitizer-applicable
```

The native-state impossible-allocation negative is skipped under memcheck as
explained in M8R2-PORT-004. Racecheck is skipped at the documented cuSPARSE
`beta == 0` false-positive boundary. Initcheck and synccheck were not run.

## GPU evidence classification

The terms below are used exactly as frozen by the milestone:

| Facet | Evidence |
| --- | --- |
| `core_cuda` | `configure-tested`; `compile-tested`; `runtime-tested` |
| `dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |

`core_cuda` is not promoted to `parity-tested`. Its execution/copy behavior
ran successfully, but the frozen independent classification does not define a
separate parity label for that facet.

The parity evidence for the other five facets is real-device agreement with
the approved independent numerical, structural, or random-bit oracles, not
merely successful kernel launch.

## ELF, symbol, and provider-dependency evidence

The GCC/NVCC Release/shared ELF report is:

```text
/tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build/tests/hardening/reports/elf-abi-Release.txt
SHA-256 0da3f872cb76f1a876567409f8baae09a80198214368faa229c0698e456646b4
```

It passed the approved target/header/ELF baseline. The only exported raw
Random provider signature is the `MutableMemoryView` plus `word_count` form;
no pointer-plus-count `CudaFillPhilox4x32` symbol exists.

Provider-free shared objects `core`, `utilities`, `dense`, `sparse`, and
`random` have no CUDA Runtime, cuBLAS, or cuSPARSE dependency even in the
CUDA-enabled build. Provider dependencies are confined as follows:

| Library closure | Provider dependency observed |
| --- | --- |
| `core_cuda` | CUDA Runtime |
| `dense_cuda` | cuBLAS and CUDA Runtime; cuBLASLt transitively |
| `sparse_cuda` | cuSPARSE and CUDA Runtime; nvJitLink transitively |
| `random_cuda`, `random_dense_cuda`, `random_sparse_cuda` | ASC/Core CUDA and CUDA Runtime only |

No cuRAND, CUDA Driver, cuSOLVER, NCCL, OpenMP, Eigen, MKL, HIP, SYCL, or
other unapproved direct product dependency was introduced.

## Installed-header compile/object observations

These are one-repetition local observations with zero warmup, compiler-process
exit as synchronization, no threshold, and known filesystem/cache/scheduling
noise. They compiled the relocated installed headers from:

```text
/tmp/asc-cpp-m8r2-port-cpu-final.99ApI7/gcc-release-static/build/tests/hardening/relocated prefix with spaces/include
```

| Compiler | Translation unit | Object bytes | Object SHA-256 | Elapsed |
| --- | --- | ---: | --- | ---: |
| GCC 11.4 | `compile_core.cc` | 6,576 | `06fca6401adb1c385c1828e45fba373e151c647697f22df6fa3efae81a8f840f` | 0.706864 s |
| GCC 11.4 | `compile_dense.cc` | 271,920 | `87cae5b660e34b164fb34924223c7ac2eeb4de3f10f4449f716405ddb7e79d82` | 0.756905 s |
| GCC 11.4 | `compile_provider_free.cc` | 2,048 | `e9310bf4c803fb74268d3e9b32cb0afb6b87bd91c0a62cfa1c5ad68383f686ea` | 0.767745 s |
| Clang 19 | `compile_core.cc` | 5,552 | `4ea7f4d8f1f55c85a53266e582f1d407beaa7e34bc0af709a5050cf7c8a57325` | 0.848178 s |
| Clang 19 | `compile_dense.cc` | 220,616 | `a896172b3dc30d912dfb3a7536575af807e2ce9fb23327cc5a97dfb31169c5ca` | 0.766944 s |
| Clang 19 | `compile_provider_free.cc` | 1,328 | `c30b4bbb9eb60397088e218affcd733bb6897bbb7e2da5da5ad9dc49eb7d2038` | 1.00464 s |

The configured Clang Release/shared observation independently produced 5,552,
220,624, and 1,328-byte objects in 0.87746, 0.768495, and 1.00351 seconds.
The small Dense-object variation reinforces that these are bounded local
observations, not an ABI or compile-time promise.

## Runtime performance observations

The reviewer manually ran the Release benchmark executables after correctness
validation:

```sh
/tmp/asc-cpp-m8r2-port-cpu-final.99ApI7/gcc-release-static/build/tests/dense/asc_dense_benchmark
/tmp/asc-cpp-m8r2-port-cpu-final.99ApI7/gcc-release-static/build/tests/sparse/asc_sparse_benchmark
/tmp/asc-cpp-m8r2-port-cpu-final.99ApI7/gcc-release-static/build/tests/random_sparse/asc_random_storage_benchmark
/tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build/tests/dense_cuda/asc_dense_cuda_benchmark
/tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build/tests/sparse_cuda/asc_sparse_cuda_benchmark
/tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build/tests/random_cuda/asc_random_cuda_benchmark
```

All six returned zero. Exact observations were:

### CPU

```text
Dense evaluate:
  shape=32x32 iterations=64 total_ns=3700478
  per_iteration_ns=57820 checksum=1283 allocations=0
Dense GEMM:
  shape=32x32 iterations=4 total_ns=70930
  per_iteration_ns=17732.5 checksum=20790.7 allocations=0

Sparse structure-preserving evaluate:
  rows=128 columns=256 nnz=512 iterations=64 total_ns=8392273
  per_iteration_ns=131129 checksum=-574.125 allocations=0
Sparse SpMV:
  iterations=256 total_ns=1480355 per_iteration_ns=5782.64
  checksum=977.375 allocations=0

Dense Random layout-left:
  float 128x128 repetitions=100 elapsed_ns=20912028
  operation_allocation_calls=0 next_offset=16401
  checksum=928521972971142723
Dense Random layout-right:
  float 128x128 repetitions=100 elapsed_ns=22442975
  operation_allocation_calls=0 next_offset=16401
  checksum=10828367647995593407
Sparse Random:
  coordinate float 64x64 count=256 repetitions=10 elapsed_ns=251699280
  allocation_calls=20 allocated_bytes=51200 live_allocations=0
  next_structure_offset=8295 next_value_offset=369
  checksum=1516538221932305239
Random aggregate checksum=8664949311000451886
```

### CUDA

```text
Dense CUDA, all checksum=64.54296875 and operation allocations=0:
  H2D: total_ns=6902452 repetitions=12 throughput=7291850490 B/s
  D2H: total_ns=7004706 repetitions=12 throughput=7185404784 B/s
  D2D: total_ns=684022 repetitions=12 throughput=73581914030 B/s
  terminal evaluate: total_ns=8420042 throughput=5977600587 B/s
  AXPY: total_ns=829959 throughput=30321767700 flop/s
  GEMV: total_ns=476352 throughput=52830310360 flop/s
  GEMM: total_ns=1059750 throughput=3039608844000 flop/s

Sparse CUDA CSR SpMV, 1024x1024, nnz=5120, warmup=3, repetitions=20:
  float: total_ns=2848525, 35948400 nnz/s, workspace=704,
         operation_allocations=0, checksum=14940377177479771011
  double: total_ns=1077539, 95031400 nnz/s, workspace=752,
          operation_allocations=0, checksum=9178157494086742915

Raw Random CUDA:
  warmup=3 repetitions=12 elapsed_ns=886272
  throughput=14197600000 words/s
  checksum=13841617604916660332 allocations=0
Dense Random CUDA:
  elapsed_ns=1542636 throughput=8156760000 values/s
  checksum=15165452046652654026 allocations=0
Sparse Random CUDA:
  logical_size=16384 exact_count=128 elapsed_ns=7364313501
  throughput=26697.4 selected values/s
  checksum=7322770344431580519
  allocation_calls=30 allocated_bytes=38400
```

The Sparse Random allocation count is exactly two result-buffer allocation
attempts for each of 15 warmup/timed invocations. These values are local
observations only. Milestone 8 establishes workload, synchronization, checksum,
and allocation envelopes; it defines no regression threshold, cross-machine
comparison, or CPU/GPU speedup claim.

## Downstream and relocation evidence

The asc-xde-shaped trial passed in all four requested package modes:

```text
build-tree
copied build-tree
installed
relocated path containing spaces
```

It requested only `dense`, observed the exact `core`/`expression`/`dense`
closure, kept CUDA discovery disabled, and produced checksum `19.95`.

The real asc-xde repository remained read-only and clean at:

```text
abcb29b51f22f40afd7f174707b7ccf83c32d4bf
```

No sibling-repository write occurred.

## Explicit skips and remaining risks

- Windows/MSVC, macOS/AppleClang, non-WSL native Linux distributions, other
  standard libraries, other CPU architectures, Ninja, and hosted CI are
  `skipped`.
- Other CUDA toolkits, drivers, GPU architectures, a true Clang CUDA
  compiler/host, Windows CUDA, multi-GPU, cross-device/peer access, MIG, and
  cross-toolkit/driver/architecture parity are `skipped`.
- Trusted device CSC evaluation is `skipped`; no approved producer constructs
  that view.
- HIP, SYCL, OpenMP device offload, and other accelerator providers are
  `skipped` and are not approved dependencies.
- Compute Sanitizer racecheck is `skipped` at the documented cuSPARSE
  `beta == 0` false-positive boundary. Initcheck and synccheck were not run.
- Complete LSan and TSan test-suite builds are `skipped` because deliberate
  test allocator interposition conflicts with those runtimes; only the exact
  12-test safe subset is claimed.
- ELF and GNU-`nm` evidence is Linux/GNU-format evidence only. It is not a
  Windows PE/COFF or Apple Mach-O ABI claim.
- Local unversioned SONAMEs and compiler/STL weak symbols remain disclosed ABI
  risks. Milestone 8 does not promise cross-toolchain or cross-minor binary
  compatibility.
- Compile/object and runtime benchmark figures are single-host observations
  without thresholds. They can detect gross local changes but are not a
  statistical performance guarantee.
- CUDA Sparse Random retains its approved correctness-oriented
  `O(logical_size * exact_count + exact_count^2)` selection cost and may be
  slow for large domains/counts.
- Arbitrary caller-owned external CUDA views retain the truthful-valid-storage
  precondition; Runtime pointer attributes do not prove every caller-declared
  terminal allocation bound.

## Independent verdict

The revision-2 Milestone 8 portability/GPU/performance scope is accepted for
lead integration and Publication Checkpoint B. The CPU Cartesian matrix,
sanitizer evidence, full GCC/NVCC CUDA suite, mixed Clang-C++ CUDA suite,
Compute Sanitizer subset, package/relocation/consumer checks, ELF/provider
boundary, installed-header observations, downstream trial, and bounded
performance evidence all support the frozen contract. The two actionable
portability findings were resolved and independently rerun. No unresolved
material design choice or release-blocking portability defect remains within
the exercised environment.

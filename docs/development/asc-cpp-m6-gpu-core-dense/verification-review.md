# Milestone 6 Independent Verification Review

Status: complete; independent verification passes with no open product blocker

Date: 2026-07-28

Scope: frozen Milestone 6 — GPU core and dense

## Independence and evidence classification

The verification design was frozen before this role inspected Milestone 6
production source. Tests, expected values, lifetime probes, negative
translation units, consumers, and the benchmark were independently derived
from the frozen contract and public predecessor APIs. This role did not
inspect or copy MdeCpp, the deleted asc-cpp CUDA implementation, third-party
test material, or a later-milestone implementation.

Final provider evidence is classified exactly as:

```text
core_cuda:  configure-tested, compile-tested, runtime-tested
dense_cuda: configure-tested, compile-tested, runtime-tested, parity-tested
```

The real-hardware environment was:

```text
host compiler:        GCC 11.4.0
CMake:                4.1.2
CUDA toolkit/compiler: 12.9.86 / nvcc 12.9.86
CUDA Runtime query:   12090
CUDA driver query:    12090
cuBLAS headers:       12.9.1
GPU:                  NVIDIA GeForce RTX 3060 Laptop GPU
compute capability:   8.6
compiled architecture: 86
primary configuration: Release, shared libraries
```

Only one physical CUDA device was available. Same-device current-device
preservation is runtime-tested. Cross-device restoration is skipped.

## Verification-owned artifacts

The verifier added:

```text
docs/development/asc-cpp-m6-gpu-core-dense/verification-design.md
docs/development/asc-cpp-m6-gpu-core-dense/verification-review.md
tests/core_cuda/test_support.h
tests/core_cuda/core_cuda_runtime_test.cc
tests/core_cuda/core_cuda_validation_test.cc
tests/core_cuda/core_cuda_native_state_test.cc
tests/dense_cuda/test_support.h
tests/dense_cuda/counting_resource.h
tests/dense_cuda/device_test_helpers.h
tests/dense_cuda/dense_cuda_owner_test.cc
tests/dense_cuda/dense_cuda_evaluate_test.cc
tests/dense_cuda/dense_cuda_linalg_test.cc
tests/dense_cuda/dense_cuda_concurrency_test.cc
tests/dense_cuda/dense_cuda_scalar_release_regression_test.cc
tests/dense_cuda/dense_cuda_move_assignment_test.cc
tests/compile/m6_core_cuda_header.cc
tests/compile/m6_dense_cuda_header.cc
tests/compile/m6_provider_headers_no_exceptions.cc
tests/compile/m6_provider_odr.h
tests/compile/m6_provider_odr_a.cc
tests/compile/m6_provider_odr_b.cc
tests/compile/m6_provider_odr_main.cc
tests/compile/m6_negative_copy_completion_event.cc
tests/compile/m6_negative_copy_cuda_resource.cc
tests/compile/m6_negative_copy_dense_cuda_context.cc
tests/compile/m6_negative_integral_cuda_destination.cc
tests/compile/m6_negative_volatile_cuda_destination.cc
tests/consumer/core_cuda/main.cc
tests/consumer/dense_cuda/main.cc
benchmarks/dense_cuda/benchmark.cc
```

The lead alone created and edited the associated CMake integration files.
Verification did not edit production or shared integration.

## Runtime and parity coverage

Core CUDA coverage passed for device discovery; invalid devices; pinned,
device, and managed resources; rejected ordinary-host resources; zero and
invalid allocations; H2D, D2H, D2D, managed, pinned, and pageable-host copies;
exact self-copy; partial overlap; pointer-space and device validation; event
query/wait/move/destruction; independent streams; native state preservation;
and stable provider/native error detail. A deliberately mislabeled host
pointer in an exact self-copy proves validation occurs before the no-op path.

Dense owner coverage passed for exact uninitialized span allocation, ranks
zero through eight, empty and padded mappings, device and managed
non-dereferenceability, pinned-host accessibility, clone round trips, failure
transactions, and zero verifier-resource allocation during operations.

The bounded evaluator passed independent CPU parity for `float` and `double`,
rank zero through eight, zero extents, terminal/scalar/negate/add/subtract/
multiply forms, left/right/padded mappings, padding sentinels, exact self,
overlap rejection, unsupported nested/external forms, placement/device/shape
errors, and independent streams. The optimized scalar-fill regression uses a
1024-element device owner and passes in Release.

Copy, Scal, Axpy, Gemv, and Gemm passed independent CPU parity for both scalar
types. Coverage includes rank-one/rank-two kernels, padded positive strides,
all Gemm transpose pairs, Gemv transpose, rectangular and degenerate shapes,
`beta == 0` with NaN-filled output, compatible padded leading dimensions,
output/input alias rejection, and provider-width narrowing. True
nondegenerate and degenerate `LayoutRight` Gemv/Gemm descriptors are rejected,
including a 1-by-1 mapping whose strides alone could resemble LayoutLeft.
Narrowing checks cover Gemv extent, leading dimension, and both increments,
and Gemm inner dimension, with zero verifier-resource allocations.

Tolerance policy was:

```text
Copy:      bit exact
Scal/Axpy: 8 * epsilon * max(1, abs(reference))
Gemv/Gemm: 32 * epsilon * max(1, reduction length)
                  * max(1, abs(reference))
```

No NaN, infinity, shape, placement, or alias failure was accepted through a
tolerance.

## Exact validation commands and results

Primary configure and clean integrated build:

```sh
cmake -S . -B /tmp/asc-cpp-m6-verifier-release-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m6-verifier-release-make --parallel 4
```

Result: pass. CUDA 12.9.86 and architecture 86 were configured and compiled;
all integrated targets rebuilt successfully with warnings as errors.

Focused final suite:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier-release-make \
  -C Release --output-on-failure \
  -R '^asc_cpp\.(core_cuda|dense_cuda|compile\.m6)' -j 1
```

Result: pass, 14/14. This comprises four provider compile/ODR fixtures, three
Core CUDA runtime fixtures, six Dense CUDA runtime/parity fixtures, and the
benchmark.

The five negative translation units were executed by CMake `try_compile`:

```text
m6_negative_copy_completion_event.cc:       expected failure
m6_negative_copy_cuda_resource.cc:          expected failure
m6_negative_copy_dense_cuda_context.cc:     expected failure
m6_negative_integral_cuda_destination.cc:   expected failure
m6_negative_volatile_cuda_destination.cc:   expected failure
```

Result: pass, 5/5 rejected as required. Both public provider headers also pass
self-containment, `-fno-exceptions`, and multi-translation-unit ODR checks.

Build-tree, installed, and relocated consumers:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier-release-make \
  -C Release --output-on-failure \
  -R '^asc_cpp\.consumer\.(core_cuda|dense_cuda)\.' -j 1
```

Result: pass, 4/4. The exact `ASC::core_cuda` and `ASC::dense_cuda` targets
configured, linked, and ran from the build tree and a relocated installation,
including a path containing spaces.

CUDA package isolation:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier-release-make \
  -C Release --output-on-failure \
  -R '^asc_cpp\.package\.cuda_' -j 1
```

Result: pass, 2/2. CUDA-disabled provider isolation and an intentionally
requested unavailable CUDA component both produced the required outcomes.

## Sanitizer evidence

UBSan-only provider management and runtime coverage:

```sh
cmake -S . -B /tmp/asc-cpp-m6-verifier-ubsan \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m6-verifier-ubsan --parallel 4 --target \
  asc_core_cuda_core_cuda_runtime_test \
  asc_core_cuda_core_cuda_validation_test \
  asc_core_cuda_core_cuda_native_state_test \
  asc_dense_cuda_dense_cuda_owner_test \
  asc_dense_cuda_dense_cuda_evaluate_test \
  asc_dense_cuda_dense_cuda_linalg_test \
  asc_dense_cuda_dense_cuda_concurrency_test \
  asc_dense_cuda_dense_cuda_scalar_release_regression_test \
  asc_dense_cuda_dense_cuda_move_assignment_test
env UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/asc-cpp-m6-verifier-ubsan \
  --output-on-failure \
  -R '^asc_cpp\.(core_cuda|dense_cuda\.dense_cuda_)' -j 1
```

Result: pass, 9/9 runtime/parity fixtures.

The combined ASan+UBSan attempt used:

```sh
cmake -S . -B /tmp/asc-cpp-m6-verifier-asan-ubsan \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m6-verifier-asan-ubsan --parallel 4
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/asc-cpp-m6-verifier-asan-ubsan \
  --output-on-failure \
  -R '^asc_cpp\.(core_cuda|dense_cuda\.dense_cuda_)' -j 1
```

Configuration and compilation passed. Runtime is skipped from product evidence
in this WSL environment: all nine CUDA processes abort at the first
`cuInit`/`cudaGetDeviceCount` with an ASan double-free entirely inside the WSL
NVIDIA driver `libcuda.so.1.1`, before the tests exercise ASC provider
behavior. The failure is reproducible and external to the ASC allocation
paths; it is not suppressed or reported as a product pass.

Compute Sanitizer was run separately, with
`--error-exitcode=99`, rather than treating one tool as evidence for another:

```sh
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool memcheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_scalar_release_regression_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool memcheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_evaluate_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool memcheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_linalg_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool memcheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_move_assignment_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool racecheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_concurrency_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool initcheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_evaluate_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool initcheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_linalg_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool synccheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_concurrency_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool synccheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_linalg_test
/usr/local/cuda-12.9/bin/compute-sanitizer \
  --tool synccheck --error-exitcode=99 \
  /tmp/asc-cpp-m6-verifier-release-make/tests/dense_cuda/asc_dense_cuda_dense_cuda_move_assignment_test
```

```text
memcheck:
  scalar Release regression, evaluator, linalg, move assignment
  pass; zero errors
racecheck:
  independent-stream concurrency
  pass; zero hazards, errors, or warnings
initcheck:
  evaluator, linalg
  pass; zero errors
synccheck:
  independent-stream concurrency, linalg, move assignment
  pass; zero errors
```

## Performance evidence

The project-owned benchmark has warmups, one waited event per measured
operation, post-timing checksums, operation/transfer separation, full hardware
metadata, and no unstable performance threshold. A Release run reported:

```text
h2d               6.953022152 GB/s
d2h               7.049683867 GB/s
d2d              61.26671511  GB/s
terminal evaluate 5.817760824 GB/s
axpy              26.78591659 GFLOP/s
gemv              33.57730934 GFLOP/s
gemm            2154.387743   GFLOP/s
```

Every measured operation reported a valid checksum and
`asc_resource_allocation_calls=0`. These values are observational, are not a
release gate, and apply only to the recorded RTX 3060 Laptop GPU environment.

## Findings and resolutions

1. A shared Release link initially found the private provider bridge
   constructors for `ExecutionContext` and `CompletionEvent` hidden. Production
   exported the two constructors; clean shared provider headers, ODR fixtures,
   consumers, and runtime tests now link and pass.
2. The frozen contract required pinned-host `DenseView::At`, but `view.h` was
   initially outside the production ledger. The lead reassigned that bounded
   file; pinned-host access and device/managed rejection now pass.
3. The optimized scalar-fill regression exposed expired
   `initializer_list` backing storage in evaluator overlap validation.
   Production replaced it with explicit operand validation; Release and
   sanitizer tests now pass.
4. Portability review found that default `DenseCudaContext` move-assignment
   could destroy an old stream before its cuBLAS handle. Production added
   ordered move-assignment; last-stream-owner and self-move tests plus
   memcheck/synccheck now pass.
5. Exact CUDA self-copy initially bypassed native pointer/device validation.
   Production validates before the no-op path; the deliberately mislabeled
   pointer regression now passes.
6. Public Dense CUDA header include ownership and cv-qualified destination
   constraints were corrected. Self-contained/no-exceptions and integral/
   volatile negative fixtures now pass.
7. Layout identity is now retained through erased descriptors so even
   degenerate LayoutRight matrices are rejected. Dedicated parity/negative
   cases pass.
8. The benchmark originally hard-coded architecture metadata and did not
   count ASC resource calls. Architecture now comes from the build record and
   verifier-owned resource wrappers report zero operation allocations.

All findings were resolved and independently retested. No open correctness,
API, dependency, package, lifetime, synchronization, allocation, or numerical
finding remains within Milestone 6.

## Remaining verification risks and skips

- Cross-device current-device restoration is skipped because the host exposes
  one GPU. Same-device preservation is runtime-tested.
- Combined ASan+UBSan CUDA runtime is skipped as external-environment
  incompatible due to the WSL driver failure described above. Compilation
  passes and UBSan-only CUDA runtime passes 9/9.
- Independent real-hardware verification covers one CUDA toolkit/compiler,
  one host compiler, one GPU architecture, and one physical device. Broader
  matrices remain CI/lead evidence.
- Independent streams were runtime-tested on one device; multi-GPU and peer
  copies are outside the approved milestone.
- Benchmark numbers have no threshold and do not establish performance on
  other architectures.

These limitations do not broaden an evidence label and do not block the
contracted single-device Milestone 6 implementation.

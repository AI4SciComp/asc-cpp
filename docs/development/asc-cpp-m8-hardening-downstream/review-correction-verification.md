# Milestone 8 Post-Checkpoint Correction Verification

Status: Independent focused verification and correction re-review complete;
findings 15--20 accepted

Date: 2026-07-28

## Boundary and inputs

This verification followed `main:AGENTS.md`, the frozen review-correction
contract and ownership ledger, and ADRs 0004, 0008--0015, and 0018. Changes
remain within the verification-owned test sources. No production, package,
CMake, public documentation, remote, branch, tag, or release action was
performed by this role.

## Focused regression coverage

### CUDA post-enqueue failure and lifetime

- `core_cuda_runtime_test.cc` injects a one-shot completion-event record
  failure after `CopyBytes` enqueues work. It requires the original
  provider/native failure, exactly one stream drain, and verifies that
  immediately mutating the source after return cannot race the completed copy.
- `dense_cuda_linalg_test.cc` injects an event-record failure after cuBLAS GEMM
  and an event-create failure after a custom `CudaScal` kernel. Both require
  exactly one drain and verify the completed device result after the failed
  API returns.
- `dense_cuda_owner_test.cc` injects an event-record failure in a device-backed
  Dense clone and requires a drain before the clone's destination rollback.
- These tests use the private, non-installed
  `src/core/cuda/runtime_test_internal.h` seam. The hook is compiled only in a
  dedicated validation configuration; it must remain disabled in ordinary
  product/shared-ABI builds.

### Sparse readable aliasing

- Coordinate evaluation rejects a destination value span that exactly or
  partially overlaps source coordinate storage, using `index_t` values so both
  simultaneously live objects have the same legal type.
- Compressed evaluation rejects destination values overlapping either source
  outer offsets or source inner indices.
- SpMV uses a byte-backed writable test adapter. The adapter's declared output
  span partially overlaps outer-offset bytes or exactly/partially overlaps
  inner-index bytes. `memcpy` makes an accidental write to object
  representation legal; the fixture restores valid structure before any typed
  read. Correct behavior rejects transactionally with zero writes.
- Existing terminal tests now assert values-only placement aliases and
  independently assert `MayAlias` at the first and last coordinate,
  outer-offset, inner-index, and value element.
- A first production attempt represented disjoint structure/value allocations
  with one bounding byte span. Clean Clang testing proved that invalid: valid
  output storage could lie in a gap and SpMV was falsely rejected. Production
  replaced it with values-only placement metadata, precise terminal
  `MayAlias`, and explicit disjoint CSR structure checks in SpMV. The final
  sparse selection passes under both GCC and Clang.

### Numerical and utility boundaries

- CPU GEMM now covers rectangular `2 x 3` by `3 x 4` multiplication for all
  four transpose pairs, with distinct padded/strided physical mappings and
  untouched output holes, for `float` and `double`.
- integral `ReduceSum` covers signed positive overflow, signed negative
  overflow, unsigned overflow, and a non-overflow control. Overflow must
  return `kOverflow` instead of publishing a wrapped result.
- Timer tests directly cover checked duration addition, elapsed-duration
  subtraction, sample-count increment, and divisor conversion boundaries.
  Public `Stop` total/count failures must preserve state, last sample, total,
  and count. `Elapsed` saturation and `Average` overflow are also checked
  deterministically through the private, non-installed Timer accessor.

### Random partition and collision properties

- CPU Dense Random covers irregular 3/5/3 partitions, an empty partition,
  reverse/noncontiguous submission order, and both `float` and `double`.
- A separate threaded test fills disjoint 31/97/128 partitions concurrently,
  includes an empty partition, and proves equality with whole-domain
  generation for both `float` and `double`.
- raw CUDA Random covers four uneven partitions (127/1/389/513), an empty
  partition, reordered submission, and two execution contexts.
- Dense CUDA Random covers uneven `double` partitions
  (17/1/509/500), an empty partition, reordered submission, and two contexts.
- CPU sparse Random forces four equal 64-bit priorities and verifies ordinal
  order `1, 2, 5, 7`.
- `priority_ordinal_collision_test.cu` launches a real-device kernel using the
  exact private host/device comparator called by production sparse generation.
  Equal priorities and input ordinals `7, 1, 5, 2` must produce
  `1, 2, 5, 7`.

### CUDA no-device classification

- Every Core CUDA and Dense CUDA runtime executable checks the deterministic
  `ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE=1` test seam before its first CUDA runtime
  call and returns 77.
- Each executable also returns 77 when successful device enumeration reports
  zero devices. CUDA enumeration failures remain failures rather than false
  skips.
- Direct execution confirmed exit 77 for all three Core and all six Dense CUDA
  runtime executables.
- Actual device masking is not usable as no-device evidence on this host:
  both `CUDA_VISIBLE_DEVICES=-1` and `CUDA_VISIBLE_DEVICES=''` abort in
  CUDA 12.9 runtime enumeration with
  `free(): double free detected in tcache 2`. A direct Python `ctypes` call to
  `libcudart.so.12::cudaGetDeviceCount`, with no ASC code involved, reproduces
  the same abort. This is therefore recorded as a host CUDA-runtime limitation,
  not classified as an asc-cpp pass or skip.

## Exact focused validation

### GCC 11.4 CPU Debug static

Configure:

```sh
cmake -S . -B /tmp/asc-cpp-m8-verification-JZ4PCE/build \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
```

Focused result:

```text
asc_cpp.utilities.timer_test                         PASS
asc_cpp.dense.array_evaluate_test                    PASS
asc_cpp.dense.linalg_test                            PASS
asc_cpp.sparse.coordinate_test                       PASS
asc_cpp.sparse.compressed_conversion_test            PASS
asc_cpp.sparse.evaluate_test                         PASS
asc_cpp.sparse.linalg_test                           PASS
asc_cpp.random_dense.dense_generation_test           PASS
asc_cpp.random_dense.thread_partition_test           PASS
asc_cpp.random_sparse.sparse_generation_test         PASS
Total: 10/10 PASS
```

### Clang 19 CPU Debug shared

Configure:

```sh
cmake -S . -B /tmp/asc-cpp-m8-verification-clang-MNzunT/build \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
```

The first run found the imprecise sparse bounding-span regression described
above: 9/10 passed and `asc_cpp.sparse.linalg_test` failed. After the exact
disjoint production correction, the same focused selection is **10/10 PASS**.

### GCC 11.4 / NVCC 12.9.86 Release static, architecture 86

The dedicated hook-enabled configuration used:

```sh
cmake -S . -B /tmp/asc-cpp-m8-verification-cuda-tLsoOI/build \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DCMAKE_CXX_FLAGS=-DASC_CPP_CUDA_RUNTIME_TEST_HOOKS \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
```

Real RTX device results:

```text
asc_cpp.core_cuda.core_cuda_runtime_test                 PASS
asc_cpp.dense_cuda.dense_cuda_linalg_test                PASS
asc_cpp.dense_cuda.dense_cuda_owner_test                 PASS
asc_cpp.random_cuda.runtime                              PASS
asc_cpp.random_dense_cuda.runtime                        PASS
asc_cpp.random_sparse_cuda.runtime                       PASS
asc_cpp.random_sparse_cuda.priority_ordinal_collision    PASS
Total focused correction selection: 7/7 PASS
```

The complete built Core/Dense CUDA runtime selection was **9/9 PASS** on the
real device. The separately registered forced-no-device runtime selection
reported all nine runtime executables and the Dense CUDA benchmark as CTest
skips through return code 77: **10/10 correctly classified as skipped**.

## Final correction re-review: findings 15--18

This was a read-only product/CMake review after the final-review amendment.
Only this verification report was edited. The running lead-owned CUDA CTest
tree at
`/tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build`
was not configured, built, cleaned, or otherwise mutated by this review.

### Finding 15: recursive test-workspace deletion

Result: **resolved**.

- `asc_cpp_validate_test_workspace` requires an existing absolute guarded
  root, validates the guard as a nonsymlink regular file with exact contents,
  resolves the work directory through its nearest existing ancestor, rejects
  the root itself, and requires the resolved path to be a strict descendant.
- `asc_cpp_prepare_test_workspace` and
  `asc_cpp_remove_test_workspace` are the only test helpers containing
  `file(REMOVE_RECURSE)`, and both delete only the validated returned path.
- The three hardening cleanup registrations pass `WORK_DIR`, `ROOT`, and
  `GUARD` to `RemoveTestWorkspace.cmake`. The repeated-component cleanup
  registration passes the same triplet through
  `test_workspace_guard_probe.cmake`; test drivers which reset their own
  workspaces call `asc_cpp_prepare_test_workspace`.
- The guard regression recursively scans every `tests/**/CMakeLists.txt` and
  `tests/**/*.cmake` source. It rejects `file(REMOVE_RECURSE)`, native
  `rm -r`/`rm -rf`, `cmake -E rm -r`/`rm -rf`, and
  `cmake -E remove_directory` outside the guarded helper. An independent
  repository-wide search found no additional recursive test deletion.
- The regression rejects `/`, source/build/workspace roots, a workspace
  parent, `..` traversal, a direct symlink escape, and a symlink-descendant
  escape while preserving outside sentinels. It also proves that an allowed
  child is reset and removed.

Focused result:

```text
asc_cpp.test_workspace.guard  PASS
```

### Finding 16: CUDA-host ELF baseline selector

Result: **resolved**.

- `asc_cpp_select_elf_baseline` receives
  `CMAKE_CUDA_HOST_COMPILER_ID` and
  `CMAKE_CUDA_HOST_COMPILER_VERSION`, in addition to the ordinary C++ and
  CUDA compiler identities. The CUDA baseline requires the detected host to
  be exactly GNU 11.4.0; changing the host version makes selection
  non-enforcing.
- The unlike-host integration configures ordinary C++ with GNU and NVCC's
  host with Clang 19. It reads CMake's generated
  `CMakeCUDACompiler.cmake`, requires detected host ID `Clang` and version
  `19.x`, then requires the generated ELF test to carry an empty baseline
  instead of the GNU-host CUDA baseline.
- Read-only inspection of the completed nested configure artifact showed:

```text
CMAKE_CUDA_HOST_COMPILER=/usr/bin/clang++-19
CMAKE_CUDA_HOST_COMPILER_ID=Clang
CMAKE_CUDA_HOST_COMPILER_VERSION=19.0.0
ASC_CPP_HARDENING_BASELINE:FILEPATH=
```

Focused selector result:

```text
asc_cpp.hardening.elf_baseline_selection  PASS
```

### Finding 17: Dense CUDA enumeration disposition

Result: **resolved**.

`DeviceCountDisposition` has three disjoint outcomes: a failed
`CudaDeviceCount` result prints the provider error and returns 2; a successful
zero count returns 77; and a successful positive count continues. The
benchmark's deterministic failure seam constructs a failed `Result` and calls
that same disposition function. Its CTest registration uses `ExpectExit.cmake`
to require exit 2 and does not attach `SKIP_RETURN_CODE`; the separate
forced-no-device registration requires exit 77.

Read-only early-exit invocations of the already-built current benchmark,
which return before any CUDA runtime call, produced:

```text
ASC_CPP_TEST_FORCE_CUDA_ENUMERATION_FAILURE=1  exit 2   PASS
ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE=1            exit 77  PASS
```

### Finding 18: operation-specific performance oracles

Result: **resolved**.

Every runtime performance row now has an operation-specific oracle which is
evaluated after the timed repetitions:

- Dense CPU evaluation reconstructs each expected affine-expression element;
  GEMM uses an independent scalar triple loop.
- Sparse CPU evaluation compares each stored value with the independently
  negated source; SpMV uses a scalar CSR row sum.
- CPU Dense Random reconstructs each physical element from the Philox and
  `Uniform01` specification for both layouts; CPU Sparse Random independently
  ranks `(priority, ordinal)` candidates and reconstructs coordinates and
  values.
- Dense CUDA H2D/D2H/D2D and terminal evaluation compare full host results;
  AXPY reconstructs the accumulated scale; GEMV uses a scalar dot-product
  oracle; GEMM uses independently recomputed, distributed output samples.
- Sparse CUDA computes float/double CSR SpMV reference rows on the host.
  Random CUDA reconstructs raw words, Dense values, and Sparse
  priority/ordinal selection plus values from the independent Philox oracle.

The CPU rows also print `oracle=independent`, and direct execution proved the
oracle-bearing paths rather than only checking process exit:

```text
Dense CPU:   evaluate, gemm                                  PASS (2 rows)
Sparse CPU:  structure_preserving_evaluate, spmv             PASS (2 rows)
Random CPU:  dense layout-left, dense layout-right, sparse   PASS (3 rows)
```

CUDA runtime timing was deliberately not duplicated while the lead's full
real-device CTest tree was active. The current Dense CUDA benchmark object and
executable postdate the amended source; its full runtime/performance result
belongs to that lead-owned integration result.

### Exact focused commands

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8-findings15-18-0KoVvA/cpu-build \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug

cmake --build /tmp/asc-cpp-m8-findings15-18-0KoVvA/cpu-build \
  --target asc_dense_benchmark asc_sparse_benchmark \
           asc_random_storage_benchmark \
  --parallel 2

ctest --test-dir /tmp/asc-cpp-m8-findings15-18-0KoVvA/cpu-build \
  --output-on-failure \
  -R '^asc_cpp\.(test_workspace\.guard|hardening\.elf_baseline_selection|dense\.benchmark|sparse\.benchmark|random_storage\.benchmark)$'
```

Result: configure **PASS**, build **PASS**, focused CTest **5/5 PASS**.

```sh
cmake \
  -DPROGRAM:FILEPATH=/tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build/tests/dense_cuda/asc_dense_cuda_benchmark \
  -DEXPECTED_EXIT:STRING=2 \
  -DENVIRONMENT:STRING=ASC_CPP_TEST_FORCE_CUDA_ENUMERATION_FAILURE=1 \
  -P tests/cmake/ExpectExit.cmake

cmake \
  -DPROGRAM:FILEPATH=/tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build/tests/dense_cuda/asc_dense_cuda_benchmark \
  -DEXPECTED_EXIT:STRING=77 \
  -DENVIRONMENT:STRING=ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE=1 \
  -P tests/cmake/ExpectExit.cmake
```

Result: enumeration failure **exit 2 PASS**; forced successful-no-device seam
**exit 77 PASS**.

No actionable residual finding remains for findings 15--18. The lead's clean
full matrix remains required before Publication Checkpoint B.

## Compatibility re-review: finding 19

Result: **the fail-closed selector is corrected; one actionable regression
portability defect remains**.

The corrected selection path is accepted:

- `_m8_trusted_cuda_host_compiler_id` and
  `_m8_trusted_cuda_host_compiler_version` start empty and are populated from
  CMake's detected variables only on CMake 3.31 or newer.
- Only those trusted variables reach `asc_cpp_select_elf_baseline`.
  Pre-3.31 cache/toolchain values remain raw observations and cannot select an
  enforcing CUDA baseline.
- The unavailable-identity test receives the empty trusted values and the raw
  observations separately. The nested untrusted-cache integration injects
  plausible GNU 11.4 values and requires an empty `elf_abi` baseline, no
  exact-host integration, and no host ID/version in the pre-3.31 generated
  CUDA compiler record.
- Official CMake 3.25.3 and 3.30.9 explicit-host configurations passed all
  three selector tests with empty baselines. CMake 4.1.2 retained and passed
  the guard, selector unit, and exact unlike-host integration; its generated
  compiler record proves the Clang 19.0.0 NVCC host.

Re-running the original adversarial CMake 3.30.9 configure:

```sh
/tmp/asc-cpp-cmake-compat-tools/cmake-3.30.9-linux-x86_64/bin/cmake \
  -S . \
  -B /tmp/asc-cpp-m8-finding19-spoof-Gdz2Gs/build \
  -G 'Unix Makefiles' \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_HOST_COMPILER_ID=GNU \
  -DCMAKE_CUDA_HOST_COMPILER_VERSION=11.4.0 \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
```

now generated:

```text
cuda_host_elf_selector_unavailable:
  CUDA_HOST_COMPILER_ID=
  CUDA_HOST_COMPILER_VERSION=
  RAW_CUDA_HOST_COMPILER_ID=GNU
  RAW_CUDA_HOST_COMPILER_VERSION=11.4.0
  SELECTED_BASELINE=

elf_abi:
  ASC_CPP_HARDENING_BASELINE=
```

The exact-host integration was absent, and the selector unit,
unavailable-identity test, and nested untrusted-cache integration passed
**3/3** in 33.68 seconds. This closes the original enforcing-baseline defect.

### Finding 20: implicit/default NVCC host

Result: **resolved and accepted**.

`cuda_host_elf_selector_untrusted_cache_integration_test.cmake` no longer
requires `CUDA_HOST_COMPILER`. Its nested command appends
`-DCMAKE_CUDA_HOST_COMPILER=...` only when the outer value is nonempty, so an
implicit NVCC host remains implicit instead of becoming an invalid empty
override.

The original failing CMake 3.30.9 tree at
`/tmp/asc-cpp-m8-finding19-default-host-TDqNYI/build` was rerun without
reconfiguration, retaining its generated empty outer host argument. The
current driver produced:

```text
asc_cpp.hardening.elf_baseline_selection
  PASS
asc_cpp.hardening.cuda_host_elf_selector_unavailable
  PASS
asc_cpp.hardening.cuda_host_elf_selector_untrusted_cache_integration
  PASS

Result: 3/3 PASS in 30.40 seconds
```

Read-only inspection of the newly configured nested tree proved all three
boundaries:

```text
CMakeCUDACompiler.cmake:
  set(CMAKE_CUDA_HOST_COMPILER "")
  no CMAKE_CUDA_HOST_COMPILER_ID or VERSION

CMakeCache.txt raw untrusted observations:
  CMAKE_CUDA_HOST_COMPILER_ID=GNU
  CMAKE_CUDA_HOST_COMPILER_VERSION=11.4.0

Nested hardening registration:
  trusted CUDA_HOST_COMPILER_ID=
  trusted CUDA_HOST_COMPILER_VERSION=
  SELECTED_BASELINE=
  ASC_CPP_HARDENING_BASELINE=
```

The exact-host integration remained absent on CMake 3.30.9. The official lead
trial additionally configured and built `asc_core_cuda` before passing 3/3 in
30.53 seconds; the independent portability rerun passed the same selection
3/3 in 32.37 seconds. The corrected recorded input digest is
`e7feae4784079c3edf331940b1ea8373f9c0748d4e20ad49ae0b2cbcae401f69`.

No actionable residual remains for findings 15--20. Finding 19 now enforces
the trusted CMake-version boundary, and finding 20 preserves the supported
implicit-host configuration while exercising the spoof-cache regression.

### Formatting

```sh
clang-format-19 --dry-run --Werror <all changed verification C++/CUDA files>
```

Result: PASS.

## Lead integration requirements

1. Keep `ASC_CPP_CUDA_RUNTIME_TEST_HOOKS` off in ordinary product, package, and
   ABI builds. Enable it only in a dedicated validation build so the three
   private fault symbols do not alter the normal shared-library symbol set.
2. Rerun the complete clean CPU compiler/configuration/linkage, sanitizer,
   package/relocation/consumer, full CUDA, and Compute Sanitizer matrix after
   integration. The focused results here do not replace that matrix.
3. Preserve the masked-device CUDA-runtime abort as a truthful skip/limitation;
   do not report it as actual no-device hardware evidence.

## Changed verification files

- `tests/core_cuda/{core_cuda_native_state_test.cc,core_cuda_runtime_test.cc,core_cuda_validation_test.cc,test_support.h}`:
  return-77 behavior plus deterministic CopyBytes drain/lifetime regression.
- `tests/dense_cuda/{dense_cuda_concurrency_test.cc,dense_cuda_evaluate_test.cc,dense_cuda_linalg_test.cc,dense_cuda_move_assignment_test.cc,dense_cuda_owner_test.cc,dense_cuda_scalar_release_regression_test.cc,test_support.h}`:
  return-77 behavior, cuBLAS/custom-kernel drain tests, and clone rollback.
- `tests/dense/{array_evaluate_test.cc,linalg_test.cc}`:
  integral reduction overflow and rectangular padded GEMM.
- `tests/sparse/{coordinate_test.cc,compressed_conversion_test.cc,evaluate_test.cc,linalg_test.cc}`:
  exact terminal alias metadata/endpoints, legal structural-overlap evaluation,
  and byte-backed SpMV overlap.
- `tests/utilities/timer_test.cc`: checked arithmetic and non-mutating failure
  boundaries.
- `tests/random_dense/{dense_generation_test.cc,thread_partition_test.cc}`:
  irregular, empty, reordered, threaded, float/double partitions.
- `tests/random_cuda/random_cuda_test.cc`: four-way reordered raw partitions.
- `tests/random_dense_cuda/random_dense_cuda_test.cc`: irregular reordered
  double partitions.
- `tests/random_sparse/sparse_generation_test.cc`: forced CPU priority
  collision.
- `tests/random_sparse_cuda/priority_ordinal_collision_test.cu`: forced
  real-device collision using the exact production comparator.

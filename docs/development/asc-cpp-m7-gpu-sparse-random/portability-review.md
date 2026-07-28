# Milestone 7 Portability, GPU, and Performance Review

Status: complete; accepted by the independent reviewer with the explicit
skips and residual risks below

Date: 2026-07-28

Role: independent portability/GPU/performance reviewer

## Review boundary

This review is separate from production implementation, independent
verification, documentation/API review, and lead integration. Its exclusive
write scope is this file. Production, tests, benchmarks, CMake, package files,
and other reports were read-only inputs.

The governing authorities are the frozen Milestone 7 contract and ownership
ledger, the approved architecture package, ADRs 0008, 0012, 0014, 0015, 0017,
and 0018, and the completed Milestone 6 CUDA checkpoint. This review did not
inspect prohibited MdeCpp, deleted asc-cpp, or Milestone 8 implementation.

No commit, push, merge, tag, release, branch deletion, or remote action was
performed.

## Reviewed environment

The reviewer's independent final CUDA build used:

```text
OS/kernel:      Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
CPU:            Intel Core i7-11800H, 16 logical CPUs
GPU:            NVIDIA GeForce RTX 3060 Laptop GPU, compute capability 8.6,
                6144 MiB
driver:         576.83; CUDA driver API version reported by the benchmarks
                as 12090
CUDA compiler:  nvcc 12.9.86
CUDA runtime:   12090
cuSPARSE header CUSPARSE_VERSION: 12510
C++ compiler:   GCC 11.4.0
header auditor: Clang 19.0.0
CMake:          4.1.2
configuration:  Release, static libraries, C++20, architecture 86,
                warnings as errors
```

The exact independent configuration command was:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m7-portability-final.MI4gyG/build \
  -G 'Unix Makefiles' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_BUILD_TYPE=Release
```

Result: pass. CMake identified GCC 11.4.0, nvcc 12.9.86, and CUDAToolkit
12.9.86. CUDA architecture remained caller-supplied.

## C++20 and public-header portability

All four public provider API families are SDK-free. CUDA and cuSPARSE native
types remain in implementation files. The APIs use ASC execution, memory,
result, event, extent, dense-view, and sparse-view vocabulary.

The reviewer ran this strict self-containment parse for each primary provider
header:

```sh
for h in \
  asc/sparse/providers/cuda.h \
  asc/random/providers/cuda.h \
  asc/random/providers/dense_cuda.h \
  asc/random/providers/sparse_cuda.h
do
  clang++-19 -std=c++20 -fsyntax-only -x c++ -Iinclude \
    -include "$h" /dev/null \
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
    -Werror -fno-exceptions
done
```

Result: pass, 4/4. Repository header, no-exception, negative ownership/type,
and multi-TU ODR targets also compiled in the final CUDA build. A volatile
strided sparse vector is now rejected by the public constraint rather than
reaching a cast that discards qualification.

The final production scope passed:

```sh
clang-format-19 --dry-run --Werror \
  include/asc/sparse/providers/cuda.h \
  include/asc/sparse/providers/cuda_export.h \
  include/asc/random/providers/cuda.h \
  include/asc/random/providers/cuda_export.h \
  include/asc/random/providers/dense_cuda.h \
  include/asc/random/providers/dense_cuda_export.h \
  include/asc/random/providers/sparse_cuda.h \
  include/asc/random/providers/sparse_cuda_export.h \
  src/sparse/cuda/context.cc \
  src/sparse/cuda/context_internal.h \
  src/sparse/cuda/operations.cc \
  src/sparse/cuda/kernels.cu \
  src/sparse/cuda/kernels_internal.h \
  src/random/cuda/raw.cc \
  src/random/cuda/raw_kernels.cu \
  src/random/cuda/raw_kernels_internal.h \
  src/random/cuda/dense.cc \
  src/random/cuda/dense_kernels.cu \
  src/random/cuda/dense_kernels_internal.h \
  src/random/cuda/sparse.cc \
  src/random/cuda/sparse_kernels.cu \
  src/random/cuda/sparse_kernels_internal.h
```

The build command below also passed:

```sh
cmake --build /tmp/asc-cpp-m7-portability-final.MI4gyG/build \
  --target \
    asc_sparse_cuda_test \
    asc_random_cuda_test \
    asc_random_dense_cuda_test \
    asc_random_sparse_cuda_test \
    asc_sparse_cuda_benchmark \
    asc_random_cuda_benchmark \
    asc_m7_cuda_contracts \
    asc_m7_provider_odr \
  --parallel 2
```

The implementation uses fixed-width ASC metadata and checked products, sums,
byte counts, address spans, and conversions. cuSPARSE receives signed 64-bit
dimensions, offsets, and indices directly; there is no silent 32-bit
narrowing.

## Device address, ownership, and lifetime audit

Raw word generation accepts `MutableMemoryView` plus `word_count` and checks
the requested byte count against the view's declared capacity. Dense and
sparse external views retain their inherited truthful-valid-storage
precondition because they do not carry an allocation terminal bound.

CUDA Runtime 12.9 exposes pointer attributes but no general allocation-range
query equivalent to the Driver API's `cuMemGetAddressRange`. The Driver API is
not an approved dependency. Consequently, arbitrary external Runtime pointers
are checked for declared placement, device, alignment, span arithmetic, and
overlap, but the implementation cannot prove that a caller-declared dense or
sparse span remains within its physical allocation. No final evidence claim
promotes a declared span to allocation-provenance evidence.

Provider-owned CSR clone and sparse-random buffers do carry resource
provenance. CSR clone makes exactly three owner-buffer allocation attempts.
Sparse random makes exactly two canonical output-buffer allocation attempts
and no computational-workspace allocation. Allocation-failure tests verify
transactional rollback and exactly-once release.

Sparse descriptors reject all pairwise structure/value overlap.
`CoordinateView::RebindValues` and
`CompressedSparseView::RebindValues` reject structure/value overlap and
partial overlap with the old value span while allowing exact in-place and
disjoint value storage. SpMV rejects output/input and output/matrix overlap.
Its full declared workspace span is device-validated and rejected if it
overlaps offsets, indices, values, input, or output.

All successful nonzero operations publish a move-only completion event. Zero
work publishes an already-complete event. If a later copy, kernel launch,
provider call, or event record fails after work may have been submitted, the
implementation drains only the affected context stream before releasing local
owners. Normal success does not add a hidden synchronization. Callers retain
contexts, resources, owners, views, workspaces, and referenced storage through
completion.

`SparseCudaContext` binds its cuSPARSE handle to the explicit execution stream
and host pointer mode. Handle-dependent workspace query and SpMV launch are
serialized by the context mutex. Move assignment releases the destination's
old handle before adopting the source state. Provider failures retain provider
name `cusparse` and signed native status.

## cuSPARSE algorithm and determinism

The unit-stride CSR path uses exactly:

```text
CUSPARSE_OPERATION_NON_TRANSPOSE
CUSPARSE_INDEX_64I offsets and indices
CUSPARSE_INDEX_BASE_ZERO
CUDA_R_32F or CUDA_R_64F storage and compute
CUSPARSE_SPMV_CSR_ALG2
caller-owned queried workspace
```

Positive nonunit strides use the original project kernel and require zero
workspace. That kernel accumulates each row sequentially and does not read the
old output when `beta == 0`.

NVIDIA's CUDA 12.9 cuSPARSE documentation describes non-transpose ALG2 as
bitwise deterministic per run, CSR SpMV as requiring caller external storage,
and `cusparseSpMV` as asynchronous. It also warns that Compute Sanitizer can
report a false race for `beta == 0`. The controlling reference is:

<https://docs.nvidia.com/cuda/archive/12.9.2/cusparse/index.html>

The result is therefore deterministic only for the selected algorithm in the
tested environment. It is not a cross-toolkit, cross-driver, cross-hardware,
or cross-workspace-alignment bit-identity guarantee.

## Random algorithm and cost audit

Raw and dense CUDA kernels map each logical position directly to explicit
Philox4x32-10 word addresses. Dense Uniform01 supports float/double, ranks zero
through eight, zero extents, left/right layouts, and unique padded
nonnegative-stride mappings. Logical dimension zero varies fastest and output
bits are launch-partition independent.

Sparse generation supports arbitrary compile-time rank. It evaluates the
independent priority/ordinal rule, selects into canonical output coordinate
storage, sorts selected ordinals into canonical order, launches one coordinate
decode kernel per dimension, and fills values from the separate value domain.
It allocates no hidden workspace.

The low-workspace sparse algorithm is intentionally correctness-oriented. Its
cost is:

```text
O(logical_size * exact_count + exact_count^2)
  selection and insertion work
+ O(rank * exact_count)
  coordinate decoding
```

The selection phase is a single-thread device kernel. Large logical domains or
counts will be slow; this is a disclosed performance limit, not a speedup
claim.

## Independent real-device verification

The exact focused runtime command was:

```sh
ctest --test-dir /tmp/asc-cpp-m7-portability-final.MI4gyG/build \
  --output-on-failure \
  -R '^asc_cpp\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.runtime$' \
  -j1
```

Result: pass, 4/4.

```text
asc_cpp.sparse_cuda.runtime          passed
asc_cpp.random_cuda.runtime          passed
asc_cpp.random_dense_cuda.runtime    passed
asc_cpp.random_sparse_cuda.runtime   passed
```

The independent verifier's distinct Release/shared, warnings-as-errors build
also passed its focused M7 command: 28/28 tests in 13.34 seconds, comprising
22 header/compile/ODR checks, the four runtime/parity tests, and the two
benchmark smokes.

The tests establish independent numerical or bit parity, not merely successful
kernel execution:

- `sparse_cuda`: canonical float/double CSR clone, exact owner allocation and
  rollback, unit-stride cuSPARSE and strided project-kernel SpMV, `beta == 0`,
  explicit workspace and overlap rejection, bounded CSR and coordinate
  evaluator forms, disjoint and in-place values, rank-nine coordinate
  evaluation, unchanged structure, and independent contexts;
- `random_cuda`: independent test-owned Philox equations, lane/block/tail
  cases, capacity and offset failures, partitions, reruns, and independent
  contexts;
- `random_dense_cuda`: independent Uniform01 bit transforms, float/double,
  left/right/padded mappings, scalar and empty shapes, partitions, sentinels,
  failures, and independent contexts; and
- `random_sparse_cuda`: an independent priority/ordinal/coordinate/value
  oracle plus provider-free parity, rank zero, zero extent, partial/full count,
  float/double, arbitrary rank-nine generation, exact two-buffer ownership,
  rollback, domains, offsets, and independent contexts.

Trusted device CSC evaluator success is `skipped`: Milestone 7 has no approved
trusted device CSC producer. Raw device CSC views correctly remain untrusted;
the review does not fabricate provenance to force a success case.

## Targets and package portability

The static generated export interfaces were inspected directly:

```text
ASC::sparse_cuda
  ASC::sparse;ASC::core_cuda;$<LINK_ONLY:CUDA::cusparse>

ASC::random_cuda
  ASC::random;ASC::core_cuda

ASC::random_dense_cuda
  ASC::random_dense;ASC::random_cuda;ASC::core_cuda

ASC::random_sparse_cuda
  ASC::random_sparse;ASC::random_cuda;ASC::core_cuda
```

Undefined-symbol inspection found cuSPARSE entry points only in
`libasc_sparse_cuda.a`; the three random archives contain no cuSPARSE, cuRAND,
cuBLAS, or cuSOLVER entry point. Source/dependency search found no cuRAND,
Thrust/CUB API, cuSOLVER, or unapproved package.

The installed config computes the requested component closure before calling
`find_dependency(CUDAToolkit 12)`. `ASC::cpp` remains provider-free. Final
independent Release/shared verification also passed:

```sh
ctest --test-dir /tmp/asc-cpp-m7-verifier-release \
  --output-on-failure \
  -R 'asc_cpp\.consumer\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.(build_tree|install_relocate)' \
  -j1
```

Result: pass, 8/8. Each M7 component passed both build-tree and
installed/relocated path-with-spaces isolated consumption. Aggregate CPU-only
validation also passed in the lead's clean GCC 11 Debug/static build:
163/163 tests, including 19 consumer and package/relocation tests plus CUDA
disabled and deliberately unavailable isolation. Thus provider-free lookup
did not discover CUDA and unavailable CUDA did not become a fallback.

The reviewer's Release/static `/tmp` build also reached a terminal M7 package
result. Its first focused consumer invocation passed all four build-tree
consumers but failed all four install-and-relocate consumers solely because
the earlier target-limited build had not produced provider-free
`libasc_utilities.a`, which whole-project installation requires. This was a
validation precondition failure, not a provider, export, or relocation
failure. The lead built the complete static tree and repeated the exact
consumer selection:

```sh
cmake --build /tmp/asc-cpp-m7-portability-final.MI4gyG/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-portability-final.MI4gyG/build \
  --output-on-failure \
  -R 'asc_cpp\.consumer\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.(build_tree|install_relocate)' \
  -j1
```

Result: full static build pass; focused static M7 consumers pass, 8/8 in
206.70 seconds.

```sh
cmake -S . -B build/m7-final-gcc-debug-static-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-gcc-debug-static-make --parallel 4
ctest --test-dir build/m7-final-gcc-debug-static-make \
  --output-on-failure --parallel 4
```

The complementary clean Clang 19 Release/shared CPU matrix also passed
163/163:

```sh
cmake -S . -B build/m7-final-clang-release-shared-cpu-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-clang-release-shared-cpu-make --parallel 4
ctest --test-dir build/m7-final-clang-release-shared-cpu-make \
  --output-on-failure --parallel 4
```

After the verifier-only M7-VER-006 correction, the lead's clean
Release/shared CUDA build and full matrix passed:

```sh
cmake -S . -B build/m7-final-gcc-cuda-release-shared-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_HOST_COMPILER=g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-gcc-cuda-release-shared-make \
  --clean-first --parallel 4
ctest --test-dir build/m7-final-gcc-cuda-release-shared-make \
  --output-on-failure --parallel 4
```

Result: pass, 217/217 with zero failed and no skipped tests in 1,137.31
seconds. The terminal matrix included 39 Milestone 7 tests, 36 package tests,
31 consumer tests, 22 M7 CUDA compile tests, four M7 runtime/parity tests, and
two benchmark smokes. The aggregate build-tree package test passed in 385.79
seconds, install-and-relocate in 373.16 seconds, registry isolation in 22.04
seconds, CUDA-disabled isolation in 0.92 seconds, and required-but-unavailable
CUDA handling in 1.64 seconds.

## Sanitizer evidence

ASan+UBSan covers provider-free and host-testable validation paths. CUDA
device memory is assessed separately with Compute Sanitizer memcheck.
Racecheck is not used as the cuSPARSE `beta == 0` correctness oracle because
the vendor documents a possible false report for that case.

The reviewer ran:

```sh
for exe in \
  tests/sparse_cuda/asc_sparse_cuda_test \
  tests/random_cuda/asc_random_cuda_test \
  tests/random_dense_cuda/asc_random_dense_cuda_test \
  tests/random_sparse_cuda/asc_random_sparse_cuda_test
do
  compute-sanitizer --tool memcheck --leak-check full --error-exitcode=99 \
    "/tmp/asc-cpp-m7-portability-final.MI4gyG/build/$exe"
done
```

Result: pass, 4/4. Every executable reported `ERROR SUMMARY: 0 errors` and
`LEAK SUMMARY: 0 bytes leaked in 0 allocations`.

The lead's clean Clang 19 Debug/static CPU ASan+UBSan configuration and build
passed, followed by 139/139 non-package/non-consumer tests with:

```sh
cmake -S . -B build/m7-final-clang-asan-ubsan-static-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-clang-asan-ubsan-static-make --parallel 4
ASAN_OPTIONS=detect_leaks=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ctest --test-dir build/m7-final-clang-asan-ubsan-static-make \
  --output-on-failure --parallel 4 --label-exclude 'package|consumer'
```

No sanitizer finding was reported. Package and consumer tests were
intentionally excluded from this sanitizer invocation and are covered by the
separate clean package matrices.

## Performance smoke

Both benchmarks use three warmups, wait on the operation's completion event in
every iteration, exclude setup/transfers/oracle work from the timed region,
validate an independent result oracle after timing, report a checksum and the
exact environment, and expose ASC resource allocation/workspace behavior.

The exact commands were:

```sh
/tmp/asc-cpp-m7-portability-final.MI4gyG/build/tests/sparse_cuda/asc_sparse_cuda_benchmark
/tmp/asc-cpp-m7-portability-final.MI4gyG/build/tests/random_cuda/asc_random_cuda_benchmark
```

One Release observation on the reviewed environment was:

| Operation | Timed work | Observation | Workspace / allocation evidence | Checksum |
|---|---:|---:|---|---:|
| CSR SpMV float | 20 x 1024x1024, 5120 NNZ | 1,227,648 ns total; 8.34115e7 NNZ/s | 704 B workspace; 0 operation allocations | 14940377177479771011 |
| CSR SpMV double | 20 x 1024x1024, 5120 NNZ | 1,383,917 ns total; 7.39929e7 NNZ/s | 752 B workspace; 0 operation allocations | 9178157494086742915 |
| raw Philox words | 12 x 1,048,576 words | 1,074,053 ns total; 1.17154e10 words/s | caller storage excluded; 0 operation allocations | 13841617604916660332 |
| dense Uniform01 float | 12 x 1,048,576 values | 1,639,973 ns total; 7.67263e9 values/s | caller storage excluded; 0 operation allocations | 15165452046652654026 |
| sparse Uniform01 float | 12 x 16,384 candidates, 128 selected | 7,336,389,504 ns total; 26,799 candidates/s | 30 calls / 38,400 B: exactly two canonical output allocations for each of 3 warmups + 12 timed runs | 7322770344431580519 |

These are smoke observations, not statistically stable comparisons. They make
no CPU/GPU or previous-version speedup claim. The sparse result illustrates
the documented workspace-free complexity limit.

## GPU evidence classification

Each facet is classified independently:

| Facet | configure-tested | compile-tested | runtime-tested | parity-tested | skipped |
|---|---|---|---|---|---|
| `ASC::sparse_cuda` | yes | yes | yes | yes | trusted CSC success; multi-GPU |
| `ASC::random_cuda` | yes | yes | yes | yes | multi-GPU |
| `ASC::random_dense_cuda` | yes | yes | yes | yes | multi-GPU |
| `ASC::random_sparse_cuda` | yes | yes | yes | yes | multi-GPU |

The labels apply to CUDA 12.9.86, GCC 11.4.0/nvcc, compute capability 8.6,
and the exact local tests. The following are `skipped`: MSVC/nvcc,
Clang-as-CUDA-compiler, AppleClang, 32-bit hosts, big-endian hosts, a second
CUDA device, other compute capabilities, older CUDA 12 minor releases,
cross-driver bit identity, HIP/ROCm, and SYCL.

## Findings and resolutions

1. **Raw destination capacity was initially ambiguous (high).** The owner
   approved `MutableMemoryView + word_count`; requested bytes are now checked
   against declared capacity. Physical allocation provenance remains a
   truthful-view precondition.
2. **Public-header self-containment and a clone declaration constraint
   mismatch (medium).** Missing standard includes and the constraint mismatch
   were corrected. Strict Clang and repository header gates pass.
3. **`CudaCsrArray::view()` initially omitted its compressed-format template
   argument (high).** The provider-controlled view factory now supplies CSR
   explicitly; float/double instantiation and runtime staging pass.
4. **Compressed sparse evaluation initially accepted only terminal copy
   (high).** Shared preparation now supports the frozen shallow
   copy/negate/add/subtract/multiply grammar for compressed and coordinate
   destinations.
5. **Nonzero scalar add/subtract could densify implicit zeros (high).** Only
   exact zero is accepted for add/subtract; matching-type scalar multiplication
   remains structure-preserving.
6. **Post-submission failure paths could release storage too early (critical).**
   Clone, random sparse generation, cuSPARSE, project kernels, and event-record
   failure paths now drain the affected stream before local storage can be
   released.
7. **Sparse generation and coordinate evaluation had implementation rank caps
   not present in the frozen contract (high).** Descriptor shape metadata no
   longer has a fixed-array cap; arbitrary compile-time rank is supported and
   rank-nine generation/evaluation passes.
8. **SpMV workspace validation was incomplete (high).** The full declared
   workspace span is now device-validated and checked against all operands.
9. **Sparse structure/value spans could overlap through checked rebinding
   (critical).** Provider validation now rejects all internal pairwise overlap,
   and provider-enabling sparse-owner evolution rejects structure and partial
   old-value overlap at `RebindValues`.
10. **Production formatting initially failed the repository gate (low).**
   Production was reformatted; the final dry-run passes.
11. **`CudaStridedVectorView<volatile T>` reached an invalid qualification-
    discarding path (medium).** The public constraint now rejects volatile
    element types and the negative compile contract passes.
12. **The existing private completed-event constructor lacked
    `ASC_CORE_EXPORT` for shared-library consumers (high).** Independent shared
    verification found it; lead integration exported the constructor before
    final shared validation.
13. **The first documentation review understated sparse-generation cost
    (medium).** The guides and API review now include the
    `O(exact_count^2)` single-thread insertion term as well as selection and
    coordinate decoding.
14. **The final shared build found a verifier-only vector-to-span deduction
    defect (low, test only).** The verifier now passes explicit spans. It also
    enlarged the SpMV overlap fixture so every operand is independently at
    least the queried workspace size; all five rejections therefore exercise
    overlap rather than the earlier undersized check.
15. **The first focused static install-consumer run lacked whole-project
    prerequisites (low, validation only).** Build-tree consumers passed, while
    install consumers could not find the not-yet-built provider-free
    `libasc_utilities.a`. Building the full static tree resolved the
    precondition; the exact eight-consumer rerun passed.

No unresolved correctness finding remains in the reviewed M7 implementation.

## Remaining risks

- Runtime-only pointer inspection cannot establish the allocation end of
  arbitrary external dense/sparse storage.
- cuSPARSE workspace size and reproducibility boundaries can change with
  toolkit, driver, hardware, alignment, or algorithm implementation.
- Pageable host CSR clone input can be staged internally by CUDA Runtime; the
  operation is explicitly named as staging but does not promise host
  nonblocking behavior for pageable storage.
- Context/resource/view lifetimes remain caller obligations through event
  completion; event destruction does not synchronize.
- The sparse exact-count selection algorithm is deliberately very slow for
  large domains/counts.
- Context destruction cannot report a failure to restore/select the device;
  it avoids destroying a handle on a device it could not safely select.
- Portability outside the exact tested host/compiler/GPU matrix remains
  `skipped`, as listed above.

The portability/GPU/performance review accepts Milestone 7 for Publication
Checkpoint B.

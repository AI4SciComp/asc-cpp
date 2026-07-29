# Milestone 7 Independent Verification Review

Status: complete; independent verification passes with no open product blocker

Date: 2026-07-28

Scope: frozen Milestone 7 — sparse CUDA and random CUDA facets

## Independence and evidence classification

The verification design and mathematical oracles were frozen before this role
inspected Milestone 7 production. The verifier did not inspect or copy MdeCpp,
the deleted asc-cpp implementation, third-party test material, or a later
milestone.

Final provider evidence is classified exactly as:

```text
sparse_cuda:        configure-tested, compile-tested, runtime-tested, parity-tested
random_cuda:        configure-tested, compile-tested, runtime-tested, parity-tested
random_dense_cuda:  configure-tested, compile-tested, runtime-tested, parity-tested
random_sparse_cuda: configure-tested, compile-tested, runtime-tested, parity-tested
```

The real-hardware environment was:

```text
host compiler:         GCC 11.4.0
CMake:                 4.1.2
CUDA toolkit/compiler: 12.9.86 / nvcc 12.9.86
CUDA Runtime query:    12090
CUDA driver query:     12090
cuSPARSE headers:      12510
GPU:                   NVIDIA GeForce RTX 3060 Laptop GPU
compute capability:    8.6
compiled architecture: 86
primary configuration: Release, shared libraries
```

Only one physical CUDA device and one CUDA toolkit/compiler were available.
Multi-GPU and cross-toolkit evidence are `skipped`. Trusted device CSC
evaluator success is `skipped` because Milestone 7 provides no approved
trusted device CSC producer; no untrusted view was promoted to manufacture
that evidence.

## Verification-owned artifacts

The verifier added:

```text
docs/development/asc-cpp-m7-gpu-sparse-random/verification-design.md
docs/development/asc-cpp-m7-gpu-sparse-random/verification-review.md
tests/random_cuda/philox_oracle.h
tests/random_cuda/test_support.h
tests/random_cuda/random_cuda_test.cc
tests/random_dense_cuda/random_dense_cuda_test.cc
tests/random_sparse_cuda/random_sparse_cuda_test.cc
tests/sparse_cuda/sparse_cuda_test.cc
tests/compile/m7_cuda_contracts.cc
tests/compile/m7_cuda_negative_copy.cc
tests/compile/m7_negative_dense_const_destination.cc
tests/compile/m7_negative_dense_integral.cc
tests/compile/m7_negative_random_result_copy.cc
tests/compile/m7_negative_sparse_integral.cc
tests/compile/m7_negative_sparse_random_result_copy.cc
tests/compile/m7_negative_sparse_volatile.cc
tests/compile/m7_provider_odr.h
tests/compile/m7_provider_odr_a.cc
tests/compile/m7_provider_odr_b.cc
tests/compile/m7_provider_odr_main.cc
tests/compile/m7_random_cuda_header.cc
tests/compile/m7_random_dense_cuda_header.cc
tests/compile/m7_random_sparse_cuda_header.cc
tests/compile/m7_sparse_cuda_header.cc
tests/consumer/sparse_cuda/main.cc
tests/consumer/random_cuda/main.cc
tests/consumer/random_dense_cuda/main.cc
tests/consumer/random_sparse_cuda/main.cc
benchmarks/sparse_cuda/sparse_cuda_benchmark.cc
benchmarks/random_cuda/random_cuda_benchmark.cc
```

The lead alone edited CMake registration and shared integration. Verification
did not edit production or shared CMake/package files.

## Independent oracle and runtime coverage

`philox_oracle.h` directly implements Philox4x32-10 with fixed-width unsigned
round equations. It does not call a production engine or use copied literal
vectors. Float and double Uniform01 expectations use independent exact bit
transforms. Raw counts 1, 3, 4, 5, and 1031 cover every lane, block
boundaries, high offsets, tails, zero count, exact next offsets, partition
equivalence, and independent-context submissions. A truthful 16-byte
`MutableMemoryView` with a five-word request proves declared-capacity rejection
before mutation.

Dense random coverage passes bit parity for float `LayoutLeft`, double
`LayoutRight`, padded unique strides with unchanged holes, rank zero, empty
extents, partition equivalence, two independent contexts, host-placement
rejection, and offset overflow. External `DenseView` terminal allocation
capacity is not runtime-visible; the test requires truthful valid storage and
does not fabricate an undersized external allocation.

Sparse random structure is independently selected by sorting
`(Philox-priority, ordinal)` and then canonical coordinates. Values use the
independent Uniform01 bit oracle, with provider-free generation as a secondary
parity check. Coverage includes ranks zero, one, two, three, and nine; zero
extent; empty, partial, and full counts; float/double; exact offsets; two
independent contexts; invalid counts/domains/offsets/resources; and injected
second-allocation failure. Every call makes exactly two canonical output
allocation attempts. Physical live allocations are correctly zero for empty
outputs, one for rank-zero count-one, and two for nonempty positive-rank
outputs; failure rollback leaves zero live allocations.

Sparse CUDA staging passes float/double and empty CSR parity. A tracking
resource proves exactly three nonempty CSR owner allocations, exactly-once
release, and transactional rollback when allocation calls zero, one, or two
fail. Unit-stride float/double SpMV uses queried explicit workspace; positive
nonunit strides use zero workspace. Independent scalar results cover
nontrivial alpha/beta, untouched padding, and `beta == 0` over NaN output.
Workspace overlapping offsets, indices, values, input, or output is rejected,
as are undersized/host workspace, shape mismatch, input/output overlap, and
untrusted device CSR.

The bounded evaluator passes float/double CSR and coordinate success for
terminal copy, negate, scalar multiply, zero add/subtract in both operand
orders, and same-structure binary add/subtract/multiply. It rejects nonzero
scalar add/subtract, nesting, partial value overlap, structure/value overlap,
and untrusted provenance. `RebindValues` admits exact full current values and
disjoint replacement values while retaining trust, and rejects partial old
value overlap or coordinate/offset/index overlap. Rank-nine coordinate
terminal evaluation succeeds with value parity and unchanged structure.
Independent contexts submit clone, evaluator, and strided SpMV operations
before waiting in reverse order.

## Compile and consumer evidence

All eight public provider/export headers pass self-containment under C++20 and
`-fno-exceptions`. Positive API/ownership translation units and the
four-header multi-translation-unit ODR fixture pass. CMake `try_compile`
rejects all seven negative sources: context/result copies, const/integral
dense destinations, integral sparse generation, sparse-generation copy, and
volatile sparse vector admission.

The four isolated component consumers configure, link, and run using only the
requested component closures. Build-tree and installed/relocated
path-with-spaces modes pass for all four components.

## Exact isolated commands and results

Clean configure:

```sh
cmake -S . -B /tmp/asc-cpp-m7-verifier-release \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
```

Result: pass; CUDA 12.9.86 and architecture 86 configured.

Clean integrated build:

```sh
cmake --build /tmp/asc-cpp-m7-verifier-release --parallel 4
```

The first shared build found M7-VER-005 below. After resolution, the exact
command passed through 100% with warnings as errors, including every M7
runtime, benchmark, header, negative configure check, and ODR target.

Focused final provider suite:

```sh
ctest --test-dir /tmp/asc-cpp-m7-verifier-release \
  --output-on-failure \
  -R 'asc_cpp\.(compile\.m7_|sparse_cuda\.(runtime|benchmark)|random_cuda\.(runtime|benchmark)|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)' \
  -j1
```

Result: pass, 28/28 in 13.34 seconds: 22 compile/header/ODR tests, four
runtime/parity tests, and two correctness/performance smoke benchmarks.
After expanding the workspace-overlap fixture to use five truthful operand
allocations each at least as large as the queried workspace and correcting
M7-VER-006, the isolated sparse target rebuilt and
`asc_cpp.sparse_cuda.runtime` passed again, 1/1 in 1.45 seconds.

Isolated consumers:

```sh
ctest --test-dir /tmp/asc-cpp-m7-verifier-release \
  --output-on-failure \
  -R 'asc_cpp\.consumer\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.(build_tree|install_relocate)' \
  -j1
```

Result: pass, 8/8. Build-tree and installed/relocated consumers pass for all
four M7 components.

Sanitizer, CPU-only package-isolation, unavailable-toolkit, full install
manifest, and aggregate test matrices are owned and reported by the separate
portability and lead reviews. This review does not relabel those results as
independent verifier evidence.

## Performance observations

These are single-system correctness smoke observations, not speed gates or
comparative claims. Setup, transfer, and oracle work are untimed except the
explicitly named sparse-generation output allocations.

```text
CSR SpMV float:
  elapsed_ns=1555123, repetitions=20, workspace_bytes=704
  operation_allocation_calls=0, checksum=14940377177479771011

CSR SpMV double:
  elapsed_ns=1478683, repetitions=20, workspace_bytes=752
  operation_allocation_calls=0, checksum=9178157494086742915

raw Philox 2^20 words:
  elapsed_ns=987024, repetitions=12
  operation allocations=0, checksum=13841617604916660332

dense float Uniform01 1024x1024:
  elapsed_ns=1350406, repetitions=12
  operation allocations=0, checksum=15165452046652654026

sparse float Uniform01 128x128, exact count 128:
  elapsed_ns=7342584846, repetitions=12
  two output allocations per generation included
  benchmark-loop allocation calls=30, allocated bytes=38400
  (three warmups plus twelve measured repetitions)
  checksum=7322770344431580519
```

The sparse-random observation documents the approved bounded low-workspace
algorithm's substantial cost; no regression threshold or unsupported speedup
claim is inferred.

## Findings and resolutions

- M7-VER-001: the initial raw pointer-plus-count surface could not prove its
  terminal bound. Owner resolution amended the frozen contract and production
  API to `MutableMemoryView + word_count`; the truthful capacity-overrun test
  passes.
- M7-VER-002: dense/sparse external view types do not carry allocation terminal
  capacity. This inherited non-owning-view limitation is disclosed; unsafe
  fabricated undersized-storage tests were removed.
- M7-VER-003: `CudaCsrArray::view()` initially omitted the compressed-format
  template argument and failed instantiation. Production supplied the explicit
  CSR format; float/double compilation and runtime pass.
- M7-VER-004: the verifier initially counted zero-byte sparse output attempts
  as physical live allocations. The test was corrected to distinguish two
  allocation attempts from zero/one/two physical allocations; production was
  unchanged.
- M7-VER-005: the independent Release/shared build failed because the private
  `CompletionEvent(bool)` constructor was not exported, leaving all M7 shared
  providers with an unresolved symbol. Production exported the existing
  constructor; the clean shared rebuild and focused suite pass.
- M7-VER-006: the verifier's large truthful-capacity workspace-overlap fixture
  initially passed `std::vector<float>` directly to a span-parameter helper,
  which prevented template deduction in a clean GCC build. The verifier now
  supplies explicit `std::span<const float>` arguments; this was a test defect,
  not a production change.
- Review hardening also added volatile exclusion, rank-nine support, checked
  sparse value rebinding, pairwise sparse-span validation, full bounded
  compressed evaluation, workspace/operand overlap checks, and shared-library
  coverage. All corresponding verifier fixtures pass after rerun.

## Remaining risks and skips

- Trusted device CSC evaluator success: `skipped`, no approved M7 producer.
- Multi-GPU wrong-device/restoration and concurrent cross-device execution:
  `skipped`, one physical CUDA device.
- Cross-toolkit/compiler and non-Linux GPU runtime matrices: `skipped`, one
  local CUDA/GCC environment.
- External dense/sparse terminal allocation bounds remain a truthful-storage
  caller precondition because those inherited views do not expose capacity.
- Sparse exact-count generation is deterministic and allocation-bounded but
  intentionally slow at the benchmarked density; this is documented, not
  promoted to a speed guarantee.

No push, merge, tag, release, branch deletion, commit, or other remote/history
operation was performed by independent verification.

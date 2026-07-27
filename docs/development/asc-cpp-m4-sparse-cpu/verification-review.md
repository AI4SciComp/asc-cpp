# Milestone 4 Independent Verification Review

Status: Accepted after corrections

Date: 2026-07-26

Branch: `feature/asc-cpp-m4-sparse-cpu`

Role: independent verification

## Independence and scope

The structural and numerical oracles in `verification-design.md` were frozen
before inspection of Milestone 4 production source or another sparse
implementation. They were derived only from the approved milestone contract,
accepted ADRs, and predecessor public contracts. No MdeCpp sparse source or
tests, user-deleted asc-cpp sparse implementation or tests, upstream sparse
test corpus, or third-party numerical table was inspected or copied.

After the independent design was frozen, this review inspected the public
Milestone 4 API, exercised the implementation, and reconciled findings from
the other independent reviewers. Verification writes stayed within the
ownership ledger. CMake registration and package integration remained
lead-owned.

## Verification artifacts

The independent verification layer consists of:

```text
tests/sparse/test_support.h
tests/sparse/allocation_counter.h
tests/sparse/allocation_counter.cc
tests/sparse/coordinate_test.cc
tests/sparse/compressed_test.cc
tests/sparse/conversion_test.cc
tests/sparse/evaluate_test.cc
tests/sparse/linalg_test.cc
tests/compile/m4_sparse_contract.cc
tests/compile/m4_sparse_multi_tu.h
tests/compile/m4_sparse_multi_tu_a.cc
tests/compile/m4_sparse_multi_tu_b.cc
tests/compile/m4_sparse_multi_tu_main.cc
tests/compile/m4_sparse_negative_const_mutation.cc
tests/compile/m4_sparse_negative_invalid_extents.cc
tests/compile/m4_sparse_negative_missing_writable.cc
tests/compile/m4_sparse_negative_owner_copy.cc
tests/compile/m4_sparse_negative_spmv_integral.cc
tests/compile/m4_sparse_negative_unsupported_element.cc
tests/consumer/sparse/main.cc
benchmarks/sparse/allocation_free_benchmark.cc
docs/development/asc-cpp-m4-sparse-cpu/verification-design.md
docs/development/asc-cpp-m4-sparse-cpu/verification-review.md
```

The corresponding lead-owned CMake registrations expose five runtime tests,
two positive compile/link tests, six expected compile failures, one benchmark,
and build-tree, installed/relocated, and subproject consumers.

## Coverage and results

### Coordinate storage

The tests independently verified:

- static, dynamic, rank-zero, zero-extent, invalid, and overflowing shapes;
- fixed capacity, capacity exhaustion, no hidden growth, and move-only state;
- coordinate/rank/bounds validation before mutation;
- stable lexicographic finalization and stable floating duplicate summation;
- duplicate reject and integral-overflow rollback;
- explicit-zero keep/drop and NaN-payload retention;
- immutable canonical structure with mutable values;
- checked lookup and rejection of host access to non-host descriptors;
- malformed and overlapping external metadata; and
- partial resource failure with exactly-once deallocation.

The frozen rank-three oracle produced coordinates
`(0,0,0)`, `(0,2,0)`, `(0,2,1)`, `(1,0,1)` and values
`0`, `4`, `-2`, `12` under keep/sum. Drop removed only the first entry.

### CSR, CSC, and conversion

The exact rectangular CSR and CSC structures in the frozen design matched.
Tests cover malformed offsets and indices, strict canonical ordering,
empty/degenerate storage, const propagation, all pairwise external-buffer
overlaps, move/resource lifetime, and allocation failure after each
destination allocation.

All six named conversion directions passed. They preserve shape, canonical
order, explicit zeros, and a chosen NaN payload. Allocation counters observe
three explicit buffers for compressed destinations and two for coordinate
destinations, with no additional conversion workspace.

### Expression evaluation

Coordinate, CSR, and CSC terminals satisfy the storage-neutral readable,
placement, writable, and sparsity contracts with the intended const and
volatile boundaries. Evaluation passed structure-preserving negation,
direct-self no-op, rank/shape/type/context/placement validation, all six
non-preserving sparsity-effect rejections, rank-zero behavior, recursive
physical-span overlap rejection, transactionality, and zero computational
allocations.

### CSR SpMV

The independently calculated exact result

```text
A*x = [-2, -8, 11, -2]
2*A*x - 0.5*y_old = [-9, -26, 7, -24]
```

passed for `float` and `double`. Additional tests cover a tolerant
non-integral case, deterministic traversal, `beta == 0` no-read behavior,
external adapters with deleted unary address-of, dense-view interoperability,
total coordinate-output coverage, empty and degenerate shapes, NaN/infinity,
placement/context/shape failures, and conservative overlap in both address
orders among matrix, input, output, partial dense views, partial coordinate
views, and independently created descriptors.

Direct `AliasToken` tests cover identity/identity, span/span, point/span,
symmetry, disjoint spans, zero-byte spans, and overflow-safe construction.
Dense subviews retain conservative root-span aliasing.

### Compile, package, and dependency contracts

The positive contract checks C++20 public concepts, top-level cv behavior,
trivially copyable views, move-only owners, supported element types, and
float/double-only SpMV. A multi-translation-unit program compiles and links.
All six intentional negative programs fail compilation for the required
reason.

The sparse-only consumer includes no dense header, defines an external vector
adapter, links only `ASC::sparse`, and executes SpMV. Installed target
inspection requires exactly:

```text
ASC::sparse -> ASC::core;ASC::expression
```

and rejects imported utilities, dense, random, aggregate, provider-facet, or
GPU targets. Source include inspection found no forbidden module or provider
dependency.

## Findings and resolutions

| Finding | Verification disposition |
| --- | --- |
| M4-DOC-01 top-level const destination could satisfy `WritableExpression` | Reproduced by concept oracle; fixed at the public concept boundary and passed. |
| M4-DOC-02 external value storage could overlap immutable sparse structure | Independent pairwise overlap cases now return `kInvalidArgument` before publication. |
| M4-DOC-03 iterable host-style spans on non-host sparse descriptors | Public API now exposes raw metadata pointers while checked access remains host-only; contract rechecked. |
| M4-DOC-04 incomplete coordinate SpMV output silently omitted writes | Independent incomplete-output case now fails unchanged before execution. |
| M4-DOC-05 overloaded/deleted unary address-of broke external adapters | External adapter with deleted `operator&` now compiles and runs through `std::addressof`. |
| M4-DOC-06 volatile types could satisfy base `ReadableExpression` and fail later | New top-level volatile and const-volatile negative concept oracles first failed as intended, then passed after the base concept was corrected. Const readable descriptors remain accepted. |
| M4-VERIFY-RANK rank-mismatched evaluation instantiated an invalid mutation body | Rank-zero and mismatched source cases now compile, validate, and return without mutation through compile-time gating. |
| M4-LEAD-ALIAS identity-only sparse aliasing missed independently created overlapping views | Physical value-span checks now reject partial overlap in both operand orders. |
| M4-PORT-01 neutral alias metadata was not span-aware | `AliasToken` now supports checked byte spans and symmetric conservative comparison; ASC dense/sparse adapters publish spans. |
| M4-PORT-02 volatile placed/writable descriptors passed concept selection | Negative placed/writable concept oracles now pass; const readable placement remains accepted. |

All release-blocking findings are resolved. No unresolved verification defect
remains.

## Exact local evidence

### GCC debug, install, consumers, and packages

```bash
cmake -S . \
  -B /tmp/asc-cpp-m4-verifier-install.3cf715/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m4-verifier-install.3cf715/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-verifier-install.3cf715/build \
  --output-on-failure --parallel 4
```

Result after all corrections: configure and strict build passed; full CTest
passed **138/138** in 214.17 seconds. This includes 24 Milestone 4 tests,
13 consumer tests, and 16 package tests. Sparse subproject, build-tree,
installed/relocated, component build/install, and package-registry checks all
passed.

The post-correction focused command was:

```bash
ctest --test-dir /tmp/asc-cpp-m4-verifier-install.3cf715/build \
  --output-on-failure -R '^asc_cpp\.sparse\.'
```

Result: **14/14 passed**, including five runtime tests, the concept contract,
multi-TU link, six expected compile failures, and the allocation benchmark.

### Clang ASan and UBSan

```bash
cmake -S . \
  -B /tmp/asc-cpp-m4-verifier-san.on2Fhn/build \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m4-verifier-san.on2Fhn/build \
  --target \
    asc_sparse_coordinate_test asc_sparse_compressed_test \
    asc_sparse_conversion_test asc_sparse_evaluate_test \
    asc_sparse_linalg_test asc_m4_sparse_contract \
    asc_m4_sparse_multi_tu asc_sparse_allocation_free_benchmark \
  --parallel 4
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m4-verifier-san.on2Fhn/build \
  --output-on-failure -R '^asc_cpp\.sparse\.'
```

The generated compile and link commands contain
`-fsanitize=address,undefined`. Result: configure probes, strict build, and
**14/14 tests passed** with no sanitizer finding.

### Optimized allocation/performance observation

```bash
cmake -S . \
  -B /tmp/asc-cpp-m4-verifier-release.Buk7kl/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m4-verifier-release.Buk7kl/build \
  --target asc_sparse_allocation_free_benchmark --parallel 4
/tmp/asc-cpp-m4-verifier-release.Buk7kl/build/tests/sparse/asc_sparse_allocation_free_benchmark
```

Observed result:

```text
compiler=GCC 11.4.0
configuration=Release-like
backend=serial-reference
format=CSR-zero-based-canonical
shape=128x128
nnz=382
vector_layout=contiguous-external
iterations=200
operation=spmv
allocations_in_operations=0
elapsed_us=501
checksum=382
```

This is a local smoke observation, not a stable threshold, provider
comparison, scalability result, or performance guarantee.

### Formatting and whitespace

```bash
clang-format-19 --dry-run --Werror \
  benchmarks/sparse/allocation_free_benchmark.cc \
  tests/consumer/sparse/main.cc tests/sparse/*.cc tests/sparse/*.h \
  tests/compile/m4_sparse_*.cc tests/compile/m4_sparse_*.h
git diff --check -- \
  tests/sparse tests/compile/m4_sparse_\* tests/consumer/sparse \
  benchmarks/sparse \
  docs/development/asc-cpp-m4-sparse-cpu/verification-design.md \
  docs/development/asc-cpp-m4-sparse-cpu/verification-review.md
```

Result: passed.

## Provider and GPU classification

CPU provider evidence is the built-in deterministic serial-reference
implementation, runtime-tested and allocation-observed locally. There is no
optional CPU provider target in Milestone 4.

GPU evidence is exactly **skipped**. Milestone 4 has no GPU target, source,
provider discovery, SDK dependency, device kernel, or device execution path.
It is not configure-tested, compile-tested, runtime-tested, or parity-tested.

## Remaining limitations and risks

- External non-ASC adapters carry semantic obligations for truthful shape,
  placement, total unique writability, and conservative alias reporting;
  concepts cannot prove those runtime facts.
- External view creation checks metadata and address arithmetic but cannot
  prove allocation provenance, lifetime, alignment truthfulness, or actual
  device accessibility.
- Reference coordinate finalization and format conversion intentionally favor
  allocation transparency and determinism over asymptotic performance.
- Local verification did not provide hosted MSVC, AppleClang, or device
  runtime evidence. Those remain CI/future-provider concerns.
- The elapsed benchmark datum is machine- and load-dependent and must not be
  treated as a regression threshold.

Subject to the lead's final cumulative diff and publication reconciliation,
independent verification accepts Milestone 4 for Publication Checkpoint B.

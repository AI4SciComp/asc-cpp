# Milestone 3 Independent Verification Review

Status: Complete for Publication Checkpoint B

Date: 2026-07-26

Branch: `feature/asc-cpp-m3-dense-cpu`

Baseline and current uncommitted `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

## Independence and scope

The verifier derived tests from the frozen Milestone 3 contract, accepted ADRs
0001, 0004, 0007--0011, and 0013, and the architecture verification strategy.
The verifier did not inspect MdeCpp tests, deleted asc-cpp tests, or upstream
implementation/test/vector corpora. Expected numerical results were calculated
directly from the mathematical operations.

Verifier-owned artifacts are confined to:

```text
tests/dense/**
tests/compile/m3_*.cc
tests/compile/m3_dense_multi_tu.h
tests/consumer/dense/**
benchmarks/dense/**
docs/development/asc-cpp-m3-dense-cpu/verification-design.md
docs/development/asc-cpp-m3-dense-cpu/verification-review.md
```

No production, root CMake, package, general documentation, branch, remote, tag,
or release state was modified by this role.

## Contract coverage

### Layouts and views

- rank-zero logical/span size one and scalar access;
- zero-extent logical/span size zero;
- empty `LayoutLeft{max,max,0}` and `LayoutRight{0,max,max}` success without
  irrelevant stride overflow;
- exact left, right, and padded-stride offsets;
- exhaustive and non-exhaustive mappings;
- conservative uniqueness and repeated-address rejection for mutable views;
- negative extent/stride, nonempty/null, index, and nonempty overflow failures;
- one-way mutable-to-const conversion and trivial-copy traits;
- host access and non-host dereference rejection;
- rank-preserving subviews, preserved strides/space/alias, empty end subview,
  and out-of-range transactions.

### Ownership

- core `Extents` acceptance and structural-extents-impostor rejection;
- move-only traits and const-view return type;
- left-default and right owner layouts;
- scalar value initialization and zero-size ownership;
- non-host resource rejection before allocation;
- move transfer and moved-from view failure;
- named deep clone into an explicit resource/context with distinct storage;
- injected allocation failure rollback for discard-resize;
- successful discard-resize, replacement value initialization, and
  exactly-once resource deallocation observations.

Tests do not dereference invalidated views. Successful resize invalidation is
verified by the owner state transition and documented lifetime contract, not
by intentionally invoking undefined behavior.

### Evaluation and reductions

- dense terminal expression metadata and alias identity;
- external storage-neutral adapted expression;
- left source, padded destination, nested temporary node, and rank-zero scalar
  expansion;
- rank-zero dense destination;
- dimension-zero-fastest read order;
- exact direct self-assignment no-op;
- nested same-storage alias and partial byte-span overlap rejection;
- wrong-shape and memory-space failure before destination mutation;
- zero allocations around successful evaluation;
- sum/minimum/maximum on padded storage;
- empty sum zero and empty min/max `kInvalidArgument`;
- checked integral sum overflow.

### Serial dense algebra

- direct mutable-view and const-view readable operands;
- float and double dispatch;
- rank-one and rank-two Copy/Scal/Axpy;
- arbitrary validated positive strides;
- exact Copy identity, safe exact Axpy identity, partial-overlap rejection,
  zero-size operations, and unchanged destinations on failure;
- Dot exact/non-unit-stride, mutable sources, empty result, and wrong shape;
- Nrm2 3-4-5, empty, NaN, infinity, and finite extreme
  `{max/4,max/4}` scaled-sum-of-squares evidence;
- Gemv no-transpose, transpose, padded matrix, mutable sources, nontrivial
  alpha/beta, `beta == 0` quiet-NaN no-read, wrong shape, invalid operation,
  overlap, and zero-inner-dimension behavior;
- Gemm all four transpose pairs, left/right/padded layouts, rectangular
  operands, mutable sources, nontrivial alpha/beta, `beta == 0` quiet-NaN
  no-read, wrong shape, overlap, zero-inner-dimension behavior, and float
  dispatch.

## Findings and resolutions

| ID | Severity | Evidence | Resolution |
| --- | --- | --- | --- |
| V-M3-001 | release-blocking portability/API | strict GCC rank-zero `Evaluate` instantiation reported `-Wtype-limits` at the loop comparing `dimension < Rank` with `Rank == 0` | production guarded the offset loop at compile time; strict GCC, Clang, rank-zero runtime, header, and exceptions-disabled evidence now passes |
| DOC-M3-01 regression | release-blocking correctness | zero-extent mappings must be vacuously unique/exhaustive | independent empty mapping/view/owner cases pass |
| DOC-M3-02 regression | release-blocking constraint correctness | Boolean/volatile dense elements and Boolean reductions must not fail inside an advertised function body | compile traits reject Boolean/volatile elements; supported traits and reduction runtime pass |
| DOC-M3-03 regression | high API usability | natural mutable-view readable operands previously failed linalg template deduction | compile and runtime calls using mutable sources pass for Copy, Axpy, Dot, Nrm2, Gemv, and Gemm |
| DOC-M3-04 regression | release-blocking empty-layout overflow | named empty mappings with huge pre-zero extents previously incurred irrelevant stride overflow | exact left/right boundary shapes succeed and report empty unique/exhaustive mappings |
| DOC-M3-05 regression | high template/error correctness | the earlier structural extents concept accepted value types incompatible with unconditional owner copy/move contracts | owner is bound to core `Extents`; a structural impostor is rejected by a positive compile-contract executable |

The first Clang build after adding the DOC-M3-05 regression reported an unused
test-only `kRank` member under `-Werror`. The verifier marked the intentionally
structural member `[[maybe_unused]]`; the subsequent strict Clang sanitizer
build and run passed. This was a verifier artifact, not a production defect.

No release-blocking independent verification finding remains open.

## Exact commands and results

### GCC 11.4 Debug static integration

Configuration and full build:

```bash
cmake -S . \
  -B /tmp/asc-m3-verification.Gpes8i/gcc-debug \
  -G "Unix Makefiles" \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/asc-m3-verification.Gpes8i/gcc-debug -j2
```

Result: configure passed; full warnings-as-errors build passed.

An initial contract-focused selection, including architecture, all M3 headers,
runtime tests, negative tests, consumers, packages, and the benchmark, passed
39/39. After the final mutable-source, empty-stride, and extents refinements,
the exact final focused commands were:

```bash
ctest --test-dir /tmp/asc-m3-verification.Gpes8i/gcc-debug \
  --output-on-failure -R '^asc_cpp\.dense\.'
ctest --test-dir /tmp/asc-m3-verification.Gpes8i/gcc-debug \
  --output-on-failure -R '^asc_cpp\.compile\.m3_header'
ctest --test-dir /tmp/asc-m3-verification.Gpes8i/gcc-debug \
  --output-on-failure -R '^asc_cpp\.consumer\.dense\.'
```

Results:

```text
dense compile/runtime/numerical/negative/benchmark: 10/10 passed
dense headers, exceptions enabled and disabled:     14/14 passed
dense subproject/build-tree/installed relocation:    3/3 passed
```

The relocated consumer path contains spaces. The dense consumer imports
`ASC::dense`, `ASC::core`, and `ASC::expression`; it rejects forbidden
utilities/sparse/random/provider/aggregate targets and confirms direct
`ASC::dense` links exactly `ASC::core;ASC::expression`.

### Clang 19 ASan and UBSan

```bash
cmake -S . \
  -B /tmp/asc-m3-verification.Gpes8i/clang-sanitized \
  -G "Unix Makefiles" \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/asc-m3-verification.Gpes8i/clang-sanitized -j2 \
  --target \
    asc_dense_layout_view_test \
    asc_dense_array_test \
    asc_dense_evaluate_test \
    asc_dense_linalg_test \
    asc_m3_dense_contract \
    asc_m3_dense_multi_tu \
    asc_dense_allocation_free_benchmark
ctest --test-dir /tmp/asc-m3-verification.Gpes8i/clang-sanitized \
  --output-on-failure -R '^asc_cpp\.dense\.'
```

Result: sanitizer configure probes passed; strict Clang build passed after the
test-only warning correction; 10/10 dense tests passed with ASan and UBSan.

### Direct strict and negative probes

Representative runtime sources were also compiled directly with:

```text
g++ -std=c++20 -Wall -Wextra -Wconversion -Wsign-conversion
    -Werror -pedantic-errors
```

The layout/view, array, evaluation, linalg, compile-contract, multi-TU, and
dense consumer programs compiled and ran successfully. Independent direct
negative compilation produced expected failure for all 3/3 sources:

```text
m3_dense_negative_const_mutation.cc
m3_dense_negative_owner_copy.cc
m3_dense_negative_unsupported_element.cc
```

### Formatting and whitespace

```bash
clang-format-19 --dry-run --Werror <all verifier-owned M3 .h/.cc files>
git diff --check -- \
  tests/dense tests/compile/m3_dense_ tests/consumer/dense \
  benchmarks/dense \
  docs/development/asc-cpp-m3-dense-cpu/verification-design.md
```

Result: passed.

## Numerical and allocation evidence

All ordinary expected values are test-owned exact formulas. Floating
comparisons use exact equality for exactly representable cases and a
case-specific relative tolerance for the extreme stable norm and composed
Gemm cases. No external numerical implementation is used as an oracle.

The allocation observer reports zero global allocations around successful
expression evaluation. The benchmark reports zero allocations across both
evaluation and Gemm after setup.

Informational Release-like benchmark command:

```bash
g++ -std=c++20 -O2 -DNDEBUG \
  -Wall -Wextra -Wconversion -Wsign-conversion -Werror -pedantic-errors \
  -Iinclude -Itests/dense \
  benchmarks/dense/allocation_free_benchmark.cc \
  tests/dense/allocation_counter.cc \
  src/dense/reference_linalg.cc \
  src/core/status.cc src/core/contracts.cc src/core/memory.cc \
  src/core/execution.cc \
  -o /tmp/asc-m3-benchmark
/tmp/asc-m3-benchmark
```

Observed on an Intel Core i7-11800H under WSL2:

```text
compiler=GCC 11.4.0
configuration=Release-like
backend=serial-reference
shape=32x32
iterations=100
operations=evaluate-add,gemm
allocations_in_operations=0
elapsed_us=20585
checksum=4.75
```

This is allocation and execution evidence only. There is no timing threshold,
provider comparison, stable baseline, or performance claim.

## Provider and GPU classification

- provider-free CPU serial reference: **runtime-tested**;
- optimized CPU provider: not implemented and not claimed;
- GPU: **skipped**.

GPU is skipped because Milestone 3 contains no GPU target, provider source,
toolkit configuration, device compilation, runtime call, or parity contract.

## Remaining risks and skips

- Independent local verification used GCC 11.4 and Clang 19 on Linux/WSL2.
  MSVC shared/multi-config and AppleClang/libc++ remain hosted-CI evidence.
- ThreadSanitizer was not run; M3 exposes deterministic synchronous serial
  operations and no production global mutable state, so ASan/UBSan were the
  applicable local runtime-safety gate.
- No optimized CPU or GPU provider exists in the approved milestone.
- The benchmark is one informational local observation and cannot establish
  comparative performance.
- The repository remains a cumulative, intentionally uncommitted M0--M3 tree;
  publication staging requires a complete lead-owned cumulative diff audit.

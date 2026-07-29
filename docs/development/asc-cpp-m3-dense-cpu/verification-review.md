# Milestone 3 independent verification review

Status: complete; independently approved for Publication Checkpoint B

## Independence and scope

Verification was derived from the frozen contract and approved ADRs before any
Milestone 3 production header or source was inspected. The resulting oracles
are recorded in `verification-design.md`. Production inspection began only
after that design was frozen.

The review did not inspect or copy MdeCpp material, deleted asc-cpp tests, or
the Milestone 3 implementation, tests, benchmarks, or reports on
`feature/asc-cpp-m8-hardening-downstream`.

Verification writes are limited to the paths assigned in the ownership ledger:

- `tests/dense/**`, excluding CMake;
- `tests/compile/m3_*.cc` and `tests/compile/m3_multi_tu.h`;
- `tests/consumer/dense/main.cc`;
- `benchmarks/dense/dense_benchmark.cc`; and
- this review and the contract-first verification design.

Lead-owned CMake registration was reviewed but not modified by verification.

## Independent coverage

The final suites cover:

- rank-zero through rank-three left/right layout properties for every small
  extent from zero through five, explicit unique padded/strided mappings,
  repeated-address mappings, span calculations, bounds, and overflow;
- mutable and const view publication, trivial-copy and const-conversion
  traits, host access, subviews, allocation freedom, descriptor preservation,
  and expression terminal classification;
- owner value initialization, rank zero, zero extent, mixed static/dynamic
  extents, left/right layout, move-only ownership, moved-from failures, deep
  clone, allocation failure cleanup, discard-resize success and transactional
  failure, non-host resource rejection, and byte-count overflow;
- scalar and compound expression evaluation, logical traversal order,
  external adapters, exact self-assignment, direct and nested alias rejection,
  direct and nested non-host source rejection, shape/context failures,
  destination transactionality, and zero allocation;
- deterministic sum/minimum/maximum reductions, empty identities and errors,
  cancellation order, strided inputs, and zero allocation;
- `float` and `double` Copy, Scal, Axpy, Dot, stable Nrm2, Gemv, and Gemm across
  contiguous and strided layouts, all transpose combinations, alpha/beta
  semantics, beta-zero no-read behavior, empty dimensions, invalid shapes and
  transpose values, overlap rejection, non-host rejection, transactionality,
  and zero allocation;
- all seven Dense headers in isolation, disabled exceptions, multiple
  translation units, and five expected-failure compile constraints;
- a Dense-only isolated consumer using the `core;expression;dense` closure;
  and
- threshold-free pointwise-evaluation and serial-Gemm benchmark smoke.

## Findings and resolutions

1. **Repeated-address const view publication.** Early production allowed a
   const view to publish a non-unique mapping while mutable publication rejected
   it. The frozen Milestone 3 contract permits only unique DenseView mappings.
   Production now rejects both forms. Independent regressions pass.
2. **Nrm2 infinity and NaN classification.** The first scaled sum-of-squares
   implementation could produce NaN for repeated infinities. Production now
   propagates NaN, returns positive infinity for one or repeated infinities
   absent NaN, preserves finite scaled stability, and returns positive zero for
   zero input. Independent regressions pass for `float` and `double`.
3. **Non-host expression sources.** Independent portability review found that
   Evaluate validated the destination but not DenseView leaves in the source
   expression, so a device-marked source could reach a fatal adapter read.
   Production added an optional recursive expression-access validation hook and
   performs it before shape, alias, or mutation. Direct and nested non-host
   source regressions now return `kMemoryAccess` with the destination unchanged.
4. **MSVC allocation instrumentation.** The verification allocation probe used
   `std::aligned_alloc` unconditionally. It now uses `_aligned_malloc` and
   `_aligned_free` on MSVC, retains `aligned_alloc` and `free` elsewhere, and
   rejects rounded-size overflow before addition.
5. **Sanitizer allocation-family mismatch.** Focused ASan exposed that Core's
   aligned nothrow allocation could pair the platform operator with the probe's
   aligned delete. The probe now supplies ordinary and aligned scalar/array
   nothrow overloads plus matching placement deletes. Nothrow forms return null
   on failure. The final focused ASan+UBSan matrix passes.

No unresolved production correctness finding remains in the independently
reviewed Milestone 3 scope.

## Exact local commands and results

Strict direct compilation and execution:

```sh
clang-format-19 -i tests/dense/*.cc tests/dense/*.h \
  tests/compile/m3_*.cc tests/compile/m3_multi_tu.h \
  tests/consumer/dense/main.cc benchmarks/dense/dense_benchmark.cc
clang-format-19 --dry-run --Werror tests/dense/*.cc tests/dense/*.h \
  tests/compile/m3_*.cc tests/compile/m3_multi_tu.h \
  tests/consumer/dense/main.cc benchmarks/dense/dense_benchmark.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/dense tests/dense/layout_view_test.cc \
  tests/dense/allocation_probe.cc \
  build/m2-doc-review/src/core/libasc_core.a -pthread \
  -o /tmp/asc_m3_layout_view_test
/tmp/asc_m3_layout_view_test
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/dense tests/dense/array_evaluate_test.cc \
  tests/dense/allocation_probe.cc \
  build/m2-doc-review/src/core/libasc_core.a -pthread \
  -o /tmp/asc_m3_array_evaluate_test
/tmp/asc_m3_array_evaluate_test
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/dense tests/dense/linalg_test.cc \
  tests/dense/allocation_probe.cc src/dense/linalg.cc \
  build/m2-doc-review/src/core/libasc_core.a -pthread \
  -o /tmp/asc_m3_linalg_test
/tmp/asc_m3_linalg_test
```

Result: **pass**. Formatting and all three direct runtime executables passed
with no output.

Header, disabled-exception, multi-TU, and negative compilation:

```zsh
for source_file in tests/compile/m3_header_*.cc \
    tests/compile/m3_exceptions_disabled.cc \
    tests/compile/m3_multi_tu_a.cc \
    tests/compile/m3_multi_tu_b.cc \
    tests/compile/m3_multi_tu_main.cc; do
  g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions \
    -Iinclude -Itests/compile -c "$source_file" \
    -o "/tmp/${source_file:t}.o" || exit 1
done
g++ /tmp/m3_multi_tu_a.cc.o /tmp/m3_multi_tu_b.cc.o \
  /tmp/m3_multi_tu_main.cc.o -o /tmp/asc_m3_multi_tu
/tmp/asc_m3_multi_tu
negative_failure=0
for source_file in tests/compile/m3_negative_*.cc; do
  if g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
      -Iinclude -Itests/compile -c "$source_file" \
      -o /tmp/m3_negative.o >/tmp/m3_negative.log 2>&1; then
    print -r -- "unexpected compile success: $source_file"
    negative_failure=1
  fi
done
exit $negative_failure
```

Result: **pass**. All pass probes compiled, and the multi-TU executable linked
and ran. Each of the following independently failed strict compilation as
required: owner copy, const-view mutation, unsupported owner element,
unsupported linear-algebra scalar, and wrong-rank Dot.

The initial clean preset attempt exposed the environment dependency:

```sh
cmake --preset test-debug
```

Result: **fail**, because this workspace does not install asc-cmake into a
default search prefix. No product compilation occurred. The approved local
asc-cmake package was then supplied explicitly:

```sh
cmake --preset test-debug \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build --preset test-debug -j 4
ctest --preset test-debug --output-on-failure
```

Result: **pass**, 109/109 tests. This included architecture, compile,
disabled-exception, runtime, build-tree consumer, installed-and-relocated
consumer, package, registry, subproject, and benchmark gates.

After final production and probe corrections, the focused Debug/static rerun
was:

```sh
cmake --build build/test-debug -j 4 --target \
  asc_dense_layout_view_test asc_dense_array_evaluate_test \
  asc_dense_linalg_test asc_dense_multi_tu asc_dense_benchmark
ctest --test-dir build/test-debug \
  -R '^asc_cpp\.dense\.(layout_view_test|array_evaluate_test|linalg_test|multi_tu|benchmark)$' \
  --output-on-failure
```

Result: **pass**, 5/5.

Focused sanitizer validation:

```sh
cmake --preset test-asan-ubsan \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build --preset test-asan-ubsan -j 4
cmake --build build/test-asan-ubsan -j 4 --target \
  asc_dense_layout_view_test asc_dense_array_evaluate_test \
  asc_dense_linalg_test asc_dense_multi_tu asc_dense_benchmark
ctest --test-dir build/test-asan-ubsan \
  -R '^asc_cpp\.dense\.(layout_view_test|array_evaluate_test|linalg_test|multi_tu|benchmark)$' \
  --output-on-failure
```

Result: **pass**, 5/5 under ASan+UBSan after the allocation-family regression
was fixed.

An earlier broad `ctest -L dense` sanitizer attempt selected isolated package
consumers in addition to focused in-tree tests. It first exposed the fixed
allocation-family mismatch. Its two isolated consumers also failed to link
because an independently configured consumer was not given sanitizer runtime
link flags. Those consumers are covered by the unsanitized clean package and
relocation matrix; the sanitizer acceptance gate is the anchored focused
command above.

Final whitespace check:

```sh
git diff --check -- tests/dense tests/compile/m3_* \
  tests/consumer/dense benchmarks/dense \
  docs/development/asc-cpp-m3-dense-cpu/verification-design.md
```

Result: **pass**.

## Performance evidence

The final Debug benchmark smoke reported:

```text
compiler=gcc configuration=debug-like operation=evaluate scalar=double shape=32x32 iterations=64 total_ns=146078618 per_iteration_ns=2.28248e+06 checksum=1283 allocations=0
compiler=gcc configuration=debug-like operation=gemm scalar=double shape=32x32 iterations=4 total_ns=12177049 per_iteration_ns=3.04426e+06 checksum=20790.7 allocations=0
```

These are reproducibility observations, not performance thresholds. Both
checksums are nonzero, and both timed loops recorded zero allocation.

## Provider evidence and remaining risk

- CPU provider evidence: serial host runtime-tested for all Milestone 3 Dense
  operations, transaction cases, allocation cases, and numerical oracles.
- GPU evidence: **skipped**. Milestone 3 has no approved GPU provider target or
  implementation.
- The MSVC-specific allocation branch is source-reviewed here; native Windows
  compilation belongs to the portability matrix.
- No performance threshold is approved, so benchmark evidence detects
  functional or allocation regressions but does not establish a speed target.

Independent disposition: **approve Milestone 3 for Publication Checkpoint B**,
subject to the lead's final clean integration matrix and checkpoint report.

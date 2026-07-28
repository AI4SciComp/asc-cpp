# Milestone 2 independent verification review

## Review status

Milestone 2 is independently verified locally at Publication Checkpoint B.
The reviewed branch is `feature/asc-cpp-m2-independent-foundations`, based on
`33b261ea33616a6395c4ad3b20646093103344f7` before the cumulative uncommitted
milestone changes.

The contract-first plan and literal random oracles were frozen in
`verification-design.md` before production inspection. No MdeCpp source or
test, deleted asc-cpp random source or test, Random123 implementation or test,
external vector corpus, or prohibited cumulative Milestone 2 implementation
was inspected to derive expected random values.

No unresolved production defect remains from independent local review.

## Verification files and purpose

Utilities:

- `tests/utilities/test_support.h`: minimal independent test harness;
- `tests/utilities/command_line_test.cc`: parser creation, conversion,
  transaction, provenance, redaction, and help falsification; and
- `tests/utilities/timer_test.cc`: complete timer state and duration arithmetic.

Expression:

- `tests/expression/test_support.h`: minimal independent test harness; and
- `tests/expression/expression_test.cc`: external adapter, shape, scalar,
  capture lifetime, alias, sparsity, effects, and allocation checks.

Random:

- `tests/random/test_support.h`: minimal independent test harness;
- `tests/random/engine_test.cc`: independently derived block and position
  vectors plus checked offset advance; and
- `tests/random/distribution_test.cc`: exact float and double object bits.

Compile and architecture:

- `tests/compile/m2_dependency_check.cmake`;
- `tests/compile/m2_exceptions_disabled.cc`;
- `tests/compile/m2_expression_compile_contracts.cc`;
- `tests/compile/m2_expression_operand.h`;
- `tests/compile/m2_expression_multi_tu_a.cc`;
- `tests/compile/m2_expression_multi_tu_b.cc`;
- `tests/compile/m2_expression_multi_tu_main.cc`;
- `tests/compile/m2_expression_negative_no_adapter.cc`; and
- `tests/compile/m2_expression_negative_wrong_rank.cc`.

Isolated consumers:

- `tests/consumer/utilities/main.cc`;
- `tests/consumer/expression/main.cc`; and
- `tests/consumer/random/main.cc`.

Review records:

- `docs/development/asc-cpp-m2-independent-foundations/verification-design.md`;
  and
- `docs/development/asc-cpp-m2-independent-foundations/verification-review.md`.

All CMake registration, package fixtures, target-edge assertions, and shared
build files were integrated by the lead, not by the verification role.

## Semantic coverage

### Utilities

The parser suite falsifies duplicate long names, short names, and destinations;
invalid ASCII and dash-prefixed names; invalid short names; missing and
unsupported schema destinations; all approved long, short, bool, negation, and
value forms; negative numeric value tokens; signed and unsigned 64-bit
boundaries; complete-token and overflow failures; non-coercion; UTF-8;
unknowns; clusters; attached short values; `--`; positional order; duplicate
destination spellings; recursive schema validation; default and command-line
origins with exact token indices; sensitive-value redaction; rollback and
repeat parsing; owned option metadata; and deterministic caller-owned help.

The timer suite covers empty, running, and stopped states; every valid and
invalid transition; reset from every state; restart after stop; sample count;
last, average, elapsed, and accumulated durations; and non-negative monotonic
intervals without a flaky sleep threshold.

### Expression

The suite specializes `ExpressionAdapter` for external non-ASC vector and
matrix types. It checks concept rejection for absent and incomplete adapters,
expected compile failures, exact full-shape and rank rejection, scalar
expansion on both sides, two-scalar rank zero, all four node operations, result
value type, lvalue reference capture, arithmetic capture by value, rvalue
ownership, rvalue view capture without storage ownership, nested temporary
ownership, stored and moved node lifetime, alias propagation, operation
categories, and every applicable sparsity rule.

Construction-side probes show zero allocation, scalar read, alias query, and
destination mutation. Multi-translation-unit instantiation passes.

### Random

Nine independently derived ten-round block vectors cover zero, all-one,
asymmetric, every counter lane, and both key lanes. Additional vectors cover
stream/key and subsequence/counter little-word mapping, direct blocks, offset
lanes zero through three, the block boundary at offsets three and four, and
`UINT64_MAX`.

Checked offset advance covers zero, the last representable value, exact arrival
at the maximum, and overflow without wrapping. Float and double transforms are
compared by exact object bits for zero, all-one endpoints, half, asymmetric
inputs, ignored low bits, and high/low word order.

### Compile, architecture, and package

All ten new public headers compile alone under C++20 and, with GCC, with
exceptions disabled. The aggregate M2 surface also compiles and runs with
exceptions disabled. Two negative expression files are required to fail
compilation. Mechanical audits enforce the exact header/source inventory,
sibling and provider isolation, public exception policy, include guards,
internal namespace policy, and absence of unapproved standard random engines
or distributions.

Lead-owned configure assertions verify:

```text
ASC::utilities  -> ASC::core
ASC::expression -> ASC::core  (interface target)
ASC::random     -> ASC::core
```

Each component configures, builds, installs, relocates through a path containing
spaces, and runs independently with forbidden siblings absent. The cumulative
subproject and package component cases also pass.

## Findings and resolutions

1. Two initial verification files had test-only C++ construction errors: a
   mixed-type initializer list and ambiguous nested braces. They were corrected
   without changing the expected semantics, then compiled under GCC and Clang.
2. The lead's first architecture fixture still expected only Milestone 1
   capabilities to be runtime-tested. It was corrected to accept implemented
   milestones through Milestone 2.
3. The lead's core-only optional-component fixture probed newly available
   `utilities`. It was corrected to probe still-unavailable `dense`.
4. Independent Release/shared validation found that a relocated random
   consumer loaded `libasc_random.so` but could not locate its transitive
   `libasc_core.so`. ELF inspection showed executable `RUNPATH` is not inherited
   for the shared library's transitive dependency. The lead added
   origin-relative install RPATH to the compiled M2 libraries: `$ORIGIN` on ELF
   and `@loader_path` on Apple. Both compiled-component relocation cases passed
   in a focused rerun, and the complete Release/shared suite then passed.

The first three findings were verification or integration-fixture corrections.
The fourth was a genuine shared-library relocation defect and is resolved by
validated build integration.

## Exact local validation

The evidence root is:

```text
/tmp/asc-cpp-m2-verification.bfDvue
```

The environment is:

- CMake 4.1.2;
- GCC/G++ 11.4.0;
- Clang 19;
- GNU Make 4.3;
- clang-format 19.0.0;
- ASCCMake 0.1.0 at
  `8a7dcbad3a97267cce59810aff24de800a3497a7`; and
- WSL2 Linux 6.18.33.2 on x86-64.

### Debug static with warnings as errors

```sh
M2_EVIDENCE=/tmp/asc-cpp-m2-verification.bfDvue
M2_ASCCMAKE="$PWD/../asc-cmake/build/prefix/share/ASCCMake"
cmake -S "$PWD" -B "$M2_EVIDENCE/debug-static" \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$M2_ASCCMAKE" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build "$M2_EVIDENCE/debug-static" --parallel 4
ctest --test-dir "$M2_EVIDENCE/debug-static" \
  -C Debug --output-on-failure
```

Result: pass, 79 of 79 tests; zero failures and zero skips.

### Release shared with warnings as errors

```sh
cmake -S "$PWD" -B "$M2_EVIDENCE/release-shared" \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$M2_ASCCMAKE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build "$M2_EVIDENCE/release-shared" --parallel 4
ctest --test-dir "$M2_EVIDENCE/release-shared" \
  -C Release --output-on-failure
```

Initial result: fail, 78 of 79 tests. The sole failure was
`asc_cpp.consumer.random.install_relocate`, exit 127, because the dynamic loader
could not find transitive `libasc_core.so`.

After the lead-owned RPATH correction:

```sh
ctest --test-dir "$M2_EVIDENCE/release-shared" \
  -C Release --output-on-failure \
  -R '^asc_cpp\.consumer\.(utilities|random)\.install_relocate$'
ctest --test-dir "$M2_EVIDENCE/release-shared" \
  -C Release --output-on-failure
```

Results: focused relocation pass, 2 of 2; complete Release/shared pass,
79 of 79; zero failures and zero skips.

### Address and undefined behavior sanitizers

```sh
cmake -S "$PWD" -B "$M2_EVIDENCE/asan-ubsan" \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$M2_ASCCMAKE" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build "$M2_EVIDENCE/asan-ubsan" --parallel 4
ctest --test-dir "$M2_EVIDENCE/asan-ubsan" \
  -C Debug --output-on-failure \
  -R '^(asc_cpp\.(architecture|compile|core|utilities|expression|random))'
```

Result: pass, 67 of 67 focused tests; zero failures and zero skips. ASCCMake's
address-plus-undefined compile and link probes also passed. Package and
isolated-consumer behavior was intentionally covered by both uninstrumented
full configurations above.

ThreadSanitizer and standalone LeakSanitizer were not run in this independent
wave.

### Independent Clang 19 focused pass

The following command shape was applied to each of the five M2 runtime tests,
two expression compile executables, aggregate exception-disabled executable,
and three isolated consumer mains:

```sh
clang++-19 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -I<test-directory> <test-sources> \
  <narrow-module-static-library> \
  "$M2_EVIDENCE/product/src/core/libasc_core.a" \
  -o "$M2_EVIDENCE/<test-name>"
"$M2_EVIDENCE/<test-name>"
```

The exception-disabled case additionally used `-fno-exceptions`.

Result: pass, 11 of 11 compiled and ran with zero failures.

### Negative compile, dependency, formatting, and whitespace

Both negative sources were compiled with strict C++20 and required to fail:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/compile \
  tests/compile/m2_expression_negative_no_adapter.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/compile \
  tests/compile/m2_expression_negative_wrong_rank.cc
```

Result: pass, 2 of 2 were rejected for the intended missing-adapter and
wrong-rank constraints. Lead-owned CMake `try_compile` enforces the same result
at configure time.

```sh
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m2_dependency_check.cmake
clang-format-19 --dry-run --Werror <all-verification-cc-and-h-files>
rg -n '[[:blank:]]+$' <all-verification-files>
git diff --check -- <verification-write-scope>
```

Result: pass. The whitespace search returned no matches.

## Provider and performance evidence

The provider-free utilities, expression, and random CPU paths are
**runtime-tested** on the named local x86-64 host in Debug/static,
Release/shared, and ASan+UBSan configurations.

GPU evidence is **skipped**. Milestone 2 contains no provider target or GPU
implementation, and no later milestone was inferred.

No dedicated throughput or latency threshold applies. The applicable
performance sanity evidence is:

- expression construction performed zero allocations, reads, alias queries,
  transfers, dispatches, and destination mutations;
- timer tests used duration arithmetic without floating conversion or flaky
  wall-time thresholds; and
- Philox and transforms were tested as direct deterministic scalar operations;
  no mutable engine or hidden entropy API is exposed.

## Remaining risks

- MSVC, AppleClang, native Windows, macOS, non-x86 hosts, and multi-config
  generators remain hosted-CI evidence.
- The minimum supported CMake 3.25 executable was unavailable locally.
- ThreadSanitizer and standalone LeakSanitizer remain unexecuted in this
  independent wave.
- A non-C locale with decimal comma was unavailable locally. Tests reject
  comma-form numeric tokens and require full-token conversion, but a positive
  parse under such a locale remains hosted evidence.
- The Apple `@loader_path` branch of the accepted relocation fix is not
  runtime-tested locally.
- GPU evidence remains **skipped** by contract.

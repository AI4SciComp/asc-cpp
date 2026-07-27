# Milestone 2 independent verification review

Status: Complete; no independent-verification release blocker remains

Date: 2026-07-26

## Independence and inspected scope

`verification-design.md`, including its independently derived Philox
expectations, was frozen before the verifier inspected any Milestone 2
production implementation. API-specific tests were then written from the
public headers. Production sources were inspected only after the corresponding
contract-first tests existed.

The verifier did not inspect MdeCpp, deleted asc-cpp random source/tests,
Random123 implementation/tests, or an upstream vector corpus. The approved
SC11 paper and frozen project mapping are the only random algorithm inputs.

## Verification-owned files

```text
tests/utilities/command_line_test.cc
tests/utilities/test_support.h
tests/utilities/timer_test.cc
tests/expression/CMakeLists.txt
tests/expression/expect_compile_failure.cmake
tests/expression/expression_test.cc
tests/expression/test_support.h
tests/random/distribution_test.cc
tests/random/engine_test.cc
tests/random/reference_philox.h
tests/random/test_support.h
tests/compile/m2_expression_adapter_contract.cc
tests/compile/m2_expression_multi_tu*
tests/compile/m2_expression_negative_*
tests/compile/m2_header_self_containment.cc
tests/compile/m2_header_self_containment_no_exceptions.cc
tests/consumer/expression/
tests/consumer/utilities/
tests/consumer/random/
```

The lead owns cross-directory registration and integrated build/package
commands. The expression-local CMake file registers its runtime, adapter,
multi-translation-unit, and expected compile-failure tests.

## Direct ad hoc verification results

All commands ran from `/home/yicai/AI4SciComp/asc-cpp` with GCC/G++ 11.4.0.

### Random syntax

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/random -fsyntax-only \
  tests/random/engine_test.cc tests/random/distribution_test.cc
```

Result: pass.

### Random independent executables

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/random \
  tests/random/engine_test.cc src/random/engine.cc \
  src/core/status.cc src/core/contracts.cc -o <temp>/engine_test
<temp>/engine_test

g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/random \
  tests/random/distribution_test.cc src/random/distribution.cc \
  -o <temp>/distribution_test
<temp>/distribution_test
```

Result: pass, two executables. The engine cases cover ten independent fixed
vectors, a separately coded test-local round oracle, all counter/key lanes,
stream/subsequence/block mapping, offsets and lane boundaries, and checked
offset overflow. The distribution cases compare exact bit patterns and
endpoint behavior for the frozen 24-bit and 53-bit transforms.

### Utilities independent executables

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/utilities \
  tests/utilities/command_line_test.cc \
  src/utilities/command_line.cc src/core/configuration.cc \
  src/core/status.cc src/core/contracts.cc \
  -o <temp>/command_line_test
<temp>/command_line_test

g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Itests/utilities \
  tests/utilities/timer_test.cc src/utilities/timer.cc \
  src/core/status.cc src/core/contracts.cc -o <temp>/timer_test
<temp>/timer_test
```

Result after the resolved finding below: pass, two executables.

Command-line coverage includes option-table validation, duplicates and aliases,
all approved scalar types, checked numeric syntax and bounds, negative numeric
values, all boolean spellings, unknown and malformed options, `--`,
positionals, UTF-8, JSON Pointer paths, origins, schema validation, redaction,
rollback, response-file non-expansion, and deterministic help. Timer coverage
includes empty/running/stopped/reset state transitions, non-negative monotonic
intervals, accounting, average arithmetic, and invalid-state non-mutation
without brittle real-time thresholds.

Diagnostics assert error codes plus the presence, without relying on complete
wording, of argv token index, exact option spelling, expected type, received
non-sensitive value, and bounds context. Sensitive conversion and schema-bound
failures require `<redacted>` and prove the value absent. Malformed sensitive
short, long, and negated-long attached forms are also non-leaking.

### Expression independent tests

Strict GCC and Clang commands compiled and ran:

```text
tests/expression/expression_test.cc
tests/compile/m2_expression_adapter_contract.cc
tests/compile/m2_expression_multi_tu_{a,b,main}.cc
```

Result: pass. Five `m2_expression_negative_*.cc` sources independently fail
compilation as required: direct unary construction, direct binary
construction, unsupported operation, reference holder from an rvalue, and
unadapted operand.

Coverage includes an external non-ASC type, exact shape and rejected
broadcast/rank/shape mismatches, rank zero, zero extent, scalar expansion,
pointwise reads, alias/sparsity/category metadata, no read during construction,
lvalue reference capture, rvalue value capture, nested temporary lifetime,
move-poisoned capture, malformed adapter rejection, and multi-TU instantiation.

### Clang independent executables

The utility, expression, random, adapter-contract, and multi-TU commands were
repeated with `/usr/lib/llvm-19/bin/clang++` 19.1.7 and the same strict C++20
warning flags.

Result: pass.

### Consumer source syntax

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only \
  tests/consumer/utilities/main.cc \
  tests/consumer/expression/main.cc \
  tests/consumer/random/main.cc
```

Result: pass.

### Fresh integrated GCC validation

```text
cmake -S . -B /tmp/asc-m2-verification-final.1EA4Tz \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASC_CPP_BUILD_TESTING=ON -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-m2-verification-final.1EA4Tz -j2
ctest --test-dir /tmp/asc-m2-verification-final.1EA4Tz \
  --output-on-failure -L 'milestone-2|consumer'
```

Result: pass, 47/47 tests, zero failures, 274.99 seconds. This fresh run
includes three architecture audits, every Milestone 2 header alone and with
exceptions disabled, all module runtime/contract/multi-TU/negative tests, all
nine core/utilities/expression/random build-tree/install-relocate/subproject
consumer fixtures, comprehensive build-tree components, installed relocation,
component closure/failure behavior, and package-registry non-mutation.

Formatting:

```text
/usr/lib/llvm-19/bin/clang-format --dry-run -Werror <all verification C++>
```

Result: pass after formatting the verification-owned C++ files.

## Findings and resolutions

### V-M2-001: Timer average did not compile with GCC 11

Severity: release-blocking

Initial evidence:

```text
src/utilities/timer.cc:74:17: error: could not convert
'std::chrono::operator/(total_, sample_count_)' from
'duration<long unsigned int, ...>' to
'Result<duration<long int, ...>>'
```

Cause: dividing a signed-representation `steady_clock::duration` by
`std::size_t` selected an unsigned common representation.

Resolution: production now range-checks `sample_count_` against
`Duration::rep` and converts it explicitly before duration division.

Verification: the strict GCC direct compile and timer executable above pass.
No test workaround was added.

### V-M2-002: parser diagnostics omit actionable option context

Severity: release-blocking contract compliance

Evidence:

```text
unknown.status().message().find("--mystery") == npos
invalid_number.status().message().find("--count") == npos
invalid_number.status().message().find("not-a-number") == npos
```

The parser reports a generic unknown-long-option message and a generic signed
integer conversion message. The utilities contract requires parser errors to
identify their source/location and option, and to identify a received
non-sensitive value and accepted alternatives while redacting sensitive
values. Accepted type text and sensitive-value redaction already pass.

Requested resolution: retain the argument index and exact option spelling in
the diagnostic path, include the received token only for non-sensitive
destinations, and preserve redaction for sensitive destinations.

Resolution: diagnostics now include argv index, exact option spelling,
expected type, received non-sensitive input, conversion or schema-bound
context, and error code. Sensitive values use `<redacted>`. Attached malformed
sensitive spellings `-sVALUE`, `-s=VALUE`, `--secretVALUE`, and
`--no-secretVALUE` do not reveal the suffix.

Verification: strict GCC and Clang command-line executables and the integrated
test pass.

### V-M2-003 through V-M2-006: expression constraint and lifetime defects

Severity: release-blocking

Findings:

- the initial concept accepted an adapter whose `Read` result contradicted
  `value_type`;
- it accepted a non-`rank_t` rank and `void` value type;
- `MakeNegate` could query a capture after moving it because constructor
  argument order was unspecified;
- public reference/node constructors permitted rvalue reference capture or
  bypassed validated factories.

Resolution: adapter header types and reads are constrained exactly, value type
must be an object, shape is captured before move, rvalue reference-holder
construction is deleted, and node constructors are private to the internal
factory.

Verification: adapter-contract compile passes, move-poisoned runtime tests pass,
and the five negative sources fail compilation under the expected-failure
harness.

### V-M2-007: malformed attached sensitive long value could leak

Severity: release-blocking security

Resolution: unknown long-option rendering detects known sensitive and negated
sensitive prefixes and replaces the suffix with `<redacted>`.

Verification: direct GCC/Clang tests and the fresh integrated test pass for
short, long, and negated-long malformed spellings.

## Remaining evidence and risks

- Lead-owned Release, shared-library, ASan/UBSan, and complete all-label CTest
  runs are not duplicated in this report.
- Hosted MSVC and AppleClang remain CI evidence rather than a local claim.
- The verifier observed no remaining release-blocking API, correctness,
  lifetime, dependency, package, or provenance defect in the integrated scope.

GPU evidence is exactly `skipped`: Milestone 2 has no provider target or GPU
implementation.

# Milestone 2 documentation and API review

Status: Complete at Publication Checkpoint B

## Authority and inspected evidence

The reviewer read the complete all-in-one runbook, frozen Milestone 2
contract, ownership ledger, random provenance record, and approved ADRs 0001,
0002, 0003, 0004, 0005, 0010, 0015, 0017, and 0018 before editing.

The retained Milestone 0 and Milestone 1 documentation diff was inspected
before replacement. Historical five-component text remains audit evidence
only; this review does not restore deleted legacy files or document deleted
APIs as current.

The review covers only the public Milestone 2 headers and installed API claims:

```text
include/asc/utilities.h
include/asc/utilities/command_line.h
include/asc/utilities/export.h
include/asc/utilities/timer.h
include/asc/expression.h
include/asc/expression/expression.h
include/asc/random.h
include/asc/random/distribution.h
include/asc/random/engine.h
include/asc/random/export.h
```

Production source, tests, package logic, and validation evidence are
read-only. Findings are reported rather than hidden by documentation wording.

## Review checklist

- [x] Exact public names and signatures match the frozen contract.
- [x] C++20 headers are self-contained, directly included, guarded, and use
      flat `namespace asc`.
- [x] Utilities depends only on core and preserves the parser/configuration
      boundary.
- [x] Expression is storage/provider neutral and has safe lvalue/rvalue
      capture.
- [x] Random base depends only on core, exposes only explicit state, and stays
      within the clean-room provenance boundary.
- [x] Ownership, lifetime, failure, cost, mutation, concurrency, and evidence
      limits are documented.
- [x] Installed public examples compile against only the documented component.
- [x] No provider/GPU or later-milestone claim appears.

## Findings

| ID | Severity | Evidence | Required resolution | Status |
| --- | --- | --- | --- | --- |
| DOC-API-01 | release-blocking portability/style | `src/utilities/command_line.cc` called `std::max` without directly including `<algorithm>` | add the direct standard header and recompile warnings-as-errors | resolved; direct include present |
| DOC-API-02 | release-blocking diagnostic contract | command-line parse failures used generic text that omitted argv source/token location, option context, and accepted value type | add stable contextual diagnostics while never printing a sensitive received value; tests assert error code and context presence rather than exact prose | resolved; conversion and leaf-schema validation include context and configured ranges while sensitive values are redacted |
| DOC-API-03 | medium reproducibility portability | exact `Uniform01` 24/53-bit claims assumed native binary `float`/`double` representations without rejecting incompatible targets | guard the required representation at compile time | resolved; IEC 60559 radix-two 24/53-bit compile-time guards added and documented |
| DOC-API-04 | release-blocking external-participation correctness | strict GCC instantiation of `MakeAdd(External&, External&)` failed because `NormalizedExpression` formed constrained `ScalarExpression<External>` in a nonselected branch | select scalar normalization without forming the scalar template for non-arithmetic `T` | resolved; specialized normalization and strict GCC/Clang external repro pass |
| DOC-API-05 | release-blocking ranked-read correctness | ranked binary scalar read was ambiguous between two `ReadOperand` overloads | retain one result-rank-aware overload | resolved; strict GCC/Clang ranked read compiles and executes |
| DOC-API-06 | high lifetime/portability correctness | `MakeNegate` queried `captured.get()` in the same constructor call that moved `captured` | store shape before moving capture | resolved; move-sensitive GCC/Clang repro executes with preserved shape |
| DOC-API-07 | release-blocking protocol correctness | `ReadableExpression` accepted an adapter whose declared `value_type` was `double` while `Read` returned `std::string` | constrain adapter `Read` to return the declared scalar value type | resolved; mismatched-read negative compile passes |
| DOC-API-08 | high constraint/API diagnostics | factories accepted scalar value types that did not support the requested operation | constrain factories to valid normalized scalar operations | resolved; unary/binary invalid-operation negative compile passes |
| DOC-API-09 | high numerical contract documentation | pointwise docs did not state ordinary C++ promotion or signed-overflow behavior | document factory-only nodes, ordinary operator/`decltype` promotion, representable signed-result precondition, and native floating behavior | resolved in module/API documentation |
| DOC-API-10 | high borrowed-shape lifetime documentation | lvalue lifetime text omitted that nodes snapshot shape and later reads require the operand's shape/index contract to remain compatible | document reshape/resize/index-contract invalidation separately from object lifetime | resolved in module documentation |
| DOC-API-11 | medium same-instance concurrency documentation | utilities docs stated timer concurrency but not immutable parser concurrency | document concurrent const `Parse`/`RenderHelp` and caller synchronization for assignment/destruction | resolved in module documentation |
| DOC-API-12 | high protocol correctness | `ReadableExpression` accepted `value_type=void` with a void `Read`, which is not a scalar value protocol | require adapter `value_type` to be a non-void object type | resolved; object-type constraint and strict negative compile pass |
| DOC-API-13 | medium value-copy cost documentation | implicit parser/timer/node copies had undocumented ownership, allocation, and running-timer behavior | document deep parser copies, timer state/start snapshots, pointer-only lvalue holders, and potentially allocating value-held operand copies | resolved in utilities/expression documentation |
| DOC-API-14 | release-blocking component packaging | six isolated utilities/expression/random consumers failed because loading the required transitive core export did not set `ASCCpp_core_FOUND` | set each loaded required component's package `_FOUND` value and rerun all affected build-tree and relocated-install consumers | resolved; affected consumer retest passes 6/6 |
| DOC-API-15 | medium documentation correctness | the command-line example called `ConfigurationSchema::SetRequired()` without its required Boolean argument | use `SetRequired(true)` and compile the exact corrected example against the installed utilities component | resolved; corrected example builds and runs |
| DOC-API-16 | medium cost-model documentation | timer/random prose broadly claimed no allocation despite possible core status diagnostic storage, and adapter shape-query cost was stated as rank-linear despite being external-type-defined | distinguish pure fixed-size operations from error diagnostic storage and make adapter/parser cost language conservative | resolved in utilities/expression/random documentation |
| DOC-API-17 | release-blocking opacity/API contract | `AliasToken` was documented as opaque while exposing a public pointer field and aggregate construction | make representation and constructor private, provide `AliasToken::FromIdentity(const void*)`, preserve equality, and update adapters/examples | resolved; public header and documented external adapter use the opaque factory |
| DOC-API-18 | high configuration construction correctness | parser creation accepted a non-object root, leaving the empty-option configuration shape ambiguous | require an object root at `Create` and reject every other root with `kInvalidArgument` | resolved; implementation, test, API map, and utilities guide agree |

No documentation workaround is accepted for these items.

## Documentation changes

- `README.md` now states the exact four available Milestone 2 components,
  direct dependencies, ASCCMake binding, configure/install/consumer paths,
  unavailable components, and evidence limits.
- `CHANGELOG.md` now separates the unreleased Milestone 2 candidate from
  earlier milestone and release records.
- `docs/README.md` now distinguishes the current API/module guides from
  retained superseded audit evidence.
- `docs/api.md` maps only the implemented four-component API and records the
  additive core origin surface used by utilities.
- `docs/modules/utilities.md` documents parser declaration, syntax,
  object-root precondition, transaction, origin, configured-range diagnostics,
  redaction, UTF-8 boundary, timer states, ownership, copy cost, concurrency,
  and deferred scope.
- `docs/modules/expression.md` documents the complete external-adapter
  protocol, operation constraints and promotion, capture/lifetime behavior,
  snapshotted shapes, numerical preconditions, opaque alias construction,
  sparsity metadata, cost, concurrency, and evaluator boundary.
- `docs/modules/random.md` documents the exact Philox round, lane/key/address
  mapping, unit-uniform bit transforms, explicit state, representation guard,
  sequence boundary, cost, concurrency, and clean-room provenance.

All public examples use only the documented installed target. No document
claims dense/sparse storage, evaluation, a random storage facet, an optional
provider, or GPU execution.

## Example and documentation validation

A fresh warnings-as-errors Debug configuration used the completed ASCCMake
build-tree package:

```text
cmake -S . -B /tmp/asc-cpp-m2-doc-review.SXhaiW/build \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m2-doc-review.SXhaiW/prefix
cmake --build /tmp/asc-cpp-m2-doc-review.SXhaiW/build -j2
ctest --test-dir /tmp/asc-cpp-m2-doc-review.SXhaiW/build --output-on-failure
cmake --install /tmp/asc-cpp-m2-doc-review.SXhaiW/build
```

Configure and build passed. The first full test run passed 80/86 and exposed
DOC-API-14 in exactly the six isolated utilities/expression/random build-tree
and relocated-install consumers. After the package fix, the affected closure
was reconfigured and retested with:

```text
cmake -S . -B /tmp/asc-cpp-m2-doc-review.SXhaiW/build \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m2-doc-review.SXhaiW/prefix
cmake --build /tmp/asc-cpp-m2-doc-review.SXhaiW/build -j2
ctest --test-dir /tmp/asc-cpp-m2-doc-review.SXhaiW/build --output-on-failure \
  -R 'asc_cpp\.consumer\.(utilities|expression|random)\.(build_tree|install_relocate)'
cmake --install /tmp/asc-cpp-m2-doc-review.SXhaiW/build
```

Result: pass, 6/6; installation passed. The lead validation record owns the
final clean whole-suite rerun rather than treating this targeted correction
run as a whole-suite result.

After the integrated opaque-alias and object-root corrections, the reviewer
rebuilt and exercised the complete utilities/expression runtime, compile,
negative-compile, and affected isolated-consumer closure:

```text
cmake -S . -B /tmp/asc-cpp-m2-doc-review.SXhaiW/build \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m2-doc-review.SXhaiW/prefix
cmake --build /tmp/asc-cpp-m2-doc-review.SXhaiW/build -j2
ctest --test-dir /tmp/asc-cpp-m2-doc-review.SXhaiW/build --output-on-failure \
  -R 'asc_cpp\.(utilities|expression|consumer\.(utilities|expression)\.(build_tree|install_relocate))'
cmake --install /tmp/asc-cpp-m2-doc-review.SXhaiW/build
```

Result: pass, 14/14; installation passed. The installed examples and strict
GCC/Clang expression repros below were then rerun against the corrected API
and all passed.

The utilities command-line, utilities timer, external expression adapter, and
random examples were copied exactly from the module guides. Each consumer
called `find_package(ASCCpp 0.2 CONFIG REQUIRED COMPONENTS <one-component>)`,
linked only its named target, and used the installed prefix:

```text
cmake -S /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities \
  -B /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities-build \
  -DCMAKE_PREFIX_PATH=/tmp/asc-cpp-m2-doc-review.SXhaiW/prefix
cmake --build /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities-build -j2
/tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities-build/utilities_command_line
/tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities-build/utilities_timer

cmake -S /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-expression \
  -B /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-expression-build \
  -DCMAKE_PREFIX_PATH=/tmp/asc-cpp-m2-doc-review.SXhaiW/prefix
cmake --build /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-expression-build -j2
/tmp/asc-cpp-m2-doc-review.SXhaiW/doc-expression-build/expression_adapter

cmake -S /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-random \
  -B /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-random-build \
  -DCMAKE_PREFIX_PATH=/tmp/asc-cpp-m2-doc-review.SXhaiW/prefix
cmake --build /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-random-build -j2
/tmp/asc-cpp-m2-doc-review.SXhaiW/doc-random-build/random_philox
/usr/lib/llvm-19/bin/clang-format --dry-run --Werror \
  -style=file:/home/yicai/AI4SciComp/asc-cpp/.clang-format \
  /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities/command_line.cc \
  /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-utilities/timer.cc \
  /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-expression/expression.cc \
  /tmp/asc-cpp-m2-doc-review.SXhaiW/doc-random/random.cc
```

Result: configure/build/runtime and repository-format checks pass for all four
examples.

Strict GCC 11 and Clang 19 syntax-only checks pass for external ranked
participation, move-sensitive capture, invalid operation constraints, direct
rvalue-reference rejection, and invalid adapter definitions. External ranked
read and move-sensitive capture also compile and run with both compilers:

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -fsyntax-only /tmp/asc_m2_expression_review.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -fsyntax-only /tmp/asc_m2_expression_move_review.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -fsyntax-only /tmp/asc_m2_expression_negative_review.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -fsyntax-only /tmp/asc_m2_expression_dangling_review.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -fsyntax-only /tmp/asc_m2_expression_adapter_negative_review.cc
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only /tmp/asc_m2_expression_review.cc
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only /tmp/asc_m2_expression_move_review.cc
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only /tmp/asc_m2_expression_negative_review.cc
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only /tmp/asc_m2_expression_dangling_review.cc
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only /tmp/asc_m2_expression_adapter_negative_review.cc
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  /tmp/asc_m2_expression_review.cc src/core/status.cc \
  src/core/contracts.cc -o /tmp/asc_m2_expression_review
/tmp/asc_m2_expression_review
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  /tmp/asc_m2_expression_move_review.cc src/core/status.cc \
  src/core/contracts.cc -o /tmp/asc_m2_expression_move_review
/tmp/asc_m2_expression_move_review
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude /tmp/asc_m2_expression_review.cc src/core/status.cc \
  src/core/contracts.cc -o /tmp/asc_m2_expression_review_clang
/tmp/asc_m2_expression_review_clang
/usr/lib/llvm-19/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude /tmp/asc_m2_expression_move_review.cc src/core/status.cc \
  src/core/contracts.cc -o /tmp/asc_m2_expression_move_review_clang
/tmp/asc_m2_expression_move_review_clang
```

Result: all pass with no diagnostic.

GPU/provider evidence is exactly `skipped`: Milestone 2 has no GPU/provider
target, source, SDK type, configure path, or runtime claim. No benchmark is
applicable to this foundation milestone; the public-operation costs are
documented without a performance claim.

There is no unresolved release-blocking documentation/API finding. Remaining
publication risk is hosted MSVC/AppleClang evidence that cannot exist before
branch publication. Lead integration evidence reports the closing local GCC
Debug build and whole-suite result at 82/82, Clang ASan/UBSan at 70/70, and,
after the final mechanical `std::to_chars` pointer-bound correction, affected
GCC utilities/consumer checks at 4/4 and Clang sanitizer utilities checks at
2/2. A non-IEC-60559 or non-radix-two native floating representation is
deliberately compile-rejected because it cannot satisfy the exact `Uniform01`
sequence contract.

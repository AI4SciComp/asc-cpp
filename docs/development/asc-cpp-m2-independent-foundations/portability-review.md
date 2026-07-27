# Milestone 2 portability, GPU, and performance review

Status: Independent review complete; no open local portability blocker

## Authority and review boundary

This review follows the complete all-in-one runbook, frozen Milestone 2
contract, ownership ledger, provenance record, and approved ADRs 0001, 0002,
0003, 0004, 0005, 0007, 0010, 0015, 0017, and 0018.

The reviewer may write only this report. Production, tests, build/package
logic, CI, and documentation are read-only. Accepted corrections are returned
to the owning engineer or lead. The retained cumulative Milestone 0 and
Milestone 1 diff is not reset or rewritten.

The review covers:

- GCC, Clang, MSVC, and AppleClang source and public-header portability;
- static/shared and interface-target build/package behavior;
- exception-disabled headers and implementation;
- unsigned Philox arithmetic and exact floating transforms;
- monotonic timer arithmetic and parser/path behavior;
- allocation, object-size, compile-time, and throughput risks;
- provider isolation and honest GPU evidence; and
- C++20 and current repository/Google style.

## Findings

| ID | Severity | Evidence | Required resolution | Status |
| --- | --- | --- | --- | --- |
| PORT-01 | release-blocking style | Clang-format 19 reports violations in the initial integrated utilities/random production set | format the final integrated production and test set and rerun `--dry-run --Werror` | resolved; final `include`, `src`, and `tests` C++ set passes |
| PORT-02 | medium reproducibility portability | `src/random/distribution.cc` initially assumed radix-2 `float`/`double` with 24/53 precision while the public exact-sequence claim did not reject other conforming representations | add compile-time representation guards sufficient for the exact mapping and compile on hosted compilers | resolved with IEC 60559/radix/precision guards, docs, and local GCC/Clang builds |
| PORT-03 | low documentation portability | `CommandLineParser::Parse` accepts supplied byte strings but neither performs nor can infer Windows native `wchar_t`/active-code-page to UTF-8 conversion | document that callers own native Windows argument conversion and that parser indices address the supplied token span | resolved in module documentation |
| PORT-04 | release-blocking lifetime | `ExpressionReference(const Expression&)` initially accepted a temporary when the holder was constructed directly, retaining a dangling pointer despite the frozen no-reference-to-rvalue guarantee | delete rvalue construction or make holder construction factory-only; add a negative constructibility/compile test | resolved with deleted const/non-const rvalue overloads; GCC/Clang negative tests pass |
| PORT-05 | release-blocking invariant | public `UnaryExpression` and `BinaryExpression` constructors initially accepted arbitrary caller-supplied stored shapes and bypassed factory validation | make unchecked node construction factory-only, or otherwise prevent invalid stored shapes; add negative/API tests | resolved with private constructors and internal factory friendship; GCC/Clang negative tests pass |
| PORT-06 | medium protocol constraint | `HasExpressionAdapter` initially checked only that adapter `Read` was a valid expression, so a `void` or unrelated result satisfied `ReadableExpression` despite the required scalar `value_type` read | constrain `Read` to the approved value contract and add a negative concept test | resolved with exact `value_type` constraint; GCC/Clang adapter-contract tests pass |
| PORT-07 | release-blocking diagnostic security | malformed short and attached-long diagnostics initially quoted an entire token/name before resolving sensitivity, so unsupported `-sSECRET`, `-s=SECRET`, `--secretSECRET`, or `--no-secretSECRET` could echo a value associated with a registered sensitive option | never echo a malformed suffix after a known positive/negative sensitive spelling; add short/long regression tests | resolved for all four forms with integrated regression coverage |
| PORT-08 | low lifetime documentation | expression nodes snapshot the shape of borrowed lvalue operands, but object lifetime alone does not keep that snapshot compatible if the operand is resized or reshaped | document shape/index-contract stability as part of the borrowed operand lifetime precondition | resolved in expression module documentation |
| PORT-09 | release-blocking numerical contract | built-in expression operations use ordinary scalar operators; signed minimum negation and signed add/subtract/multiply overflow are undefined, while promotion/overflow/NaN behavior was undocumented | document ordinary C++ promotion and floating behavior plus a representability precondition for signed results, or narrow the scalar contract | resolved with operation constraints, a negative compile test, and explicit promotion/signed/FP documentation |
| PORT-10 | release-blocking Windows DLL risk | utilities initially marked entire `CommandLineParser` and `Timer` classes for DLL export even though their private layouts contain `ConfigurationSchema`/`std::vector` and `std::chrono` template types; VS `/W4 /WX` can issue C4251 and implicit special-member export is toolset-sensitive | use method-level exports as for STL-owning core APIs, or provide successful VS2022 shared `/WX` evidence | resolved to method-level exports; local Clang shared build/package matrix passes; hosted MSVC remains publication evidence |
| PORT-11 | medium copy-cost contract | implicit parser copies deep-copy schema/options and can allocate; expression-node copies copy every value-held operand; timer copies snapshot a possibly running start point | document supported copy/move semantics and inherited cost/allocation, or delete unintended copy operations | resolved with explicit parser, expression-node, and timer copy/move/cost documentation |
| PORT-12 | release-blocking warning portability | the final diagnostic-byte loop implicitly converted `char` to `unsigned char`, failing GCC `-Wsign-conversion -Werror` | make the byte conversion explicit and rerun strict GCC/Clang syntax checks with exceptions on and off | resolved; all four strict variants pass |

No source defect was found in the reviewed Philox unsigned arithmetic:
32-by-32 products are widened before multiplication, high halves use unsigned
right shift, low halves use defined modulo conversion, XOR is unsigned, and
the `std::uint32_t` Weyl additions wrap modulo 2^32 by definition.

The `Uniform01` implementation selects the intended high 24 or 53 bits. On a
radix-2 representation with the required precision and range, every integer
conversion and power-of-two scale is exact, zero remains positive zero, and
the all-one inputs remain below one. PORT-02 is required because the C++20
standard alone does not guarantee those native floating representations.

The timer uses `std::chrono::steady_clock`; its interval, total, and sample-count
overflow checks occur before mutation. A failed `Stop` retains the running
state. `Average` uses duration division rather than a floating seconds
conversion. The public duration and clock types intentionally follow the host
standard library, so absolute tick periods are not a cross-platform ABI or
serialization promise.

The parser uses locale-independent `std::from_chars`, consumes complete tokens,
and compares option-name bytes without locale-dependent character functions.
Accepted-range diagnostics use `std::to_chars` and never format through the
process locale. Floating `from_chars`/`to_chars` support is present in the local
GCC 11/libstdc++ and Clang 19/libstdc++ configurations; current MSVC and
AppleClang/libc++ remain hosted-CI evidence rather than local claims.

## Local commands and results

The final production-only syntax check used:

```text
g++ -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion
  -Wsign-conversion -Werror [-fno-exceptions] -Iinclude -fsyntax-only
  src/utilities/command_line.cc src/utilities/timer.cc
  src/random/engine.cc src/random/distribution.cc

/usr/lib/llvm-19/bin/clang++ -std=c++20 -pedantic-errors -Wall -Wextra
  -Wconversion -Wsign-conversion -Werror [-fno-exceptions] -Iinclude
  -fsyntax-only src/utilities/command_line.cc src/utilities/timer.cc
  src/random/engine.cc src/random/distribution.cc
```

Results: GCC 11.4 exceptions-on pass; GCC 11.4 `-fno-exceptions` pass;
Clang 19 exceptions-on pass; Clang 19 `-fno-exceptions` pass. The first final
GCC attempt identified PORT-12. The owning engineer made the explicit byte
conversion; the repeated four-command gate then passed.

The corrected expression header and tests were compiled independently with
local GCC and Clang, both with and without exceptions:

```text
<compiler> -std=c++20 -O2 -pedantic-errors -Wall -Wextra -Werror
  [-fno-exceptions] -Iinclude -Itests/expression
  src/core/contracts.cc src/core/status.cc
  tests/expression/expression_test.cc
```

Results: four runtime-test variants pass. The adapter-contract and multi-TU
programs pass under both compilers. Direct binary construction, direct unary
construction, invalid operation, rvalue `ExpressionReference`, and
unadapted-operand negative sources are rejected as expected. The same five
negative sources also pass as negative CTest fixtures in the final GCC static,
Clang shared, and Clang sanitizer builds.

The final formatting command was:

```text
find include src tests -type f \( -name '*.h' -o -name '*.cc' \)
  -print0 | sort -z |
  xargs -0 /usr/lib/llvm-19/bin/clang-format --dry-run --Werror
git diff --check
```

Results: pass after integration and the PORT-12 correction.

The independent clean GCC static gate was:

```text
cmake -S . -B <tmp>/gcc-debug
  -DASCCMake_DIR=<tmp>/asc-cmake
  -DCMAKE_BUILD_TYPE=Debug
  -DASC_CPP_BUILD_TESTING=ON
  -DASC_CPP_INSTALL=ON
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build <tmp>/gcc-debug --parallel 2
ctest --test-dir <tmp>/gcc-debug --output-on-failure
```

Results: configure pass, build pass, and 82/82 CTest pass. This includes all
public-header exception-on/off executables, multi-TU and dependency audits,
five expression negative-compile fixtures, utilities/expression/random unit
tests, nine build-tree/relocated/subproject consumer fixtures, comprehensive
component package fixtures, and package-registry isolation.

The independent Clang sanitizer gate was:

```text
cmake -S . -B <tmp>/clang-asan
  -DASCCMake_DIR=<tmp>/asc-cmake
  -DCMAKE_CXX_COMPILER=/usr/lib/llvm-19/bin/clang++
  -DCMAKE_BUILD_TYPE=Debug
  -DASC_CPP_BUILD_TESTING=ON
  -DASC_CPP_INSTALL=ON
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build <tmp>/clang-asan --parallel 2
ctest --test-dir <tmp>/clang-asan --output-on-failure
  -LE 'package|consumer'
```

Results: sanitizer configure pass, build pass, and 70/70 selected CTest pass.
The package and consumer scripts were intentionally excluded from this
instrumented runtime pass because they launch independent nested builds; they
are covered by the unsanitized static/shared matrices.

The first sanitizer configure attempt requested `-G Ninja` and failed before
project generation because no Ninja executable was installed
(`CMAKE_MAKE_PROGRAM` unavailable). Repeating the same configuration with the
available Unix Makefiles generator produced the passing result above. This is
a local tool-availability result, not a project build failure.

The lead's independent Clang 19 Debug shared, warnings-as-errors matrix
configured and built cleanly and passed 82/82 CTest, including all nine
consumer fixtures and the complete install/relocation/package fixture set.

No MSVC or AppleClang binary is installed locally. Their results must be
reported only after the hosted jobs run; source inspection is not a pass.

## Final post-correction review

A final production self-review produced four bounded corrections after the
main matrix above. This reviewer inspected the corrected current tree:

- `AliasToken` now keeps its pointer representation private and exposes only
  the opaque `FromIdentity` factory and equality. Call sites and documentation
  use the factory; there is no public representation access or lifetime
  extension;
- parser creation now rejects every non-object root before validating option
  destinations, avoiding an ambiguous scalar/list/null configuration result;
- missing, unparseable, and schema-invalid option-value diagnostics now render
  the configured bool/numeric/string accepted range while retaining sensitive
  value redaction; and
- the Philox implementation now includes `<cstddef>` directly for
  `std::size_t`, rather than relying on a transitive include.

The exact strict GCC 11 and Clang 19 production syntax commands shown above
were repeated with exceptions on and off: 4/4 pass. The all-headers/source/test
clang-format 19 gate and `git diff --check` were repeated: pass. Both the GCC
static build and Clang ASan/UBSan build were refreshed, then this focused
selection was run:

```text
ctest --test-dir <build> --output-on-failure
  -R '^asc_cpp\.(compile\.m2_header|utilities|expression|random)'
```

Results: GCC 32/32 pass; Clang ASan/UBSan 32/32 pass. This covers every
Milestone 2 public header with exceptions on/off, utilities tests including
the object-root/range/redaction regressions, expression runtime/adapter/
multi-TU/negative tests, and random engine/distribution tests. No new local
portability, GPU, or performance blocker was found.

## Build/package review

The integrated target design is appropriate:

```text
ASC::utilities  compiled, direct ASC::core
ASC::expression interface, direct ASC::core
ASC::random     compiled, direct ASC::core
```

`ASC::expression` correctly uses standard CMake
`target_compile_features(... INTERFACE cxx_std_20)` because released ASCCMake
0.1.0 rejects interface libraries in its C++20 helper. This is use of a real
standard-CMake capability, not an invented ASCCMake API. The interface target
has no fake compiled object. The final build-tree and installed/relocated
expression consumers assert that it remains an `INTERFACE_LIBRARY` and build
successfully.

Utilities/random visibility macros distinguish static consumers, shared-library
producers, and Windows DLL consumers. Their targets publish the static marker
only for static builds and retain hidden/default visibility on ELF/Mach-O.
Method-level exports avoid exporting private standard-library template
members. Local static and shared matrices, relocation, and isolated-component
consumers pass. Native Windows DLL/import-library and multi-config behavior
remain hosted evidence, not a local pass.

The package config loads `core` before any requested Milestone 2 dependent
export and does not import sibling components. The package scripts exercise
required, optional, unavailable, unknown, and no-component requests, plus
paths with spaces and relocation. Those tests pass in both final local
matrices. Hosted Windows multi-config behavior and AppleClang install behavior
remain unverified until CI.

## Performance and allocation review

- Philox block generation is fixed work: ten rounds, no dynamic allocation,
  I/O, provider dispatch, transfer, or synchronization. Word generation
  regenerates one four-lane block for the selected offset; there is no hidden
  mutable cursor or cache.
- Both uniform transforms are constant time and allocation-free.
- Timer operations are constant time and retain only one start point, total,
  last interval, state, and sample count. There is no sample buffer or hidden
  synchronization.
- Parser creation and parsing allocate owned schema/option/configuration trees
  and ordered lookup structures. Parsing is control-plane work, not a
  numerical kernel. No performance gate or third-party benchmark dependency is
  justified for this milestone.
- Expression lvalue holders store one pointer and rvalue holders store their
  operand by value. Nodes add fixed-rank shape snapshots, retain no result
  storage, and contain no allocator, provider/context handle, virtual dispatch,
  or runtime rank. Factory-only node construction and deleted rvalue-reference
  capture close the reviewed invariant/lifetime bypasses. Template
  parse/instantiation cost is proportional to expression type depth and
  compile-time rank; there is no type erasure or runtime operation registry.

No benchmark is proposed as a required CI gate. The appropriate Milestone 2
performance evidence is structural fixed-work/allocation evidence plus the
standalone optimized expression compile/run above, not a noisy wall-time
threshold. No Milestone 2 throughput or speedup claim is made, so there is no
applicable performance benchmark result to report.

## GPU/provider evidence

GPU evidence: **skipped**.

Reason: Milestone 2 contains no provider target, provider source, vendor header,
GPU implementation, device compile, runtime call, or CPU/GPU parity claim.
Local toolkit or hardware inventory is not provider evidence and does not raise
this classification.

## Remaining hosted-platform risks

- Current MSVC shared/multi-config build and installed import libraries require
  hosted validation.
- Current AppleClang/libc++ floating `from_chars`, shared/static install, and
  relocated consumers require hosted validation.
- The CI checkout of private ASCCMake depends on the configured read token;
  untrusted fork pull requests cannot be assumed to receive that secret.
- Method-level exports substantially reduce the reviewed Windows C4251 risk,
  but do not substitute for the hosted MSVC shared `/W4 /WX` result.
- No GPU/provider implementation exists in this milestone; later provider work
  must establish new configure/compile/runtime/parity evidence rather than
  inheriting this milestone's `skipped` classification.

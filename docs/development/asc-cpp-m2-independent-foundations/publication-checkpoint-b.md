# Milestone 2 Publication Checkpoint B

Status: Local publication candidate; awaiting owner approval

Date: 2026-07-26

Branch: `feature/asc-cpp-m2-independent-foundations`

Baseline commit:
`33b261ea33616a6395c4ad3b20646093103344f7`

No commit, push, pull request, merge, tag, release, registry write, or branch
deletion was performed.

## Retained predecessor state

The branch contains the intentional uncommitted Milestone 0 and Milestone 1
layers approved in earlier checkpoints, including the user's removal of the
old implementation. Milestone 2 was integrated on top without resetting,
restoring, or discarding that work. The unrelated modified MdeCpp `Makefile`
was not touched.

## Milestone 2 production files

```text
include/asc/utilities.h
include/asc/utilities/command_line.h
include/asc/utilities/export.h
include/asc/utilities/timer.h
src/utilities/command_line.cc
src/utilities/timer.cc

include/asc/expression.h
include/asc/expression/expression.h

include/asc/random.h
include/asc/random/distribution.h
include/asc/random/engine.h
include/asc/random/export.h
src/random/distribution.cc
src/random/engine.cc
```

The additive core integration is confined to command-line configuration
origins in:

```text
include/asc/core/configuration.h
src/core/configuration.cc
tests/core/configuration_test.cc
```

## Build, package, and integration files

```text
CMakeLists.txt
CMakePresets.json
.github/workflows/ci.yml
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
src/utilities/CMakeLists.txt
src/expression/CMakeLists.txt
src/random/CMakeLists.txt
tests/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/dependency_check.cmake
tests/consumer/CMakeLists.txt
tests/consumer/run_component_consumer.cmake
tests/consumer/core/CMakeLists.txt
tests/consumer/subproject/CMakeLists.txt
tests/consumer/subproject/main.cc
tests/package/CMakeLists.txt
tests/package/package_test.cmake
tests/package/component_unavailable/CMakeLists.txt
```

These files advance the unreleased package candidate to `0.2.0`, add only the
four approved components/exports, enforce the direct graph mechanically, and
exercise component closure, isolation, build trees, installs, relocation,
paths containing spaces, static/shared modes, subprojects, failure behavior,
and registry non-mutation.

## Verification files

```text
tests/utilities/CMakeLists.txt
tests/utilities/command_line_test.cc
tests/utilities/test_support.h
tests/utilities/timer_test.cc
tests/expression/CMakeLists.txt
tests/expression/expect_compile_failure.cmake
tests/expression/expression_test.cc
tests/expression/test_support.h
tests/random/CMakeLists.txt
tests/random/distribution_test.cc
tests/random/engine_test.cc
tests/random/reference_philox.h
tests/random/test_support.h
tests/compile/m2_expression_adapter_contract.cc
tests/compile/m2_expression_multi_tu.h
tests/compile/m2_expression_multi_tu_a.cc
tests/compile/m2_expression_multi_tu_b.cc
tests/compile/m2_expression_multi_tu_main.cc
tests/compile/m2_expression_negative_direct_binary.cc
tests/compile/m2_expression_negative_direct_unary.cc
tests/compile/m2_expression_negative_operation.cc
tests/compile/m2_expression_negative_reference_rvalue.cc
tests/compile/m2_expression_negative_unadapted.cc
tests/compile/m2_header_self_containment.cc
tests/compile/m2_header_self_containment_no_exceptions.cc
tests/consumer/utilities/CMakeLists.txt
tests/consumer/utilities/main.cc
tests/consumer/expression/CMakeLists.txt
tests/consumer/expression/main.cc
tests/consumer/random/CMakeLists.txt
tests/consumer/random/main.cc
```

## Documentation and evidence files

Current user/API documents changed:

```text
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/core.md
docs/modules/utilities.md
docs/modules/expression.md
docs/modules/random.md
```

Architecture evidence changed:

```text
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/release-roadmap.md
```

Milestone records created:

```text
docs/development/asc-cpp-m2-independent-foundations/milestone-contract.md
docs/development/asc-cpp-m2-independent-foundations/ownership.md
docs/development/asc-cpp-m2-independent-foundations/provenance-record.md
docs/development/asc-cpp-m2-independent-foundations/dependency-audit.md
docs/development/asc-cpp-m2-independent-foundations/verification-design.md
docs/development/asc-cpp-m2-independent-foundations/verification-review.md
docs/development/asc-cpp-m2-independent-foundations/documentation-api-review.md
docs/development/asc-cpp-m2-independent-foundations/portability-review.md
docs/development/asc-cpp-m2-independent-foundations/production-self-review.md
docs/development/asc-cpp-m2-independent-foundations/publication-checkpoint-b.md
```

## API, targets, and dependencies

```text
build target       exported target    kind                 direct ASC dependency
asc_core           ASC::core          static/shared        none
asc_utilities      ASC::utilities     static/shared        ASC::core
asc_expression     ASC::expression    interface             ASC::core
asc_random         ASC::random        static/shared        ASC::core
```

Utilities adds transactional schema-bound command-line parsing, origins,
deterministic help, and a steady-clock timer. Expression adds the external
adapter protocol, opaque alias identity, exact shape/scalar expansion,
safe capture, four validated pointwise factories, and conservative sparsity
metadata without storage or evaluation. Random adds explicit Philox4x32-10
raw-bit addressing, checked offset advance, and exact binary `Uniform01`
transforms.

No external dependency was added. No sibling edge, dense/sparse target,
random storage facet, aggregate, or provider target exists.

## Exact local validation

Minimum supported CMake and optimized static package:

```text
/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/cmake -S . \
  -B /tmp/asc-cpp-m2-validation.ckoFuy/gcc-release-cmake325 \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/tmp/asc-cpp-m2-validation.ckoFuy/asc-cmake-325
/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/cmake --build \
  /tmp/asc-cpp-m2-validation.ckoFuy/gcc-release-cmake325 --parallel 4
/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/ctest --test-dir \
  /tmp/asc-cpp-m2-validation.ckoFuy/gcc-release-cmake325 \
  --output-on-failure --parallel 4
```

Result: pass, CMake 3.25.0, GCC 11.4.0, Release static, 82/82.

Shared-library/compiler matrix:

```text
cmake -S . -B /tmp/asc-cpp-m2-validation.ckoFuy/clang-debug-shared \
  -DCMAKE_CXX_COMPILER=/usr/lib/llvm-19/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/tmp/asc-cpp-m2-validation.ckoFuy/asc-cmake
cmake --build /tmp/asc-cpp-m2-validation.ckoFuy/clang-debug-shared \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m2-validation.ckoFuy/clang-debug-shared \
  --output-on-failure --parallel 4
```

Result: pass, Clang 19.0.0, Debug shared, 82/82.

Final integrated static refresh:

```text
cmake -S . -B /tmp/asc-cpp-m2-validation.ckoFuy/gcc-debug \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/tmp/asc-cpp-m2-validation.ckoFuy/asc-cmake
cmake --build /tmp/asc-cpp-m2-validation.ckoFuy/gcc-debug --parallel 4
ctest --test-dir /tmp/asc-cpp-m2-validation.ckoFuy/gcc-debug \
  --output-on-failure --parallel 4
```

Result: pass, GCC 11.4.0, Debug static, 82/82. A final mechanical
`to_chars` iterator-bound cleanup was then rebuilt; utilities runtime and
build-tree/relocated consumers passed 4/4.

Sanitizers:

```text
cmake -S . -B /tmp/tmp.lz5y4UNcCo/clang-asan-make \
  -DASCCMake_DIR=/tmp/tmp.lz5y4UNcCo/asc-cmake \
  -DCMAKE_CXX_COMPILER=/usr/lib/llvm-19/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build /tmp/tmp.lz5y4UNcCo/clang-asan-make --parallel 4
ctest --test-dir /tmp/tmp.lz5y4UNcCo/clang-asan-make \
  --output-on-failure -LE 'package|consumer' --parallel 4
```

Result: pass, Clang 19 ASan+UBSan, 70/70 eligible tests. Package and consumer
scripts launch separate uninstrumented builds and are covered by the static
and shared matrices. The final mechanical utilities cleanup was rebuilt in
this tree and its two runtime tests passed 2/2.

Strict portability and formatting:

```text
g++ and /usr/lib/llvm-19/bin/clang++ \
  -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror \
  [-fno-exceptions] -Iinclude -fsyntax-only \
  src/utilities/command_line.cc src/utilities/timer.cc \
  src/random/engine.cc src/random/distribution.cc
/usr/lib/llvm-19/bin/clang-format --dry-run --Werror \
  <all include/src/tests .h and .cc files>
git diff --check
```

Result: pass for both compilers with exceptions on and off; formatting and
whitespace checks pass.

Independent verification passed 47/47 contract-focused tests in a fresh GCC
tree. Documentation/API review passed 14/14 corrected utilities/expression
tests, installed and ran all four exact examples, and passed strict GCC/Clang
expression reproducers.

## Provider and performance evidence

CPU evidence is runtime-tested for the provider-free serial base behavior.
Philox CPU behavior is independently vector-tested.

GPU evidence is exactly **skipped**. Milestone 2 has no provider target,
provider source, GPU compilation, runtime call, or CPU/GPU parity claim.
Installed CUDA hardware/toolkit inventory is not Milestone 2 evidence.

No throughput or speedup claim is made and no benchmark gate is applicable.
Structural review establishes fixed ten-round Philox work, constant-time
uniform transforms and timer operations, no expression result allocation, and
documented parser/control-plane allocations and expression copy costs.

## Review findings

Production, independent verification, documentation/API, and
portability/GPU/performance reviews are complete. The resolved findings cover
timer arithmetic, parser context/range/redaction security, expression adapter
constraints and lifetime, factory-only nodes, alias opacity, object-root
configuration, floating representation guards, component closure, DLL export
boundaries, IWYU, formatting, and documentation examples. No local
release-blocking finding remains.

## Provenance and license

The implementation is project-owned. Philox was independently derived from
Salmon et al., SC11 and the frozen mapping; the retrieved primary PDF hash and
clean-room exclusions are in `provenance-record.md`. No MdeCpp, deleted
asc-cpp, Random123 implementation/test, or upstream vector corpus was copied.
MdeCpp remains comparison/provenance evidence only.

The existing Apache-2.0 `LICENSE` is unchanged. Its SHA-256 remains
`c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4`,
matching the bound asc-cmake release repository.

## Remaining risks

- Hosted VS 2022 shared/multi-config and AppleClang/libc++ results require the
  publication CI; they are not claimed locally.
- Fork pull requests may not receive the private ASCCMake read token.
- Non-IEC-60559/radix-two native float representations are intentionally
  compile-rejected to preserve the exact sequence contract.
- The branch contains cumulative uncommitted Milestones 0--2, so staging and
  rollback must preserve predecessor work.
- All GPU/provider evidence remains skipped until later approved milestones.

## Proposed publication and cleanup

After explicit owner approval, review the complete cumulative status and then:

```text
git add -A -- .
git commit -m "Establish asc-cpp architecture through Milestone 2"
git push -u origin feature/asc-cpp-m2-independent-foundations
gh pr create --draft --base main \
  --head feature/asc-cpp-m2-independent-foundations \
  --title "Establish asc-cpp architecture through Milestone 2" \
  --body-file <reviewed-pr-body>
```

Inspect every required hosted check and resolve failures on the same branch.
Do not merge, tag, or release without the separately required approvals. The
candidate is unreleased `0.2.0`; no tag is currently proposed.

Only after an approved merge and proof that no unique work remains:

```text
git switch main
git pull --ff-only origin main
git branch -d feature/asc-cpp-m2-independent-foundations
git push origin --delete feature/asc-cpp-m2-independent-foundations
```

Local and remote branch deletion remain separate approval actions. Before a
commit, rollback reverses only the enumerated Milestone 2 paths/edits and
retains Milestones 0--1. After a commit, rollback uses a reviewed revert,
never reset or force-push.

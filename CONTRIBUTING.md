# Contributing to ASCCpp

Changes must preserve C++20, the six-module architecture, public API and
numerical semantics, package isolation, provenance, and the explicit CPU/CUDA
support boundary.

## Architecture and ownership

Allowed direct dependency edges are defined in
[`docs/contracts/dependency-manifest.yaml`](docs/contracts/dependency-manifest.yaml).
In summary:

- Core has no ASCCpp dependency.
- Utilities depends on Core.
- Expression depends on Core.
- Dense and Sparse depend on Core and Expression, not on each other.
- Random depends on Core; its Dense/Sparse adapters add only the corresponding
  storage module.
- CUDA facets add the matching CPU facet and explicit CUDA prerequisites.

Changes to dependencies, public APIs, numerical behavior, ABI, provider
support, or reproducibility require an accepted ADR in
[`docs/architecture/decisions/`](docs/architecture/decisions/).

## Style and public API

- Use C++20 and the repository Google-derived `.clang-format`/`.clang-tidy`.
- Public headers must be self-contained, include what they use, and compile
  with exceptions disabled.
- Use `Status`/`Result` for recoverable failures; do not silently translate
  contract failures or provider-native errors.
- Preserve ownership, lifetime, memory-space, allocation, synchronization,
  aliasing, precision, stride/layout, and reproducibility semantics.
- Every public declaration requires useful Doxygen documentation: parameters,
  return/failure behavior, pre/postconditions, ownership/lifetime, concurrency,
  CPU/GPU availability, and complexity where contractual.

## Configure, build, and test

All presets build outside the source tree. A prepared local `asc-cmake`
checkout avoids network access:

```bash
export ASC_CPP_ASCCMAKE_SOURCE=/path/to/asc-cmake
cmake --preset test-debug \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset test-debug --parallel 2
ctest --preset test-debug --no-tests=error --output-on-failure

cmake --preset test-release \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset test-release --parallel 2
ctest --preset test-release --no-tests=error --output-on-failure

cmake --preset test-shared \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset test-shared --parallel 2
ctest --preset test-shared --no-tests=error --output-on-failure

cmake --preset test-release-shared \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset test-release-shared --parallel 2
ctest --preset test-release-shared --no-tests=error --output-on-failure
```

Unexpected skipped tests are failures. Never delete, weaken, relabel away, or
suppress a failing test to obtain a green build.

## Formatting and tidy

Use the CI-pinned Clang 18 tools:

```bash
git ls-files '*.h' '*.cc' '*.cu' -z | \
  xargs -0 clang-format-18 --dry-run --Werror

cmake --preset tidy \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset tidy --parallel 2
clang-tidy-18 -p ../asc-cpp-build/tidy \
  src/core/*.cc src/utilities/*.cc src/dense/*.cc src/sparse/*.cc src/random/*.cc
```

## Sanitizers

```bash
for preset in test-asan-ubsan test-lsan test-tsan; do
  cmake --preset "$preset" \
    -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
  cmake --build --preset "$preset" --parallel 2
  ctest --preset "$preset" --no-tests=error --output-on-failure
done
```

TSan is intentionally bounded to concurrency-labeled tests. LSan excludes
recursive package consumers that reinstrument separate build trees.

## Documentation and examples

Install Doxygen 1.9.8 or newer and Graphviz, then run:

```bash
cmake --preset docs \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset docs --target asc_cpp_docs_check
```

The target must produce HTML, XML, and `ASCCpp.tag` with zero warnings, no
missing public symbols, valid links, and no machine paths. Example sources in
`examples/` are the snippets shown by Doxygen and must compile/run against a
relocated installed package.

## Package, install, relocation, and isolation

```bash
cmake --preset install-test \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASC_CPP_ASCCMAKE_SOURCE"
cmake --build --preset install-test --parallel 2
ctest --preset install-test --no-tests=error --output-on-failure
```

Run the same package label under static/shared configurations. Consumers may
link only exported `ASC::*` targets and may not include source directories or
use the package registry.

## Benchmarks and numerical evidence

Benchmarks are built with the full test configuration and run through CTest:

```bash
ctest --preset test-release --label-regex 'benchmark|performance' \
  --no-tests=error --output-on-failure
```

Every performance claim includes exact hardware, compiler/options, dataset,
repetitions, raw results, and correctness checks. Reference CPU BLAS is never
advertised as an optimized provider. Numerical changes require reference
oracles, error/tolerance rationale, edge cases, allocation/lifetime tests, and
reproducibility evidence.

## CUDA

CUDA is experimental for 0.9.0. Local CUDA work uses `test-cuda` and
`test-cuda-shared`, but a support claim requires the complete trusted
real-NVIDIA matrix, Compute Sanitizer, exact toolkit/driver/device/architecture,
all six component consumers, parity, lifetime/concurrency, and failure paths.
CPU compilation cannot substitute for that evidence.

## Provenance

Do not copy from MdeCpp or another project unless an approved disposition,
compatible license, and traceable provenance record exist. Preserve
`THIRD_PARTY_NOTICES`, the Joe--Kuo data/license, `docs/provenance/`, and the
contract generators. Regenerated BLAS/Random/provenance/ABI output must have no
unexplained diff.

## Issue, branch, ADR, and PR workflow

Start work from `develop` on a focused branch. Release work uses
`release/<version>`; never work directly on `main`. Link an issue, add an ADR
when contracts change, keep commits reviewable, complete the PR template, and
record exact validation. Resolve every review conversation before merge.

## Definition of done

A change is complete only when formatting/tidy, relevant Debug/Release and
static/shared builds, complete CTest, sanitizers, headers, packages/relocation,
contracts/provenance, strict Doxygen, installed examples, ABI, links, and
applicable CUDA evidence pass without unexplained skips or drift.

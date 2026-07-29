# Milestone 2 production self-review

Status: implementation complete; independent verification passed

## Scope delivered

The production surface is limited to the ten public headers and four compiled
sources frozen by the Milestone 2 contract:

- `utilities`: an owning, validated command-line option table; transactional
  schema-default plus command-line parsing; deterministic caller-owned help;
  and a `steady_clock` timer state machine;
- `expression`: the storage-neutral `ExpressionAdapter<T>` customization
  point, readable-expression protocol, opaque alias identity, complete
  sparsity metadata, safe scalar/lvalue/rvalue capture, and the four approved
  pointwise node factories;
- `random`: pure Philox4x32-10 block generation, the frozen
  stream/subsequence/offset mapping, checked offset advance, and exact binary
  `Uniform01<float>` and `Uniform01<double>` transforms.

No file parser, writable expression protocol, storage facet, provider,
compatibility source, entropy source, mutable engine, later distribution, or
later-milestone API was introduced.

## Architecture and dependency review

- Every public declaration is directly in `namespace asc`; implementation-only
  names use file-specific `internal_*` namespaces.
- Every public header is a guarded, self-contained `.h` file. The expression
  implementation remains in its owning header and the compiled modules use
  only their approved `.cc` files.
- Utilities and random include only core and C++20 standard-library headers.
  Expression includes only core and C++20 standard-library headers. None of
  the three modules includes another Milestone 2 sibling.
- The source contains no provider, dense, sparse, GPU SDK, MdeCpp, Random123,
  generated table, or optional-dependency edge.
- Error-bearing operations use core `Status` and `Result`; no production
  exception control flow, global registry, hidden transfer, or hidden
  synchronization was added.

## Semantic review

### Utilities

- `CommandLineParser::Create` owns the copied schema and option table and
  rejects invalid ASCII names, duplicate long names, duplicate short names,
  duplicate destinations, unknown JSON Pointer paths, unsupported leaf types,
  and collisions with implicit boolean `no-` names.
- Parsing recognizes the frozen long, exact-short, boolean-negation, and `--`
  forms. A required non-boolean value consumes the next token regardless of a
  leading dash. Numeric `from_chars` conversions are locale-independent,
  complete-token, and range checked.
- Values and origin metadata are accumulated privately, the complete fresh tree
  is validated once, and no `Configuration` is returned on any failure. The
  stored origin location is the zero-based option-token index, not the consumed
  value-token index.
- Diagnostics do not echo supplied values, so a failed conversion cannot expose
  a sensitive scalar. Core validation retains its own schema-driven redaction.
- Timer transitions and duration arithmetic match the frozen empty/running/
  stopped contract. No wall clock or floating conversion is used.

### Expression

- External types participate only through explicit `ExpressionAdapter<T>`
  specialization. The protocol requires value type, constant rank, exact
  fixed-rank shape, indexed scalar read, conservative alias query, and sparsity
  effect.
- Arithmetic scalars are copied. External lvalues are held through
  `std::reference_wrapper<const T>`; rvalues and nested nodes are moved or
  copied into the node. No node stores a direct reference to an rvalue.
- Binary construction checks rank and every extent before publishing a node.
  Rank-zero expansion is the only exception. Nodes contain fixed-size shape
  state through their operands and introduce no framework allocation,
  destination, evaluator, execution context, or dispatch.
- Alias queries propagate conservatively. Negation, binary operations, and
  scalar-expanded operations report the frozen operation and sparsity
  categories.

### Random

- Production was derived only from the frozen contract and its approved SC11
  mathematical source. MdeCpp, deleted asc-cpp random source/tests, Random123
  implementation/tests, prior milestone production/tests, and external vector
  corpora were not inspected.
- Round products are explicitly widened to unsigned 64-bit values; lane
  extraction, xor, modulo-2^32 key addition, and the nine between-round key
  bumps implement exactly ten rounds.
- Stream and subsequence split little-word-first. Offset selects `offset / 4`
  as the little-word-first block counter and `offset % 4` as the public result
  lane. Offset addition checks before adding.
- Uniform transforms retain the most-significant 24 or 53 concatenated bits
  and multiply by hexadecimal binary powers, producing exact `[0,1)`
  endpoints without a standard-library distribution.

## Production validation

Commands were run from the repository root.

```text
for each frozen public header:
  clang++-19 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -Iinclude -x c++ -fsyntax-only -
result: PASS (10/10)

clang++-19 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -fsyntax-only <four production .cc files>
result: PASS

clang++-19 ... representative external adapter, scalar expansion,
  lvalue capture, moved nested node, alias, and indexed read
result: PASS

g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror ... \
  representative external adapter and scalar multiplication
result: PASS

clang-format-19 --dry-run --Werror <all 14 production files>
result: PASS

cmake -S . -B /tmp/asc-cpp-m2-production.c8s0c2 \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/install \
  -DBUILD_TESTING=OFF -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
result: PASS

cmake --build /tmp/asc-cpp-m2-production.c8s0c2 \
  --target asc_utilities asc_random -j2
result: PASS

CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m2-production-shared.0tMOk0 \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/install \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m2-production-shared.0tMOk0 \
  --target asc_utilities asc_random -j2
result: PASS

g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror ... \
  <standalone utilities/expression/random production smoke> &&
  /tmp/asc-cpp-m2-production-smoke
result: PASS
```

The first attempted clean configure used `-G Ninja` and failed before language
configuration because Ninja is not installed in this environment. Re-running
with Unix Makefiles passed. An attempted direct build of `asc_expression`
reported no Make target because the frozen target is a genuine interface
library; its headers were compiled in the standalone checks instead.

## Risks and explicit non-claims

- The pre-1.0 expression customization signatures and command-line rendering
  layout require independent API usability review on all hosted compilers.
- Node capture deliberately does not extend the lifetime of storage referenced
  by an external non-owning view.
- `Timer` deliberately has no cross-thread synchronization guarantee.
- Random correctness still requires the verification engineer's independently
  derived paper vectors; the production smoke intentionally checks mapping and
  endpoint invariants rather than importing a vector corpus.
- Local GPU evidence is exactly `skipped`: this milestone has no GPU target or
  implementation.

## Independent portability follow-up

The dedicated portability/GPU/performance reviewer completed its independent
audit with no unresolved correction. Its Clang 19 Release/shared matrix passed
79/79, and the relocated Utilities and Random ELF libraries both retained
`NEEDED libasc_core.so` with `RUNPATH [$ORIGIN]`. The reviewer confirmed from
Apple's official C++ support record that floating `std::from_chars` is
available for deployment targets macOS 13.3 and newer, covering the required
macOS 15 job; no fallback parser was required. Hosted MSVC/Apple compilation
and Apple `@loader_path` runtime relocation remain non-local evidence rather
than local claims. GPU evidence remains exactly `skipped`.

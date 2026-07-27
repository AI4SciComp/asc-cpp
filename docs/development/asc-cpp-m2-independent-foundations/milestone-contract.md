# Milestone 2 contract: independent foundation modules

Status: Frozen after owner approval on 2026-07-26

Branch: `feature/asc-cpp-m2-independent-foundations`

Corrections: None

## Authority and predecessor

This contract is subordinate to the owner's current instructions, the
all-in-one runbook, and the approved Stage A architecture package and ADRs. It
binds released ASCCMake `v0.1.0` at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

The owner advanced directly from the locally validated Milestone 1 Publication
Checkpoint B without authorizing a commit or publication. This branch therefore
carries the complete uncommitted Milestone 0 and Milestone 1 predecessor layers.
Milestone 2 must preserve those layers and every unrelated retained deletion or
modification.

## Objective and exact scope

Implement the three mutually independent, provider-free Milestone 2 waves:

1. transactional command-line configuration and a monotonic timer in
   `utilities`;
2. a storage-neutral pointwise expression protocol and safe nodes in
   `expression`;
3. a clean-room Philox4x32-10 raw-bit engine and exact scalar `Uniform01`
   transforms in base `random`.

No local configuration-file syntax is approved. No built-in-array convenience
wrapper has been justified and frozen. Both are deferred rather than inferred
from the broad roadmap wording.

Package version is the unreleased `0.2.0` candidate.

## Exact target and dependency contract

```text
build target       installed/build-tree target   target kind     direct ASC dep
asc_core           ASC::core                     compiled        none
asc_utilities      ASC::utilities                compiled        ASC::core
asc_expression     ASC::expression               interface       ASC::core
asc_random         ASC::random                   compiled        ASC::core
```

`asc_utilities` and `asc_random` are real libraries following
`BUILD_SHARED_LIBS`. `asc_expression` is an interface library because its
approved facilities are C++20 concepts, customization, holders, and node
templates; an empty compiled object is prohibited. All three new targets use
only the C++20 standard library and the actual released asc-cmake APIs.

Utilities, expression, and random do not include or link one another. They do
not include or link dense or sparse. Provider SDK names, headers, handles, and
types are prohibited.

## Exact public files

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

All public declarations are directly in `namespace asc`. Headers are
self-contained `.h` files with full-path guards and direct includes. Template
definitions remain in the owning public header; no public `detail` namespace,
provider header, `*_impl.h`, C++20 module, or `#pragma once` is approved.

## Utilities semantic surface

### Command-line configuration

- `CommandLineOption` describes one long name, optional one-character short
  name, JSON Pointer configuration path, value label, and help text.
- `CommandLineParser::Create` validates and owns an option table. Long and
  short names and destination paths must be unique. Option names are ASCII and
  do not contain their leading dashes.
- An option's value type is read from the supplied `ConfigurationSchema`.
  Milestone 2 accepts bool, signed/unsigned 64-bit integer, double, and UTF-8
  string leaves. Null, list, and object destinations return `kUnsupported`.
- Long options accept `--name=value` or `--name value`. Short options accept
  exactly `-x value`; short clusters and attached values are unsupported.
  Boolean options accept `--flag`, `--no-flag`, `--flag=true|false`, and
  `-f`; negation is long-form only.
- A token expected as a value is consumed even when it begins with `-`, so
  negative signed integers and doubles are ordinary values. Numeric conversion
  is locale-independent, consumes the complete token, checks overflow, and
  performs no coercion.
- `--` ends option recognition. Remaining and otherwise bare tokens are
  returned, in order, as positional arguments. Unknown options and malformed
  option tokens fail.
- Supplying the same destination more than once at command-line precedence is
  a duplicate scalar error, including long/short aliases and positive/negative
  boolean spellings.
- Parsing constructs a fresh tree, validates the complete tree with the core
  schema, and publishes a result only after success. Failure exposes no partial
  configuration.
- Non-default values carry
  `ConfigurationOriginKind::kCommandLine`; token index is retained as the
  origin location. Core origin values append `kCommandLine` without changing
  the stable numeric values of `kDefault` or `kProgrammatic`.
- `RenderHelp` returns deterministic caller-owned text from the validated
  option table. It performs no terminal I/O and has no implicit `--help`
  control flow.
- The frozen precedence remains schema default, then approved files in command
  order, then command line, then explicit programmatic override. This wave
  implements only schema-default plus command-line input. Environment
  variables, response files, a file parser, list repetition, short clusters,
  and implicit programmatic merging are not implemented.

### Monotonic timer

- `Timer` uses `std::chrono::steady_clock` and exposes `TimerState::kEmpty`,
  `kRunning`, and `kStopped`.
- `Start` succeeds from empty or stopped and fails with `kInvalidState` when
  already running.
- `Stop` succeeds only while running, records one non-negative interval, adds
  it to the total, increments the sample count, transitions to stopped, and
  returns that interval.
- `Elapsed` returns the accumulated duration plus the current interval while
  running. `Last` and `Average` fail with `kInvalidState` before the first
  completed interval. `Average` uses duration arithmetic without floating
  conversion.
- `Reset` is valid in every state, discards all samples and any in-progress
  interval, and returns to empty.
- No wall-clock time, fixed sample buffer, global timer registry, logging,
  thread synchronization guarantee, or GPU event timing is provided.

## Expression semantic surface

- `ExpressionAdapter<T>` is the single non-intrusive customization point. An
  external type participates by explicitly specializing it; ASC inheritance,
  dense/sparse headers, and provider knowledge are unnecessary.
- `ReadableExpression` requires an adapter-supplied scalar `value_type`,
  compile-time `rank`, exact shape, indexed read, alias query, and sparsity
  effect. Shape extents use core `extent_t`; rank uses core `rank_t`; indexed
  access uses core `index_t`.
- `AliasToken` is an opaque caller-supplied identity. `MayAlias` is
  conservative: `true` permits aliasing and `false` guarantees that the
  expression does not reference that token.
- `SparsityEffect` has exactly
  `kStructurePreserving`, `kStructureFiltering`, `kStructureUnion`,
  `kStructureIntersection`, `kValueDependent`, `kDensifying`, and
  `kDestinationRequired`.
- Scalar terminals accept arithmetic values and capture them by value with
  rank zero. Lvalue expressions are held through documented non-owning
  references. Rvalue expressions and nested nodes are held by value. No node
  contains a reference to an rvalue; capturing a non-owning view by value does
  not extend the viewed storage lifetime.
- `MakeNegate`, `MakeAdd`, `MakeSubtract`, and `MakeMultiply` construct the
  approved built-in pointwise node set. Construction returns `Result` where
  full-shape compatibility can fail.
- Ranked operands require identical rank and every identical extent. A rank-zero
  scalar may expand against one ranked operand. Two rank-zero operands produce
  rank zero. No other broadcasting is accepted.
- Nodes expose result value type, compile-time rank, shape, indexed scalar read,
  conservative alias propagation, operation category, and sparsity effect.
  Negation is structure-preserving; addition/subtraction are structure-union;
  multiplication is structure-intersection, except a scalar-expanded operation
  is conservatively value-dependent.
- Construction and scalar reads perform no result allocation, destination
  mutation, transfer, synchronization, execution-context selection, or provider
  dispatch. Expression defines no storage type, evaluator, reduction, algebra
  descriptor, or result materialization.

## Random semantic surface and sequence contract

### Approved primary source and clean-room boundary

The owner approved Milestone 2 with no corrections after approving ADR 0015
and the architecture's named Philox4x32-10 contract. This resolves the random
gate only for the following primary source:

- John K. Salmon, Mark A. Moraes, Ron O. Dror, and David E. Shaw,
  *Parallel Random Numbers: As Easy as 1, 2, 3*, SC11 (2011),
  [author-hosted PDF](https://www.thesalmons.org/john/random123/papers/random123sc11.pdf)
- retrieved 2026-07-26; 280039 bytes; 12 pages
- SHA-256
  `841f68114b052f436a818680c90d685ce796e492a448a1064e939adefbe596c6`
- governing material: counter/key model in Section 2, generalized Feistel
  equations and Philox construction in Section 4.3, the published
  Philox4x32 multipliers, the Weyl key schedule, and the ten-round safety-margin
  recommendation

Production and verification are independent derivations from this frozen
contract and primary paper. MdeCpp, deleted asc-cpp random files, Random123
implementation headers, upstream test vectors, and their literal corpora are
prohibited sources.

### Exact raw-bit mapping

- `Philox4x32Counter` is four `std::uint32_t` words in public lane order
  `[c0,c1,c2,c3]`; `Philox4x32Key` is two words `[k0,k1]`; the result is four
  words in the same lane order.
- One round computes 64-bit products
  `p0 = 0xD2511F53 * c0` and `p1 = 0xCD9E8D57 * c2`, then returns
  `[high(p1) xor c1 xor k0, low(p1), high(p0) xor c3 xor k1, low(p0)]`.
- Round zero uses the supplied key. Between successive rounds, modulo-2^32 key
  addition applies
  `[k0 + 0x9E3779B9, k1 + 0xBB67AE85]`.
- `Philox4x32_10` applies exactly ten rounds and is a pure `noexcept` function.
  Multiplication and key arithmetic are explicitly unsigned.
- `RandomStream`, `RandomSubsequence`, and `RandomOffset` are unsigned 64-bit
  vocabulary. Stream maps little-word-first to `[k0,k1]`; subsequence maps
  little-word-first to `[c2,c3]`; a word offset selects block
  `offset / 4`, mapped little-word-first to `[c0,c1]`, and lane `offset % 4`.
- Direct block generation and position-to-word generation are both public.
  `AdvanceRandomOffset` returns `kOverflow` rather than wrapping.
- Algorithm identity and these mappings are exact pre-1.0 sequence API for the
  ASCCpp `0.2.x` line.

### Exact uniform transforms

- `Uniform01<float>` consumes one raw 32-bit word, selects its most significant
  24 bits, and multiplies by the exactly representable binary scale `2^-24`.
- `Uniform01<double>` consumes two words in `(high, low)` order, concatenates
  them into a 64-bit bit string, selects its most significant 53 bits, and
  multiplies by the exactly representable binary scale `2^-53`.
- Results are exactly in `[0,1)`: zero maps to positive zero and all-one input
  maps to `1-2^-24` or `1-2^-53`. No standard-library distribution is part of
  the sequence.

There is no entropy acquisition, mutable/default engine, state serialization,
normal or rejection distribution, Sobol data, dense/sparse generation facet,
logical fill mapping, or provider conformance claim.

## Exact compiled production files

```text
src/utilities/command_line.cc
src/utilities/timer.cc
src/random/distribution.cc
src/random/engine.cc
```

Expression remains a genuine header-only interface. No compatibility source,
provider source, generated table, vendored source, or explicit instantiation
outside this list is approved.

## Build and package contract

- Minimum CMake 3.25; strict public C++20 with extensions off.
- ASCCMake 0.1.0 is found exactly. Its verified public functions are used only
  as documented; standard CMake owns ASCCpp component exports.
- Build and install configs expose exactly `core`, `utilities`, `expression`,
  and `random`. Dense, sparse, random facets, `cpp`, and every provider remain
  unavailable and unexported.
- Requesting any new component expands its internal dependency to `core`, loads
  `ASCCppCoreTargets.cmake` first, and then loads exactly the requested target
  export. A core-only request does not create another imported target.
- Each of `ASC::utilities`, `ASC::expression`, and `ASC::random` must configure,
  build, install, relocate, and consume independently with forbidden siblings
  absent. Static/shared, build-tree/install-tree, paths containing spaces, and
  subproject consumers remain required.
- No-component lookup still requests unavailable `cpp` and fails. Unknown or
  unavailable required components fail. Optional unavailable components report
  false without invalidating an otherwise successful available request.
- No user package-registry write is permitted.

## Verification contract

- Compile every new public header alone under C++20 and, where supported, with
  exceptions disabled; instantiate representative expression nodes in multiple
  translation units.
- Mechanically audit public/source includes, direct build dependencies, installed
  imported-target graphs, component isolation, and the absence of provider,
  dense, sparse, and sibling-module edges.
- Falsify command-line names, duplicates, negative numerics, conversion
  boundaries, unknowns, `--`, bool negation, UTF-8, schema validation, origin,
  redaction, help determinism, and transactional rollback.
- Exercise timer empty/running/stopped/reset transitions and non-negative
  monotonic durations without flaky wall-time thresholds.
- Use an external non-ASC expression, compile negatives, exact-shape and scalar
  expansion, lvalue/rvalue/nested lifetime, alias and sparsity metadata, and
  zero allocation/evaluation/dispatch at construction.
- Verification independently derives Philox raw-word vectors from the paper and
  frozen mapping. It tests zero, all-one, asymmetric, lane-sensitive, key,
  stream, subsequence, offset/lane-boundary, and overflow cases. It tests exact
  float/double bit transforms and endpoints.
- Run Debug and Release, warnings-as-errors, ASan/UBSan, package relocation,
  static/shared, subproject, isolated consumers, and formatting. Hosted
  GCC/Clang/MSVC/AppleClang remains CI evidence, not a local claim.
- GPU evidence is exactly `skipped`: this milestone contains no provider target
  or GPU implementation. Toolkit and hardware inventory do not raise that
  evidence level.

## Prohibited and deferred

- No dense, sparse, random storage facet, `ASC::cpp`, provider facet, production
  GPU code, local-file parser, built-in-array wrapper, general broadcasting,
  reduction, evaluator, result allocation, entropy source, normal distribution,
  Sobol implementation/data, or serialized random state.
- No production exception API, mutable global policy/registry/default engine,
  hidden transfer/allocation/fallback, or provider header.
- No copied or mechanically translated MdeCpp/deleted asc-cpp/upstream
  implementation source, test, literal corpus, or generated data.
- No unapproved dependency, C++23 feature, branch publication, PR, merge, tag,
  release, or branch deletion.

## Stop and rollback

Stop at Publication Checkpoint B. Before a commit, rollback removes only the
Milestone 2 additions and reverses its explicit package/CI/current-document
changes; the complete Milestone 0 and Milestone 1 predecessor diff remains.
After a separately approved commit, rollback is a reviewed `git revert`, never
reset, force-push, or broad deletion.

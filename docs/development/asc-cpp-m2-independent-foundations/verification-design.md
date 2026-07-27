# Milestone 2 independent verification design

Status: Contract-first design frozen before production inspection

Date: 2026-07-26

## Independence statement

This design was written from the frozen Milestone 2 contract, ownership
ledger, provenance record, runbook, and approved ADRs 0001, 0002, 0003, 0004,
0005, 0007, 0010, 0015, 0017, and 0018. The verification engineer had not
inspected any Milestone 2 production implementation when this file was
created.

For random verification, the only algorithm sources are the frozen contract
and the approved SC11 paper artifact with SHA-256
`841f68114b052f436a818680c90d685ce796e492a448a1064e939adefbe596c6`.
MdeCpp, deleted asc-cpp random source/tests, Random123 implementation/tests,
and upstream vector corpora are prohibited.

## Verification goals

Verification will attempt to falsify these independent claims:

1. `utilities`, `expression`, and base `random` each have only the approved
   direct dependency on `core`.
2. Command-line parsing is checked, deterministic, origin-preserving, and
   transactional.
3. Timer state transitions and accumulated durations follow a monotonic clock
   without relying on brittle elapsed-time thresholds.
4. Expression participation is storage-neutral and non-intrusive, node capture
   cannot directly bind an rvalue by reference, and pointwise construction
   performs no evaluation or allocation.
5. Philox4x32-10 implements the exact frozen lane/key/counter mapping and the
   stream/subsequence/offset vocabulary without wraparound.
6. `Uniform01<float>` and `Uniform01<double>` implement the exact binary
   transforms and endpoints.
7. Every public header is self-contained under strict C++20 and the installed
   component targets remain isolated.

## Planned verification files

The independent test set will be organized under the assigned scopes:

```text
tests/utilities/command_line_test.cc
tests/utilities/timer_test.cc
tests/expression/expression_test.cc
tests/random/engine_test.cc
tests/random/distribution_test.cc
tests/compile/m2_expression_negative_*.cc
tests/compile/m2_header_*.cc
tests/consumer/utilities/
tests/consumer/expression/
tests/consumer/random/
```

The lead owns CMake registration and may consolidate names without weakening
the cases below.

## Utilities test model

### Command-line schema and option-table validation

- Accept unique ASCII long names, optional unique one-character short names,
  unique JSON Pointer destinations, value labels, and help text.
- Reject an empty or malformed long name, a non-ASCII name, a name containing
  its leading dash, duplicate long names, duplicate short names, duplicate
  destinations, invalid JSON Pointer destinations, and schema destinations
  that do not exist.
- Reject `null`, list, and object leaves with `kUnsupported`.
- Verify factory failure publishes no partially usable parser.

### Syntax and conversion

- Accept `--name=value`, `--name value`, and exactly `-x value`.
- Accept boolean `--flag`, `--no-flag`, `--flag=true`, `--flag=false`, and
  `-f`; reject short boolean negation, short clusters, and attached short
  values.
- Treat a value-position token beginning with `-` as a value. Exercise the
  minimum signed integer and negative finite/exponent double forms.
- Exercise signed and unsigned 64-bit endpoints, one-past endpoint strings,
  negative unsigned input, finite double syntax, overflow, trailing junk,
  empty values, and locale-independent decimal conversion.
- Exercise UTF-8 string values and path-like strings without interpreting them
  as response files.
- Verify `--` ends option recognition and preserves every following token,
  including dash-prefixed tokens, as ordered positionals.
- Verify otherwise bare tokens remain ordered positionals.
- Reject unknown long/short options and malformed option tokens.

### Duplicates, validation, origins, and rollback

- Treat long/short aliases, `--flag`/`--no-flag`, and split/equal forms as one
  destination for duplicate detection.
- Verify schema defaults remain defaults when no option overrides them.
- Verify each parsed non-default leaf reports
  `ConfigurationOriginKind::kCommandLine` and the originating token index.
- Exercise schema constraint failure after several earlier valid tokens and
  prove the call returns no configuration or positional result.
- Exercise conversion, duplicate, and unknown-option failures after earlier
  valid tokens and prove no partial configuration is published.
- Verify sensitive received values do not appear in diagnostics.
- Verify deterministic help output across repeated calls, declaration order,
  aliases, boolean syntax, value labels, and stable newline behavior. Help
  rendering must perform no terminal I/O and must not add implicit help
  control flow.

### Explicit non-features

Negative tests will ensure there is no environment-variable input,
response-file expansion, local-file parser, list repetition, short clustering,
or implicit programmatic merge in the Milestone 2 API.

## Timer state-machine tests

The model is:

```text
kEmpty --Start--> kRunning --Stop--> kStopped
   ^                   |                 |
   |-------Reset-------+------Reset------+
                       |
                 Start while running: kInvalidState
```

Tests will verify:

- initial state, zero elapsed duration, zero sample count, and invalid
  `Last`/`Average`;
- `Start` from empty and stopped;
- `Start` while running returns `kInvalidState` without changing state or
  completed sample accounting;
- `Stop` only succeeds while running and returns a non-negative interval;
- each successful stop increments the count exactly once, records `Last`, and
  updates total/average consistently using duration arithmetic;
- elapsed time while running is at least the previously completed total;
- stop from empty/stopped returns `kInvalidState` without mutation;
- reset from every state discards completed and in-progress intervals and
  restores empty behavior.

The test will yield or perform bounded work only to exercise a running
interval. It will not require a positive minimum wall duration or compare
wall-clock timestamps.

## Expression protocol tests

### External participation and metadata

- Define a third-party-like ranked readable type outside `namespace asc` and
  specialize only the approved `ExpressionAdapter`.
- Verify its scalar type, compile-time rank, exact shape, indexed read, alias
  query, and sparsity effect through the public protocol.
- Verify `ReadableExpression` rejects unspecialized and incompletely adapted
  types with compile-only negative cases.
- Verify no dense, sparse, provider, allocation, or evaluation type is needed
  by the consumer.

### Shape and scalar expansion

- Construct rank-zero scalar terminals and rank-zero/rank-zero nodes.
- Construct exact-shape ranked negate/add/subtract/multiply nodes.
- Expand a rank-zero scalar on both the left and right of a ranked operand.
- Reject equal-size but unequal-shape operands, unequal ranks, and mismatched
  extents at construction.
- Exercise zero extents and rank zero.
- Verify there is no trailing-axis or singleton broadcasting.

### Capture and lifetime

- Mutate a live lvalue operand after node construction and verify reads observe
  the live non-owning reference.
- Pass temporary expression nodes into nested factories, then read the final
  node after the full expression that created the temporaries has ended.
- Move a node and verify the moved-to node remains readable.
- Use copy/move-counted adapter types to distinguish lvalue non-owning capture
  from rvalue value capture.
- Document and test that a non-owning view captured by value does not extend
  the viewed storage lifetime; no test will dereference an intentionally
  dangling view.

### Reads, aliasing, sparsity, and side effects

- Check representative scalar values for all four built-in operations.
- Check conservative alias propagation from every operand and unrelated token.
- Check negation as structure-preserving, addition/subtraction as
  structure-union, ranked multiplication as structure-intersection, and every
  scalar-expanded binary operation as value-dependent.
- Instrument external reads and allocations: construction performs zero scalar
  reads, zero destination writes, and no dynamic allocation attributable to
  the expression node; indexed reads happen only when explicitly requested.
- Check the pointwise operation category and absence of an evaluator,
  destination, transfer, synchronization, or provider dispatch API.

## Independent random oracle

The verifier will implement a small test-local reference round directly from
the frozen equations, using explicit `std::uint64_t` products and
`std::uint32_t` modulo arithmetic. It will not call production helpers except
for the result under test. Expected literals will be generated and
cross-checked from that oracle before the production implementation is read.

Raw-block cases will include:

- zero counter and zero key;
- all-one counter and key;
- asymmetric lane-distinguishing counter and key;
- each individual counter lane and key lane perturbed;
- key-schedule-sensitive cases;
- direct stream/subsequence mappings with unequal high/low halves.

Position cases will include offsets `0`, `1`, `2`, `3`, `4`, a nontrivial
multi-block offset, and values straddling low/high 32-bit counter words.
Mapping tests will use asymmetric stream and subsequence words to detect
endianness or lane reversal. `AdvanceRandomOffset` will exercise zero advance,
ordinary advance, exact maximum, and overflow without mutation or wrap.

The reference implementation will also compare a ten-round call with a
round-by-round independently calculated result for every raw-block case. This
guards against self-confirming a single expected-vector typo.

### Independently derived raw-word expectations

Before inspecting production, the verifier transcribed the frozen round
equations into a temporary calculation that uses only unsigned 64-bit products,
32-bit lane extraction, ten rounds, and the two frozen Weyl additions. It
produced these expectations:

| Case | Counter `[c0,c1,c2,c3]` | Key `[k0,k1]` | Result `[r0,r1,r2,r3]` |
| --- | --- | --- | --- |
| zero | `00000000 00000000 00000000 00000000` | `00000000 00000000` | `6627e8d5 e169c58d bc57ac4c 9b00dbd8` |
| all one | `ffffffff ffffffff ffffffff ffffffff` | `ffffffff ffffffff` | `408f276d 41c83b0e a20bc7c6 6d5451fd` |
| asymmetric | `01234567 89abcdef fedcba98 76543210` | `13579bdf 2468ace0` | `f3b36d22 1c1759f9 ad23a12e 11413b1c` |
| `c0` only | `00000001 00000000 00000000 00000000` | `00000000 00000000` | `f8e4cca4 5cb200db b1a574eb 097eff67` |
| `c1` only | `00000000 00000001 00000000 00000000` | `00000000 00000000` | `6ad0c5ec ea236249 73a459f5 074944b3` |
| `c2` only | `00000000 00000000 00000001 00000000` | `00000000 00000000` | `844515e1 f08d6eaa 0f19c053 83f875f0` |
| `c3` only | `00000000 00000000 00000000 00000001` | `00000000 00000000` | `2dce73e5 1348e23f fcf8e0ec a287aadb` |
| `k0` only | `00000000 00000000 00000000 00000000` | `00000001 00000000` | `e3e80670 e50a0ebc 95f222c0 b615aa27` |
| `k1` only | `00000000 00000000 00000000 00000000` | `00000000 00000001` | `fdde3e0b fa7e58b6 3380ec46 d8d55c4f` |
| lane mapping | `55667788 11223344 ddeeff00 99aabbcc` | `05060708 01020304` | `ac7db954 73017cb1 1bf18b86 3312161e` |

These literals are verification-owned derivations, not values copied from an
implementation or upstream vector corpus. The test-local reference round will
remain as a second oracle.

## Uniform transform oracle

Float cases:

- `0x00000000` gives positive zero;
- low discarded bits alone do not change the result;
- the top mantissa-selection bit and several asymmetric words select exactly
  the most significant 24 bits;
- `0xffffffff` gives exactly `1 - 2^-24`.

Double cases:

- `(0x00000000, 0x00000000)` gives positive zero;
- discarded low bits do not change the result;
- asymmetric high/low words detect concatenation order;
- `(0xffffffff, 0xffffffff)` gives exactly `1 - 2^-53`.

Expected results will be compared by floating-point bit pattern or exact
equality because every frozen scale and selected integer is exactly
representable for the destination precision. No standard-library distribution
is used as an oracle.

## Compile, package, and dependency tests

- Compile each new public header as the first and only project include under
  C++20 with extensions disabled.
- Compile representative expression templates in multiple translation units.
- Add useful compile-fail sources for an unspecialized expression, an
  incomplete adapter, and an unsupported broadcast; the lead will choose the
  repository's supported compile-failure registration mechanism.
- Build isolated consumers that request and link exactly one of
  `ASC::utilities`, `ASC::expression`, or `ASC::random`.
- Each isolated consumer must observe `ASC::core` and its requested target,
  while forbidden sibling targets remain absent.
- The lead will run build-tree, install-tree, relocation, path-with-spaces,
  static/shared, Debug/Release, GCC/Clang, warnings-as-errors, and
  ASan/UBSan validation.
- Include and imported-target audits must show no utilities/expression/random
  sibling edge, no dense/sparse edge, and no provider header, target, or SDK
  discovery.

## Result reporting

`verification-review.md` will record:

- the production revision/diff inspected after this design was frozen;
- test files added and contract cases covered;
- exact commands with pass/fail/skip counts;
- defects with file/symbol evidence and integration requests;
- resolutions verified after lead/production changes;
- remaining risks and unavailable hosted-platform evidence;
- GPU evidence exactly `skipped`, because Milestone 2 has no GPU/provider
  target or implementation.

# Milestone 2 production self-review

Status: Complete; all findings resolved by lead integration on 2026-07-26

## Review boundary

This review covers only the current Milestone 2 production surface:

```text
include/asc/utilities.h
include/asc/utilities/**
src/utilities/{command_line,timer}.cc

include/asc/expression.h
include/asc/expression/**

include/asc/random.h
include/asc/random/**
src/random/{distribution,engine}.cc
```

The review used the frozen Milestone 2 contract and ownership/provenance
records plus ADRs 0001--0005, 0007, 0010, 0015, 0017, and 0018. It did not
inspect MdeCpp, deleted ASCCpp random implementation or tests, Random123
implementation or tests, an upstream vector corpus, or independent
verification implementation details.

## Contract conformance

### Module and dependency boundary

- The utilities, expression, and random public/source files include only their
  own module and `core`; they do not include one another, dense, sparse, or a
  provider.
- No provider SDK name, header, handle, storage type, evaluator, device
  transfer, entropy source, mutable default engine, local-file parser,
  built-in-array wrapper, reduction, or random storage facet occurs in this
  production scope.
- Public declarations use the flat `asc` namespace. Private namespaces contain
  `internal`.
- The compiled surface is exactly the four frozen `.cc` files. Expression is
  header-only.

### Utilities

- `CommandLineParser::Create` owns its schema and option table and validates
  long names, short names, JSON Pointer destinations, scalar leaf types,
  generated boolean-negation collisions, and uniqueness.
- Parsing recognizes the frozen long/short forms, consumes leading-dash
  numeric values when a value is expected, stops at `--`, returns bare
  positionals in order, rejects malformed/unknown/duplicate options, uses
  locale-independent full-token `from_chars` conversion, and validates both
  the leaf and complete configuration before publishing a result.
- Command-line origins use `kCommandLine`, source label `argv`, and the
  zero-based option-token index. Sensitive conversion and bounds diagnostics
  redact the received value.
- Help generation is deterministic and performs no I/O.
- `Timer` implements the empty/running/stopped transitions, cumulative and last
  durations, sample count, running elapsed time, duration-arithmetic average,
  reset, invalid-state errors, and defensive duration/count overflow checks.

### Expression

- `ExpressionAdapter<T>` permits an external non-ASC type to supply value,
  compile-time rank, exact shape, read, alias, operation, and sparsity
  metadata.
- Scalar terminals are captured by value at rank zero. Lvalue expressions use
  non-owning `ExpressionReference`; rvalue expressions and nested nodes use
  `ExpressionOwner`. No holder binds a direct reference to an rvalue.
- Negate/add/subtract/multiply factories validate negative extents and exact
  ranked shape before returning a node. Rank-zero scalar expansion is the only
  broadcast.
- Reads recurse without allocation, destination mutation, dispatch, transfer,
  or synchronization. Alias queries propagate conservatively. Frozen
  structure-preserving/union/intersection/value-dependent sparsity metadata is
  present.

### Random

- The raw round uses unsigned 64-bit products of `0xd2511f53 * c0` and
  `0xcd9e8d57 * c2`, the frozen high/low lane permutation, and the two frozen
  Weyl increments between ten rounds.
- Stream maps low-word-first to the two key lanes. Subsequence maps
  low-word-first to counter lanes 2 and 3. Word offset maps `offset / 4`
  low-word-first to counter lanes 0 and 1 and selects `offset % 4`.
- `AdvanceRandomOffset` rejects unsigned overflow.
- The float transform selects the top 24 bits and scales by exact `2^-24`; the
  double transform concatenates `(high, low)`, selects the top 53 bits, and
  scales by exact `2^-53`. Compile-time representation guards require the
  native binary precision used by the frozen sequence.
- There is no standard-library distribution, hidden state, provider, normal
  distribution, Sobol material, serialized state, or storage-generation code.

## Diagnostics run

The following completed successfully:

```text
git diff --check -- <all owned production headers and sources>
/usr/lib/llvm-19/bin/clang-format --dry-run --Werror <all owned .h/.cc files>
g++ -std=c++20 -fno-exceptions -Wall -Wextra -Werror -Iinclude
    -x c++ -fsyntax-only <each public header>
g++ -std=c++20 -fno-exceptions -Wall -Wextra -Werror -Iinclude
    -fsyntax-only <each compiled source>
/usr/lib/llvm-19/bin/clang++ <the same header/source syntax matrix>
```

The stronger non-project diagnostic
`-Wconversion -Wsign-conversion -Werror` found SR-03 below.

## Findings and dispositions

| ID | Severity | Finding | Disposition |
| --- | --- | --- | --- |
| SR-01 | contract | `AliasToken` exposes its `const void* identity` representation publicly, while the frozen contract calls the token opaque. | Reported to lead. Make representation private and provide an explicit caller factory, or record why “opaque” means only uninterpreted identity before publication. |
| SR-02 | contract edge | A root scalar option uses the empty JSON Pointer successfully when supplied, but an absent root scalar option builds an empty object. A root scalar schema default therefore cannot occupy the schema-default precedence level. | Reported to lead. Either support the absent root-scalar/default case transactionally or constrain/document the parser root schema as an object; the latter changes the currently accepted `Create` surface and needs contract review. |
| SR-03 | portability | GCC with `-Wconversion -Wsign-conversion -Werror` rejects the range-for conversion from `char` to `unsigned char` in `QuoteDiagnostic`. The project warning set used by the ordinary syntax check passes. | Reported to lead. Use an explicitly converted `char` loop variable before the Publication Checkpoint matrix if conversion warnings are claimed. |
| SR-04 | IWYU | `src/random/engine.cc` spells `std::size_t` but does not directly include `<cstddef>`. Current standard-library transitive inclusion makes it compile. | Reported to lead. Add the direct standard header to satisfy the repository source policy. |
| SR-05 | diagnostic quality | Parser failures identify argv index and option spelling and redact sensitive values, but schema-bound failures repeat the core “outside bounds” text without rendering the accepted numeric range. | Reported to lead as lower severity. Add accepted bounds if the runbook's diagnostic field requirement is enforced literally for this milestone. |

No algorithm-mapping, forbidden dependency, provider leakage, hidden state,
later-milestone implementation, or provenance-boundary defect was found.

## Lead integration disposition

- SR-01: resolved. `AliasToken` now has private representation, explicit
  `FromIdentity`, and public equality without a pointer accessor.
- SR-02: resolved. `CommandLineParser::Create` requires an object root and
  rejects other root schema types with `kInvalidArgument`.
- SR-03: resolved. Diagnostic byte conversion is explicit; strict GCC and
  Clang conversion-warning checks pass with exceptions on and off.
- SR-04: resolved. The engine source directly includes `<cstddef>`.
- SR-05: resolved. Accepted descriptions render configured signed, unsigned,
  double, and string-size ranges while retaining sensitive-value redaction.

The affected utilities and expression runtime tests pass under strict GCC and
Clang, the documentation/API reviewer retested the corrected installed
examples, and the final sanitizer refresh passed 70/70 eligible tests. No
production self-review blocker remains.

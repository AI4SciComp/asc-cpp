# Random module design

> [!WARNING]
> **Superseded historical document.** The body below records the deleted
> five-component implementation at commit
> `33b261ea33616a6395c4ad3b20646093103344f7`. It is retained only for audit
> history and does not describe the active API or package. See the
> [current documentation][current-docs] and the
> [approved Stage A architecture][stage-a].

[current-docs]: ../README.md
[stage-a]: ../development/asc-cpp-architecture/architecture-blueprint.md

Status: **frozen for Random Milestone 1 (M1) implementation**  
Owner: ASC Random module team  
Approved architecture: `architecture_blueprint_v1.md`  
Frozen by: Lead Architect after Implementation, Verification, and Documentation
Engineer read-only reviews

## 1. Purpose

Random M1 establishes the first canonical reproducibility contract in
`ASC::random`. It is additive and deliberately smaller than the inherited
MdeCpp sampling surface. M1 provides:

1. one named and versioned counter-based integer engine;
2. one ASC-owned integer-to-uniform-real transform;
3. one explicit-context bulk fill over canonical Array views; and
4. a minimal Random component whose direct dependencies are Array and Core.

The milestone does not attempt to make every inherited sampler canonical.
Normal, spherical, Latin, Halton, Hammersley, and Sobol redesigns require
separate numerical and state contracts. Freezing those contracts together
with the first counter engine would obscure the small set of guarantees that
M1 can verify exhaustively.

## 2. Inputs and Lead resolutions

This design follows:

- `AGENTS.md` and `generator.md`;
- `architecture_review_v1.md` and `architecture_blueprint_v1.md`;
- the completed Core M1, Array M1, and Linalg M1 contracts;
- the inherited Random headers, compiled sources, tests, package rules, and
  dependency graph; and
- the three Random engineer reviews performed before any Random M1 code or
  test change.

The Lead resolves the engineer proposals as follows.

| Proposal | Decision | Reason |
| --- | --- | --- |
| Philox4x32-10 counter engine | Adopt | It has a compact integer-only contract, published known vectors, and a natural key/counter split. |
| Caller-owned canonical stateful engine | Defer | Direct counter addressing already provides M1 reset/skip/partition semantics without mutable state. |
| Uniform real transform | Adopt for `float` and `double` | It is sufficient to prove scalar and bulk reproducibility without delegating output to `std::uniform_real_distribution`. |
| Canonical standard normal | Defer | Transcendental mapping needs a separate fixed-consumption and portability decision. |
| Canonical Sobol | Defer | The inherited implementation has large per-instance direction state and needs a separately reviewed immutable-table API. |
| Pre-factored multivariate normal fill | Defer | It depends on the canonical normal decision and deserves an independent alias, shape, and complexity contract. |
| Random capability object | Defer | M1 has one always-available serial operation and no alternate constructible provider; direct validation is sufficient. |
| Preserve legacy Random through `ASC::random` | Reject | It would keep the canonical component coupled to Linalg and would make a minimal component promise misleading. |
| Preserve legacy Random through `ASC::cpp` | Adopt | It retains the compatibility API without making it part of the canonical Random target contract. |

## 3. Module boundary and dependencies

The canonical target is a compiled-plus-template component:

```text
ASC::random --> ASC::array
ASC::random --> ASC::core
```

It must not depend directly on:

- `ASC::linalg`;
- `ASC::utilities`;
- `ASC::cpp`;
- Eigen, MKL, BLAS/LAPACK, CUDA, OpenMP, or another optional SDK; or
- inherited Core/Array/Random compatibility headers.

Core owns `ExecutionContext`, `Status`, scalar/index types, and memory-space
vocabulary. Array owns canonical tensor descriptors and views. Random owns the
integer engine, uniform transform, and bulk generation policy.

Random M1 guarantees no Random-owned SDK include or direct SDK link. In an
optional-backend build, the current Core target still exports its OpenMP/CUDA
requirements and the shared build helper may compile dependent sources with
that toolchain. Removing those transitive Core/package requirements is the
approved provider/package-hardening work, not a Random-local change. M1 must
not misdescribe such a build as having a canonical Random OpenMP/CUDA
provider.

The aggregate `ASC::cpp` remains a compatibility surface and already links all
five modules, including Linalg. The inherited Random headers move to that
aggregate's public file set during M1. This keeps their physical include paths
and behavior available while making it explicit that they are not supplied by
the minimal canonical Random component.

## 4. M1 repository layout

The implementation must create or update this structure:

```text
include/asc/
  random.h
  random/
    types.h
    counter_engine.h
    distribution.h
    fill.h
    detail/
      fill_validation.h

src/random/
  CMakeLists.txt
  engines/
    philox.cc
  permutation.cc              # compatibility implementation
  sobol.cc                    # compatibility implementation

tests/random/
  CMakeLists.txt
  unit_test_engine.cc
  unit_test_distribution.cc
  unit_test_fill.cc
  release_contract_test.cc
  dependency_check.cmake
  odr_a.cc
  odr_b.cc
  odr_main.cc
```

Private helper spelling may change only when required to keep a public header
self-contained. No new public M1 operation may be added without returning to
the Lead Architect.

## 5. Canonical umbrella and namespace

`<asc/random.h>` is the canonical umbrella. It includes only the M1 types,
engine, distribution, and fill headers. It must not include any inherited
sampler header, Linalg header, compatibility Array header, aggregate umbrella,
or optional SDK.

All declarations remain in the flat `asc` namespace. There is no
`asc::random` namespace and no compatibility namespace alias.

`<asc/cpp.h>` includes `<asc/random.h>` and continues to include the inherited
Random headers separately during the compatibility window.

## 6. Key and counter model

M1 exposes two trivial fixed-width values:

```cpp
struct RandomKey {
  std::uint64_t value = 0;
};

struct RandomCounter {
  std::uint64_t subsequence = 0;
  std::uint64_t offset = 0;
};
```

Both types support value equality. In this design, "trivial fixed-width
values" means standard-layout and trivially copyable value records; it does
not require `std::is_trivial_v`. The default member initializers are
intentional so both default initialization and value initialization select
the all-zero address. Their object representation is not a file or wire
format, and M1 provides no text/binary serialization API.

The semantics are:

- a caller-supplied seed is used explicitly as `RandomKey::value`;
- a key identifies a logical stream;
- `subsequence` is the high 64 bits of the Philox counter domain;
- `offset` is the low 64 bits and identifies a logical output sample;
- selecting an output is a pure function of algorithm version, key,
  subsequence, and offset; and
- there is no hidden entropy, global counter, default stream registry, or
  mutable shared state.

M1 does not expose `Seed`, `Reset`, `Skip`, or state serialization on the
counter engine. Directly changing `RandomCounter` expresses those operations
without mutation. Entropy-based construction is deferred and, when added,
must remain explicit opt-in.

## 7. Philox4x32-10 contract

`Philox4x32_10` is a non-polymorphic compiled engine with this conceptual
surface:

```cpp
class Philox4x32_10 {
 public:
  using ResultType = std::array<std::uint32_t, 4>;

  static ResultType Generate(RandomKey key,
                             RandomCounter counter) noexcept;
  static std::uint64_t Generate64(RandomKey key,
                                  RandomCounter counter) noexcept;
};
```

The version is the Random123 Philox4x32 algorithm with ten rounds, standard
round constants, and standard key bumps. Native counter words are formed in
little-word order from `offset` followed by `subsequence`; native key words
are formed from `RandomKey::value`. `ResultType` preserves algorithm word
order. `Generate64` concatenates result word zero as the high 32 bits and word
one as the low 32 bits.

The all-zero key/counter raw-word vector is frozen as:

```text
6627e8d5 e169c58d bc57ac4c 9b00dbd8
```

Tests must also freeze consecutive and nonzero key/subsequence vectors that
are calculated independently of the production helper.

The raw integer result is bitwise stable for the same algorithm version,
fixed-width input values, and conforming unsigned-integer semantics. Any
future algorithm change requires a new named type or explicit version rather
than silently changing these outputs.

## 8. Uniform distribution contract

M1 exposes `Uniform01<T>` for exactly `float` and `double`. It is stateless and
maps a 64-bit word to the half-open interval `[0, 1)` without using a standard
library distribution.

For `float`, use the most significant 24 bits and scale by `2^-24`. For
`double`, use the most significant 53 bits and scale by `2^-53`.

Consequently:

- word zero maps exactly to positive zero;
- the documented midpoint word maps exactly to `0.5`;
- an all-one word maps to `1 - 2^-24` for `float` or `1 - 2^-53` for
  `double`;
- the result never equals one; and
- bits below the selected precision do not affect the result.

The integer-to-unit transform is bitwise guaranteed on conforming IEC 60559
binary32/binary64 implementations for the same scalar type. M1 does not offer
affine bounds, integers, Bernoulli values, or custom distributions.

`bool`, integral types, `long double`, volatile scalars, complex types,
automatic-differentiation scalars, and mixed distribution/destination types
fail constraints rather than convert silently.

## 9. Bulk fill surface

The single M1 operation has this conceptual form:

```cpp
template <WritableRandomTensor Destination, typename T>
  requires SameRandomValueType<Destination, Uniform01<T>>
Status FillRandom(const ExecutionContext& context,
                  const Destination& destination,
                  Uniform01<T> distribution,
                  RandomKey key,
                  RandomCounter counter);
```

The exact concepts may be implementation details, but they must enforce the
frozen behavior. The destination is a structural canonical TensorView-like
descriptor, not an owner. It may have any fixed compile-time rank, including
rank zero, and its `ValueType` must be exactly `float` or `double`.

The caller owns the destination storage and all state. The operation never
allocates, resizes, transfers, synchronizes, or retains the destination.

## 10. Logical order and partition guarantee

Fill enumerates logical coordinates lexicographically with the rightmost axis
varying fastest. The logical linear index is independent of physical layout.
For logical index `i`:

```text
counter_i.subsequence = counter.subsequence
counter_i.offset      = counter.offset + i
bits_i                = Philox4x32_10::Generate64(key, counter_i)
value_i               = distribution(bits_i)
```

The value is written through the destination's actual non-negative strides.
LayoutLeft, LayoutRight, and proven-unique padded LayoutStride destinations
therefore receive identical values at the same logical coordinates. Padding
holes are never touched.

Partitioning is exact: filling `N` elements once is bit-identical to filling
any ordered or reordered collection of logical partitions whose counters use
the corresponding starting offsets. M1 is serial, but this rule is the future
parallel-provider contract and makes generation independent of scheduling.

Subviews whose logical coordinate order differs from the original full view
must be given offsets that correspond to the caller's intended global logical
indices. Random does not infer a global tensor identity from a pointer.

## 11. Validation and status contract

All recoverable validation finishes before the first destination write.
Validation order is:

1. execution context/provider;
2. descriptor extents, logical size, strides, required span, available span,
   memory space, and output uniqueness;
3. successful empty-output return;
4. checked `counter.offset + logical_size - 1` arithmetic; and
5. non-null data access for a nonempty destination.

The implementation revalidates structural operands rather than assuming they
came from `TensorView::Create`. It uses checked signed 64-bit arithmetic for
extent products, maximum offsets, spans, and logical indexing.

Status mapping is frozen as follows:

| Failure | Status |
| --- | --- |
| invalid/negative/inconsistent descriptor metadata or null nonempty data | `kInvalidArgument` |
| insufficient available span | `kOutOfRange` |
| extent, stride, span, byte, or counter arithmetic overflow | `kOverflow` |
| writable mapping not proven unique | `kFailedPrecondition` |
| non-host-accessible memory or unsupported scalar/mapping capability | `kUnsupported` |
| unavailable execution provider/backend | `kUnavailable` |

Core M1 can construct only a synchronous serial context. That context selects
the Random M1 serial-reference fill. Fallback policy never permits an implicit
transfer or a different mathematical sequence.

An empty valid destination succeeds without reading its data handle and
without evaluating counter addition. A maximal offset is therefore valid for
an empty fill. A zero extent in any dimension makes the logical size zero.

Every validation failure leaves every byte of the backing storage unchanged,
including padded holes. Status diagnostics may allocate; successful engine,
distribution, and fill execution must not allocate.

## 12. Compiled and template boundary

The public Philox declarations are backed by one compiled definition in
`src/random/engines/philox.cc`. This gives the named algorithm one ODR-safe
implementation and shared-library symbol boundary.

`Uniform01<T>` and structural-view traversal remain header-visible templates.
The supported scalar set is closed by constraints, not by unexplained link
failure. Header-visible validation helpers live under `asc/random/detail` and
are not stable public customization points.

No optional provider or standard distribution implementation appears in the
canonical headers.

## 13. Compatibility disposition

The following inherited files remain installed and available through the
aggregate `ASC::cpp` compatibility surface:

- `generator.h`;
- `sampler.h` and `pseudo.h`;
- `latin.h`, `halton.h`, `hammersley.h`, and `sobol.h`;
- `normal.h` and `spherical.h`; and
- `permutation.h`.

They retain their existing source behavior and are not included by
`<asc/random.h>`. Their virtual sampler base, entropy reseeding, mutable prime
and permutation singletons, build-global layout branches, covariance-taking
normal facade, and Linalg include are compatibility characteristics, not M1
canonical guarantees.

The legacy Random regression executable links `ASC::cpp`, reflecting those
actual dependencies. Existing source that uses an inherited sampler should
temporarily link the aggregate. New source links `ASC::random` and uses the
canonical umbrella.

No inherited symbol is declared removed or deprecated by M1. Removal requires
a later breaking-release decision and migration table.

## 14. Explicitly deferred scope

Random M1 does not add or promise:

- implicit or entropy seeding;
- a mutable canonical engine, reset/skip methods, or state serialization;
- affine, integer, Bernoulli, exponential, or user-defined distributions;
- normal/transcendental bitwise portability;
- canonical Latin, Halton, Hammersley, Sobol, or scrambling APIs;
- shared immutable Sobol direction-table storage;
- spherical or multivariate-normal generation;
- covariance validation or factorization;
- precomputed-factor transform application;
- OpenMP, CUDA, asynchronous, or distributed providers;
- a public provider SPI or capability object;
- complex, long-double, custom, or AD scalar generation; or
- statistical-quality certification beyond the named algorithm contract.

These items require separate designs. In particular, Linalg computes future
covariance factors; Random will consume a validated precomputed factor without
depending on Linalg.

## 15. Build and package changes

`ASC::random` remains a compiled exported target and publicly links exactly
`ASC::array` and `ASC::core` among ASC components. Its public file set contains
only canonical M1 Random headers.

The inherited Random headers are owned by the `ASC::cpp` file set during the
compatibility window. The aggregate target already links Array, Linalg,
Random, and Core, so their inherited header requirements remain available.

Package configuration must stop treating a request for component `random` as
a request for Linalg or optional Linalg SDKs. Component `cpp` continues to
require them when those build options are enabled.

The installed Random-only consumer:

- requests `find_package(ASCCpp COMPONENTS random)`;
- includes `<asc/array.h>` to construct a canonical view;
- includes `<asc/random.h>` for generation;
- links only `ASC::random`; and
- verifies a frozen deterministic result after package relocation.

## 16. Required verification

The Verification Engineer must add minimal-target canonical executables for:

1. Philox raw-word and key/counter semantics;
2. uniform transform endpoints and ignored-bit behavior;
3. typed `float`/`double` bulk fill;
4. release-active validation and transaction behavior;
5. dependency/header residue checks; and
6. multi-translation-unit ODR use.

The test matrix includes:

- independently hard-coded Philox vectors for zero, consecutive, nonzero key,
  nonzero subsequence, and high counter inputs;
- exact uniform zero, half, maximum-below-one, and low-bit invariance cases;
- LayoutLeft, LayoutRight, rank-zero, rank-one, rectangular rank-two, and
  proven-unique padded-stride destinations;
- hole preservation and exact logical-coordinate equality across layouts;
- whole-fill versus two-way, irregular three-way, and reverse-order partition
  equivalence;
- concurrent independent fills compared with serial results;
- empty/null storage, zero dimensions, maximal empty offset, last valid
  counter, and counter overflow;
- structural foreign descriptors with null data, inconsistent metadata,
  insufficient span, non-host space, non-unique mapping, and overflow;
- poisoned pointers behind earlier validation failures;
- compile-time rejection of const/volatile output, unsupported scalar types,
  and mixed distribution/destination types;
- zero operation-time allocation after context/view setup;
- canonical header self-containment linked only to `ASC::random`;
- exact target edges, forbidden include scanning, ODR, and installed consumer;
  and
- preservation of the inherited Random regression suite through `ASC::cpp`.

Required gates after the module lands are:

```text
focused canonical default tests
full default tests including package relocation and legacy regression
full strict warnings-as-errors build and tests
exceptions-off/assertions-off applicable canonical tests
ASan/UBSan focused canonical binaries
cpplint/format and dependency/header scans
```

The repository's documented local ASan `handle_segv=0` workaround may be used
only for the known host signal-handler instability; findings must not be
suppressed.

## 17. Documentation deliverables

After implementation and test evidence exists, the Documentation Engineer
creates:

- `docs/modules/random.md`;
- `docs/migration/random.md`; and
- updates to README, API, architecture, build-system, testing,
  optional-backend, inventory, and relevant Core/Array/Linalg cross-links.

Documentation must distinguish four levels of claim:

1. Philox raw integer bitwise stability;
2. `Uniform01<float/double>` bitwise stability under its stated IEC 60559
   assumptions;
3. layout- and partition-independent bulk fill for the serial-reference path;
   and
4. inherited statistical sampler behavior, which is compatibility only and
   carries no new cross-library bitwise promise.

Examples and tests always use explicit literal keys/counters. Documentation
must not describe a deferred feature as implemented.

## 18. Completion gate

Random M1 is complete only when:

- this design predates all M1 production/test/documentation edits;
- the canonical engine, distribution, and fill match this frozen contract;
- `ASC::random` has only Array and Core ASC dependency edges;
- `<asc/random.h>` is self-contained and compatibility-free;
- legacy Random behavior remains available and its regression test is green;
- focused, full, strict, no-exception/assertion, sanitizer, dependency, ODR,
  header-isolation, allocation, and installed-consumer gates pass; and
- all Random documentation is reconciled to verified implementation behavior.

Any desire to add a normal distribution, Sobol sequence, transform, mutable
engine, provider, or compatibility removal returns to the Lead Architect as a
new milestone rather than expanding M1 during implementation.

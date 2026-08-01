# ADR 0020: Random inventory, provenance, and adapter contract

Status: Accepted for Issue 12 Feature Gate B candidate

Date: 2026-08-01

## Context

Issue 12 freezes the Random work that may proceed in Issues 13, 14, and 15.
It is an architecture and provenance milestone only. It adds no engine,
distribution, sampler, helper, adapter, target, dependency, or public API.

The source comparison is the tracked tree of
`escapetiger/MdeCpp` commit
`f6294e9079262682ce63ae7ff2d8a643e658bf5d`. Its root `LICENSE` is
GPL-3.0-only and has SHA-256
`230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809`.
The Random source headers name Yi Cai or the MdeCpp team, but have no
per-file SPDX grant. Git history attributes every inspected Random path to
the `escapetiger` account. These facts do not establish an Apache-compatible
route for the local adaptations.

The machine-readable [Random crosswalk][crosswalk] and generated
[human-readable report][report] are part of this decision. Classification
means:

- `equivalent`: current asc-cpp completely implements the selected behavior;
- `incomplete`: current asc-cpp implements only a proper subset;
- `absent`: no current behavior exists and no more specific provenance class
  applies;
- `rejected`: the source behavior or artifact is not selected;
- `clean-room-required`: the behavior is selected, but MdeCpp expression is
  unusable and implementation must follow the approved independent source;
- `permission-relicensing-required`: exact third-party material may be used
  only through the recorded compatible permission and notice route.

The frozen 33-row result has a decision identity recorded by the crosswalk,
as does the complete evidence-metadata envelope that pins repositories,
licenses, counts, and authoritative upstream artifacts. It contains 18
clean-room-required rows, three
incomplete rows, one permission-based row, and 11 rejected rows. There are no
claims that a future implementation is already equivalent or merely absent.
Each row identity hashes the ordered, length-prefixed field names and values;
the decision identity hashes sorted `id|row-identity` records. This preserves
punctuation such as the semicolon-delimited file lists without relying on
CMake list encoding. The evidence-metadata identity hashes sorted
`field|value` records plus sorted `upstream|name|url|sha256` records and
excludes only its own declared identity field.

## Source artifact audit

The complete 20-artifact inventory has identity
`f47d8ecf5cfbb689fe0643d1bbb8e772138523b68a8055cdb73dc4ae3b1cb6ff`.
The identity is SHA-256 over sorted `path|artifact-sha256` records without a
terminal newline.

| MdeCpp artifact | SHA-256 |
| --- | --- |
| `random/CMakeLists.txt` | `3e562c25682c9fd1cb47dc700d3dc69366047a9228ce61472cca7143586f7046` |
| `random/generator.h` | `efb0ddedcb6a11ed3defd85764e352f16b09f13784c15225a1b2dd19dcbc2d6b` |
| `random/permutation.h` | `9ce197d46b1dc35392abfbee31d96f2645ea773c72d9a2148ede41be8cfa5e04` |
| `random/permutation.cc` | `c1cf0ca3fc6496d971cef10a3214f06db68f9fa57ab85177c26550c9589122e1` |
| `random/sampler.h` | `21495bdbb7578b0ad201cebf03198f2a840b1ca178ab9dd61ce8cd62a777af0e` |
| `random/pseudo.h` | `293480a20a9c811c994ed9b92ca2d1e88eb1645b82b0178c5923e0de97c15049` |
| `random/latin.h` | `bdf92696b098fc6ccfcf697ec0dbf928fd98da13fd45a7d90b73ddba9fabb88b` |
| `random/halton.h` | `750787ee8bfdc4a9c15f9327b9515a1efb6279fdce3ec37827345f17cc8c4d96` |
| `random/hammersley.h` | `c70853a78a93f1840690f7093734838db3b8932114e1e054e63ddced09fc22f3` |
| `random/sobol.h` | `d77a3caac5830c6dbd155999a65497cbaf5eeaf0d01398f03fb7abe00a4fb7fd` |
| `random/sobol.cc` | `ebadbac5b74e47512642d02eac4e3d654d0aa260c3c79ec6d45e76df03f78a0a` |
| `random/normal.h` | `27cdce117132f9684bc932ce640d56b43ea45a799a1ea21f02719ffac3f78827` |
| `random/spherical.h` | `9c61d4445824b9ee04793edbb6c4dbcf6aa9fe2c0d34b83feaeb3de8f2594bce` |
| `random/data/convert_sobol_to_binary.py` | `f43157752ad613be02b0e5c2271fcd801f0f5695f6f4b85b2d17cf5a5f8ceda2` |
| `random/data/sobol.txt` | `16722f0f9978c1d4f5f0cfc67228f5417f217f4fa30bc7666a03e10d52bd3e14` |
| `random/data/sobol.bin` | `836668fdbb3b72fabc0f0d07febfa06d9b022bed59b51d8eb5e5deed32a4b3cd` |
| `tests/random/unit_test_generator.cc` | `84ecc456307776a5c63fca9ddfca4ebf0088109c73a8f32fa1792661bedc462b` |
| `tests/random/unit_test_perm.cc` | `0fe2b8f50993ded69bae47fbd1305011e1b699698a19ccd21cdf9c3cbfe8ae2f` |
| `tests/random/unit_test_sampler.cc` | `7f61b63f25b86deb3397f3ced5db0be27f7a612520ce04b1d3b233ea83e4c903` |
| `benchmarks/benchmark_random_sequences.cc` | `6b2b56f09304362e9785f8e77bd12315ada5ee7081829a2eb395ea3f765358e8` |

The hashes identify evidence and do not confer reuse permission. Source
comments attribute SplitMix and xoroshiro variants to Sebastiano Vigna and
David Blackman and PCG32 to Melissa O'Neill. The Sobol source retains a
distinctive `JVB, 24 January 2006` comment and control structure matching the
MIT-licensed Burkardt adaptation of Bennett Fox's work, but omits that
upstream author/license block. The local text and binary direction artifacts
contain no origin, version, copyright, or license statement. The binary is
551,056 bytes of generated little-endian signed 64-bit values, yet the reader
loads it as native `int64_t` from a configured source-tree path and validates
only byte count. All three MdeCpp Sobol data artifacts are rejected.

## Approved provenance routes

No MdeCpp source, test, benchmark, literal vector, table, generated data, or
prose may be copied or mechanically translated. MdeCpp is behavioral and
negative-architecture evidence only.

The selected stateful engines must be independently implemented from these
frozen authoritative identities:

| Component | Exact source and route | Frozen SHA-256 |
| --- | --- | --- |
| SplitMix64 | [Vigna fixed-increment 2015 source][splitmix], public-domain dedication; clean-room sequence version 1 | `071795a8e29978a5cbd7015ce8f7d772e7ab4631e574e9102b748fe99105ff3d` |
| xoroshiro64* | [Blackman/Vigna version 1.0][xoroshiro64], public-domain dedication; clean-room version 1 | `bfa545b28687f3c7f05d8574bc5d6727adbcc845d91db5c953f141c4652e1cba` |
| xoroshiro128+ | [Blackman/Vigna version 1.0 parameters 24,16,37][xoroshiro128], public-domain dedication; clean-room version 1 | `522f2d69a1e44a4713ae70adbda72ab40a325f1933c1a4491e629fe6240b6aa3` |
| PCG32 | [PCG minimal C 0.9][pcg], Apache-2.0 XSH-RR set-sequence contract; clean-room version 1 | `143f9cc0edc7e81064bde5ed6c5f16cbf7ca2f373d35543286f5fe4475a2eff1` |

The algorithms must use original asc-cpp expression. Test vectors must be
derived from the pinned authoritative equations or official published examples
before production implementation is inspected. Attribution and source
identities remain in public documentation. No upstream source file is vendored
for these engines.

Sobol implementation and data have separate routes:

- the recurrence and Gray-code addressing must be independently implemented
  from the published Joe/Kuo description;
- the exact `new-joe-kuo-6.21201` dataset, last updated 2010-09-16, is the
  selected direction-number input with SHA-256
  `68eedd2a4e3b659b9695e7aff0f8ac68718bcf620730fc3d3a8c65df2a067441`;
- the dataset and accompanying program are covered by the Kuo/Joe
  BSD-3-Clause-style license, frozen with SHA-256
  `9d10226b50eeb34be0ab06bfa3392c7bd1f04bf602f9af4343295d1fd003d0e3`;
- Issue 14 must retain that license in source and binary distributions, add
  `THIRD_PARTY_NOTICES`, cite Joe and Kuo, and make no endorsement claim;
- an original deterministic generator must validate the input grammar and
  dimensions and emit portable checked-in C++ data; Python is not a configure,
  build, install, or consumer dependency;
- normal builds perform no network access and no runtime data-file lookup;
- the exact input, license, generator, generated output, installed source
  data, and compiled-table identities are checked independently; and
- the MdeCpp text, binary, converter, polynomial table, and vectors remain
  forbidden even if their outputs happen to overlap.

The selected data supports dimensions 1 through 21,201. Sobol indices are
unsigned 64-bit values. Index zero is the all-zero point. Values are in
`[0,1)` and are not repaired by clamping.

## Dependency and ownership decision

The approved six-module graph does not change:

```text
ASC::random        -> ASC::core
ASC::random_dense  -> ASC::random, ASC::dense
ASC::random_sparse -> ASC::random, ASC::sparse
```

The base Random target owns only fixed-width seed/state vocabulary, engines,
scalar distributions, QMC index mathematics, and immutable table access. It
must not include or link Dense, Sparse, Expression, Utilities, a provider SDK,
or a new third-party package.

`random_dense` owns functions that accept Dense views or use Dense BLAS.
`random_sparse` owns functions that accept Sparse views, construct Sparse
owners, or use sparse structure. Neither facet enters the opposite storage
closure. Dense and Sparse continue to have no dependency on Random. No new
adapter architecture, target, or provider facet is approved.

Existing Philox4x32-10, `Uniform01`, Dense fill, exact-count Sparse generation,
and their CUDA facets retain their current public spelling and sequence
contracts. The new stateful engines and samplers are additive pre-1.0 API.

## Seed, state, stream, and version contract

- Every deterministic constructor requires explicit fixed-width seed, state,
  key/counter, stream, or index values. Examples and tests use fixed literals.
- Nondeterministic acquisition is a named explicit `Result<uint64_t>`
  operation over a caller-owned C++20 `std::random_device&`. Version 1
  supports only a source whose `min()` is zero and whose `max()` is exactly
  `UINT32_MAX`; any other range returns `kUnsupported` without invoking the
  source. A supported source is invoked exactly twice, first for the high
  32-bit word and then for the low word, and the seed is
  `(uint64_t(high) << 32) | low`. Asc-cpp never constructs that source or
  calls it from a default constructor, reset, sampler, or adapter. A thrown
  standard-library acquisition failure is translated without publishing a
  seed; calls already made cannot be rolled back.
- Time-based seeding is rejected. Applications may pass time-derived values
  as ordinary explicit seeds, but asc-cpp neither labels them entropy nor
  supplies a helper.
- SplitMix64 state is one `uint64_t`. PCG32 state is current-state plus an odd
  encoded stream increment, both `uint64_t`. xoroshiro64* state is two
  `uint32_t`; xoroshiro128+ state is two `uint64_t`. All-zero xoroshiro state
  is invalid.
- Single-seed xoroshiro construction expands with sequence-version-1
  SplitMix64. PCG32 also exposes the official two-argument state/stream
  initialization; only the low 63 stream bits select a sequence.
- `jump` and `long_jump` use only the pinned version-1 polynomials. Arbitrary
  runtime jump-polynomial machinery is not approved in Issue 13.
- Engine values are copyable value types. A copy continues identically and
  independently. Moves are equivalent to copies for trivially copyable state.
- State export/import uses exact-width public state structs carrying an
  algorithm/version tag. Unknown versions and invalid states fail before
  mutation. No byte-stream serialization schema is approved in 0.9.x.
- Engine sequence, distribution transform, sampler index/consumption, storage
  mapping, and provider parity are distinct versioned claims. A change to any
  output mapping requires a new named version and cannot silently alter an
  existing type.

Stateful engines are not safe for concurrent mutation. Independent values,
immutable state snapshots, pure Philox calls, pure QMC indexed evaluation,
and immutable Sobol table reads are safe for concurrent use. No process or
thread-local mutable engine, seed, permutation, prime, or direction cache is
allowed.

## Distribution contract

The generic generator is a nonvirtual value composition of a caller-selected
engine and distribution. It performs no allocation and owns no seed source.
Invalid distribution parameters fail before consuming engine state or
mutating a destination.

All added version-1 distributions consume an explicit canonical request for
`b` bits. Approved engines have a full unsigned 32- or 64-bit result domain.
The request invokes the engine `ceil(b / engine_bits)` times, concatenates
complete results in call order from high to low, retains the leading `b` bits,
and discards any unused low bits without caching them. Thus a float unit value
uses 24 leading bits, while a double uses 53; this is one engine call for a
64-bit engine and two for a 32-bit engine in the double case. Engine type is
therefore part of the sequence contract.

- Uniform integer results use a closed `[lower, upper]` interval and rejection
  mapping from a canonical candidate whose bit width equals the unsigned
  result width. Let `range` be the widened exact interval size. A full-width
  range returns the candidate directly. Otherwise, in unsigned candidate
  arithmetic, set `threshold = (-range) % range`, reject candidates below the
  threshold, and return `lower + candidate % range`. Full-width ranges and
  signed endpoint arithmetic are checked; modulo-only mapping is forbidden.
- Uniform real results use a half-open `[lower, upper)` interval. The version-1
  unit mapping scales the canonical 24- or 53-bit integer by `2^-24` or
  `2^-53`. The parameterized result evaluates `span = upper - lower` in the
  destination type, then `candidate = fma(span, unit, lower)`. Bounds must be
  finite and strictly ordered, and `span` must be finite; invalid input fails
  before an engine call. If rounding makes a finite candidate equal to
  `upper`, return `nextafter(upper, lower)`. Any other nonfinite or out-of-range
  candidate is a numerical failure after the documented calls and no value is
  published. Existing raw-word `Uniform01` bit mappings remain unchanged.
- Scalar normal version 1 obtains `U1` and `U2` from those unit mappings and
  computes, in order, `u_radius = 1 - U1`, `u_angle = U2`, and
  `z = sqrt(-2 * log(u_radius)) * cos(2 * pi * u_angle)`. It returns only
  `mean + standard_deviation * z`, consumes exactly two unit values per
  sample, and holds no cached sine spare. Mean must be finite and standard
  deviation finite and positive; invalid parameters fail before consumption.
  A nonfinite intermediate or final value is a numerical failure after both
  unit values have been consumed and no result is published. Version 1 uses
  float `pi = 0x1.921fb6p+1F` and double
  `pi = 0x1.921fb54442d18p+1`; the final affine step is
  `fma(standard_deviation, z, mean)`. These constants and the written operation
  order must appear in Issue 13 known-answer tests. Transcendental results are
  reproducible for the same supported math ABI but are not promised
  bit-identical across CPU/GPU or different libm versions.

The standard-library distribution algorithms are not sequence authority.
There is no cryptographic-strength claim for any accepted engine.

## QMC contract

Prime lookup, radical inverse, scrambled radical inverse, permutation fill,
and indexed QMC evaluation are stateless or operate on explicit caller spans.
No singleton cache or hidden allocation is permitted.

- Radical inverse accepts an integer base at least two and an unsigned 64-bit
  index. Version 1 initializes `value = 0` and `factor = 1` in the destination
  type. Starting from the least-significant digit, it repeatedly sets
  `factor *= 1 / base` and `value = fma(digit, factor, value)`. If final
  rounding produces exactly one, it returns `nextafter(Real{1}, Real{0})`;
  any other nonfinite or out-of-range result is a numerical failure.
- Scrambling accepts a caller-owned immutable permutation with exactly one
  occurrence of each digit and requires digit zero to map to zero, eliminating
  an implicit infinite trailing-digit convention. Permutation generation uses
  unbiased Fisher-Yates selection, an explicit engine, and caller-owned
  writable workspace.
- Latin hypercube with `n` samples assigns, for every dimension `d`, a
  permutation `p_d` of `[0,n)`. Midpoint coordinate `(i,d)` is
  `(p_d[i] + 1/2) / n`; jittered coordinate `(i,d)` is
  `(p_d[i] + U(i,d)) / n`, where `U` is the version-1 unit mapping. Explicit
  permutation generation visits dimensions in increasing order and performs
  each Fisher-Yates shuffle from `n - 1` down to 1. Jitter then consumes one
  unit value for each coordinate in sample-major order, dimension zero first.
  Arithmetic is performed in the destination type, with an exact-one result
  repaired to `nextafter(Real{1}, Real{0})`. Each dimension has exactly one
  sample in every stratum. Empty output consumes nothing; nonempty output
  requires `n > 0` and explicit engine and permutation workspace.
- Halton is an infinite indexed sequence. Coordinate `d` is the radical
  inverse of the unsigned index in the `d`th prime base, with `p_0 = 2`;
  optional digit permutations are explicit. Index zero is the all-zero point.
- A `d`-dimensional Hammersley point for total count `n > 0` and `0 <= i < n`
  has coordinate zero `i / n`; coordinate `j > 0` is the radical inverse of
  `i` in prime base `p_(j-1)`. Total count is part of every evaluation/fill
  contract. Coordinate zero is evaluated exactly as
  `static_cast<Real>(i) / static_cast<Real>(n)`; if rounding produces one,
  version 1 returns `nextafter(Real{1}, Real{0})`.
- Sobol is an unsigned 64-bit indexed sequence using the selected Joe/Kuo D6
  table. For index `i`, compute Gray code `g = i ^ (i >> 1)` and XOR the
  dimension's 64-bit direction word `v_k` for every set bit `k` of `g`.
  Float converts the high 24 result bits and double the high 53 result bits
  with the same exact powers-of-two as `Uniform01`. Dimension is zero-based in
  the API, with valid range `[0, 21201)`. Skip and reset are checked index
  assignment, not hidden replay; indexed and sequential APIs produce the same
  point, including the all-zero point at index zero.

Dense QMC fill has logical shape `[sample_count, dimension_count]`, maps
coordinate `(sample, dimension)` to that logical position, visits dimension
zero fastest for any operation whose state consumption is observable, and is
invariant under physical left/right/padded layout. The caller supplies any
permutation or point workspace named by the operation. Empty destinations
consume no state.

## Dense and Sparse adapter contract

All adapters are free functions in the existing `random_dense` or
`random_sparse` facets. They accept existing views, contexts, resources, and
explicit random state. There is no `RandomSampler` base, raw generator
ownership, virtual dispatch, implicit result allocation, or reversed storage
dependency.

Multivariate normal preparation validates finite mean, square covariance that
is exactly symmetric under scalar equality, matching dimensions, and positive
definiteness before publishing a lower-triangular factor into caller-owned
Dense workspace. No hidden symmetry tolerance is applied. Sampling accepts
the prepared factor, destination, scalar normal generator, and caller
workspace. Dense BLAS use stays in `random_dense`; base Random never sees a
Dense type. Singular or indefinite covariance is a numerical failure.

Uniform hypersphere sampling generates a Gaussian candidate in caller
workspace and normalizes only a finite nonzero norm. Dimension one consumes
one canonical 32-bit request and maps its high bit `0` to `-1` and `1` to
`+1`. Higher dimensions use versioned attempt domains and a documented finite
attempt limit; exhaustion returns failure without publishing the current
sample. Previous completed samples and engine consumption remain observable.

Sparse integration has three distinct operations:

1. **Existing-pattern value fill** accepts a mutable canonical
   `CoordinateView` or compressed view. Coordinates, offsets, and indices are
   read-only; only stored values change in canonical stored order. Generated
   zero is retained and structure is never compacted.
2. **Structure generation** writes an exact number of unique canonical
   logical ordinals or coordinates into caller-owned workspace and returns
   the next structure state/offset. It allocates no Sparse owner and consumes
   no value domain.
3. **Combined object generation** retains the current explicit
   `MemoryResource` owner construction, separate structure/value domains,
   canonicalization, exact count, zero preservation, and returned next states.

Changing a value domain cannot change structure. Changing a structure domain
cannot change the canonical value sequence for a fixed value domain and
count. Invalid shape, count, domain alias, parameter, state, offset, workspace,
or placement is detected before destination mutation or owner allocation.

## CPU, GPU, allocation, and failure contract

Issues 13 through 15 approve only the portable serial CPU path for the new
stateful engines, scalar distributions, QMC, and advanced adapters. Passing a
CUDA context to a new CPU-only adapter returns `kUnsupported` before access or
mutation. No CUDA verification row may be claimed for these additions.

The existing Philox/Uniform01 CUDA contracts remain the only approved Random
GPU operations. They stay asynchronous, device-resident, explicit-completion,
and bit-identical to their provider-free counterparts. This decision neither
extends nor weakens them.

No accepted operation silently allocates, transfers, packs, converts,
synchronizes, changes device/stream, selects a provider, or falls back. Any
owner allocation or workspace is explicit in the signature. Public failures
use existing `Status`/`Result`; no new production exception contract is
introduced. Errors include stable operation context without seed/state output
values that would disclose application randomness.

Placement, shape, parameter, state, offset, alias, and workspace validation
completes before the first engine call or destination write. A data-dependent
numerical failure after valid draws cannot roll back caller-owned engine state:
completed logical outputs remain observable, the failing output is not
published, and later outputs are untouched. No operation allocates a shadow
destination to simulate transactionality.

## Complexity and memory-traffic contract

The portable reference complexity is part of the child contract. An
optimization may reduce work only when it preserves the named sequence,
validation order, dependency boundary, and explicit resource behavior.

- Explicit seed acquisition and every stateful-engine transition use constant
  ASC-owned storage and `O(1)` work. Supported seed acquisition performs
  exactly two `random_device` calls; an unsupported source range performs
  none. A transition reads and writes only the engine's exact-width state.
  Construction and lifetime of the caller-owned `random_device` remain
  outside the operation; its calls may have implementation-defined
  operating-system latency. Asc-cpp adds no cache or allocation.
- Fixed-width real transforms and Box-Muller samples use `O(1)` work and
  storage. Uniform integer rejection has expected `O(1)` work but no finite
  worst-case attempt bound; each attempt assembles one documented fixed-width
  candidate from engine results and no destination is published before
  acceptance.
- Prime enumeration uses `O(1)` auxiliary storage and a deliberately loose
  `O(p_d * sqrt(p_d))` upper bound for the prime `p_d`. Radical inverse uses
  `O(log_base(index))` work and constant storage. Fisher-Yates permutation
  generation is `O(n)` with exactly the caller's `n`-element workspace.
- A `d`-dimensional Halton or Hammersley point performs the sum of its `d`
  radical-inverse costs and writes `d` values. A Sobol point reads at most 64
  direction words and writes one value per dimension, for `O(64d)` work and no
  mutable table storage. Latin point-set work is `O(nd)` plus `d` linear
  shuffles in caller-owned permutation workspace.
- Dense scalar or QMC fill is linear in logical element count and writes each
  logical destination once; padding is neither read nor written. Prepared
  multivariate-normal factorization is `O(d^3)` work in caller-owned `O(d^2)`
  factor storage, and each sample is `O(d^2)` with explicit `O(d)` workspace.
  A sphere attempt is `O(d)` with `O(d)` caller workspace and the documented
  finite attempt cap.
- Sparse existing-pattern value fill is `O(k)` and writes only `k` stored
  values. New structure-only exact-count selection is `O(n)` over the logical
  domain and writes `k` canonical ordinals to caller storage. The existing
  combined generator retains its documented `O(kn + k^2r)` reference upper
  bound, result-buffer allocations, and `O(r)` local coordinate storage for
  logical size `n`, stored count `k`, and rank `r`.

All byte traffic is therefore caller-visible state/table reads, named
workspace access, and destination reads or writes. CPU operations do not touch
device memory. A CUDA context supplied to a new CPU-only adapter fails before
any of that traffic occurs. Existing CUDA facets retain their separately
documented device-resident traffic and explicit completion boundary.

## Reproducible statistical verification

Known-answer and invariant tests are primary. Statistical tests use fixed
algorithm versions and seeds, so reruns are deterministic. They are health
checks, not proof of randomness and not replacements for upstream validation.

The child issues must use at least these frozen tests:

- Uniform integer/real: 65,536 samples in 16 equal bins. Pearson chi-square
  must be at most 72. For 15 degrees of freedom, the Laurent-Massart bound
  with `x = ln(10^6)` gives an upper bound below 72, so the nominal false
  rejection probability is below `10^-6` under the model.
- Scalar standard normal: 131,072 samples. Require absolute sample mean at
  most 0.02 and absolute variance error at most 0.03. These are more than
  seven asymptotic standard errors for mean and variance.
- Multivariate normal: 65,536 samples for a fixed non-diagonal SPD matrix.
  Each mean error must be at most 0.04 and each covariance error at most 0.06;
  the chosen covariance fixture must document that these bounds exceed six
  estimated standard errors.
- Unit sphere: 131,072 samples for dimensions 1, 2, 3, and 8. Per-coordinate
  mean magnitude and cross-moment magnitude must be at most 0.015; diagonal
  second moments must be within 0.02 of `1/d`; every norm must satisfy the
  scalar-type numerical tolerance.
- QMC tests use published low-index points, exact stratum/permutation
  invariants, bounds, and deterministic discrepancy-oriented comparisons.
  They do not apply probabilistic pass/fail thresholds to a deterministic
  point set.

Every statistical test records engine/version, seed/state, sample count,
bins or moments, threshold derivation, and observed statistic on failure.

## Frozen child boundaries

### Issue 13: Random engines and distributions

Required branch: `feature/13-random-generators`.

Approved components are explicit nondeterministic seed acquisition,
SplitMix64, PCG32 XSH-RR set-sequence, xoroshiro64* 1.0, xoroshiro128+ 1.0,
value generator composition, uniform integer/real, and scalar Box-Muller
normal. Planned product files are existing `engine.h/.cc` and
`distribution.h/.cc` plus `seed.h/.cc` and `generator.h`. Tests are
`seed_test.cc`, `stateful_engine_test.cc`, `generator_test.cc`,
`distribution_test.cc`, deterministic `statistical_test.cc`, isolated package
consumers, and `benchmarks/random/random_benchmark.cc` with correctness guards.

Non-goals are QMC, multivariate normal, hypersphere, storage adapters, GPU
implementation, cryptographic claims, time seeding, arbitrary jump-polynomial
machinery, byte serialization, and any copied upstream or MdeCpp source.

### Issue 14: Quasi-random samplers

Required branch: `feature/14-random-qmc`.

Approved components are stateless prime/radical-inverse helpers, explicit
digit permutations, and storage-neutral Latin hypercube, Halton, Hammersley,
and Sobol algorithms. Planned product/data files are `quasi.h/.cc`, the pinned
Joe/Kuo input and license under `data/random`, an original offline generator
under `tools/random`, checked-in generated table source, and
`THIRD_PARTY_NOTICES`. Tests cover published points, dimensions, indices,
skips/resets, endpoints, permutation/stratum invariants, data and generated
hashes, install/relocation consumers, allocation, concurrency, and
correctness-guarded additions to `benchmarks/random/random_benchmark.cc`.

Non-goals are MdeCpp/Burkardt source or data reuse, runtime file lookup,
networked builds, a Python build dependency, mutable caches, multivariate
normal, hypersphere, sparse integration, and GPU implementation.
Dense and Sparse storage integration are also excluded; the approved Dense QMC
adapters belong to Issue 15.

### Issue 15: Advanced samplers and storage adapters

Required branch: `feature/15-random-samplers`.

Approved components are generic pseudo fill, prepared multivariate-normal
Dense sampling, uniform hypersphere Dense sampling, Dense adapters for the
Issue 14 Latin/Halton/Hammersley/Sobol algorithms, existing-pattern Sparse
value fill, structure-only Sparse generation, and combined Sparse object
generation. Planned product changes are confined to existing
`random/dense.h`, `random/sparse.h`, and `src/random/CMakeLists.txt` unless a
private implementation file is needed. Tests are
`random_dense/advanced_sampler_test.cc`,
`random_dense/qmc_fill_test.cc`, `random_sparse/random_adapter_test.cc`,
dependency/public-header checks, package/relocation consumers, deterministic
statistics, allocation/failure tests, and correctness-guarded additions to the
existing Random storage benchmark.

Non-goals are a sampler hierarchy, new storage protocol, reversed dependency,
new factorization dependency, hidden workspace, GPU implementation, sparse
density/Bernoulli mode, compressed-output generation, or a public break.

## Consequences and stop conditions

The exact child API spelling may be refined only within the named files and
semantics without changing ownership, dependencies, provenance, state
mapping, or compatibility. Implementation must stop and return to Gate A if
it requires:

- a seventh module, new target edge, external package, provider, or SDK;
- a public break to existing Philox, Uniform01, Dense fill, Sparse generation,
  or CUDA behavior;
- a sampler base class, hidden mutable cache, new storage customization
  protocol, or Dense/Sparse dependency reversal;
- source-derived work outside the exact pinned permissive routes;
- direction data other than the exact approved Joe/Kuo artifact;
- hidden allocation, transfer, synchronization, provider choice, or fallback;
  or
- a CPU/GPU reproducibility claim without actual declared-backend evidence.

[crosswalk]: ../random-crosswalk.yaml
[pcg]: https://www.pcg-random.org/download.html
[report]: ../../../random-crosswalk.md
[splitmix]: https://prng.di.unimi.it/splitmix64.c
[xoroshiro64]: https://prng.di.unimi.it/xoroshiro64star.c
[xoroshiro128]: https://prng.di.unimi.it/xoroshiro128plus.c

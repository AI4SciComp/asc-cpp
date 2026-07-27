# Random migration

> **Superseded historical document.** This file describes the deleted implementation at historical HEAD `33b261ea33616a6395c4ad3b20646093103344f7`; it is retained only for auditability and is not current API, build, package, or implementation guidance. See the [approved Stage A six-module blueprint](../development/asc-cpp-architecture/architecture-blueprint.md).

Random M1 adds one canonical deterministic generation path while moving the
inherited MdeCpp Random surface to aggregate compatibility ownership. It is not
a source-compatible rewrite of every sampler.

## Milestone status

| Milestone | Status | Scope |
| --- | --- | --- |
| Random M1 | Implemented | `RandomKey`, `RandomCounter`, `Philox4x32_10`, `Uniform01<float/double>`, and serial canonical-view `FillRandom` |
| Distribution milestone | Planned | Normal and other transforms with fixed-consumption and portability contracts |
| Sequence milestone | Planned | Canonical Halton/Hammersley/Sobol state, indexing, scrambling, and immutable tables |
| Transform milestone | Planned | Pre-factored multivariate transforms and spherical generation |
| Provider milestone | Planned after Core providers | OpenMP/CUDA generation conforming to the M1 logical partition contract |
| Pre-1.0 cleanup | Planned | Remove inherited APIs only after replacements, downstream migration, and a breaking-release table |

M1 does not deprecate or remove an inherited Random symbol.

## Family classification

| Inherited API or mechanism | Canonical direction | M1 compatibility disposition |
| --- | --- | --- |
| `Splitmix64`, `Pcg32`, and xoroshiro engines | Explicit named/versioned engines with complete state semantics | Remain aggregate compatibility; canonical M1 adds only stateless counter-addressed Philox |
| `DeviceSeed` and `TimeSeed` | Explicit opt-in entropy helper | Deferred; no canonical entropy API |
| `Generator`, `UniformGenerator`, `NormalGenerator` | Separate explicit engines and ASC-owned distribution transforms | Remain compatibility; `std::uniform_*`/`std::normal_distribution` output is not a canonical reproducibility contract |
| `RandomSampler` ownership hierarchy | Value/reference composition or a separately justified runtime interface | Remains compatibility; no canonical sampler base |
| `PseudoSampler` | Counter-addressed `FillRandom` for unit-uniform canonical views | Unit-uniform bulk use may migrate; other generator/distribution behavior remains compatibility |
| `LatinSampler` | Reviewed stratified-sampling contract using explicit state and real descriptors | Deferred; compatibility unchanged |
| `HaltonSampler` and `HammersleySampler` | Explicit dimension/index/offset and immutable scrambling state | Deferred; compatibility unchanged |
| `SobolSampler` and compiled direction data | Random-access/explicit-state sequence with shared immutable table | Deferred; compatibility implementation and compiled data remain available |
| `NormalSampler(mean, covariance)` | Linalg-created validated factor plus Random transform | Deferred; covariance-taking facade remains aggregate compatibility |
| `SphericalSampler` | Reviewed normal-consumption and normalization transform | Deferred; compatibility unchanged |
| `Prime` and `LowDiscrepancyPermutation` singletons | Immutable compiled data or caller-owned synchronized state | Deferred; mutable singleton behavior remains compatibility |

Do not emulate a deferred canonical family by routing a canonical view through
a legacy array, standard distribution, mutable singleton, or hidden copy.

## Choosing the M1 path

Use canonical Random M1 when all of the following hold:

- output is `float` or `double` in `[0, 1)`;
- storage can be represented by a writable canonical `TensorView`-like
  descriptor;
- host-accessible synchronous serial generation is sufficient;
- the caller can supply an explicit literal key, subsequence, and offset; and
- layout/partition-independent logical output is required.

Keep the complete call chain on aggregate compatibility when it needs a
standard-library distribution, mutable sequential generator, Latin or quasi-
random sampler, normal/spherical output, covariance input, legacy
`DenseMArray`, mirrored/device storage, or the process-wide `Device` path.

## Component and include migration

Canonical code changes its package and includes to:

```cmake
find_package(ASCCpp REQUIRED COMPONENTS random)
target_link_libraries(my_target PRIVATE ASC::random)
```

```cpp
#include <asc/array.h>
#include <asc/random.h>
```

`<asc/array.h>` supplies canonical owners/views; `<asc/random.h>` supplies
generation. The Random umbrella deliberately does not re-export Array
construction APIs.

Compatibility code keeps its inherited narrow include but links the aggregate:

```cmake
find_package(ASCCpp REQUIRED COMPONENTS cpp)
target_link_libraries(my_target PRIVATE ASC::cpp)
```

```cpp
#include <asc/random/halton.h>  // Example compatibility header.
```

The physical inherited paths remain installed through the `ASC::cpp` public
file set. Requesting package component `random` no longer requests Linalg or
its optional SDK dependencies and does not promise inherited headers.

## State migration

Legacy `Generator` and sampler objects carry mutable engine/distribution state.
Several sampler constructors call `Reset`, which may seed through
`std::random_device`; the word `Reset` does not have one consistent rewind
meaning across all inherited families.

Canonical M1 has no mutable state object. Replace implicit construction and
reset/skip calls with explicit values:

```text
seed or stream identity -> RandomKey::value
independent stream part -> RandomCounter::subsequence
sample position         -> RandomCounter::offset
```

Store those integer fields in the application's experiment/checkpoint schema,
not the in-memory object representation of the structs. Reproducing an output
also requires the named `Philox4x32_10` algorithm version and scalar transform
type.

Changing a key selects a different logical stream. Changing `subsequence`
selects another 64-bit counter region. Adding to `offset` expresses direct
skip/random access. There is no global stream registry or entropy fallback.

## Uniform-generation migration

The canonical unit transform is not specified as equivalent to a legacy
`std::uniform_real_distribution`. Even when both return `[0, 1)`, their bit
consumption and exact values can differ.

Migrate only when the intended contract is the new one:

- `float`: top 24 bits scaled by `2^-24`;
- `double`: top 53 bits scaled by `2^-53`;
- lower discarded bits have no effect;
- zero, half, and maximum-below-one endpoints are exact under the documented
  IEC 60559 assumptions.

Applications that require `[a,b)`, integral bounds, normal values, or a custom
distribution keep the legacy implementation or provide their own separately
reviewed layer; M1 does not silently scale or convert.

## Destination migration

`PseudoSampler::Sample` accepts legacy nominal dense types. Canonical
`FillRandom` accepts structural writable canonical views, not owners:

1. construct checked canonical extents and a left, right, or unique
   non-negative-stride mapping;
2. create/own storage through canonical Array, or validate external storage
   with `TensorView::Create`;
3. obtain a writable view explicitly from a `Tensor` owner;
4. create a serial `ExecutionContext`;
5. pass matching `Uniform01<float>` or `Uniform01<double>`, key, and counter;
6. propagate the returned `Status`.

Keep the owner or external allocation alive for the synchronous call.
`FillRandom` will not resize a destination, allocate missing storage, transfer
a mirror, or synchronize legacy state. Never wrap the same allocation in both
canonical `Buffer<T>` ownership and compatibility `Memory<T>` ownership.

## Layout and partition migration

Legacy samplers may branch on the build-global `DefaultLayout`. Canonical fill
uses each destination's actual descriptor and defines sequence position by
logical coordinates, with the rightmost axis varying fastest.

Consequences for migration:

- changing only physical left/right/stride layout does not change the value at
  a logical coordinate;
- padding holes are preserved;
- filling a partition uses the full tensor's intended logical starting offset;
- scheduling partitions in another order does not change results; and
- a subview with a different local coordinate order needs an explicitly
  adjusted counter mapping.

Do not derive a counter from a pointer or physical byte offset. Random does not
infer allocation identity or a global tensor coordinate system.

## Reproducibility migration

Record the guarantee actually required:

| Level | M1 promise |
| --- | --- |
| Raw engine | Bitwise stable Philox words for fixed algorithm version, key, subsequence, and offset |
| Unit transform | Bitwise stable `float`/`double` value under the stated IEC 60559 representation |
| Bulk fill | Same logical coordinates receive identical values across supported layouts and valid partition schedules on the serial-reference path |
| Inherited samplers | No new cross-standard-library, cross-compiler, or cross-backend bitwise promise |

Legacy tests that check ranges, moments, coverage, or equality of two instances
do not establish a frozen algorithm vector. Canonical regression data should
store independently calculated raw vectors and exact transform values.

## Failure migration

Legacy samplers commonly use compatibility verification macros and may access
storage while generating. Canonical fill returns `Status` and validates the
complete operation before writing.

Handle `kInvalidArgument`, `kOutOfRange`, `kOverflow`,
`kFailedPrecondition`, `kUnsupported`, and `kUnavailable` separately where the
application can correct the condition. A failed operation leaves all backing
bytes unchanged. Empty valid output succeeds without touching a null data
handle or evaluating counter addition; therefore a maximal offset remains
valid for an empty fill.

Unsupported scalar, const/volatile output, and mixed distribution/destination
types are compile-time failures rather than runtime conversions.

## Normal and Linalg migration

The inherited covariance-taking `NormalSampler` owns Cholesky work and includes
legacy Linalg. It remains usable only through aggregate compatibility in M1.

The target direction is composition:

```text
Linalg: validate/factor covariance -> explicit factor
Random: generate canonical normal values -> apply validated factor
```

Neither half is canonical Random M1. Do not duplicate covariance
factorization inside Random to preserve the old constructor or add a Random-to-
Linalg dependency. A later reviewed transform milestone will define shape,
alias, numerical-failure, and normal-consumption rules together.

## Compatibility window

Inherited Random APIs remain available during the bounded pre-1.0 transition.
Removal requires:

1. a canonical replacement for the ordinary family use case;
2. documented algorithm/state/reproducibility and numerical contracts;
3. migration of known downstream users;
4. focused canonical and inherited regression coverage;
5. provider, package, dependency, sanitizer, and configuration gates; and
6. a published breaking release and removal table.

Random M1 itself declares no inherited header deprecated and no removal
release.

## Provenance

The compatibility headers and Sobol data were selected and adapted from
MdeCpp as recorded in [the migration inventory](inventory.md). Canonical M1 is
a new asc-cpp contract derived from the approved architecture and frozen
[`random_design.md`](../design/random_design.md); it is not a rename of the
MdeCpp sampler hierarchy.

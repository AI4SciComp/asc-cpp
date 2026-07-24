# Random module

`ASC::random` owns reproducible random-bit generation, deterministic scalar
transforms, and bulk generation over canonical Array views. Random Milestone 1
(M1) deliberately establishes one small, exhaustively specified path instead
of promoting the inherited sampler hierarchy to canonical status.

The contract below is frozen by
[`random_design.md`](../design/random_design.md) and implemented. Focused,
full, strict, sanitizer, dependency, ODR, allocation, compatibility, and
installed-package gates have passed.

| Surface | M1 status | Intended use |
| --- | --- | --- |
| `RandomKey` and `RandomCounter` | Canonical M1 | Explicit logical stream and sample address |
| `Philox4x32_10` | Canonical M1 | Versioned counter-based integer generation |
| `Uniform01<float>` and `Uniform01<double>` | Canonical M1 | ASC-owned half-open unit-interval transform |
| `FillRandom` | Canonical M1 | Deterministic serial fill of canonical writable views |
| Inherited generators and samplers | `ASC::cpp` compatibility | Existing MdeCpp-derived callers |
| Mutable canonical engines, normal/Sobol/transforms, parallel providers | Deferred | Require separately frozen contracts |

M1 has no hidden entropy, global stream registry, mutable engine state, or
standard-library distribution dependency. Every generated value is selected
by an explicit algorithm, key, subsequence, and offset.

## Include and component

New code uses the canonical umbrella and the minimal component:

```cpp
#include <asc/array.h>
#include <asc/random.h>
```

```cmake
find_package(ASCCpp REQUIRED COMPONENTS random)
target_link_libraries(my_target PRIVATE ASC::random)
```

`<asc/random.h>` includes only the canonical M1 types, engine, distribution,
and fill headers. It does not include inherited sampler headers, legacy Array
types, Linalg, the aggregate, or an optional SDK. Array ownership and view
construction remain explicit through `<asc/array.h>`.

`ASC::random` is a compiled-plus-template component with direct public
dependencies on `ASC::array` and `ASC::core`. It has no Linalg dependency.
Existing code using an inherited sampler temporarily links `ASC::cpp`; the
aggregate separately installs and exposes those compatibility headers.

The canonical Random headers and Random-owned implementation include no
optional SDK. In an optional-backend build, the current Core target can still
export OpenMP/CUDA requirements and the shared build helper can use that
toolchain for dependent compiled sources. Those transitive package
requirements are a Core/provider-hardening limitation, not a canonical Random
provider or a change to the M1 sequence.

The canonical call sequence is:

```cpp
#include <asc/array.h>
#include <asc/random.h>

#include <utility>

int main() {
  using Shape = asc::Extents<4>;
  using Values = asc::Tensor<double, Shape>;

  auto values_result = Values::Create(Shape{});
  if (!values_result.ok()) return 1;
  Values values = std::move(values_result).value();

  auto view_result = values.View();
  if (!view_result.ok()) return 1;

  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  const asc::Status status = asc::FillRandom(
      context, view_result.value(), asc::Uniform01<double>{},
      asc::RandomKey{0x4d315f6578616d70ULL},
      asc::RandomCounter{2, 16});
  if (!status.ok()) return 1;

  return view_result.value()(0) >= 0.0 &&
                 view_result.value()(0) < 1.0
             ? 0
             : 1;
}
```

The literal key, subsequence, and offset are part of the example's result
identity. Production experiment metadata should record them with the named
algorithm and scalar transform.

## Key and counter

The public address types are trivial fixed-width values:

```cpp
struct RandomKey {
  std::uint64_t value = 0;
};

struct RandomCounter {
  std::uint64_t subsequence = 0;
  std::uint64_t offset = 0;
};
```

A key names a logical stream. `subsequence` is the high 64 bits of the native
Philox counter domain; `offset` is the low 64 bits and names one logical output
sample. Both types compare by value.

The default member values are deterministic zero values, not requests for
entropy. Applications and examples should still spell literal keys and
counters so experiment state is visible at the call site.

M1 provides no wire/text representation for these structs. Their object
layout must not be persisted. It also has no `Seed`, `Reset`, `Skip`, or state
serialization method: selecting another `RandomKey` or `RandomCounter`
directly expresses that operation without mutable state.

## Philox4x32-10

`Philox4x32_10` is a non-polymorphic counter engine with compiled definitions:

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

It implements the named Random123 Philox4x32 algorithm with ten rounds,
standard multipliers, round constants, and key bumps. Counter words are formed
in little-word order from `offset` followed by `subsequence`; key words are
formed from `RandomKey::value`. `Generate` preserves native algorithm word
order. `Generate64` places result word zero in the high 32 bits and word one in
the low 32 bits.

The frozen all-zero raw-word vector is:

```text
6627e8d5 e169c58d bc57ac4c 9b00dbd8
```

For the same named algorithm version and fixed-width inputs, raw integer output
is bitwise stable under conforming unsigned-integer semantics. A future
algorithm revision must use a new name or explicit version; it cannot silently
change `Philox4x32_10` output.

This is an algorithm-identity guarantee, not a statistical certification for
every scientific application.

## Uniform unit transform

`Uniform01<T>` is stateless and accepts exactly `float` or `double`. It maps a
64-bit word to `[0, 1)` without calling `std::uniform_real_distribution`:

- `float` uses the most significant 24 bits and scales by `2^-24`;
- `double` uses the most significant 53 bits and scales by `2^-53`;
- zero maps to positive zero;
- the documented midpoint maps exactly to `0.5`;
- an all-one word maps to `1 - 2^-24` or `1 - 2^-53`; and
- discarded low bits do not affect the result.

The transform is bitwise stable for the same supported scalar type on
conforming IEC 60559 binary32/binary64 implementations. That statement is
narrower than Philox's integer guarantee and depends on the stated floating-
point representation.

Boolean, integral, `long double`, volatile, complex, automatic-
differentiation, and mixed distribution/destination types fail constraints
rather than converting silently. M1 provides no affine bounds, integer range,
Bernoulli, normal, or user-defined distribution interface.

## Bulk fill

`FillRandom` takes a mandatory `ExecutionContext`, a writable structural
canonical view, a matching `Uniform01<T>`, and explicit key/counter values:

```cpp
Status FillRandom(const ExecutionContext& context,
                  const WritableRandomTensor& destination,
                  Uniform01<T> distribution,
                  RandomKey key,
                  RandomCounter counter);
```

The signature above summarizes the constrained template surface; it is not a
type-erased runtime interface. The destination may have any fixed compile-time
rank, including rank zero. Its non-const `ValueType` must exactly match the
distribution scalar and be `float` or `double`.

The caller owns storage and keeps it alive for the synchronous call.
`FillRandom` does not allocate, resize, transfer, synchronize, retain the view,
or consult the legacy process-wide device.

## Logical order and layouts

Logical coordinates are enumerated lexicographically with the rightmost axis
varying fastest. If `i` is that logical linear index, the generated value is:

```text
counter_i = {counter.subsequence, counter.offset + i}
bits_i    = Philox4x32_10::Generate64(key, counter_i)
value_i   = Uniform01<T>{}(bits_i)
```

The traversal writes through the destination's actual non-negative strides.
Left, right, and proven-unique padded stride mappings therefore receive the
same value at the same logical coordinate. Padding holes are not part of the
logical sequence and are never touched.

This rule is independent of the legacy build-selected `DefaultLayout`.
Subview pointers do not carry a global tensor identity: a caller filling a
subview supplies the counter offset corresponding to the intended global
logical indices.

## Partition guarantee

For one key and subsequence, a full `N`-element fill is bit-identical to any
ordered or reordered collection of non-overlapping logical partitions whose
starting counter offsets match their positions in the full sequence. This is
true even though M1 has only a serial provider.

The guarantee is about logical partitioning, not arbitrary physical byte
ranges. A partition whose local coordinate order differs from the full view
must use offsets chosen for that local-to-global mapping.

The four reproducibility levels must remain distinct:

1. Philox raw integer output is bitwise stable by named algorithm version.
2. `Uniform01<float/double>` is bitwise stable under its IEC 60559 assumptions.
3. Serial-reference fill is layout- and partition-independent under the
   logical-index rule above.
4. Inherited statistical samplers gain no new cross-library or cross-backend
   bitwise promise from M1.

## Validation and transaction behavior

All recoverable validation finishes before the first write. A failure leaves
the complete backing span unchanged, including padded holes.

Validation checks the execution provider first, then descriptor metadata,
logical size, actual strides, required and available spans, memory space, and
output uniqueness. Valid empty output succeeds without reading the data handle
or adding to the counter. Nonempty output then validates counter range and
data access.

| Condition | Status code |
| --- | --- |
| Negative, inconsistent, or otherwise invalid descriptor metadata; null nonempty data | `kInvalidArgument` |
| Available span smaller than the mapping requires | `kOutOfRange` |
| Extent, stride, span, byte-range, logical-index, or counter overflow | `kOverflow` |
| Writable mapping not proven unique | `kFailedPrecondition` |
| Non-host storage or unsupported scalar/mapping capability | `kUnsupported` |
| Requested execution provider unavailable | `kUnavailable` |

Rank and scalar-authority errors are compile-time constraint failures. M1's
valid serial context selects synchronous serial-reference generation. There is
no implicit fallback transfer.

## Compatibility boundary

The following MdeCpp-derived headers remain available through the aggregate
`ASC::cpp` compatibility surface, but are not part of `ASC::random`:

- `<asc/random/generator.h>`;
- `<asc/random/sampler.h>` and `<asc/random/pseudo.h>`;
- `<asc/random/latin.h>`, `<asc/random/halton.h>`,
  `<asc/random/hammersley.h>`, and `<asc/random/sobol.h>`;
- `<asc/random/normal.h>` and `<asc/random/spherical.h>`; and
- `<asc/random/permutation.h>`.

Those headers retain their existing engines, standard-library distributions,
virtual sampler base, entropy-reset behavior, mutable prime/permutation
singletons, build-global layout branches, legacy arrays, compiled Sobol data,
and covariance-taking normal facade. `normal.h` includes legacy Linalg and is
one reason compatibility belongs in the aggregate.

No inherited header or symbol is deprecated or removed by M1. Existing code
should include the same narrow header if desired but link `ASC::cpp` during the
compatibility window. Passing the inherited Random regression suite
characterizes that surface; it does not make it canonical.

## Deferred work

Later reviewed milestones may add mutable caller-owned engines, explicit
entropy helpers, more scalar distributions, canonical quasi-random sequences,
shared immutable Sobol tables, normal and spherical generation, validated
pre-factored transforms, or parallel providers.

Normal/transcendental portability needs a fixed-consumption and math contract.
Future covariance factorization belongs to Linalg; Random will accept a
validated precomputed factor without acquiring a Linalg dependency. OpenMP,
CUDA, asynchronous, distributed, custom-scalar, AD, provider-SPI, and
capability-object work is not part of M1.

See [Random migration](../migration/random.md) for old-to-new guidance and the
frozen [`random_design.md`](../design/random_design.md) for the complete M1
contract.

# ADR 0007: signed 64-bit metadata and compile-time rank

Status: Proposed at Architecture Checkpoint A

## Context

Historical metadata was partly 32-bit. Scientific local and distributed
adapters need wider values, while provider APIs require checked narrowing.
Rank-zero, zero extents, mixed extents, and overflow require one shared
contract.

## Decision

- `index_t`, `extent_t`, `stride_t`, and `nnz_t` are signed 64-bit.
- Runtime `rank_t` is unsigned 32-bit; template rank parameters use
  `std::size_t`.
- Allocation byte counts use `std::size_t`.
- Valid extents are non-negative. A dynamic compile-time extent uses the
  distinct signed sentinel `-1`.
- Core supplies compile-time-rank extents with any mixture of static and
  dynamic dimensions.
- Rank zero is a scalar descriptor with logical size one.
- Any zero extent gives logical size zero.
- Products, sums, offsets, spans, byte sizes, and conversions are checked.
- Provider 32/64-bit indices and index bases are explicit capabilities;
  narrowing/base conversion cannot be silent.
- Fully runtime-rank numerical owners are deferred until a concrete downstream
  need and code-size/dispatch study.

Core owns extents/shape vocabulary; dense and sparse own layout/format
mappings.

## Consequences

This is source/ABI-breaking relative to historical `int` APIs, for which no
compatibility is promised. Signed metadata allows negative input diagnosis
before unsigned conversion.

## Verification

Test rank zero, zero and negative extents, values above `INT_MAX`, every
overflow boundary without allocation, coordinate/span formulas, and provider
narrowing/base conversion.

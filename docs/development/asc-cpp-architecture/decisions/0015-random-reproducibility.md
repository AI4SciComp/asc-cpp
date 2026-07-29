# ADR 0015: explicit counter random and separate storage generation facets

Status: Proposed at Architecture Checkpoint A

## Context

Scientific random results require explicit state and parallel partitioning.
Historical random mixed engines, storage, mutable caches, entropy seeding,
algebra, and unresolved Sobol data.

## Decision

Base `ASC::random` depends only on core. Its first approved algorithm contract
is Philox4x32-10 with explicit fixed-width key/counter/stream/subsequence/
offset vocabulary. The raw result is a pure function of algorithm version and
state. There is no hidden entropy, global/thread-local engine, default engine,
or mutable pool.

Implementation and vectors are independently derived from an approved primary
paper/specification or separately approved authoritative permissive upstream.
MdeCpp/deleted asc-cpp source, tests, and literals are not copied.

The first scalar transforms are versioned `Uniform01<float>` and
`Uniform01<double>` with exact bit-selection/scaling rules. Standard-library
distribution implementations are not sequence guarantees. Entropy acquisition,
normal/rejection distributions, and state serialization are separately gated.

Reproducibility claims are independent:

1. raw engine bits;
2. distribution transform;
3. logical coordinate-to-counter fill;
4. provider conformance;
5. serialized-state compatibility.

`ASC::random_dense` fills caller-provided dense destinations in a fixed logical
order independent of physical layout and partition. `ASC::random_sparse`
initially generates an exact number of unique coordinates without replacement,
in canonical order, with independent structure and value streams. Sparse
density/Bernoulli mode is deferred.

CPU/GPU identity is claimed only after bit-vector hardware evidence. Sobol is
blocked pending implementation and direction-data provenance.

## Consequences

Exact sequences are versioned API. Variable-consumption distributions need a
substream/attempt mapping and cannot reuse simple sequential consumption.
Random facets are separately consumable and never create a base storage edge.

## Verification

Independently derive vectors; test copy/state/counter overflow, layout/stride/
partition/thread equivalence, sparse uniqueness/order/stream independence, no
hidden allocation/transfer, and CPU/GPU bits only where promised.

# Random completion audit

Status: unreleased `0.9.0` Issue 16 Feature Gate B candidate

Date: 2026-08-02

Required branch: `test/16-random-completion`.

Issue 16 independently audits the Random work completed by Issues 12 through
15. It adds no public API, sampler, algorithm, backend, target, dependency,
storage protocol, or compatibility behavior.

## Audited inventory and disposition

The machine-readable [Random crosswalk][crosswalk-source] pins 20 inspected
MdeCpp source artifacts and 33 decisions. The completed inventory resolves to
22 equivalent and 11 rejected rows, with no incomplete, absent,
clean-room-pending, or permission/relicensing-pending row.

Accepted rows are `RND-001`, `RND-003` through `RND-014`, `RND-017` through
`RND-022`, and `RND-026` through `RND-028`. The generator now verifies that
every accepted row names an existing ASC API, every named implementation,
header, test, benchmark, consumer, data, generator, license, and notice path
exists, and applicable source/test files occur in their owning CMake
registration.

Rejected rows are `RND-002`, `RND-015`, `RND-016`, `RND-023` through
`RND-025`, and `RND-029` through `RND-033`. Each retains an explicit reason:
implicit time seeding, post-hoc clamping, virtual sampler ownership, the
unlicensed MdeCpp Sobol artifacts/converter, GPL-covered tests and benchmark,
or monolithic packaging. The audit scans Random product, data, tool, test, and
benchmark paths for the frozen rejected hashes and forbidden artifacts.

The generated [Random crosswalk][crosswalk-report] must exactly match its YAML
source. The 33-row decision identity remains
`3a54b6ab7d39cc2a6936c8fa39cccdbea589a5545509f75f0a20d19b6ed66bd4`.
Issue 16 freezes evidence metadata identity
`2890bf8fce4a7d45ef4860e2778e174f580f42eb9f3a28f02f5125c2d6e83280`
against destination baseline
`0aef277789b6204e580b63da95ca6ed5d5f4829f`.

## Provenance and license resolution

MdeCpp remains a GPL-3.0-only behavioral catalogue under the conservative
working assumption. No MdeCpp implementation, test structure, literal vector,
benchmark, converter, generated file, table, or prose is reusable in the
Apache-2.0 destination. The completion audit preserves the accepted routes:

- original ASC composition for value generators, storage adapters, tests, and
  benchmarks;
- clean-room implementation from pinned public-domain or Apache-compatible
  SplitMix64, xoroshiro, and PCG specifications and published mathematical
  definitions;
- clean-room QMC recurrence using the separately licensed Joe--Kuo D(6)
  direction input; and
- retained Joe--Kuo source, license, generator, generated-table, notice,
  install, and compiled-table checksums.

The MdeCpp Sobol text/binary data, converter, Burkardt-derived implementation,
and local vectors remain rejected. The [provenance review][provenance] records
the authorship, authoritative URLs, immutable hashes, license boundary, and
notice obligations. `THIRD_PARTY_NOTICES` grants no right to MdeCpp content.

## Semantics and ownership

The [Random module guide][module] is the semantic authority for every accepted
operation. It records algorithm and version, seed/state construction, stream
and counter mapping, draw order, reproducibility domain, thread-safety,
CPU/GPU support, storage and workspace ownership, failure publication, and
complexity. In particular:

- all deterministic construction is explicit and fixed-width;
- mutable engines/generators are caller-owned and not safe for concurrent
  mutation, while independent copies and pure indexed functions are safe;
- sequence mappings are versioned separately for engines, distributions,
  samplers, storage adapters, and GPU parity;
- portable additions from Issues 13 through 15 are serial CPU operations;
  CUDA contexts are rejected before access or mutation; and
- the older Philox/`Uniform01` CUDA facets retain only their exercised
  asynchronous device-resident contracts.

No operation silently introduces global or thread-local state, allocation,
transfer, packing, synchronization, provider selection, or fallback.

The module graph remains:

```text
ASC::random        -> ASC::core
ASC::random_dense  -> ASC::random, ASC::dense
ASC::random_sparse -> ASC::random, ASC::sparse
```

Base Random headers contain no Dense or Sparse include. The Dense and Sparse
facets do not import one another, and storage modules do not depend on Random.
The completion test checks these header boundaries directly and requires the
repository dependency-manifest test in the registered evidence set.

## Verification coverage

The completion test inventories the registered checks rather than replacing
them. Its required CPU evidence includes:

- authoritative and independently derived engine/distribution/QMC
  known-answer tests;
- state, stream, draw-count, layout, sparsity, workspace, failure, and
  allocation invariants;
- invalid parameter, range, overflow, placement, alias, and unsupported
  context tests;
- fixed-seed scalar-normal, uniform, multivariate-normal, and unit-sphere
  statistical tests;
- self-contained and exceptions-disabled public-header compilation;
- build-tree and installed/relocated Random, Random Dense, and Random Sparse
  consumers;
- component aggregation, installed artifact, header-manifest, documentation,
  and downstream tests; and
- correctness-guarded, allocation-observed scalar/QMC/storage benchmarks.

When CUDA is enabled, registration of the provider headers, raw Random runtime
and benchmark, Dense/Sparse runtime boundaries, priority-collision test, and
all three provider consumer pairs is additionally mandatory. A real-device
run is required for a GPU verification claim; forced-no-device behavior is
only negative evidence.

Statistical workloads are deterministic health checks, not proof of
randomness. Uniform uses 65,536 samples and a chi-square threshold of 72 for
15 degrees of freedom, whose documented Laurent--Massart bound is below a
`10^-6` nominal false-rejection probability. Scalar normal uses 131,072
samples and thresholds over seven asymptotic standard errors. Multivariate
normal uses 65,536 samples; mean/covariance thresholds exceed nine/eight
largest fixture standard errors. Sphere tests use 131,072 samples in
dimensions 1, 2, 3, and 8, with the shared mean threshold exceeding five
dimension-one standard errors. Exact vectors and invariants remain the primary
evidence.

## Reproducing the audit

Configure with the released ASCCMake package, build with warnings as errors,
and run the complete suite:

```sh
cmake -S . -B build/issue-16-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASCCMake_DIR=/absolute/path/to/ASCCMake-0.1.0
cmake --build build/issue-16-debug --parallel
ctest --test-dir build/issue-16-debug --output-on-failure
```

For GPU evidence, configure a separate Release build with
`ASC_CPP_ENABLE_CUDA=ON` and an explicit `CMAKE_CUDA_ARCHITECTURES`. Run the
complete Random-labelled selection on actual hardware. Exact local compiler,
linkage, sanitizer, device, test-count, timing, and benchmark observations are
recorded in the [support matrix][support] and [performance guide][performance].

[crosswalk-report]: random-crosswalk.md
[crosswalk-source]: development/asc-cpp-architecture/random-crosswalk.yaml
[module]: modules/random.md
[performance]: performance.md
[provenance]: development/asc-cpp-architecture/provenance-review.md
[support]: support-matrix.md

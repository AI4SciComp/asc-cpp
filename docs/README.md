# asc-cpp documentation

The repository is at the unreleased **Quasi-random samplers (Issue 14)
Feature Gate B** candidate. All six modules, both Random storage
facets, the provider-free aggregate, and all six approved opt-in CUDA facets
remain in the same dependency graph. This issue adds only storage-neutral CPU
QMC helpers and algorithms to `ASC::random`.

## Current API documentation

- [API map](api.md): component, header, target, and direct-dependency map.
- [Core module](modules/core.md): errors, configuration vocabulary, I/O,
  ownership, host memory, serial execution, and the optional CUDA facet.
- [Utilities module](modules/utilities.md): transactional command-line
  configuration and monotonic timing.
- [Expression module](modules/expression.md): storage-neutral customization,
  capture, shape, aliasing, and sparsity semantics.
- [Dense module](modules/dense.md): mappings, views, ownership, expression
  evaluation, serial CPU algebra, and the optional CUDA facet.
- [Sparse module](modules/sparse.md): canonical coordinate/CSR/CSC storage,
  conversions, structure-preserving evaluation, the standardized Sparse BLAS
  compute surface, and the optional CUDA facet.
- [Random module](modules/random.md): explicit seed acquisition, versioned
  stateful engines, generic value composition, uniform and scalar normal
  distributions, Latin/Halton/Hammersley/Sobol QMC, Philox4x32-10 addressing,
  and storage facets.
- [Storage-neutral QMC example](examples/random-qmc.md): indexed and
  sequential Sobol plus Latin-hypercube caller-workspace use.
- [Support matrix](support-matrix.md): locally tested and skipped platform,
  compiler, linkage, sanitizer, and provider combinations.
- [API compatibility](api-compatibility.md): distinct source, ABI, numerical,
  random-bit, package, provider, and schema compatibility boundaries.
- [Package capabilities](package-capabilities.md): metadata, component
  closures, versions, relocation, and provider discovery.
- [Extension guide](extension-guide.md): dependency-safe external expression
  and storage-neutral extension rules.
- [Downstream integration](downstream-integration.md): the isolated
  asc-xde-shaped package trial and its limits.
- [Performance](performance.md): measurement contract and bounded local
  observations.
- [BLAS coverage](blas-coverage.md): generated Dense/Sparse BLAS standards
  inventory, current evidence states, and dense-to-sparse crosswalk. All 36
  applicable Sparse BLAS compute rows carry portable CPU and real-CUDA
  implementation and test evidence; 43 handle-oriented rows remain explicitly
  not applicable under the approved typed-owner mapping.
- [BLAS completion audit](blas-completion-audit.md): frozen evidence identity,
  build registration, retired-interface, package, consumer, and benchmark
  audit contract and reproduction commands.
- [Random crosswalk](random-crosswalk.md): generated 33-row MdeCpp inventory,
  classifications, approved routes, destination owners, and child boundaries.
- [Random contract](development/asc-cpp-architecture/decisions/0020-random-contract.md):
  frozen seed, state, stream, reproducibility, QMC, storage-adapter, failure,
  CPU/GPU, testing, packaging, and performance semantics.
- [Linalg-to-BLAS migration](migration/linalg-to-blas.md): mechanical path and
  type changes for the approved breaking pre-1.0 rename.

The [Milestone 8 contract][m8-contract], [ownership ledger][m8-ownership], and
[preflight][m8-preflight] freeze the current scope and preserved stale-branch
boundary. The [Milestone 7 records][m7-contract] remain authoritative for the
unchanged product predecessor.

The post-checkpoint [correction contract][m8-correction-contract] freezes the
20 accepted review corrections. The [correction integration
report][m8-correction-integration] and [corrected Publication Checkpoint
B][m8-checkpoint] are the current workspace handoff; they supersede the
pre-correction evidence snapshot where results differ.

## Package status

The unreleased `ASCCpp` 0.9.0 candidate package exposes:

```text
ASC::core        direct ASC dependency: none
ASC::utilities   direct ASC dependency: ASC::core
ASC::expression  direct ASC dependency: ASC::core
ASC::dense       direct ASC dependencies: ASC::core, ASC::expression
ASC::sparse      direct ASC dependencies: ASC::core, ASC::expression
ASC::random      direct ASC dependency: ASC::core
ASC::random_dense  direct ASC dependencies: ASC::random, ASC::dense
ASC::random_sparse direct ASC dependencies: ASC::random, ASC::sparse
ASC::cpp           direct ASC dependencies: all provider-free targets
ASC::core_cuda     direct ASC dependency: ASC::core
ASC::dense_cuda    direct ASC dependencies: ASC::dense, ASC::core_cuda
ASC::sparse_cuda   direct ASC dependencies: ASC::sparse, ASC::core_cuda
ASC::random_cuda   direct ASC dependencies: ASC::random, ASC::core_cuda
ASC::random_dense_cuda
  direct ASC dependencies: ASC::random_dense, ASC::random_cuda, ASC::core_cuda
ASC::random_sparse_cuda
  direct ASC dependencies: ASC::random_sparse, ASC::random_cuda, ASC::core_cuda
```

Consumers request components explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_cuda)
```

Dense and Sparse each load Core and Expression before themselves and do not
load one another. Random Dense and Random Sparse each load only their exact
Random/storage closure and never the sibling storage facet. A no-component
request loads the provider-free `cpp` aggregate. Required unknown components
fail; an optional unknown component does not invalidate a successful request.
CUDA components are advertised only when built with `ASC_CPP_ENABLE_CUDA=ON`.
Only a requested CUDA component triggers downstream CUDAToolkit discovery;
each request loads only its exact transitive component closure.

## Authoritative architecture

The approved Stage A package remains the architectural source of truth:

- [architecture blueprint][blueprint];
- [ADRs 0001–0020][adrs];
- [dependency manifest][dependencies];
- [capability manifest][capabilities];
- [BLAS coverage manifest][blas-manifest];
- [asc-cmake consumption][asc-cmake];
- [testing strategy][testing-strategy];
- [CI strategy][ci-strategy];
- [provenance review][provenance];
- [release roadmap][roadmap]; and
- [implementation plan][implementation].

The principal hardening decisions are ADR [0002][adr-0002] for package
components, [0003][adr-0003] for C++20/public-source policy,
[0008][adr-0008] for memory/execution,
[0009][adr-0009] for ownership/views,
[0010][adr-0010] for storage-neutral protocols, [0012][adr-0012] for Sparse
invariants, [0016][adr-0016] for sibling isolation, [0017][adr-0017] for
provenance, [0018][adr-0018] for the 0.9.x boundary, and
[0019][adr-0019] for the Dense/Sparse BLAS contract, and
[0020][adr-0020] for the complete Random contract. ADR 0019 supersedes the
linear-algebra/provider scope in ADRs [0013][adr-0013] and [0014][adr-0014].
ADR 0020 extends [0015][adr-0015] without changing its existing Philox and
storage-generation guarantees.

## Deferred scope

Sparse addition/multiplication, BSR/VBR/SELL formats, file parsing, general
broadcasting, HIP, SYCL, and other providers require later explicit approval.
Issues 13 and 14 implement their approved CPU engine/distribution and
storage-neutral QMC rows. Multivariate normal, hypersphere sampling, Dense QMC,
and new storage adapters remain deferred to Issue 15. Reserved API or provider
names do not make those features available.

[adr-0002]: development/asc-cpp-architecture/decisions/0002-package-target-naming.md
[adr-0003]: development/asc-cpp-architecture/decisions/0003-namespace-and-source-policy.md
[adr-0005]: development/asc-cpp-architecture/decisions/0005-configuration-boundary.md
[adr-0008]: development/asc-cpp-architecture/decisions/0008-memory-and-execution.md
[adr-0009]: development/asc-cpp-architecture/decisions/0009-ownership-buffers-views.md
[adr-0010]: development/asc-cpp-architecture/decisions/0010-expression-protocol.md
[adr-0011]: development/asc-cpp-architecture/decisions/0011-dense-semantics.md
[adr-0012]: development/asc-cpp-architecture/decisions/0012-sparse-semantics.md
[adr-0013]: development/asc-cpp-architecture/decisions/0013-dense-linalg-providers.md
[adr-0014]: development/asc-cpp-architecture/decisions/0014-sparse-linalg-providers.md
[adr-0015]: development/asc-cpp-architecture/decisions/0015-random-reproducibility.md
[adr-0016]: development/asc-cpp-architecture/decisions/0016-mixed-dense-sparse.md
[adr-0017]: development/asc-cpp-architecture/decisions/0017-third-party-provenance.md
[adr-0018]: development/asc-cpp-architecture/decisions/0018-versioning-release-boundaries.md
[adr-0019]: development/asc-cpp-architecture/decisions/0019-blas-contract.md
[adr-0020]: development/asc-cpp-architecture/decisions/0020-random-contract.md
[adrs]: development/asc-cpp-architecture/decisions
[asc-cmake]: development/asc-cpp-architecture/asc-cmake-consumption.md
[blas-manifest]: development/asc-cpp-architecture/blas-coverage.yaml
[blueprint]: development/asc-cpp-architecture/architecture-blueprint.md
[capabilities]: development/asc-cpp-architecture/capability-manifest.yaml
[ci-strategy]: development/asc-cpp-architecture/ci-strategy.md
[dependencies]: development/asc-cpp-architecture/dependency-manifest.yaml
[implementation]: development/asc-cpp-architecture/implementation-plan.md
[m2-contract]: development/asc-cpp-m2-independent-foundations/milestone-contract.md
[m2-ownership]: development/asc-cpp-m2-independent-foundations/ownership.md
[m2-provenance]: development/asc-cpp-m2-independent-foundations/provenance-record.md
[m3-contract]: development/asc-cpp-m3-dense-cpu/milestone-contract.md
[m3-ownership]: development/asc-cpp-m3-dense-cpu/ownership.md
[m3-provenance]: development/asc-cpp-m3-dense-cpu/provenance-record.md
[m4-contract]: development/asc-cpp-m4-sparse-cpu/milestone-contract.md
[m4-checkpoint]: development/asc-cpp-m4-sparse-cpu/publication-checkpoint-b.md
[m4-ownership]: development/asc-cpp-m4-sparse-cpu/ownership.md
[m4-provenance]: development/asc-cpp-m4-sparse-cpu/provenance-record.md
[m5-contract]: development/asc-cpp-m5-random-storage-generation/milestone-contract.md
[m5-ownership]: development/asc-cpp-m5-random-storage-generation/ownership.md
[m5-provenance]: development/asc-cpp-m5-random-storage-generation/provenance-record.md
[m6-contract]: development/asc-cpp-m6-gpu-core-dense/milestone-contract.md
[m6-ownership]: development/asc-cpp-m6-gpu-core-dense/ownership.md
[m6-provenance]: development/asc-cpp-m6-gpu-core-dense/provenance-record.md
[m7-contract]: development/asc-cpp-m7-gpu-sparse-random/milestone-contract.md
[m7-ownership]: development/asc-cpp-m7-gpu-sparse-random/ownership.md
[m7-provenance]: development/asc-cpp-m7-gpu-sparse-random/provenance-record.md
[m8-contract]: development/asc-cpp-m8-hardening-downstream/milestone-contract.md
[m8-correction-contract]: development/asc-cpp-m8-hardening-downstream/review-correction-contract.md
[m8-correction-integration]: development/asc-cpp-m8-hardening-downstream/review-correction-integration.md
[m8-checkpoint]: development/asc-cpp-m8-hardening-downstream/publication-checkpoint-b.md
[m8-ownership]: development/asc-cpp-m8-hardening-downstream/ownership.md
[m8-preflight]: development/asc-cpp-m8-hardening-downstream/preflight.md
[provenance]: development/asc-cpp-architecture/provenance-review.md
[roadmap]: development/asc-cpp-architecture/release-roadmap.md
[testing-strategy]: development/asc-cpp-architecture/testing-strategy.md

# Security policy

## Current status

asc-cpp is an unreleased `0.9.0` Milestone 8 correction candidate. It has no
published runtime version, support window, security-service commitment, or
production-readiness claim.

| Version or line | Supported |
| --- | --- |
| Unreleased `0.9.0` Milestone 8 candidate | Review and validation only |
| Deleted five-component implementation | No |
| Any inferred historical release line | No |

Milestone completion, local validation, publication, and release are separate
decisions. Do not infer support from a branch, package version, test report,
historical document, or provider evidence label.

## Reporting a vulnerability

Do not disclose a suspected vulnerability, credential, exploit, or sensitive
environment detail in a public issue. Use the repository host's private
security-reporting channel when it is enabled, or contact an AI4SciComp
maintainer privately and ask for a secure reporting route.

Include only the information needed to reproduce and assess the issue:

- the affected revision, component, public target, and files;
- build system, CMake version, generator, platform, compiler, standard
  library, build mode, and linkage;
- CUDA toolkit, host compiler, driver, device, and architecture when relevant;
- whether the issue affects source behavior, numerical results, asynchronous
  lifetime, package configuration, installation, relocation, CI, test
  tooling, documentation, or provenance;
- minimal reproduction steps, observed impact, and any known mitigation; and
- whether untrusted input, an external allocation, a custom resource/adapter,
  or concurrent use is required.

Do not include real tokens, private-repository credentials, personal paths,
proprietary inputs, or third-party confidential data. Replace secrets and
identifiers with inert placeholders.

The maintainers will acknowledge and triage reports as capacity permits. No
response or remediation deadline is promised for this unreleased candidate.

## Candidate security boundary

The reviewed surface contains:

- the provider-free Core, Utilities, Expression, Dense, Sparse, and Random
  modules;
- the Dense and Sparse Random storage facets and the provider-free aggregate;
- the opt-in Core, Dense, Sparse, and Random CUDA facets;
- CPU and CUDA numerical kernels, explicit contexts, asynchronous completion,
  caller-owned workspace, non-owning views, and external expression adapters;
- command-line configuration, scalar byte I/O, memory resources, and timing;
- CMake component discovery, build-tree and installed packages, relocation,
  and isolated downstream consumption; and
- repository-owned CMake, test, hardening, ABI, benchmark, and CI tooling.

Relevant vulnerability classes include:

- integer overflow, invalid metadata, out-of-bounds access, undefined
  behavior, and incorrect numerical failure handling;
- non-owning-view, memory-resource, expression-capture, provider-context,
  workspace, stream, event, and asynchronous-storage lifetime errors;
- incomplete alias metadata, structural overlap, invalid sparse provenance,
  and mutation before validation completes;
- races involving caller-owned storage, custom resources/adapters, independent
  execution contexts, or provider state;
- hidden transfer, allocation, synchronization, precision change, provider
  selection, or fallback that contradicts the public contract;
- unsafe path construction, traversal, symlink escape, recursive deletion,
  source/build path leakage, or unintended CMake package-registry use;
- misleading required/optional component availability, dependency closure,
  installed headers, relocation metadata, ABI observations, or GPU evidence;
- credential or sensitive-value disclosure in configuration diagnostics,
  build logs, CI, package metadata, test artifacts, or reports; and
- unreviewed source/data, archive/install contents, license/notice errors, or
  provenance defects.

ASCCpp does not sandbox arbitrary downstream code. A custom `MemoryResource`,
expression adapter, byte source/sink, external allocation descriptor, or CUDA
stream/workspace is a caller-supplied trust boundary. Its implementation and
declared sizes, placement, uniqueness, aliasing, lifetime, and concurrency
properties must be truthful.

## Input, numerical, and provider limits

Utilities currently parses approved command-line configuration input; no
general local-file parser, network protocol, application checkpoint format, or
Dense/Sparse/random-state serialization schema is part of this candidate.
Core byte I/O helpers do not authenticate, authorize, encrypt, or validate an
application-level format.

Numerical correctness is security-relevant when a wrong result can affect a
downstream decision. Callers must check every `Status` and `Result<T>`.
Unsupported shapes, types, mappings, memory spaces, providers, overflows, and
workspace conditions are failures rather than authorization to use a wrapped,
partial, silently converted, or fallback result.

CUDA facets are explicit optional providers. Their evidence applies only to
the named toolkit, host compiler, driver, device, architecture, operation
subset, and evidence level. ASCCpp does not secure the CUDA SDK, driver,
firmware, device, external stream, or host process, and it makes no isolation
claim between hostile GPU workloads. Provider-free package consumption must
not discover CUDA; requesting a CUDA facet crosses into the CUDA Toolkit's own
security and update boundary.

## Dependencies and provenance

Building ASCCpp uses the released Apache-2.0 `ASCCMake` 0.1.0 package at the
recorded immutable revision. Provider-free product targets otherwise require
only the C++20 standard library. CUDA-enabled targets add only their documented
private CUDA Runtime, cuBLAS, or cuSPARSE edges.

Never commit credentials used to retrieve a private dependency or provider.
Third-party source, data, generated artifacts, test vectors, and CI actions
must pass the approved provenance and license review before entering the
repository. Provider discovery is not approval to copy or redistribute a
provider SDK.

See the [current documentation](docs/README.md), [support and evidence
matrix](docs/support-matrix.md), [API compatibility
policy](docs/api-compatibility.md), and [package
capabilities](docs/package-capabilities.md) for the exact unreleased surface
and its evidence limits.

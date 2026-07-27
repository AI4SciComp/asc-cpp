# Milestone 8 documentation and API review

Status: Independent review complete; documentation/API accepted

Date: 2026-07-27

Role: documentation and API reviewer

## Review boundary

This review is independent from production tooling, verification,
portability/GPU/performance review, and lead integration. Its exclusive write
scope was:

```text
docs/support-matrix.md
docs/api-compatibility.md
docs/package-capabilities.md
docs/extension-guide.md
docs/downstream-integration.md
docs/performance.md
docs/development/asc-cpp-m8-hardening-downstream/documentation-api-review.md
```

Product headers/sources, CMake/package files, tests, benchmarks, architecture
records, root/module documentation, and sibling repositories were read-only.
Defects were reported to the lead rather than documented as acceptable.

## Material reviewed

- the complete frozen M8 contract, ownership ledger, and preflight;
- the approved release roadmap and ADR 0018;
- the complete CMake package config/component metadata and root/component
  target declarations;
- every current public header and public file-set declaration;
- all six current module guides and prior M7 documentation, production,
  verification, and portability review boundaries;
- the cumulative M7 Publication Checkpoint B evidence;
- the approved asc-xde trial boundary and actual sibling commit recorded by
  preflight; and
- existing CPU/CUDA benchmark sources and their correctness/timing boundaries.

## Findings

### M8-DOC-001 — stale package availability diagnostic

Severity: integration blocker

The reviewed `cmake/ASCCppConfig.cmake.in` described an unavailable component
as not implemented in a “Milestone 7 build” while the generated package
advertises the M8 `0.9.0` candidate.

Required resolution: use version/milestone-neutral package wording or correct
M8 wording, then verify unknown, unavailable-required, and
unavailable-optional component behavior.

Resolution: the package now reports that the component is “not available in
this ASCCpp build.” The wording is version-neutral and does not imply that a
known optional component is unimplemented. Required/optional unavailable and
unknown-component package checks remain registered for independent execution.

Status: resolved after lead integration.

### M8-DOC-002 — stale `0.7.0` assertions and guide examples

Severity: integration blocker

At initial review, multiple lead-owned package/consumer/architecture checks
still asserted `0.7.0`, and module guides described older candidate versions
or requested older package minors while the root project version was `0.9.0`.
Those checks would either fail the M8 build or present conflicting package
guidance.

Required resolution: update current candidate assertions/examples to `0.9`
or `0.9.0` as appropriate without rewriting historical milestone evidence.

Resolution: the root project, package/consumer assertions, approved-product
architecture check, root/API documentation, and all six current module guides
now consistently identify the unreleased `0.9.0` candidate. Current
non-`EXACT` examples request `0.9`; exact metadata checks request `0.9.0`.
Historical references that describe when an API was introduced remain
historical rather than current package-version claims.

Status: resolved after lead integration.

### M8-DOC-003 — package metadata and closure semantics

Severity: contract review

The known component set contains exactly six modules, two random storage
facets, six CUDA facets, and the provider-free aggregate. The available set
truthfully depends on the producer's CUDA option. The direct target edges and
transitive package closures match the frozen M8 contract. A no-component
request selects provider-free `cpp`; a CUDA closure alone discovers
CUDAToolkit.

The `ASCCpp_<component>_FOUND` variables are meaningful for requested or
loaded components; an unrequested variable need not be defined. The complete
inventory is queried through `ASCCpp_AVAILABLE_COMPONENTS`.

Follow-up: the package config now derives transitive closure and export-file
names from the frozen component metadata rather than repeating a manual
component dispatch. Registered M8 probes cover exact `0.9.0`, compatible
`0.9`, rejected newer `0.9.1`, rejected `0.8`, rejected `1.0`, known and
available inventories, component targets, CUDA-disabled isolation, and the
complete CUDA closure where enabled. Build-tree, copied-build-tree, installed,
and relocated package modes are registered separately.

Status: accepted after integration inspection; execution results belong to
the independent matrix and final checkpoint.

### M8-DOC-004 — public versus internal API boundary

Severity: compatibility review

The public header inventory is exactly the target public file sets. Provider
umbrellas remain separate. Public template headers necessarily expose some
`internal_` declarations, and shared builds may need visible support symbols,
but neither makes those names supported extension points.

The new compatibility guide distinguishes source, ABI, symbol, numerical,
random-bit, provider, file/schema, and package compatibility. It makes no
cross-minor, cross-toolchain, or `1.0` ABI promise.

Follow-up: the integrated hardening registration compares the source public
surface with the frozen header-owner and target/component baselines, compares
installed headers with the declared file sets, exercises the installed API
from the relocated package, and registers local ELF ABI inspection only for
matching shared-build environments. It does not turn ELF observations into a
cross-platform ABI promise.

Status: accepted after integration inspection; final test results remain
independent evidence.

### M8-DOC-005 — ownership and asynchronous lifetime

Severity: API contract review

Move-only owners, non-owning views, resource lifetime, expression lvalue
borrowing, and CUDA completion boundaries are consistent across the reviewed
headers. A completion event retains provider completion state but not user
storage, workspaces, memory resources, or dense/sparse provider contexts.
Trusted sparse provenance is not a readiness event.

The extension/downstream guides state these obligations explicitly, including
the ABI risk of downstream-derived polymorphic resources and byte streams.

Status: accepted with the existing arbitrary-external-CUDA-allocation bound
limitation disclosed.

### M8-DOC-006 — extension boundaries

Severity: API review

The supported downstream customizations are expression adapters, placement,
writable adapters, `MemoryResource`, byte source/sink implementations, and
downstream-owned wrapper targets. There is no public provider registry,
component-registration API, native-handle adoption surface, or permission to
use implementation namespaces.

CUDA evaluators remain bounded and do not accept arbitrary external expression
adapters merely because they satisfy the provider-neutral protocol.

Status: accepted.

### M8-DOC-007 — asc-xde trial claim

Severity: scope review

The real asc-xde repository is skeletal at
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf`. A checked-in, asc-cpp-owned
fixture is therefore the only truthful downstream trial in M8. Documentation
calls it a dependency/API trial, not an asc-xde feature, solver, or completed
migration, and records that no sibling write is authorized.

Follow-up: the asc-cpp-owned fixture is registered against build-tree, copied
build-tree, installed, and relocated packages. It requests only `dense`, checks
the exact `core;expression;dense` closure, rejects every unrelated/provider
target and CUDAToolkit discovery, includes only public headers, compiles and
runs ODE/PDE-shaped dense workflows, and rejects visibility of the deleted
`asc/array.h` and `asc/linalg.h` headers. Its harness verifies the real
asc-xde commit and clean status before and after the trial with Git optional
locks disabled.

Status: accepted after integration inspection; whether each registered trial
passed or skipped belongs to the independent matrix and final checkpoint.

### M8-DOC-008 — performance claims and evidence

Severity: performance/API review

The performance guide freezes environment/workload/warmup/repetition/
synchronization/checksum/noise fields, separates setup/transfer from operation
timing, and prohibits cross-machine thresholds or speedup claims. It records
complexity/allocation envelopes without promising absolute throughput.

Compile-time/object-size values and aggregate CPU/GPU measurements must be
reported only after the verifier/lead run them. Sanitizer timing is explicitly
not treated as representative performance.

Follow-up: GCC and Clang public-header compile/object probes are registered
with three repetitions and isolated output directories. Existing
correctness-checked CPU/CUDA runtime probes remain separate. No observed time,
object size, regression percentage, or GPU result is inferred by this review.

Status: accepted methodology and registration; final observed values belong
to Checkpoint B.

### M8-DOC-009 — GPU evidence labels

Severity: evidence vocabulary review

Every new guide uses exactly:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

No label is inferred from another. Trusted device CSC evaluation, unavailable
platform/toolchain/topology combinations, and missing hosted runs remain
truthful skips.

Status: accepted; final per-provider labels remain lead checkpoint evidence.

### M8-DOC-010 — toolkit targets are not the ASCCpp closure

Severity: package portability clarification

The portability follow-up identified that CMake's `FindCUDAToolkit` may create
additional `CUDA::` helper/imported targets beyond CUDA Runtime, cuBLAS, and
cuSPARSE. Treating the complete post-discovery CMake target population as an
exact package closure would make the contract depend on CMake/toolkit
implementation details.

Resolution: the package guide now distinguishes three facts:

- the requested transitive `ASC::` target closure is exact;
- each ASCCpp imported target has an exact linkage-mode-dependent exported
  link interface, including `LINK_ONLY` private provider dependencies where a
  static export requires them; and
- `FindCUDAToolkit` may define other `CUDA::` targets whose mere existence
  neither makes them ASCCpp components nor links them to an ASCCpp target.

The corrected repeated-component fixture checks exact `ASC::` target presence
and absence, checks exact imported `INTERFACE_LINK_LIBRARIES` for static and
shared forms, and requires the provider targets needed by the selected facet.
It no longer assumes that unrelated `CUDA::` targets cannot exist after
toolkit discovery. The provider-free first lookup continues to prove that a
CPU-only request does not discover CUDAToolkit.

Status: resolved and accepted after final inspection; execution results remain
checkpoint evidence.

## Independent disposition

M8-DOC-001 and M8-DOC-002 are resolved. The integrated package metadata,
data-driven target closure, current `0.9` documentation and consumers,
hardening/API registrations, asc-xde-shaped downstream fixture, and clarified
CUDAToolkit target boundary match the frozen Milestone 8 documentation/API
contract.

The documentation/API review accepts the integrated candidate with no
remaining documentation or public-API blocker. This disposition does not
infer that registered matrix tests ran or passed: exact package,
symbol/header, downstream, performance, sanitizer, and provider results and
skips remain the independent reviewers' and lead checkpoint responsibilities.
No new component, provider, dependency, operation, schema, compatibility
facade, or later-milestone feature is documented.

No commit, push, merge, tag, release, branch deletion, or sibling-repository
write was performed.

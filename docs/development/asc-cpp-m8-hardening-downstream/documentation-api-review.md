# Milestone 8 revision-2 documentation and API review

Status: Complete; accepted for Publication Checkpoint B

Date: 2026-07-28

Role: documentation and API reviewer

## Review boundary

The review is independent from production hardening, verification,
portability/GPU/performance review, and lead integration. Its exclusive write
scope is:

```text
docs/support-matrix.md
docs/api-compatibility.md
docs/package-capabilities.md
docs/extension-guide.md
docs/downstream-integration.md
docs/performance.md
docs/development/asc-cpp-m8-hardening-downstream/documentation-api-review.md
```

Product headers/sources, CMake/package files, tests, benchmarks, hardening
tools, ABI baselines, shared architecture records, root/module documentation,
and sibling repositories were read-only. Defects were reported to the lead
rather than documented around.

## Material reviewed

- the complete repository `main:AGENTS.md` and 2,262-line asc-cpp runbook;
- the frozen revision-2 M8 contract, ownership ledger, and preflight;
- the complete approved architecture blueprint, ADRs 0001--0018, dependency
  and capability manifests, backend matrix, and release roadmap;
- the M7 contract, public headers, module/API documentation, verification and
  portability reviews, and Publication Checkpoint B;
- the current root/component target declarations, package component metadata,
  generated-config template, and all 49 public headers;
- the M8 hardening baselines/tools as read-only audit material;
- the independently frozen M8 verification design, public API/header/package
  probes, compile/object observations, and asc-xde-shaped fixture; and
- the unchanged real asc-xde commit recorded by preflight.

The superseded `feature/asc-cpp-m8-hardening-downstream` branch was inspected
only as read-only evidence. No statement or example was retained without
rechecking it against the revision-2 M7 surface.

## Findings

### M8R2-DOC-001 — candidate version integration

Severity: integration blocker

Initial revision-2 inspection found root CMake at `0.7.0` while new
root/changelog framing already claimed the unreleased `0.9.0` candidate.

Required resolution: advance the package producer and current package/version
assertions to `0.9.0`, use non-`EXACT` `0.9` in ordinary current consumption
examples, and leave historical milestone evidence unchanged.

Resolution: the lead advanced the project and package/consumer assertions to
`0.9.0`/`0.9`. Version compatibility remains `SameMinorVersion`.

Status: resolved; independent integrated and standalone package execution
passed.

### M8R2-DOC-002 — live module guides requested the predecessor minor

Severity: integration blocker

All six live module guides initially contained twelve current consumption
examples or package-boundary statements requesting ASCCpp `0.7`.

Required resolution: update current guide requests to `0.9` without rewriting
historical M7 introduction records.

Resolution: the lead revised the ownership ledger before adding the six guides
to lead scope, then changed all twelve current references. A final scan finds
no current root/API/module example requesting a pre-0.9 minor.

Status: resolved.

### M8R2-DOC-003 — superseded extension guidance was not revision-2 API

Severity: high documentation/API defect in stale evidence

The older M8 branch used nonexistent current adapter members such as
`kRank`/`kOperationCategory`, referred to alias helper APIs absent from the
current headers, and claimed internal allocation-range registration for
CUDA-resource allocations. The approved M7 implementation exposes none of
that allocation-registry contract.

Required resolution: do not copy the stale guide. Re-derive the extension
surface from the current headers and document arbitrary external allocation
bounds truthfully.

Resolution: the revision-2 extension guide uses the actual `rank`,
`sparsity_effect`, placement alias, uniqueness, and write contracts. Its
complete adapter skeleton leaves the optional operation category at the
external default and parses under C++20 against current headers. It states
that CUDA pointer attributes do not prove the terminal bound of every
external allocation.

Status: resolved in documentation; no product edit required.

### M8R2-DOC-004 — raw CUDA Random capacity and lifetime

Severity: API contract review

The sole raw provider entry point accepts:

```text
ExecutionContext + MutableMemoryView + word_count
  + stream + subsequence + offset
```

There is no bare-pointer overload. Requested bytes are checked against the
view's declared size, while the declaration itself remains a caller-truthful
live-storage contract. Completion retains provider state, not destination
storage or its resource; the documented context, result, resource, and storage
remain alive until completion.

The compatibility, extension, downstream, and support guides state the same
capacity/lifetime boundary and do not repeat the superseded signature.

Independent public-surface compilation and the standalone CUDA package probe
accepted only the `MutableMemoryView` signature. The real-device unchanged-M7
revalidation passed the raw runtime/bit oracle.

Status: accepted.

### M8R2-DOC-005 — package metadata and closure

Severity: package contract review

The known inventory has exactly 15 components. A CUDA-disabled producer makes
the nine provider-free components available; a CUDA-enabled producer makes
all 15 available. Direct edges match the dependency manifest, no-component
lookup selects `cpp`, and CUDAToolkit discovery occurs only for a requested
CUDA closure.

The package guide distinguishes exact `ASC::` target closure from additional
helper `CUDA::` targets that CMake's `FindCUDAToolkit` may define. It also
distinguishes public ASC edges from static linkage-only private provider
requirements.

Independent GCC Debug/shared hardening passed build-tree, copied-build-tree,
installed, relocated, and path-with-spaces package modes. The CPU projection,
versions, component closures, required/optional failures, linkage mode, and
provider isolation passed. A standalone CUDA shared package passed all 15
component closures and metadata checks.

Status: accepted.

### M8R2-DOC-006 — public, symbol, and ABI boundaries

Severity: compatibility review

The documented surface matches the 49-header ownership baseline and 15-target
inventory. Focused production checks passed for all 49 source headers, the
37-header CPU installed projection, the full 49-header CUDA projection, nine
CPU targets with six truthful CUDA skips, and all 15 CUDA-enabled targets.
Public file-set membership, not physical source-tree presence, defines the
supported header surface. `internal_` declarations and support symbols
required by templates are not downstream APIs.

The compatibility guide separates source, ABI, symbol, numerical, random-bit,
provider, file/schema, and package compatibility and makes no cross-minor,
cross-toolchain, or `1.0` ABI promise. It also discloses the local unversioned
SONAMEs and compiler/STL weak-symbol observations rather than describing hidden
visibility as a complete dynamic-symbol whitelist.

Focused GCC, Clang, and GCC/NVCC ELF reports passed deterministic baseline
comparison for 5, 5, and 11 libraries respectively. Those reports remain
Linux/toolchain/configuration-specific.

Independent GCC Debug/shared hardening/downstream validation passed 28/28,
including representative shared-symbol and deterministic ELF observations.
The lead's clean CPU full suite passed 189/189 with the package path-leak
assertion enabled.

The terminal CPU matrix passed every GCC/Clang x Debug/Release x
static/shared selected row, and fresh GCC Debug/static and GCC Release/static
full suites each passed 189/189.

Status: accepted.

### M8R2-DOC-007 — extension ownership and binary boundary

Severity: API review

The supported external mechanisms are expression adapters, placement/writable
adapters, custom `MemoryResource`, `ByteSource`/`ByteSink`, and
downstream-owned wrapper targets. There is no provider registry, component
registration API, native-handle adoption surface, or permission to use
implementation namespaces.

The guide documents resource/view/expression lifetimes, asynchronous
completion, unique nonfailing writes, partial byte I/O, and the ABI risk of
downstream-derived polymorphic objects.

Status: accepted.

### M8R2-DOC-008 — asc-xde-shaped trial claim

Severity: scope review

The real asc-xde repository is skeletal at
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf`. The only truthful M8 trial is an
asc-cpp-owned scratch fixture. It requests `dense`, expects exactly
`ASC::core`, `ASC::expression`, and `ASC::dense`, disables CUDA discovery,
performs one explicit-Euler Dense workflow, and checks per-value and reduction
results.

Documentation describes it as a package/dependency/API trial, not an asc-xde
feature, solver, completed migration, or sibling-repository change.

The trial passed against the configured build tree, copied build-tree package,
installed prefix, and relocated prefix, including paths with spaces. Every
record preserved the exact real asc-xde commit and clean before/after status.

Status: accepted.

### M8R2-DOC-009 — performance evidence contract

Severity: performance/API review

The performance guide records environment, workload, warmup, repetitions,
synchronization, allocation/transfer boundary, checksum/oracle, and noise. It
prohibits unlike-machine thresholds and speedup claims.

Compile-time/object-size observations name their installed-header translation
units and record compiler/options, elapsed observation, object bytes, and
source/object SHA-256 values without treating them as ABI or application-build
predictions. The CUDA Sparse Random envelope is correctly
`O(N*K + K^2 + R*K)`, not the stale `O(K^2*R)` expression.

The final independent Debug observations passed with one strict C++20
invocation per unit:

| Compiler | Core | Dense | provider-free umbrellas |
| --- | --- | --- | --- |
| GCC 11.4.0 | 6,576 bytes; 0.795574 s | 271,904 bytes; 0.816891 s | 2,048 bytes; 0.988198 s |
| Clang 19.0.0 | 5,552 bytes; 0.986439 s | 220,600 bytes; 0.864525 s | 1,328 bytes; 1.05307 s |

These are noisy local compile/object observations, not a comparison or
threshold.

Status: accepted.

### M8R2-DOC-010 — GPU evidence vocabulary

Severity: evidence review

All revision-2 guides use exactly:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

No positive label is inferred from another. Trusted device CSC, multi-GPU,
cross-toolkit/cross-platform runtime, unavailable generators/platforms, and
hosted CI remain skips.

Independent real-device revalidation of the unchanged M7 product build passed
13/13. The fresh current-branch GCC/NVCC Release/shared architecture-86 suite
then passed 245/245. Its final classifications are `configure-tested`,
`compile-tested`, and `runtime-tested` for Core CUDA, plus `parity-tested` for
Dense CUDA, Sparse CUDA, and all three Random CUDA facets. Compute Sanitizer
memcheck passed 12/12 applicable runtimes with zero memory errors and leaks.
A Clang 19 C++ plus NVCC 12.9/GNU 11 CUDA-host Release/static focused suite
also passed 28/28.

Status: accepted.

### M8R2-DOC-011 — stale superseded-branch roadmap status

Severity: integration blocker

The shared release roadmap already claimed that Milestone 8 reached
Publication Checkpoint B on 2026-07-27. That statement came from the
superseded M8 branch and cannot describe revision 2 before its independent
evidence closes. The dependency manifest, capability manifest, and backend
matrix still truthfully identify the M7 predecessor.

Required resolution: remove or replace the stale completion/date, then advance
all shared architecture status/evidence records together only after the
revision-2 matrix and reviews pass.

Resolution: the lead replaced the stale completion/date with a truthful
revision-2 validation-in-progress status and retained Milestone 7 as the
completed predecessor. Final architecture evidence advancement remains gated
on the revision-2 matrix.

Status: resolved.

### M8R2-DOC-012 — API map invented a Random CUDA context

Severity: public API documentation defect

The live API map described `<asc/random/providers/cuda.h>` as providing a
move-only CUDA Random context. The current header defines
`CudaRandomWordGeneration` and `CudaFillPhilox4x32` over Core's
`ExecutionContext`; there is no Random-specific context type.

Required resolution: describe raw Philox generation into a capacity-carrying
`MutableMemoryView` with explicit address state, without inventing a context.

Resolution: the lead now documents `CudaRandomWordGeneration` over Core's
`ExecutionContext` and the capacity-carrying `MutableMemoryView`, explicit word
count, and explicit Random address accepted by `CudaFillPhilox4x32`.

Status: resolved.

### M8R2-DOC-013 — external adapter operation category

Severity: documentation example defect

The first revision-2 extension example assigned
`ExpressionOperation::kTerminal` to a downstream adapter. The operation member
is optional, and an external adapter should normally use the `kExternal`
default. CUDA recognizes only its bounded built-in terminal/node shapes;
assigning a built-in category does not turn an external type into one.

Resolution: omit the member from the complete example and explain the
distinction. The corrected example passes the same strict C++20 parse.

Status: resolved before final acceptance.

### M8R2-DOC-014 — compile/object observation record completeness

Severity: evidence-closure blocker

The first compile/object observation record contained compiler/version,
language/include/options, one elapsed observation, object bytes, and
source/object SHA-256 per translation unit. The frozen M8 performance contract
also requires the applicable build mode, hardware/provider, workload,
warmup/repetition, synchronization, checksum, pass/fail/skip, and known-noise
fields. Compile-only fields may truthfully be `not applicable`, and common
environment context may be recorded once, but they may not be omitted.

Required resolution: make every contract field explicit in the final
observation or aggregate checkpoint record without inventing a threshold or
extra timing run.

Resolution: independent verification extended the observation record with
build mode, host OS/release/processor/logical cores, compile-only provider
boundary, per-unit workload, zero warmup, one repetition, compiler-process
completion, pass result, and named noise sources. The source/object hashes are
the compile-probe checksums; runtime provider and runtime checksum are
truthfully not applicable.

The final GCC and Clang observation records contain the new fields and passed.

Status: resolved.

### M8R2-DOC-015 — CUDA benchmark synchronization boundary

Severity: performance-methodology documentation defect

The first guide draft described all measured CUDA repetitions as enqueued
before one final wait. The live Dense CUDA, Sparse CUDA, and Random CUDA
probes wait for each operation inside the measured loop.

Resolution: state that the host timer includes repeated enqueue plus
per-operation completion. It remains a host-observed measurement rather than a
kernel-event-only result.

Status: resolved before final acceptance.

### M8R2-DOC-016 — CUDA matrix Cartesian implication

Severity: support-evidence wording defect

The first support draft could be read as applying every GCC/Clang,
Debug/Release, and static/shared CPU combination to CUDA. CUDA evidence is
valid only for the exact CUDA compiler, host compiler, configuration, linkage,
and architecture actually exercised.

Resolution: state the CPU Cartesian matrix separately and prohibit inference
of unrun CUDA combinations. The final matrix names both the GCC C++/NVCC/GNU
host Release/shared full run and Clang C++/NVCC/GNU host Release/static
focused run, without claiming a true Clang CUDA compiler/host combination.

Status: resolved before final acceptance.

### M8R2-DOC-017 — sanitizer scope distinction

Severity: evidence-boundary review

ASan+UBSan passed the selected 137-test suite. Standalone LSan and TSan each
passed a 12-test sanitizer-safe runtime subset, but the full test-suite build
contains deliberate global allocation-interposition tests that collide with
those sanitizer runtimes' own allocator interceptors. Compute Sanitizer
memcheck ran 12 applicable CUDA runtimes; the native-state negative
intentionally requests an impossible allocation that the tool reports as an
API error, although its normal runtime test passed.

Resolution: record the ASan+UBSan and Compute Sanitizer passes, classify the
LSan/TSan full-suite builds and the intentionally incompatible memcheck
negative as `skipped`, and do not convert test-harness exclusions into product
passes or failures.

Status: accepted with exact boundaries.

## Current disposition

The documentation-role files match the frozen revision-2 product and package
contract by inspection, and all identified shared-documentation defects are
resolved. Independent package/header/API/downstream execution is accepted.
The terminal portability and lead matrices are accepted with their exact
combination and skip boundaries. No open documentation, public API, package,
dependency, provider-evidence, performance-methodology, or downstream blocker
remains.

No product target, dependency, operation, provider, compatibility facade,
schema, or later-milestone feature is documented. No commit, push, merge, tag,
release, branch deletion, pull-request mutation, or sibling-repository write
was performed by this role.

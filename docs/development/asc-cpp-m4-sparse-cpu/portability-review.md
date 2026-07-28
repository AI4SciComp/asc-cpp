# Milestone 4 Sparse CPU portability, GPU, and performance review

Status: complete; no unresolved portability, GPU, or performance blocker

Date: 2026-07-28

## Scope and authority

This independent review covers only the frozen Milestone 4 Sparse CPU
contract, its ownership and provenance records, approved architecture and ADRs,
the M4 production self-review, and the current integrated M4 production,
verification, package, and documentation changes.

The review did not use MdeCpp, the deleted asc-cpp implementation, a provider
implementation, or a later-milestone implementation or test. Its only writable
artifact is this report.

## Portability review

The production surface uses standard C++20 facilities supported by the
approved compiler line. It does not use compiler builtins, inline assembly,
nonstandard integer extensions, C++23 APIs, provider SDK types, or
platform-specific production headers. Public headers directly include the
standard and ASC declarations they use, compile with extensions disabled, and
retain the full-path guard and flat-namespace policy.

The compiled SpMV bridge uses ordinary function pointers and C++20 designated
aggregate initialization. The only compiled Sparse definitions are the
explicit `float` and `double` serial reference entry points. GCC shared-library
symbol inspection exposes those two bridge symbols while keeping the reference
template implementations local.

Windows portability is addressed by the public export header:

- shared producers use CMake's `asc_sparse_EXPORTS` definition and
  `__declspec(dllexport)`;
- shared consumers use `__declspec(dllimport)`; and
- static consumers receive the propagated `ASC_SPARSE_STATIC_DEFINE`.

The local environment cannot execute native MSVC or AppleClang builds. Their
workflow definitions and source paths were inspected, but native compilation,
linking, and runtime remain CI evidence rather than a local pass claim.
Apple shared installs select `@loader_path`; ELF shared installs select
`$ORIGIN`; Windows does not receive an RPATH.

The storage and alias checks use `std::uintptr_t` only after checked byte
counts. An overflowing integer address range is treated conservatively as
overlap or rejected metadata. External views and adapters remain responsible
for truthful provenance, allocation length, alignment, lifetime, alias, and
uniqueness information; this is an unavoidable non-owning protocol boundary,
not a platform-specific guarantee.

## Static/shared, symbols, RPATH, and packages

An independent GCC 11.4 static Debug configure and full build passed with
warnings as errors. A GCC 11.4 shared Release configure and full build also
passed. The lead's stabilized clean matrices then passed:

```text
GCC 11.4 Debug/static/current CMake:       141/141
Clang 19 Release/shared/current CMake:     141/141
GCC 11.4 Release/static/CMake 3.25.0:      141/141
```

In the independently inspected shared build:

```text
libasc_sparse.so dynamic dependencies:
  libasc_core.so
  libstdc++.so.6
  libgcc_s.so.1
  libc.so.6

defined dynamic Sparse symbols:
  SpmvReference(float, ...)
  SpmvReference(double, ...)
```

There is no Dense, Utilities, Random, optional provider, CUDA, BLAS/LAPACK, or
other external-library dependency. The installed imported target records
`INTERFACE_LINK_LIBRARIES "ASC::core;ASC::expression"` exactly.

The build-tree Sparse shared object uses the expected build-tree Core path.
The installed shared object uses `RUNPATH [$ORIGIN]`, has no source/build
directory in its runtime path, and continues to resolve `libasc_core.so` after
installation into and relocation through paths containing spaces. The
installed package contains the six approved Sparse headers and the
`ASCCppSparseTargets.cmake` export pair.

The independent shared Release M4 selection passed 49/49 tests. The full clean
matrix passed 141/141. These selections include the Sparse build-tree consumer,
relocated installed Sparse-only consumer, subproject consumer,
component/package closure checks, header probes, and runtime/benchmark tests.

## Sanitizer suitability

Sparse owners rely on the already-reviewed Core `Buffer`/`MemoryResource`
lifetime mechanism. M4 verification injects allocation failures at every
destination-buffer boundary, compares successful allocations with
deallocations, and checks that partial construction leaves no live allocation.
Views are explicitly non-owning, so use after owner destruction remains a
caller error suitable for ASan detection rather than a promised runtime
lifetime check.

The operation-allocation probe interposes ordinary and aligned global
`new`/`delete`, with `_aligned_malloc`/`_aligned_free` on MSVC and
`std::aligned_alloc`/`std::free` elsewhere. Its state is used by serial,
single-threaded tests only. The stabilized compatible sanitizer selections
passed with no diagnostic:

```text
Clang 19 ASan + UBSan: 125/125
Clang 19 LSan:          10/10
Clang 19 TSan:          10/10
```

Package/consumer subprocess tests are intentionally outside the compatible
ASan/UBSan selection. The focused M4 ASan/UBSan verification also passed 6/6,
including all four Sparse runtime executables, multi-TU use, and the
allocation/no-densification benchmark.

## Allocation, no-densification, and performance

Code inspection confirms that structure-preserving evaluation and CSR SpMV
traverse caller-owned Sparse/vector storage directly. Neither operation
creates a destination owner, workspace, coordinate temporary, packed buffer,
dense buffer, transfer, synchronization, or provider dispatch. Verification
wraps both operations in a process allocation probe and requires zero
allocations. Conversion tests separately account for the explicit destination
offset/index/value allocations and partial-failure releases.

The benchmark is deterministic and informational only. It records compiler,
configuration, rows, columns, NNZ, CSR format, operation, iterations, elapsed
time, per-iteration time, checksum, and allocation count. It imposes no
unstable speed threshold. An independent GCC 11.4 Release/shared run reported:

```text
compiler=gcc configuration=release-like rows=128 columns=256 nnz=512
operation=structure_preserving_evaluate iterations=64
total_ns=12030979 per_iteration_ns=187984 checksum=-574.125 allocations=0

compiler=gcc configuration=release-like rows=128 columns=256 nnz=512
operation=spmv iterations=256
total_ns=1840564 per_iteration_ns=7189.7 checksum=977.375 allocations=0
```

These timings are observations from one local run, not regression gates or
provider comparisons. Coordinate finalization and several conversions use the
documented deterministic repeated-scan reference algorithms; compressed
evaluation also reconstructs stored coordinates by repeated outer-offset
scans. Their complexity is acceptable for the reference milestone but remains
a performance risk for large inputs.

## GPU and provider classification

GPU evidence is exactly **skipped**.

Milestone 4 contains no GPU option, provider target, provider component,
provider discovery, CUDA/HIP language enablement, SDK header, SDK link,
provider dispatch, device runtime, or CPU/GPU parity execution. No detection
or hardware inventory is relabeled as configure-tested, compile-tested,
runtime-tested, or parity-tested.

## Findings and resolutions

1. An early independent static test run found a coordinate overflow test
   expecting `kOverflow` from an NNZ value that already violated
   `NNZ <= logical_size`; production correctly returned `kShape` first. The
   verification owner changed the test shape to
   `{numeric_limits<extent_t>::max(), 1}` with maximum NNZ, preserving valid
   shape metadata and reaching the intended coordinate-byte overflow. A fresh
   rebuild and runtime pass resolved the finding.
2. No unsupported compiler extension, native platform branch defect,
   symbol-visibility defect, package-closure defect, hidden provider edge,
   densification, or operation allocation was found in the reviewed M4
   surface.

## Remaining risks

- Native MSVC and AppleClang are not locally runtime-tested; their current
  workflows are the next authoritative evidence when publication is approved.
- External non-owning adapters remain a caller truth boundary.
- Reference repeated-scan algorithms are intentionally unoptimized and have no
  performance threshold in M4.

Within the frozen Milestone 4 boundary, this independent review approves the
integrated candidate for Publication Checkpoint B.

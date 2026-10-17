# PBTRF/PBTF2/PBTRS INFO correction

Status: bounded correction implemented and locally verified on 2026-09-12.
The twelve S/D/C/Z rows retain their existing incomplete family status.

The checked adapters previously initialized INFO to zero. An admitted foreign
call that omitted its INFO write therefore appeared successful, including an
empty call. On the actual little-endian ILP64 profile a 32-bit zero write also
appeared successful. Both factor routines share one call site; the solve has
the other. Initializing both slots to the full-width minimum sentinel lets the
existing INFO interpreter reject these cases as provider errors with unusable
output. Native INFO remains recorded. This changes no provider arithmetic,
UPLO mapping, Hermitian handling, valid INFO interpretation or workspace size.

The additive fault tests cover all four scalar types, three routines, both
triangles, both band layouts and independently selected RHS layouts. They
retain existing assertions and add omitted writes, actual ILP64 partial-width
writes and empty calls. Packed output is withheld; directly supplied storage
may already have been changed by the foreign call and cannot be rolled back.
Twelve compile-time checks compare complete fault-wrapper signatures with
the pinned header or the existing compiler-derived PBTF2 declarations.

## Evidence

All new artifacts are external under
`master-continuation-20260910-01/continuation-20260912-01`, relative to the
existing programme evidence root. `pb-info-final-audit/audit.json` binds the
completed profiles to the five current production/header/test input hashes.

| Check | LP64 | ILP64 |
| --- | --- | --- |
| Regression before correction | 4 failed processes, 672 assertions | 4 failed processes, 1,008 assertions |
| Static Release numerical/fidelity/failure/limits | 13/13 | 13/13 |
| Static Debug, including public header modes | 15/15 | 15/15 |
| Static ASan/UBSan, including public header modes | 15/15 | 15/15 |
| Shared Release, including public header modes | 15/15 | 15/15 |
| Relocated static installed PB consumer | 1/1 | 1/1 |
| Relocated shared installed PB consumer | 1/1 | 1/1 |

Every profile has zero skips. Six strict translation-unit checks pass. The
public documentation audit covers 140 headers and 2,414 members with zero
Doxygen warnings. Installed consumers use the maintained public example,
copied outside the source tree, and isolated relocated packages; metadata,
include paths and runtime dependencies are checked. The pinned foreign
provider is uninstrumented in the sanitizer profiles.

Two runner issues are retained explicitly: a log-directory/build-directory
collision before shared compilation and an initial whole-package installation
before unrelated production libraries had been built. The runners resumed
the same configured builds and prefixes after those omissions were corrected.
Neither issue skipped or changed a failed numerical test.

The public-header checksum and 32 affected compute/driver/dependent artifact
indexes were amended atomically through
`docs/contracts/lapack-pb-info-evidence-extension.json`. No historical execution
record, native acceptance, array-I/O acceptance or robust PPSVX result changes.
Coverage, backlog generation, documentation consistency, links and public
surface checks pass. Reference callable/fully-verified counts remain 80/0.

## Remaining scope

This correction does not complete normalized PB family contracts or the wider
provider/platform profile. Shared Debug/sanitizer and TSan are not claimed by
this audit. The original [band review](band-cholesky-review.md) retains its
source and numerical distinctions. PTTRS and other previously recorded
mathematical failures remain open. The next dependency-ready implementation
is the four required GBTF2 rows, currently absent from the public API.

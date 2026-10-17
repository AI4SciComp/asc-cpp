# Required native Matrix Market integration

This P10 implementation adds the required Dense array and Sparse COO/CSR/CSC
coordinate paths. It does not add optional Dense-coordinate/Sparse-array
conversions, a new product module, foreign runtime dependency, implicit transfer
or densification. The full program and final array behavior/evidence ledger
remain separate from this bounded integration checkpoint.

## Reviewed ownership and behavior

Core owns bounded lexical/field/symmetry parsing and scalar formatting in
`asc/core/matrix_market.h` and `src/core/matrix_market.cc`. Dense and Sparse
own independent public headers and compiled implementations. Their only direct
product dependencies remain Core and Expression. All three headers are
explicitly registered in target file sets, normal/no-exception self-containment
tests, architecture inventories, ownership and normalized header baselines.
These are explicit implementation self-review changes, not owner license or
release approval.

The complete production files were reviewed against the frozen Matrix Market
profile before registration. Preparation copies checked metadata without
allocating values. Owning reads use explicit resources; Into reads use disjoint
typed staging and publish only after complete parse/expansion/assembly/EOF.
Sparse checks every destination coordinate before value commit. Full reachable
spans, report/reader metadata aliases, byte/count products, sort comparisons,
peak resource requests, short/failing stream progress and rollback are tested.
Dense preflight/allocation failure leaves its prepared cursor reusable; Sparse
payload attempts consume theirs even on preflight failure, as documented.
Neither rolls back source progress or staging contents.

Integer conversion never passes through double; integer-to-floating requires
exact representability. Finite real/complex input rounds directly to the chosen
component type, rejecting overflow and nonzero underflow to zero. Pattern units
are explicit. Sparse duplicate groups are stable in original record order,
checked in destination precision before one symmetry expansion and optional
zero removal. Bounded insertion sort is quadratic, not advertised as linear.
General Sparse output preserves stored entries under the explicit zero policy;
structured output checks the represented matrix and reports unavoidable
stored-zero projection. Native ASC archives remain the exact-structure format.

Stream writers perform complete validation/counting before first sink output;
I/O failure can leave partial bytes. File saves require explicit truncate,
check flush/close and do not promise atomic replacement or crash durability.
In particular a Sparse save can truncate before later whole-matrix validation
fails. No stronger cross-codec file rollback guarantee is implied.

## Exact frozen source identities

| Artifact | SHA-256 |
| --- | --- |
| Core header | `502db0a675457ea6b4e220b4ca5873bff066d16cac67af807233f52b35aa98ae` |
| Core source | `41e4aacbf7cab1e29ef9b64f1cfa675d4dbd1d83146a5e96c167cfd66702991e` |
| Dense header | `524ab033acfa9c38797d08b529180c09cb3d4a2db8e49f13130c834b99ebd8b7` |
| Dense source | `3d5aff8945743cfef058c137585eca268ead5961996d59d8f6800de98139a418` |
| Sparse header | `82250d8639acf8df9ed43a7686e346285dcea75b7fb3d9267f138f7349a5196d` |
| Sparse source | `2cc86196bc82dca8550675f0d15692e355f792c6da2448cd996fee406eafb2d5` |

The independent scoped helper04, Dense05 and Sparse05 evidence is retained,
not relabeled as integrated execution. Dense05 has15 internal groups and1024
mutation cases; Sparse05 has258 internal cases and12294 mutation/seed cases.
These counts are not CTest counts. Core helper04 exercises12 scalar paths.
Each has separate Debug/Release/sanitizer, strict style/header and bounded
allocation diagnostics. External Linux/glibc interposition uses positive
controls for direct libc and shared libstdc++ allocations; its exact allocator
entry-point/runtime scope is not a universal other-platform guarantee.
The [Dense review](dense-matrix-market-review.md) gives its detailed provenance.
Sparse final external summary is `p10-sparse-mm-05/verification-summary.md`,
SHA256 `8f83e886ccd009627908b7d33fa24b925d2c134957cee066d3d58cfd31247958`.

Independent hand-derived fixtures remain primary. External SciPy1.15.3 with
NumPy2.2.3 adds bidirectional development evidence: all10 Dense native array
classes and three Sparse Hermitian/int64-above-2^53/pattern pairs. Neither
package is an ASC runtime or ordinary-build dependency. The registered Dense
write/read executable additionally checks its ten fixed expected matrices;
that native round-trip test is not represented as an external codec test.

## Integrated candidate01 and required correction

Tree `a26e0e7587b56f8838edd4f54d6acbd458e98800`, archive SHA256
`e865f24fa6c2b6316a3581f33b6a369abdb74a5e200bd98e78a972355a7dffb6`,
is preserved in external `p10-matrix-market-01`. It is built upon the corrected
LU layout tree8c53d8d, excluding unregistered advanced LU and native/provider
Cholesky/QR files via a separate exact-file index.

All six selected Matrix Market CTests pass with Clang19 ASan/UBSan, with Core,
Dense and Sparse rebuilt under instrumentation; system libraries are not.
Strict Doxygen passes71/71 headers,1413 public members and zero warnings.
Full Debug and Release each execute271 tests:267 pass,4 fail; shared Release
executes273 with269 passing and4 failing. All have zero skips. The four
package-header tests still expected68 headers and omitted the
three new headers. The independent exact oracle is corrected to all71; no
comparison or test is removed. Codec/header/numerical tests and the relocated
installed examples pass within these failed full runs.

The installed examples include independent Dense Hermitian array and Sparse
Hermitian COO/CSR/CSC oracles, explicit save/reload and malformed-payload
rollback. The shared package runner additionally configures each consumer
alone, rejecting unrelated storage/provider targets and non-C++ language
enablement. Public example paths and byte sinks may allocate; they are not
misrepresented as bounded production allocation probes.

## Corrected integrated candidate02

Tree `74ff2fb12edd81b011bddc2efa44239bcbbad6f6`, archive SHA256
`f8e1e1d4cacbaff08e5e2e97d6903440532a8f2f421541855d6c51cd9e684361`,
is preserved in external `p10-matrix-market-02`. All production headers,
sources, numerical tests and examples are byte-identical to candidate01.
Changes are the corrected independent header oracle, exact capability
manifest/consumer names and program records.

Fresh full provider-free GCC11 Debug and Release each pass271/271; shared
Release passes273/273. All have zero skips. BLAS/Random regressions and
component isolation execute in each full suite. Doxygen again passes71/71
headers,1413 members and zero warnings; Markdown checks pass. Candidate01's
6/6 selected Clang19 ASan/UBSan result is retained under its own identity,
not relabeled as a candidate02 full sanitizer run.

Actual relocated package logs show nine aggregate examples plus one separately
configured Dense-only and one Sparse-only Matrix Market example passing in
each lane. The shared Release ELF report lists only the expected Core and
system runtime dependencies; no LAPACK/Fortran or Dense-Sparse edge appears.
The configured Release baseline path is empty because the retained GCC11 CPU
baseline is Debug. ELF inspection executes, but does not establish equivalence
to that older Debug baseline. No baseline is overwritten or skip credited.

Raw records/logs/JUnit hashes for both candidates are indexed in
[verification-checkpoints.json](verification-checkpoints.json). Final array
mode/evidence closure, remaining platform/ABI gates and the full rest of
P00–P11 remain required. Next implementation is the separately frozen
GECON/GERFS/GESVX and equilibration stride correction, then native and provider
Cholesky/QR and all remaining advanced/specialized families.

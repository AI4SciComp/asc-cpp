# Recovered P05 band expert audit

Scope: all 54 owned files in the four preserved manifests for S/D/C/Z PBSV,
PBEQU, PBCON, PBRFS and PBSVX. The 14 earlier PBTRF/PBTF2/PBTRS files remain
unchanged dependencies. Native coverage, installed integration, shared registry
updates and the rest of P05 are outside this isolated handoff.

## Concrete fixes and controls

The 20 foreign-calling main functions now instantiate the shared test-only
NormalReturnGuard. This is the exact integrator helper with SHA-256
6d0d088037e666f0e9c92d912dbddb2a7c48501284183c52cb79e3b0c9bdc81d.
The integrator retains executed normal-return/exit/actual pinned Fortran STOP
controls. No provider error-handler override or test-result reinterpretation
is present in this harness.

Eight FACT=F parameter descriptions in lapack_cholesky_band_expert.h had called
A unscaled, contradicting its file contract, pinned PBSVX argument contract,
and the existing scaled supplied-factor tests. Candidate02 corrects these
comments to require A already scaled as supplied equilibration specifies.
Production declarations and bodies are unchanged.

Candidate01 and candidate02 strict runs identify the unused cstdint include in
band_expert_edges_test.cc. Candidate03 removes only that include and retains
all numerical assertions, fixtures, loops and tolerances. The prior strict
failures remain frozen. Candidate03 recompiles the affected target and all
six linked band adapters in every actual ABI/build mode. Other candidate02
executables have unchanged source/dependency identities; final evidence must
show this composition explicitly, never label an old source tree current.

The old exact-twenty-inventory-01.json is a truncated tool-output artifact,
not valid JSON. Its stable historical hash proves preservation, not validity.
The replacement exact-twenty-inventory-02.json is valid JSON extracted from
the full frozen source inventory. All 20 source-file hashes were checked
against pristine pinned Git blobs at 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca,
tree 7217db728e4f7ee87dabf545a1a87a3d6cd30e9b. No shared manifest was changed.

## Source, ABI, error and memory audit

All 54 owned files were read against the current runbook P05 and cross-cutting
requirements, frozen contracts and D022. Pinned source bodies and precision
pair differences retain the source's fixed work formulas, all FACT modes,
real/complex auxiliaries and explicit hidden CHARACTER lengths. Integer
estimator entries are live, separate native-sized kInteger objects created
by placement new; no kPivotConversion entry size changed.

Queries use checked fixed formulas and make no foreign query calls. Metadata,
shape, placement, plan freshness, workspace sizes/alignment and pairwise
alias checks precede mutation. Tests retain real-backed invalid placement and
alias cases. Empty calls use actual dummy live objects and still call the
provider. Source cursor expressions have separate pure integer-boundary tests.
Row-major copies retain raw supplied factors, selected storage and padding
rules; Hermitian original diagonals use only their real components. FACT=N/E
output-only factor and solution storage is reserved without reading old data.
INFO mapping retains documented partial factors and mutable scaling outputs,
while invalid or impossible raw INFO does not publish packed outputs. Direct
column-major foreign mutations cannot be rolled back and are documented.

Regenerated graphs match all historical objects, edges and external leaves
after resolving only relative archive paths. Full/checked object counts are
67/66 for PBSV/PBEQU, 60/59 for PBCON, 42/41 for PBRFS and 138/137 for PBSVX,
identically for actual LP64 and ILP64. Full graphs retain XERBLA I/O/STOP edges;
only the source-preflight-proven XERBLA path is removed from conditioned graphs.
Runtime leaves and runtime identities match the previous exact review. The
libc lanes add the existing positive-control allocation probe; they do not
claim interposition into every foreign runtime implementation.

## Required mathematical failures

All required mathematical tests remain normal registered CTest tests. Complex
lower PBCON on scaled KD=1 reports 0.25 instead of independently proved 1/9.
Tiny PBRFS reports infinite FERR where the independent guarded bound is finite.
Tiny PBSVX FACT=N/F reports RCOND=0 and infinite FERR; FACT=E evaluates S*S*A
with overflowing intermediate product, causing infinite factors and zero/NaN
outputs. The complex lower condition defect also reaches PBSVX. Raw pinned
fidelity is checked separately and does not satisfy these mathematical gates.
No alternate triangle, algorithm, pin, exclusion, skip or expected-fail label
was introduced.

## Documentation and verification scope

The unchanged repository BuildDoxygen.cmake and check_doxygen.py pass for all
97 frozen public headers, 1885 documented members, zero warnings. Two earlier
standalone harness attempts are retained: missing parent output directory,
then omission of the repository's removal of generated xml/Doxyfile.xml.
The successful replay uses the actual repository helper without suppressing
warnings or changing checker semantics. All 11 owned public/private headers
compile independently in C++20 with exceptions disabled for both actual ABIs.

This is a diagnostic direct-link handoff using exact frozen V9 foundations.
The sanitizer instruments candidate C++ adapters/tests and the recorded V9
C++ archive; pinned Fortran, BLAS and runtime libraries are not instrumented.
Root owns integration and installed public-consumer verification.

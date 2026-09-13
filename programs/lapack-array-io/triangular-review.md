# Full triangular source and contract review

This bounded slice implements actual S/D/C/Z TRTRI, TRTI2 and TRTRS from
Reference-LAPACK commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`.
TRTI2 has no pinned LAPACKE declaration. Its four private declarations are
bound to actual GNU Fortran11 compiler emissions in both real integer ABIs.
No LAPACKE-only denominator, local replacement algorithm or native credit is
introduced. All12 routines remain required, with root registration pending.

The31-member conditional source closure per ABI is recorded in
`p05-full-triangular-root-02/source-closure-{lp64,ilp64}.json`. It binds all
root and reachable Reference BLAS member bytes, pinned source candidates,
imports and installed headers. No included member has writable static data.
The external imports are `_gfortran_concat_string`, `memcmp` and `memset`.
The concat implementation in the actual libgfortran runtime uses only bounded
memcpy/memset branches; TRTRI supplies lengths1+1 and a two-byte destination.
This source/runtime review supplements executed observations; it is not a
claim that the foreign archives are sanitizer-instrumented.

The root reviewed all24 public declarations and all five implementation
files, including the private count and signature headers. The two uncompiled
default constructions of LapackWorkspacePlan were invalid; candidate04
constructs plans from the validated identity. Four query RHS descriptions
were corrected earlier to say the descriptor remains unchanged. Direct
include ownership and private native-name macros were corrected under the
unmodified strict configuration. No numerical branch or INFO acceptance was
changed by these compilation corrections.

## Source integer and storage admission

The full TRTRI/TRTI2/TRTRS computational statements are reviewed together
with the selected TRMM/TRMV/TRSM/SCAL branches. Fourteen complete S/D and C/Z
statement-pair comparisons confirm the precision variants differ only in
explicit scalar types/literal precision, scalar routine and SCAL argument
names, conjugation intrinsic spelling and EXTERNAL declaration ordering.
Every original file has its own preserved hash. The comparison is a review
aid, distinct from the24 actual compiler-emission signature checks.

TRTRI first checks flags/dimensions, returns for n=0, and scans a nonunit
diagonal before mutation. ILAENV receives ISPEC1 and the fixed scalar TRTRI
name; its TR/TRI branch returns64. Other ILAENV branches and their object
imports are unreachable for these calls. Upper blocked J advances by64,
including its terminal update: the checked last start plus64 must fit.
Lower J descends by64; J+JB may evaluate n+1 before its branch, so n must be
less than the native limit. The nonunit INFO scan also evaluates n+1.
The same n guard is conservatively used for unit TRTRI.

Each diagonal TRTI2 block is at most64. TRMM receives a left triangular
suborder at most n-64 and at most64 columns; right TRSM receives the same
bounds, with alpha exactly minus one. Their positive loop terminals and
K+1/J+1 expressions therefore fit under the enclosing guards. LDA remains
at least the full order. All submatrix origins and last touched elements lie
inside the selected triangle of the original checked full storage.

Standalone upper TRTI2 advances J=1..n and requires n+1 to fit. Lower TRTI2
descends n..1 to0 and can admit n at the native limit in the pure count
function. Its TRMV/SCAL suborders are at most n-1. Increment-one real SCAL's
cleanup/unrolled-five loop ends at suborder+1; complex SCAL's ordinary loop
has that same bound. Lower/no-transpose TRMV and its J+1 bound remain within
n. No native strided SCAL or nonunit INCX branch is reached.

TRTRS returns locally only for n=0. With n>0 and nrhs=0 it still scans a
nonunit diagonal. Unit zero-RHS reaches TRSM's zero-column return and reads
neither A nor B. Active left TRSM uses alpha exactly one. Every active RHS
loop requires nrhs+1 to fit. Only upper/no-transpose/unit permits n at the
native limit: its descending K and inner1..K-1 loops avoid n+1. Other
operation/triangle branches or a nonunit diagonal require n below the limit.
These dimension checks use actual normalized foreign leading dimensions.

The Dense BLAS factory computes the element span and byte span with checked
signed stride arithmetic, verifies alignment and containment in real backing
storage, then preserves the original leading dimension in the descriptor.
This also bounds compiler-generated native address products. Row packing
uses checked n*n and n*nrhs entry sums with the caller's byte budget. The
packer reads only the selected triangle and skips an implicit unit diagonal.
Inverse publication follows the same selection. No transpose reinterpretation
or complex conjugation shortcut replaces the native operation.

For a zero-RHS nonunit solve, the row-major diagonal has the same i*(ld+1)
addresses as a column-major diagonal, so A need not be packed. Its original
LDA is retained when n>1. Unit zero-RHS and n=1 use normalized unused LDA;
B uses a live local dummy with LDB=max(1,n). Original A/B strides still enter
the plan identity. Empty completion never fabricates a raw INFO value.

## Error, ownership and interpretation

Provider, plan, workspace and report object aliases are rejected before
report reset. Remaining structural failures reset diagnostics, mutate no
numerical/workspace byte and call none of the12 observed native routes.
Plans bind scalar, operation, both layouts/strides, actual foreign sizes and
provider identity. A/B overlap is rejected by the shared disjoint check.
The new preflight test specifically executes invalid flags, altered plan,
short nonempty packing, operand/workspace overlap and live report/plan/
workspace byte aliases; it does not forge numerical backing spans.

Every scalar inverse/solve dispatch initializes full-width INFO to its native
minimum after preflight. Negative, unwritten, tested low-half partial writes
and impossible positive INFO are provider defects. Native minimum is never
negated. Positive INFO in1..n is singular only for nonunit TRTRI/TRTRS;
reported singular results preserve all operands and give zero-based index.
TRTI2 has no singularity check: it executes a zero nonunit diagonal and may
return INFO0 with nonfinite output. No finite-result, conditioning or factor
provenance certificate is invented. Source packing is withheld on a provider
defect; directly borrowed output is marked unusable because native writes
cannot be rolled back.

## Executed and open scope

Candidate05's initial audit binds six4/4 configurations,30 fresh objects and
24 actual emitted signatures. Candidate08 adds INFO and real-singularity
controls and concrete preflight classes. Each six-mode lane reports5/5,
6272 analytic workflows/1143040 assertions and7696 failure/preflight profiles/
61072 assertions, without skips. The exact59-TU v14 ASC libraries are reused
only after rehashing all audited objects, libraries and compiler-read inputs.
One new triangular TU and eight test TUs are freshly compiled per lane.

Current public tests include both layouts, all four scalar types, both
triangles/diagonal choices, N/T/C, n=0/1/2/5/64/65/67, zero/three RHS and
power-of-two moderate scaling. Analytic inverse coefficients derive from the
nilpotent bidiagonal formula; solve RHS values derive from an independent
known solution. Byte preservation checks retain ignored triangles, unit
stored diagonals and padding. They do not alone prove absence of ignored
reads. Stronger read observation, all cold/warm real allocation paths,
mutation-on-defect controls, full normalized mode evidence, extreme-range
mathematics, concurrency/platform/shared-provider gates and root installed
package acceptance remain open. None is recategorized as inapplicable.

# GBTRF/GBTRS normalized continuation

Status: bounded local contract and engineering verification complete; GBTRF
mathematical acceptance remains blocked. The eight
APIs, backend and public declarations already exist and remain unchanged.
The preserved [original review](lu-band-review.md) and
[GBTF2 continuation](lu-band-unblocked-review.md) retain their earlier evidence.

## Contract reconciliation

The eight exact source records are retained externally in
`master-continuation-20260910-01/continuation-20260912-01/gb-contract-01`.
They match the generated inventory and Reference LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. The checked provider ABI, scalar
representations, nominal factor format and explicit CPU boundary are unchanged.

| Property | GBTRF | GBTRS |
| --- | --- | --- |
| Scalars | S/D real and C/Z general complex | Same scalar as the factors and RHS |
| Shape | Rectangular m-by-n | Square n-by-n factors, n-by-nrhs RHS |
| Band storage | Column-major LDAB >= 2*KL+KU+1, diagonal at zero-based KL+KU | Immutable identical factor storage including U fill |
| Pivot encoding | min(m,n) one-based interleaved row swaps | n same-origin swaps, checked again before solving |
| Options | Provider chooses blocked/unblocked branch through pinned ILAENV | N/T/C, independently selected column/row RHS layout; complex C conjugates |
| Numerical storage | AB and IPIV are overwritten; padding is preserved | Only logical B/X is overwritten; factors/pivots remain unchanged |
| Native workspace | No WORK argument; fixed WORK13/WORK31 arrays in the blocked source | No WORK argument |
| ASC workspace | min(m,n) actual-width aligned native integers | n native integers when active, plus n*nrhs live scalars for active row-major RHS |
| INFO | Positive i retains completed singular raw factors and checked pivots, diagnostic i-1 | Native INFO has no positive numerical outcome; exact zero U is rejected before the call without inventing INFO |

Queries inspect metadata only. Exact plan identity, dimensions, source integer
intermediates, placement, capacities and all operand/workspace/report/provider/
plan aliases are checked before mutation. Invalid provider INFO/pivots produce
unusable output and prevent converted pivot or packed RHS publication. Direct
foreign writes cannot be rolled back. Empty calls retain the real native INFO;
GBTRS still supplies LDB >= max(1,N) before its empty quick return.

The nominal view requires complete successful same-scalar/same-provider
GBTRF or explicit GBTF2 provenance. Rectangular factors may be inspected;
solving requires square factors. Borrowed immutable factor reuse and independent
RHS storage do not change the recorded originating routine. No dense GETRF
factor tag, inverse, automatic equilibration or hidden factorization is used.

The normalized mapping records one column-major factor mode per scalar and
six solve modes per scalar: N/T/C times column/row RHS layout. Shape, bandwidth,
empty/scalar, pivot, alias and numerical cases remain required test classes.
All four scalar factorizations and every solve option are independently
required; successful native fidelity does not establish mathematical success.

## Evidence reuse and remaining range check

The immediately preceding GBTF2 audit includes eight actual-ABI producer
profiles whose existing GBTRF/GBTRS ordinary/failure/boundary/ABI/limit checks
all pass. Those completed processes are reused after comparing current source
and provider identities. Production code, public API and the existing tests
are unchanged in this continuation. No frozen native20, array-I/O or PPSVX
acceptance is replayed.

The four installed packages and consumer executables from that delivery remain
present, relocated and isolated. Their default GBTRF/GBTRS invocation now passes
1/1 each in static/shared LP64/ILP64, using identical maintained consumer/header
bytes. The previously executed GBTF2 invocation remains a separate result.

The new maintained `lu_band_range_test.cc` has independent classes:

- GBTRF factor mathematics for A=scale*I, in both provider-selected branches:
  KL=1/KU=0 and KL=32/KU=65, both at order two. Exact multipliers must be finite
  zero and independent reconstruction must recover the original matrix.
- Direct GBTRF provider fidelity for the same cases, separately reported.
- GBTRS scalar mathematics at the same scales, both RHS layouts, N/T/C and two
  RHS columns; complex factors have both real and imaginary components. The
  known solution is one, with independent scaled residual and forward checks,
  unchanged factors/pivots, padding and direct-provider comparison.

The first LP64 run passes eight of twelve processes, with four GBTRF
mathematical failures and 48 failed assertions, zero skips. Both factor
branches form ONE/pivot before scaling the exact zero multiplier. At
min-normal/1024 and twice the smallest positive subnormal this produces
nonfinite factors with INFO zero; the twice-min-normal control passes. The
blocked path has the same arithmetic cause as GBTF2. Scalar GBTRS performs
its divisions through the pinned triangular-band solve and passes this range
class, including complex conjugate transpose. A factor failure therefore does
not justify claiming that this solve class fails.

The final LP64 and ILP64 static Debug, static Release, static ASC ASan/UBSan
and shared Release runs each pass **8/12 processes**, with only the four
GBTRF mathematical failures, 48 failed assertions and zero skips. All eight
profiles have identical failure signatures. The foreign provider remains
uninstrumented. Both strict checks for the added test pass after replacing a
nested conditional with equivalent explicit control flow; the original style
failure and first LP64 execution remain preserved.

Current raw records are `gb-range-{linkage}-{mode}-{abi}` (final LP64 Release
has suffix `-final`), `gb-range-strict-final`, and `gbtrf-installed-reuse`,
under the external continuation root above. The family audit binds current
inputs and compares the reused original test/provider/package identities.
No existing ordinary process or installed package was rebuilt to recover a
lost connection. Four default installed consumer invocations passed on the
already verified relocated packages.

The contract index now has 120 reviewed rows, including these eight contracts
and 28 modes, without changing route states or historical executed identities.
There are still 2,113 required rows: 80 implemented-unverified, 326 in progress,
1,707 not started and zero fully verified Reference rows. Coverage, backlog,
public surface, documentation consistency, links and the actual hosted-workflow
selector audit pass locally. All public documentation and ABI declarations
match the preceding verified input hashes; no new public declaration is added.

The GBTRF numerical requirement remains unmet; successful scalar GBTRS tests
establish only their explicit tested domain. Shared Debug/sanitizer, TSan,
hosted checks and broader provider/platform/full execution records are not
claimed. Next: complete the already implemented PBTRF/PBTF2/PBTRS normalized
contracts and missing numerical classes using the completed INFO-correction
engineering evidence. Status remains FULL_PROGRAM_INCOMPLETE.

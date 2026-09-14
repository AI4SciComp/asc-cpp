# Complete-pivoting LU contract and source review

The checked GETC2/GESC2 interfaces and ordinary local validation are implemented.
This is an implementation self-review; required deferred admission gates remain
open and no Reference verification or release approval is granted.

The exact pinned S/D/C/Z GETC2/GESC2 sources are from Reference-LAPACK 3.12.1
commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. All eight complete files,
the two actual-ABI GNU 11 prototype emissions, and bounded native probes are in
`master-continuation-20260910-01/continuation-20260912-01/completion-execution-01/`.
The installed provider headers omit these eight names. Private declarations
match the emitted definitions; no provider header or library is modified.

GETC2 complete pivoting searches the entire trailing square, selecting the last
maximum on ties. It swaps full rows and columns, stores two distinct one-based
sequential swap sequences, and uses unconjugated GERU for complex updates.
Applying both forward swap sequences to A gives L*U. These are neither final
permutations nor interchangeable GETRF pivots.

GETC2 replaces small U pivots using the provider's SMIN rule and completes the
factorization. Positive INFO identifies the last perturbed pivot, not the first
zero or an unusable early factor. The checked result retains an accuracy warning
and a reusable factor for the perturbed system. GESC2 solves that factor system
with one RHS and returns the explicit real SCALE satisfying A_factor*x=SCALE*b;
no silent unscaling is permitted. Empty GESC2 must be handled locally because
the source dereferences RHS(I) and A(N,N) without a safe N=0 path.

The selected checked numerical input domain is finite real/complex components.
Nonfinite inputs are rejected locally before provider entry, with unchanged
numeric output. This is an explicit new-interface boundary, not numerical
normalization or a waiver of previously failing cases. In particular, an all-NaN
first GETC2 search leaves IPV/JPV uninitialized in the pinned source; no native
fixture exercises that path. Finite inputs can still overflow during elimination
or solve; output finiteness must be checked before publication and no universal
finite-input success or conditioning guarantee is made. Final contract/mode
admission remains separate from bounded prerequisites.

Both matrix layouts use caller-owned live scalar staging with compact native
LDA=N. This avoids exposing unused padding and allows withholding malformed
provider output. Two private native-width pivot arrays use explicit aligned
byte workspace; public indices remain signed 64-bit values. Count checks cover
the actual N*N+1 BLAS cursor, N*N(+N) scalar staging, 2*N integer entries and
separate aggregate byte limits. Metadata alias checks precede report reset;
other structural validation precedes numeric reads and workspace writes.

The initial native02 prerequisites pass 2596 cases per ABI, with independently
reconstructed moderate finite factors and scaled solve residuals. The additional
perturbation-native01 probes pass 300 cases per ABI: zero matrices of order 0..8,
three paddings and tiny/ordinary/large finite RHS. INFO equals N, all pivots
select N, L=I and U=SMLNUM*I exactly, and the independent scaled identity solve
passes. Every argument, factor, pivot, RHS and scalar-output guard passes. Empty
GETC2 is tested; native empty GESC2 and nonfinite calls are deliberately absent.
The maintained combined native probe separately passes 2,896 cases per ABI,
including those finite, empty-GETC2, scaling and perturbation fixtures, and its
signatures compile against all eight actual emitted declarations.

The public header adds 16 query/execute overloads and a named borrowed factor
view. Its factory checks routine/scalar/provider, complete outcome, actual INFO,
last-perturbed diagnostic, shape, placement, disjoint storage and each pivot.
A copied factor view preserves provider identity and perturbation state. The
shared factor-family enum remains unchanged; GETC2 reports leave factor_family
absent and use exact routine provenance. Existing GETRF factor views cannot
accept these reports. GESC2 revalidates factor/pivot/RHS values before staging.

Eight Release/Debug static/shared LP64/ILP64 profiles pass 208/208 selected tests,
with zero failures or skips. Mathematical and fidelity suites each cover 1,460
cases per scalar, both layouts, six orders, three paddings, twelve input seeds,
three scales, zero matrices and finite extreme singleton RHS. Repeated plans,
independent wider factor reconstruction, scaled residuals, complete guards,
allocation observations and disjoint concurrent use are checked. Each scalar
also exercises 320 live metadata aliases and structural/value/factory rejection.
Typed return fixtures check exact native entry counts, valid guarded real-call
controls, missing/malformed INFO or pivots, scale defects, factor/pivot mutation,
nonfinite output and explicit zero SCALE. Omitted or wrong finite numeric writes
with valid metadata remain deliberately uncertified; fixtures document that
limitation rather than implying a numerical output certificate.

Four independently relocated static/shared actual-ABI packages each pass the
public consumer's 5,840 mathematical cases, using only installed public headers
and ASC::dense_lapack. Runtime resolution and package metadata isolation pass.
The consumers use production libraries. Shared test interception libraries are
built from the exact production PIC objects; Fortran/BLAS/runtime internals
remain uninstrumented. No provider source/library or existing test is weakened.

Calibrated deliberate-fault observations, instrumentation requiring an ASLR
change, remote/security-sensitive operations, broader platform admission and
owner gates remain recorded and deferred under the user's execution constraint.
The unsafe native all-NaN GETC2 and empty GESC2 paths are never executed. No
historical failing execution is waived, and the existing Native verified rows
and historical evidence remain preserved.

# Checked matrix row and column permutations

This leaf implements S/D/C/Z LAPMR and LAPMT with sixteen query/execute
functions and a direction enum. Each routine has forward/backward and
row/column-storage modes. Local implementation and contract evidence remain
separate from Reference verification and programme acceptance.

## Source and ABI review

All eight complete definitions at the pinned Reference-LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read. LAPMR bodies and
prose are identical after scalar/name normalization. For LAPMT, S/C form one
identical pair and D/Z another; the pairs differ in local declaration order,
equivalent loop labels and comment spacing. Neither routine has an INFO
argument, workspace query, numeric arithmetic, argument validation or external
calls. LAPMR returns immediately when M <= 1; LAPMT when N <= 1.

FORWRD is a native LOGICAL. The actual GNU11 emitted headers match all eight
installed C declarations after removing input-pointee const qualifiers only.
Prepared LP64 uses 32-bit LOGICAL and INTEGER; global ILP64 uses 64-bit values
for both. True is passed as full-width one and false as full-width zero.
Complex objects retain the existing pinned GNU C++ ABI.

K is a one-based permutation of the selected dimension. Native code temporarily
negates its entries while visiting cycles and restores them before ordinary
return. Forward gathers input K(i) into output i; backward scatters input i
into output K(i). Malformed K can access invalid indices or fail to terminate;
only bounded valid permutations are passed to native prerequisites or controls.
The checked adapter validates the complete range and uniqueness before entry.

Native matrix storage is always compact column staging with LDX=M. Source DO
terminals can form M+1/N+1, so both nonempty dimensions must be strictly below
the actual INTEGER maximum. The existing neutral kScratch limit additionally
bounds M*N by that maximum. This also bounds native staged address offsets.
Pure count tests check square and highly rectangular limits without fabricated
large arrays. Aggregate byte limits are checked separately.

## Checked behavior

Public K is immutable, unit-contiguous `index_t` storage with one-based values.
LAPMR requires M entries; LAPMT requires N. Dense matrix layout is independent
of direction and permutation axis. Query inspects metadata only and binds
routine, scalar, dimensions, matrix layout/leading dimension, direction, index
shape and provider. Values may change between calls using the same plan.

All operands, workspace and live metadata are disjoint. Metadata overlap is
rejected before report reset; remaining structural failures reset the report
and preserve numeric arrays and workspace. Empty matrices validate structural
metadata and complete locally, without reading K or matrix values and without
native entry. Nonempty singleton permutations still validate K and enter the
provider. There is never a native INFO or native argument in the report.

For nonempty calls, raw aligned kInteger workspace holds constructed native
integer objects. It first supplies a uniqueness bitmap and then the copied K.
Invalid values return kIndex, can change integer workspace, and preserve caller
matrix and permutation storage. Live scalar kScratch stages the complete
logical matrix. The provider receives only these explicit buffers; no implicit
allocation, transfer, synchronization or provider fallback occurs.

After return, every native K entry must equal the original public index.
Failure returns kProvider with unusable output and preserves the caller matrix.
Success publishes only logical matrix entries. Padding, caller K, signed zeros,
subnormals, finite extremes, infinities, quiet NaNs and complex components keep
their values. No filtering, conjugation or numerical warning is introduced.
Restored K and ordinary return do not certify arbitrary numerical writes or
detect omitted writes; explicit omitted/wrong-output controls test that limit.
Independent providers, arrays, plans, workspaces and reports are reentrant.

## Local execution records

Evidence lives under
`master-continuation-20260910-01/continuation-20260912-01/completion-execution-01/`.
`matrix-permutation-source-review-01`, `matrix-permutation-emissions-01` and
`matrix-permutation-native-01-*` retain source identities, all emitted headers,
full-signature checks and 254,976 valid native cases for each ABI. Cases include
all permutations of dimensions zero through five, four cycle patterns at
8/17/33, both axes/directions, other dimensions 0/1/3/7, two paddings and twelve
value seeds for all four scalar types. Full matrix, K, LOGICAL and dimension
guards are checked against independent direct indexing.

Initial configure01 detected the missing additive header-owner entry; the
manifest and consumers were updated. Initial02 passes 17/17 groups per ABI.
Initial03 passes all 22 groups per ABI after adding bounded return-value
fixtures, no-entry checks and pure counts. Strict01 passes implementation and
ordinary tests and identifies missing direct includes and combined declarations
in the ABI/fault tests; those are corrected without changing assertions.

The maintained mathematical/fidelity grids have 127,488 cases per scalar,
including both layouts, and execute each plan twice. Concurrent tests use four
independent contexts with one seed each. Return-value fixtures use typed GNU
wrappers, real guarded nonreal-complex controls, returned K corruption and
explicit omitted/wrong-output cases. Invalid public indices are rejected before
any foreign call; no malformed K enters the provider.

Deliberate-fault memory observations, instrumentation requiring the historical
ASLR workaround, remote/security checks and wider-platform admission remain
required and deferred under the user's execution constraint. No existing test
is disabled, skipped or weakened, and no historical failure is waived.
Implementation self-review does not substitute for owner approval.

Initial04 also passes all 22 groups on both ABIs after adding operand placement,
index shape/stride and zero-entry validation. A final direct include was added
before strict02. All five maintained translation units pass strict02; the
copied public consumer passes installed-strict01. The maintained native02
probe passes the same 254,976 cases per ABI against all emitted signatures.
The sixteen actual archive-member disassemblies in address-review01 show
32/64-bit K negation and LOGICAL tests matching their respective ABI.

Four `matrix-permutation-installed-01-*` packages install, relocate and run
169,984 independent public cases each, with repeated plans. The consumer uses
only public headers and `ASC::dense_lapack`; it derives native integer workspace
bytes/alignment from the plan. All relocated metadata and runtime checks pass.

Four standalone/no-exception header checks, ten package manifests and three
architecture checks pass in `matrix-permutation-integration-01`. All five
168-header source/installed surfaces pass. Each ABI adds sixteen public exports
and removes none. The actual workflow selector passes with 1,893 tests, adding
twenty-two permutation groups and two header checks while retaining all prior
selections. Doxygen checks all 168 headers, 2,811 documented public members and
28 explicit programme pages with zero warnings. The eight ordinary
Release/Debug static/shared ABI executions are recorded in
`matrix-permutation-profile-results.json`; the final source/record binding is
`matrix-permutation-final-audit.json`. These records grant no Reference
verification or deferred-platform admission.

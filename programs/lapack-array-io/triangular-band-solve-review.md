# Triangular-band solve admission and memory review

Self-review admits the four actual pinned TBTRS routes and a distinct neutral
triangular-band descriptor. This checkpoint is unregistered; all12 triangular
band routines remain required and not_started in the root mapping. The existing
BLAS bandwidth domain, provider-free default and native20 scope are unchanged.

The adapter preserves the source's independent A/B layouts, N/T/C and unit
options, including KD>=N. Metadata-only queries bind all options, dimensions,
original/effective strides, scalar and provider identity. Caller workspace holds
row conversion in live T objects. Exact source-derived integer bounds cover
KD+1, nonunit diagonal and RHS loop terminals and selected lower TBSV J+KD.
Only selected coefficients are packed; unit diagonal and unused corners remain
unread. Zero-order execution is local, while nonunit zero-RHS execution retains
the actual source diagonal scan and first-zero INFO. Full-width native INFO is
initialized to a sentinel and checked before publication. No provider patch,
implicit rescaling, fallback, global handler or heap workspace is introduced.

The final candidate04 executes seven tests on each GNU Release/Debug and
ASC-only Clang19 ASan/UBSan LP64/true-ILP64 lane:42/42 pass, zero skips.
Each public suite executes36,864 independently formed solve workflows and
1,907,712 assertions. Each failure suite executes42,048 profiles and305,280
assertions, including real first/last singularity, actual operand and metadata
aliases, stale dimensions/strides/layouts/options/provider/scalar, placement,
workspace and observed C/C++ allocation checks. Integer controls execute186,643
checks; four exact native signatures match the independently emitted ABI types.
Foreign archives remain unsanitized and their source closure is separately bound.

Linux protected-memory tests now create actual C++20 arrays with non-allocating
placement array new before applying PROT_NONE. Earlier descriptor candidates
constructed individual scalars; their executed results are preserved but do not
establish the containing array lifetime. The corrected descriptor test and288
provider profiles per lane observe unit diagonal reads with active RHS, entirely
ignored unit/no-RHS A, and protected off-diagonal storage between readable
nonunit/no-RHS diagonals. All backing values and RHS padding are preserved;
zero C++ allocations are observed. This remains scoped Linux evidence.

Candidate03 both Release7/7 pass; four original strict failures remain retained.
Candidate04 fixes mapping move declarations, nodiscard accessors and a nested
conditional, and keeps the required <new> include using the existing repository
IWYU annotation. No assertion is removed. Four fresh strict checks and14-file
format validation pass; unchanged production/fault/header inputs retain their
explicit earlier strict and eight isolated public-header evidence bindings.

Audit46a0675262847b42dab863aca41103d8ce4f4ea29f2be3486224a65da3e44361
binds124 original records and78 fresh objects, with each lane's63 prior ASC
primary translation units explicitly rehashed/reused. Its completion record
passes. The earlier audit counter bug and failed script/log remain bound;
corrected audit02 binds its original82 records. The normalized ledger now has
2,746 records. The committed v18 released bytes still match their frozen audit.

Root integration, installed packages, complete normalized routine/mode,
concurrency and other platform gates remain open. TBCON/TBRFS8 still needs
source progress and mathematical review and implementation. All2,113 required
Reference routines, native20, P00-P11,130 missing XBLAS and existing required
mathematical failures remain visible. No fully verified routine credit is given.

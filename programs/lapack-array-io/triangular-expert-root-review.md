# TRCON/TRRFS root integration evidence

Eight typed S/D/C/Z condition-estimation/error-bound routes are registered in
the explicit optional v16 provider. Coverage remains294 Reference in_progress,
1819 required not_started,20 native implemented_unverified andzero fully
verified of2113 required Reference routines. TRRFS preserves X. The candidate
semantics and finite-scalar defects remain in `triangular-expert-review.md`.

The current product is v16-02 tree
`6daca25a38cbde2f9c8b476a741946c577b7c100`, source archive SHA256
`411945cf32f880dc2158f746787f3e91a6a99020cf6196b4a85a8f75ccdf7ad7`.
Its only release-source difference from v16-01 tree
`66d0768b4920c29758e1644d8dfa51a2b143db3a` is the header-inventory test's
asserted count110-to112. Both independently enumerated new headers remain
required. No test was removed, skipped or made an expected-failure property.

Executed results on both actual LP64/true ILP64 static providers:

- GNU Release full v16-01:831 tests each,773passes,54 required mathematical
  failures and4 header-count failures,zero skips.
- GNU Debug affected v16-01:40 tests each,28passes,8 required mathematical
  failures and4 header-count failures,zero skips. Both original runners retain
  their nonzero assertions because their expected sets omitted the4 defects.
- Corrected v16-02 header replay:10/10 each, comprising all4 header-manifest
  checks and all6 fresh guarded fixture setup tests. Prior unchanged Debug
  libraries/configuration are reused; no new full v16-02 suite is claimed.
- Current v16-02 ASC-only Clang19 ASan/UBSan:21 tests each,13passes andthe8
  ordinary mathematical failures,zero skips andno sanitizer diagnostics.
  Foreign archives remain unsanitized; the public GNU-only package gate stays.
- All4 Release/Debug relocated installations pass20 family consumers plus
  example and configuration rejection checks, with the original300-second
  package timeout.14 strict C++ and6 static checks pass; Doxygen covers112/112
  headers and2084 documented public members withzero warnings.

Each ofthe6 numerical build lanes freshly compiles all61 primary
Core/Dense/provider TUs:366 objects. The root audit binds actual compile and
link commands, dependency files, static-archive ELF member multiplicities,
relocated consumer headers/libraries and exact provider attestations. All
83 relevant foreign archive objects perABI and their pinned sources are
rehashed. Each reused v16-01 production/header/compiler input is unchanged in
v16-02. Strict/static evidence is source-bound to v16-01 and is reused only
across the single test-count correction.

Each lane retains6656 public workflows/104448 checks,6272 injected/real fault
profiles/52416 checks,17842 pure source-count checks and8 typed signatures.
All8 native entry points initialize full-width INFO to its native minimum.
Allocation probes retain their exact scope: GNU C++ observations and linked
static C allocator interception, with no foreign-runtime-wide claim.

Audit: `p05-triangular-expert-integration-v16-02/audit.json`, SHA256
`fdde49a93e0c03100a13452164e80d401baa218ea789364fa6269aee44a48bb6`.
It binds48 original raw command records and both1007-file frozen snapshots;
`completion-record/record.json` successfully rehashes the immutable audit,
bringing this integration checkpoint to49 records. This is root self-review,
not independent-review or owner approval.

The54 mathematical gates,130 missing required XBLAS routines, remaining
operand/metadata alias and ignored-read matrices, foreign allocation coverage,
normalized modes, concurrency, shared providers, other platforms andP00-P11
closure remain open. No complete routine or program verification is asserted.
Next update the release guide/coverage prose to this precise bounded result,
then continue packed TPTRI/TPTRS8 from the preserved source/count draft.

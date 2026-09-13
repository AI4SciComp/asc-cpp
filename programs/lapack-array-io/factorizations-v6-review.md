# P05/P06 v6 integration checkpoint

This is an incremental, unreleased command-level checkpoint, not completion of
P05/P06 or the full program. It adds twenty actual POCON/PORFS/POSVX/POEQU/
POEQUB scalar routes and twelve GELS/GELST/GETSLS routes. The existing reference
QR and new least-squares INTEGER checks also gain source-conditioned reflector
row-cursor bounds. No native routine coverage is inferred from these wrappers.

## Exact inputs

The tested source tree is `98f341bc6f4c2f655cae9a503ecbda5b266f3e32`;
external `p05-p06-factorizations-v6-02/source.tar` has SHA256
`2b5d8ee5018998b86d5498ac949cf70bae359ca3cb723e9392a0b1d80021c06c`.
Its mapping SHA256 is
`a9c265020bc5c6d6b7bd07901a90b7719634dba6bc5ba3f5dce3459bafe9ceca`:
124 reference rows in progress, 1989 reference rows not started, 20 native
rows implemented-unverified, 2113 required and zero fully verified rows.

Pinned LAPACK commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` and the
unchanged inventory SHA256
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`
remain the source denominator, not the installed LAPACKE declaration list.
Actual provider build identities remain LP64
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
and true global ILP64
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
These GNU11/Linux static providers exclude XBLAS; they are not a full-profile
or portable-Fortran approval.

## Actual verification

All five clean configure/build lanes passed. Full GNU11 CTests passed with
zero failures and zero skips:

| Configuration | Passed / executed | Raw record SHA256 |
| --- | --- | --- |
| LP64 Release | 365 / 365 | `bc6e4d14795eaaf7a8e159a8058d7ee393453fb9e79d9388b5368862736b3a0b` |
| True ILP64 Release | 365 / 365 | `1bacbcf022c4e53a2c880e395e49b115c21b05ad544c41cccde0d1314624cbbf` |
| Provider-free Debug | 277 / 277 | `4e9b0be4466ed611346792c2c12754b0aac3234de1dbfd8fd7bc6d59c4d48943` |
| Provider-free Release | 277 / 277 | `d787f28681641c46b2fd43ae2357f892e4058206fa3bc30142419dcd81784368` |
| Provider-free shared Release | 279 / 279 | `5419bd158a98c928050d3720cabfa8e706142c2ae1881b99f5ab7d6fba022ca1` |

Records are `ctest-<configuration>/record.json` under the external candidate
directory. Sanitized commands, JUnit/log hashes and source-before/after
identities are retained in [the checkpoint index](verification-checkpoints.json).
The increase from v5's 344 provider tests is ten normal/no-exception header
checks and eleven expert/least-squares test registrations. No earlier check
was removed. Baseline totals are unchanged because the five new headers belong
only to the explicit optional Dense facet.

Both relocated installed packages independently built and executed all seven
public-only consumers. The nested `lu-families-test.log` hashes are LP64
`5b7548e7095adf9a25587e59f20f8fd5273005bbd4f7163e6a9cc58f81929be5`
and ILP64
`7c219249acc1c8e6fea26bee547c65ba999300833db950460242694290890506`.
Their external paths are `build-<abi>/asc-cpp-test-workspaces/reference
provider package/lu-families-test.log`. New consumers exercise condition,
refinement/equilibration/expert solves, and least-squares/minimum-norm
reconstruction using installed public headers. The package tests also retain
explicit C++-only language and absent/unrequested-provider isolation checks.

Strict Clang18 analysis passed all nineteen affected translation units on each
ABI, including the corrected private QR/least-squares count headers. Doxygen
passed 84/84 public headers and 1654 members with zero warnings; Markdown
checking passed. Clang19 ASan/UBSan passed 19/19 affected tests per ABI with
all Core/Dense/reference C++ translation units instrumented. Fortran/BLAS
archives and dynamic runtimes were not instrumented. Exact auxiliary command
and configuration fields describe Clang; common metadata's GNU compiler field
describes the full integration lanes, not the auxiliary sanitizer compiler.

## Preserved failures and scope limits

Candidate01 omitted the five new public headers from its staged archive.
Both provider configure commands failed without building or testing. Each
provider-free lane executed its full suite and failed eight exact inventory/
installation checks: Debug/Release 269/277, shared 271/279, zero skips.
Candidate02 changes only those five missing header files outside program
annotations. No numerical implementation or assertion was changed to repair
the archive. Candidate01's 79-header Doxygen pass does not cover omitted files.

The first external sanitizer harness omitted QR-placement fault support and
failed to link on both ABIs. Harness02 adds the required test TU and linker
wrappers, with no product or test predicate change. It reuses already built
instrumented objects; it is not represented as a second clean compilation.
Both original failures and frozen harness hashes remain indexed externally.

Root reviewed the imported expert and least-squares source/tests, added two
independent public consumers, and audited the reflector cursor correction.
See [expert review](cholesky-expert-review.md),
[least-squares review](least-squares-review.md), and
[cursor review](reflector-cursor-review.md) for exact owned-file identities,
source closure, workspace and partial-output semantics. Command-level passes
do not replace exact normalized mode evidence or owner/import/license review.

Band, indefinite and rank-revealing work is separate and not included in this
source or mapping. Advanced and specialized P04–P09 families, P11 closure,
XBLAS/other required dependencies, platform gates, and the retained tiny-input
and GELSY mathematical failures remain required. No denominator is reduced.

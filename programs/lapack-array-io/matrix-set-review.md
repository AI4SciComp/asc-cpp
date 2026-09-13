# LASET checked matrix initialization

This slice adds eight public query/execute declarations for S/D/C/Z LASET in
`lapack_matrix_set.h`. It reuses the existing public `LapackMatrixPart` enum
through its owning copy header. The four upstream routines remain four entries
in the unchanged 2113-row Reference catalogue. Registration is callable and
unverified; limited execution evidence does not imply universal acceptance.

The pinned 3.12.1 provider commit remains
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. The four source/prototype identities
and prerequisite review are in `master-continuation-20260910-01/laset-prerequisite-01`.
Only first-party checked admission, layout publication, tests and examples are
implemented here. No upstream algorithm or source is copied or modified.

## API, workspace and report contract

All/upper/lower selected trapezoids support either full-matrix layout, padded
leading dimensions, square/rectangular/scalar shapes and zero dimensions.
Selected diagonal cells receive beta; selected off-diagonal cells receive alpha.
Complex diagonal imaginary components are assigned unchanged. There is no
conjugation, scaling, finiteness filter or replacement of exceptional values.

Alpha and beta are by-value scalar inputs. The metadata-only query intentionally
omits them; an immutable plan may be reused with changed values and independent
buffers of matching shape/layout/stride, scalar kind, selection and provider.
Empty dimensions complete locally after validation. Active column-major calls
write caller storage directly with no workspace. Active row-major calls require
M*N live scalar objects in `kLayoutConversion`; selected cells alone publish
from that column-major staging array. It is ASC layout storage, not a native
LWORK count. The review explicitly selected this existing role to avoid imposing
the unrelated LP64 scalar-work-array count restriction on layout storage.

The shared full-matrix count helper bounds native M+1/N+1 loop terminals,
leading dimensions and M*N overflow. Valid descriptors bound complete reachable
storage; workspace validation retains byte/alignment/access/alias checks.
Query, stale-plan and undersized-workspace rejection inspect metadata only and
preserve numeric output. Old output and scratch values are not read before the
foreign entry. Unselected cells and padding are never packed or published.

LASET has no native INFO parameter or numerical failure channel. Reports reset
before validation, record actual native entry, and always leave native INFO
absent. Ordinary native return completes publication; it is not a finiteness
certificate. The operation allocates, transfers and synchronizes nothing.
Independent buffers/workspace/reports/contexts and concurrent immutable-plan
reuse are supported; no shared mutable workspace promise is introduced.

## Finite required tests and observation

The maintained numerical test uses the independent assignment equation, with
exact object-byte comparisons for signed zeros, subnormals, maximum finite
values, infinities, quiet NaNs and nonreal complex values. Each scalar covers
all three selections, both padded layouts, M=0/1/2/5, N=0/1/3/4 and 64 independent
alpha/beta class pairs on the same queried plan. It checks complement, padding,
allocation, exact one-byte workspace deficiency, stale selection/plan and a
representable invalid enum. Pure integer boundary checks use no fabricated
array pointers. Four real-provider threads reuse two immutable plans with
independent caller storage, context, workspace and reports; allocation wrapper
state is not used concurrently. TSan instrumentation is ASC/test-only.

The Linux observer reuses the established valid-lifetime mmap/mprotect and
actual GNU foreign-entry wrapping mechanism. It requires 96 deliberate
pre-entry output-read negative controls, 24 ordinary native output-writing
calls, 72 local empty completions and 16 independently calibrated protected
unselected-output cases across four scalars. Real initialized containing arrays
are protected with PROT_NONE and restored at validated foreign entry. Children
use the default fault action, with no signal-handler resumption. The protected
region covers caller numeric storage; scalar alpha/beta copies and descriptor
metadata are legitimate inputs. There is no input array to manufacture a
supplied-factor control. This observes ASC preparation and selected publication;
it does not claim to instrument the provider or runtime. Per-profile compiler
and linker settings are recorded with actual command logs and selected IDs.

The public example uses only installed headers and the exported ASC target. It
checks all four scalars, three selections, both padded layouts, changed-value
plan reuse, diagnostic absence and guard cells. The maintained package gate
copies this example, disables source/private dependency discovery, installs and
relocates ASC, and inspects the exact shared provider/runtime closure separately
from provider-free components. Provider/runtime files are never bundled.

## Execution and preservation

`laset-candidate-{lp64,ilp64}-01` passes 6/6 initial isolated tests with the actual
prepared ABI providers, zero skips. The calibrated observer and public example
pass. `laset-executed-source-01` preserves that exact initial source; later
review adds explicit concurrent immutable-plan reuse. `laset-pre-style-fix-01`
and `laset-style-01` preserve mechanical include/nested-conditional findings.
All corrected product/test/observer/example checks pass with the repository's
full configuration in `laset-style-02`; the exact four native signature checks
pass in `laset-style-01/matrix_set_entry.cc`.

`laset-root-import-01/manifest.json` binds the imported first-party files and
preserves the original integration consumers. The external isolated driver is
excluded. `laset-registration-02` corrects the initial uncommitted mapping's
operation name from Lascl to Laset; both attempts and their exact inputs remain
retained. Four callable-unverified rows bring the Reference registration count
to 386: 44 callable-unverified and 342 partial, with 1727 not started and zero
verified, out of 2113 required routines. There are 72 reviewed routine contracts.
Native20 and the separately named first-party robust PPSVX records are unchanged.

Static Debug, Release and ASC-only ASan/UBSan pass 6/6 in each actual ABI;
static TSan passes 4/4 per ABI. `laset-local-profile-audit-01/audit.json` records
all 44 selected processes, IDs, exits and input comparisons. Shared Debug and
ASan/UBSan pass 6/6 per ABI and shared TSan passes 4/4 per ABI, with no skips or
sanitizer diagnostics. The latter 32 processes are bound to exact commands and
JUnit identities in `laset-final-local-audit-01/audit.json`. The provider and
Fortran/runtime libraries are not instrumented.

The one frozen product is tree `fbed0a859dc07044c0b184d8c634e3d863df7056`,
archive SHA256 `e04047a639365b29cfdcaa9c7468f536e91109329f53a31cbf73d31a87e57342`.
Each fresh static/shared LP64/true-ILP64 producer initially passed 12/13 selected
checks. The strict public-file policy failed because its independent header and
compiled-source inventories omitted the new files. The reviewed two-entry
companion `laset-public-policy-amendment-02` passes that exact check against all
four immutable frozen sources; its first header-only attempt remains retained.
Both strict equality assertions remain. Of 1146 frozen build/API/test/example/
ABI inputs, 1145 match the final root exactly; this single test-policy file is
the only difference. This is a composed result, not a claim that the original
13-test invocations passed.

All four complete frozen-source installed/relocated package tests pass 1/1,
including the maintained public consumer and 28 isolation controls. Static
LP64/ILP64 times are 288.010/285.390 seconds; shared times are 286.638/285.132.
All four independent installed-header selections pass 11/11 including their
build/copy/install/relocation fixtures. `laset-package-audit-01/audit.json` binds
the installed header, eight exported symbols, public binary and exact shared
runtime closure. No provider/runtime or observation DSO is installed with ASC;
provider-free components retain their isolation.

Strict integrated Doxygen, documentation consistency, links and whitespace
checks pass in `laset-documentation-01`. The scoped coverage/backlog checks pass
in `laset-record-checks-01`. Fresh hosted checks must name the actual delivered
revision; local execution does not establish hosted or wider-platform admission.
Existing Reference PPSVX/PT/SGEDMDQ/GBRFS and other numerical failures remain
separate required failures, with no suppression or numerical acceptance credit.

Delivered revision `d346a16bee2f7cb50b4ababd7f512df3daae2eff` has fresh hosted
CI success (34577976090) and CodeQL success (34577976086). Family run
34577976123 executes 155 tests in each of four static/shared actual-ABI profiles:
146 pass, all six LASET tests pass, exactly the retained PT4/SGEDMDQ1/GBRFS4
mathematical processes fail, and zero tests skip. The selector also includes
the two new header checks; test counts come from actual JUnit, not the number
of family declarations. `laset-hosted-audit-01/audit.json` binds all four raw
artifacts. Two CLI artifact-read failures remain retained; the recorded direct
API download succeeds. The PR merge commit
`ae269a9d9e8a7329d6c76947b6931df7d7efa250` has tree
`d6b4edc40b9434cdba619af1985854fa76835f93`, identical to the delivered feature
head, as checked in `laset-hosted-final-state-01`. PR47 remains a draft.

# Positive-definite band expert driver: in-progress review

This isolated P05 slice implements the four actual `S/D/C/ZPBSVX` routines,
with distinct checked `FACT=N`, `E`, and `F` entry points. It does not close
P05 or the full LAPACK program. The PBSV/PBEQU, PBCON, PBRFS, and amended
PBTRF/PBTF2/PBTRS checkpoints remain byte-frozen and separately identified in
their reviews. Integration, installed consumers, and release approval are
not established by this diagnostic work.

## Source and contract

The worktree starts at commit
`9ffb62c183b22a1232790a01c0940a5d731630c2`, tree
`1939ba7b255b40d1d67d57b2360b540900345400`. Exact upstream source is Reference
LAPACK 3.12.1 commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`. The source-derived twenty-routine
inventory is external artifact
`p05-band-expert-agxp4Sxi/exact-twenty-inventory-01.json`, SHA-256
`21e433f273a7394405a75ecffb3053a706f38f5aa550ade49db2cceb8c9789de`.

The optional provider header exposes immutable A/B compute, mutable A/B
equilibration, and immutable already-scaled A/raw AF with mutable B factor
reuse as separate legal modes. The existing band descriptor is unchanged.
Packing remains caller-owned O(n*(kd+1)), never dense n*n storage. All four
A/AF/B/X layouts are independent. Complex original A diagonals are read
real-component-only; raw FACT=F factors are not normalized or certified.

`LapackCholeskyEquilibration` is reused through the approved, unchanged
dependency-only `lapack_cholesky_driver.h` copy, SHA-256
`ebaf7fb435a6687a04a6e5cba9486c4d44a48e0408102f9df5a7c1707bb16bfa`.
This introduces no new linked full-storage driver dependency.

Complex N/E uses explicit original-band packing even for column-major A.
Output-only AF/X packing reads no previous values. PBSVX copies every selected
A entry into AF before factorization. Consequently, after legal partial
factorization return, every selected AF component is defined and published;
all future complex AF diagonal imaginary components are already zero from
that copy. The in-place PBTRF normalized-prefix rule is deliberately not used
for this output-only AF. FACT=F retains all raw factor components. Padding
is not published. Actual n=0 provider calls, INFO, EQUED, scaling, and warning
outputs are retained, rather than adopting the older POSVX local empty rule.

## Current diagnostic evidence and unmet mathematics

Artifacts below are relative to the external program evidence root. The
live diagnostic harness links exact pinned LP64/true-ILP64 libraries and
older frozen ASC archives from `p04-lu-v2-02`; it is not final integrated or
installed evidence. The first ordinary/control suite passed 9/9 per ABI
(`test-expert-{lp64,ilp64}-diagnostic-02.log` in the slice directory).
The expanded required suite executes 13 tests per ABI: 9 pass, four required
mathematical tests fail, none skip, CTest exits 8
(`test-expert-{lp64,ilp64}-diagnostic-03.log`). Those tests are not marked
expected-failure and are not weakened. Float/double have 352 failing
assertions each; complex float/double have 384 each.

For n=1, kd=0, A=B=t*t and AF=t, t=2^-70 in float and 2^-520 in double,
the independent scalar condition is one and exact solution is one. Direct
typed pinned calls in `expert-tiny-probe-01.cc` and both
`expert-tiny-probe-run-{lp64,ilp64}-01.log` show:

- FACT=N/F: INFO=n+1, RCOND=0, X=1, FERR=infinity and source-guarded BERR
  approximately 0.999939 / 0.999996. The PBCON inverse estimator and PBRFS
  unweighted inverse-before-weight multiplication limitations propagate.
- FACT=E: finite S=1/t, but the source LAQSB/LAQHB S*S*A multiplication
  overflows before multiplying tiny A. A=AF=infinity, X=0, RCOND=0,
  FERR/BERR=NaN and INFO=n+1. Exact equilibrated A is one. Finite scaling and
  solution mathematics are an additional unmet gate, not an approved
  reassociation or implicit fallback.

For complex lower A=t*t*[[1,1],[1,2]], the independently derived reciprocal
one-norm condition is 1/9. The already-recorded PBCON lower scaled route
produces 1/4 and propagates through FACT=N/F; real and upper controls remain
in the same required test. INFO=0 fidelity is not mathematical completion.

## Next required work

Continue from the real sources in this worktree: add independent direct
PBSVX comparison tests, early/blocked partial AF publication tests, fault
endpoints, every operand/workspace placement and alias preflight, exact
allocation and source-conditioned integer/runtime closure. Then freeze the
new files, run both ABIs Debug/Release/scoped sanitizers, strict Clang 18,
standalone headers and full strict Doxygen, preserving the failed numerical
gates. Supply exact identities and registration requirements to the parent
integrator; do not edit shared registries or frozen dependencies.

## Source-specific admission and runtime audit

All four actual source argument documents and bodies were read. Private
calls use the pinned `lapack.h` typed base prototypes, with three trailing
character lengths of one: FACT, UPLO, EQUED. Integer values are actual
provider-width `lapack_int`, including real IWORK; complex auxiliary storage
is underlying-real RWORK, not integer storage. There are no LWORK queries.
Copied formula plans bind all seven foreign dimensions and seven option
slots: mode/triangle/four layout bits, four original ASC strides, and scale
count/stride. Row or mandatory complex-original packing uses foreign band
LD=kd+1; independent row RHS/X uses max(1,n), while original ASC strides stay
in options. The mode is not inferred from mutable output EQUED.

Nonempty PBSVX estimates condition even when NRHS=0, so work remains real
3*n plus INTEGER n or complex 2*n plus real n. All scalar LACN2 source routes
need the checked INTEGER 3*n guard. PBSVX N/E inherits the blocked PBTRF
source bounds, and every mode inherits PBCON LATBS/TBSV and PBRFS active
2*kd+2/NZ source bounds. NRHS=maximum is rejected even for n=0 because the
source's output-clearing DO loop increments past NRHS. Upper real FACT=E
LAQSB additionally requires kd+n+1 <= INTEGER maximum; complex LAQHB's
off-diagonal I<=n-1 route instead requires kd+n <= maximum (n=1 uses kd+1).
These are source expression guards, not limits on arbitrary ASC allocation
counts. Pure maximum32/64 tests need no forged nonempty memory or enormous
allocations. Empty row descriptors with real backing and stride exceeding
LP64 maximum are tested through actual source execution.

The static audit `static-expert-closure-{lp64,ilp64}-01.json` has 138 full
objects and 137 source-conditioned objects per ABI. Only preflight-proven
unreachable XERBLA paths are excluded; full raw graphs retain their runtime
I/O/termination branches. Actual remaining external leaves are cabs, cabsf,
logf, lroundf, memcmp, memcpy and memset; there are no unexpected symbols.
The exact runtime and branch analyses combine the prior band factor review
(logf/lroundf/string-memory leaves) and band condition review (cabs/cabsf
direct jumps to fully disassembled, call-free hypot/hypotf implementations).
This applies only to the exact pinned libraries, compiler configuration and
runtime, not an alternative vendor or unchecked invalid Fortran call.

Upstream SHA-256 source identities:

| Source | SHA-256 |
| --- | --- |
| spbsvx.f | `3134dbb6a1237a90b50d05a4226e2764ea6f15fc614ec5cc1f1e625ced3188ca` |
| dpbsvx.f | `7aa72c6029cce6db829b3379d54f099ea1cf4ad8f40d91f0bf13b7766da144d8` |
| cpbsvx.f | `a4fdcf146056e30bcf1070407e52e129423129562b1ef563e713576f49335108` |
| zpbsvx.f | `6c18dfcbc61553d168048a8e022b45d3e17d477ec04eaf5b255a048d0a9407dc` |
| slaqsb.f | `8017723cff48e8bf9afba1d30e67f1f68e3cea52824f5183451a30b9879b72ce` |
| dlaqsb.f | `b553f46945b783548b470b3cb94a77d8154de6d31e015a1142fbf490cb0905ed` |
| claqhb.f | `9f12c55e8b95dc5ae5e76895aa24749cf79d42cef094372709e95e73e75b9d65` |
| zlaqhb.f | `e41847039965d57d15d701cecd703f0a310ad5d0def1a759cf7f81ccc2486302` |

The full pinned inventory and dependency identities remain those in the
separate band-expert, condition and refinement reviews. Runtime libm is
SHA-256 `df621c68dbfed7e843434ef2faedb9f4d4b0543ad161e9a55eaf4d4ce2443176`.
No provider patch, numerical reassociation, fallback, owner approval or
license approval is implied by this implementation.

## Candidate history, retained failures

Strict diagnostic snapshot `source-expert-strict-01` found 24 mechanical
findings: private enum size/template names, direct includes, a local optional
access whose presence had only been checked by an earlier helper, and the
intentional malformed enum test. Private naming/size and includes were
corrected; the scale helper now guards its optional locally. The exact
malformed-enum assertion stays active with the repository's already-used
single-line analyzer annotation for deliberately invalid enum construction.
No numerical assertion or checker was disabled. Both-ABI independent headers
and full strict documentation passed for that diagnostic public header
(86/86 headers, 1682 documented public members, zero warnings).

Frozen `expert-candidate-01.sha256`, SHA-256
`0d54229aa9c3475127dc97d70582131bdf457b93aa6f6a91b8066056ec506e31`,
and archive `source-expert-candidate-01.tar`, SHA-256
`6975c996c2f059b48f2c62c2b7faa8e3d6425bb242704a85849aa1a9c6f822d9`,
preserve a test-only compile error: the extended metadata-alias fixture used
nonexistent `MemoryView` instead of existing `MutableMemoryView`. Both Debug
and Release builds stopped with exit 2 before CTest. Both sanitizer builds
were explicitly cancelled with exit 143 after that known error, not counted
as executed or passing checks. Concurrent compilation was reduced after
observing the environment's 16-core, 15-GiB memory limit. All paused process
groups were resumed for termination; no stopped verification jobs remain.
The replacement `source-expert-candidate-02` differs only in that test type
name; standalone GCC syntax verification passed. Final six-lane evidence
must refer to this replacement (or a later fully identified correction).

## Recovery: final focused validation resumed

Recovered the existing branch and exact base without resetting any work. All
54 new code files matched the four historical manifests, and all prior14
dependencies matched current frozen V9 integration. Historical evidence
ledgers retain190 verified raw/build artifact hashes without mismatch.

Twenty owned foreign-calling test mains now use the integrator's common
NormalReturnGuard; pure integer tests and prior14 code are untouched. The
first recovered frozen diagnostic is external
`p05-band-expert-recovered-01/candidate01`. Its two Release lanes each execute
84 tests:74 pass,10 required mathematical gates fail,zero skip,CTest exit8.
Debug/sanitizer/libc and complete strict checks are still running at this
checkpoint; no future result is counted here.

Review found the eight FACT=F parameter descriptions incorrectly called A
"unscaled", contradicting the file-level contract, pinned source and existing
scaled-factor reuse behavior. These descriptions now require A already
scaled as supplied equilibration specifies. No operation or assertion changed;
the next freeze must include this documentation correction and revalidate
its strict header/documentation evidence.

## Final recovered handoff: candidate03

The dependency-satisfied isolated implementation/review slice is complete.
All twenty S/D/C/Z PBSV/PBEQU/PBCON/PBRFS/PBSVX routes are implemented through
exact pinned typed calls. This is not full P05 or program acceptance: the ten
required mathematical test instances below remain failing, and root integration
and installed checks remain required. No native capability credit is claimed.

The final 54-file payload is external
`p05-band-expert-recovered-01/candidate03/owned-code.tar`, SHA-256
`1a656bfdaf35fe9f8e8938e911fee55290caf6ce96b29a1947000c6071a73e7b`.
Its `owned-code.sha256` manifest has SHA-256
`fb7d906aeaf7115762d36b98451566ef7d5f5c4ebd4ae7ef85bd4cdda935efa7`.
The separate 55-file lock, including the integrator's unchanged common test
helper, is `candidate03/source-lock.json`, SHA-256
`3c113e5a25d616d9f2ae777726ba7f2ce1514ccd7677ddb2ebe9d5369e9472a5`.
The complete final diagnostic source content identity is
`cf536894a59f83160fb161666cc9b46f0115f05326ec7f4bbc40eae4ae37554a`.
The preserved worktree still has HEAD
`9ffb62c183b22a1232790a01c0940a5d731630c2`; no commit, reset, stash or remote
operation was performed.

The immutable executed ledger is
`p05-band-expert-recovered-01/candidate03/verification-ledger.json`, SHA-256
`c40ad74610a799ef9fe7cfb33040b89fcc854d5450cf0917489c70c2ccf81c56`.
It checks 188 executed command records, their source-before/after identities,
raw logs, JUnit, input archives/configurations and generated artifact hashes.
It includes 264 per-TU/build compiler dependency proofs, 66 selected strict
TU/ABI proofs, the source delta proof and explicit retained limitations.
`audit-review-01.md` explains the complete 54-file source/ABI/memory audit.

Final-source evidence is explicitly composed, not mislabeled as one CTest run.
Candidate02 contains the corrected FACT=F public parameter contract and ran
all 84 tests in all eight actual ABI/build lanes. Candidate03 differs only by
removing the unused `<cstdint>` include from `band_expert_edges_test.cc`, a
strict failure preserved independently in candidate01 and candidate02 for
both ABIs. It recompiles all six band adapters and the affected edge target
and executes all four edge scalar tests in every lane. Every other selected
executable/strict dependency has identical bytes in the final source.

| Executed lane | Full candidate02 LP64 / ILP64 | Fresh candidate03 edge LP64 / ILP64 |
| --- | --- | --- |
| GCC 11.4 Release | 84 selected, 74 pass, 10 fail, 0 skip each | 4/4 each, 0 skips |
| GCC 11.4 Debug | 84 selected, 74 pass, 10 fail, 0 skip each | 4/4 each, 0 skips |
| Clang 19 ASan/UBSan | 84 selected, 74 pass, 10 fail, 0 skip each | 4/4 each, 0 skips |
| GCC 11.4 libc allocation probe | 84 selected, 74 pass, 10 fail, 0 skip each | 4/4 each, 0 skips |

Both full libc lanes execute positive controls in all 80 foreign-calling
processes; each delta lane does so in all four processes. Sanitizers report
no additional failures; instrumentation covers the candidate C++ adapters/tests
and recorded V9 C++ dependency archive, not pinned Fortran/BLAS/runtime code.
All 33 owned translation units pass strict Clang-tidy 18 for both actual ABIs
using the byte-verified dependency selection. Clang-format passes all 55 files.
All eleven public/private owned headers compile individually in C++20 with
exceptions disabled for both ABIs. The actual repository documentation helper
and unchanged checker pass 97/97 headers, 1885 members, zero warnings. Earlier
standalone documentation harness failures remain recorded; the successful
replay follows the repository's generated Doxyfile.xml removal step.

The ordinary failing tests are `band_condition_math_gate_{c,z}`,
`band_refinement_math_gate_{s,d,c,z}`, and
`band_expert_math_gate_{s,d,c,z}`. Their respective assertion counts per lane
are 2/2, 64/64/64/64, and 352/352/384/384. All full candidate01/candidate02
outputs match after source-path normalization. Complex lower PBCON gives
0.25 instead of the independently proved 1/9; tiny PBRFS gives infinite FERR;
tiny PBSVX N/F gives zero RCOND/infinite FERR and E overflows S*S*A, yielding
infinite factors and zero/NaN outputs. These remain required mathematical
failures. No skip, exclusion, expected-fail label, changed tolerance, alternate
triangle/algorithm or provider-pin change was introduced. Fidelity remains
separate from mathematical acceptance.

Recovery also established that historical `exact-twenty-inventory-01.json`
was a truncated tool-output artifact, despite its preserved hash. It is not
valid evidence of a parsed twenty-row extraction. New external
`exact-twenty-inventory-02.json` is valid, extracted from the unchanged full
3551-routine inventory and checked against all twenty actual pinned Git blobs.
The original malformed artifact and all 190 historical raw/build artifact
hashes remain preserved. Eight regenerated static graphs match the historical
archive objects, edges and runtime leaves after resolving relative paths.
The full graphs retain XERBLA I/O/STOP edges; only source-preflight-proven
argument-error paths are absent from the conditioned graphs.

The final delta proof preserves all 471 assertion sites and 199 loop sites in
the guarded mains. Production declarations/bodies are unchanged; only eight
FACT=F parameter comments changed. All 33 other owned files, nineteen unowned
preserved paths, fourteen prior band dependencies and the original tracked
diff are unchanged. The shared root/provider registries and public consumer
were not modified by this owner.

Exact next verification command, from the preserved owner worktree:

```sh
sha256sum --check /home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p05-band-expert-recovered-01/candidate03/owned-code.sha256
```

Root's next task is to integrate exactly these 54 files and the existing common
helper, add the five optional public provider headers and five adapters, and
register all 84 focused tests as ordinary tests. The earlier 14 band files are
already dependencies and must not be reimported. The external full CMakeLists
shows the four fault-support sources, exact symbol wrappers and allocation
probes. The first-eight fidelity/validation test names now use their source
stem `band_driver_equilibration_*` to avoid colliding with PBSVX test names;
no fixture was dropped. Root must execute current integration and installed
checks, update shared manifests/coverage atomically, retain the mathematical
blockers and continue the next unaffected required family. This direct-link
handoff does not replace those remaining gates.


## Root V12 final-source import

Root imports exactly54 reviewed recovered candidate03 files with the eight-file
INFO correction applied, giving57 final C++ files, and the independent public
consumer05 with the existing common normal-return guard. Original worktree,
all failed controls and original/corrected reviews remain preserved. The
independent54-file review is `band-expert-recovered-review.md`; the subsequent
INFO correction is `band-expert-info-review.md`.

Root independently audited3119 original artifact hashes,63 corrective command
records,640 compiler dependency records and37 external hashes. Audit
`p05-band-root-final-review-01/audit.json` has SHA256
`638deac4eae32028abe1e22459bfd89f4e2965064a5216c8e1cd52113e5e0de0`.
All20 new fault-wrapper types match pinned C declarations on both actual ABIs
in separate root compiler checks. Root read the final five initializer deltas
and all three additive files. The original54-file review remains independently
owned, not falsely described as a second complete root source review.

Consumer05 rebuilds five corrected adapters against its recorded V9 archives
and passes1/1 per ABI plus strict. Each run asserts256 workflows,2048 successful
calls and256 stale-plan rejections, all four scalars/both triangles/sixteen
independent layouts/explicit equilibration reuse. This direct diagnostic is
not an installed or current whole-tree pass.

V12 registers20 actual rows, five new headers,88 ordinary tests,ten header
probes and an installed consumer. Counts are250 partial reference rows,
1863 not_started,2113 required,20 separately unverified native operations and
zero fully verified rows. All ten mathematical failures remain ordinary
failures; current combined evidence is pending. Initial mapping preparation
referred to a nonexistent private band-layout filename and stopped before
writing mapping/docs/state; corrected preparation uses the actual private
header set and preserves all source and test artifacts.


V12 candidate01 frozen tree `b7cdafb950310ea7f5fa320ba911c55c1438e933`, archive SHA256 `e2af76dff56bb08f4f5d349b6cbddcd972a697575a90867a877ad5ed62f85afb`, mapping `34e8f9537afcc6614620c0597e430798b23a7bc6ab2dddb1b585a1366a624cc4`. Full five lanes and documentation started; owned58C++ files include36translation units. External sanitizer rebuilds all ASC CPUCore/Dense/provider C++, selects88Band+publicconsumer+STOPcontrol, retains ten ordinary mathfailures; foreign archives/runtimes unsanitized. No completed currentV12runtime claim yet.


## V12 implementation checkpoint

V12 implementation checkpoint: exact frozen product matches live root; format58Cpp/coverage/publicsurface/Markdown and Doxygen102headers1969members0warnings pass. Full5lanes/ASan90perABI/strict36TUsperABI still running. Original ten Band and four GT ordinary required math failures retained. Root GETRF/GETRS independent six4-test replay confirms old4/4fail bothABI, fixedRelease/ASan4/4pass each; separate correction not imported yet.

External `p05-band-expert-integration-v12-01/implementation-checkpoint-01/audit.json` SHA256 `ead2db495ac6f3c6aab84d3dc97f43cf62b6c8fa1abdef2829db45fc89be07a4` records current completed command hashes. No full-suite success is inferred from a pending process.

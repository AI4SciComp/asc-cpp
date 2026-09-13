# Checked selected matrix copying

This P09 leaf integrates SLACPY, DLACPY, CLACPY and ZLACPY through eight public
query/execute declarations and the explicit `LapackMatrixPart` enum. These are
four callable-unverified Reference rows, with12 modes each (three parts and
four independent layout pairs). Linux x86-64 GNU11.4 static, actual LP64 and
true global ILP64 remain the supported provider profiles. Normalized Reference
execution evidence, wider platform admission and owner acceptance remain
incomplete. The unchanged provider, native20/I/O subset and experimental
first-party robust PPSVX retain their separate identities and acceptance.

## Source and contract review

The four pinned source definitions use assignment in all/upper/lower nested
loops. They do not have INFO, a workspace query, a numerical status channel,
scaling or conjugation. The actual installed declarations are checked by four
compile-time signature comparisons. Eight extracted archive members and their
disassembly are preserved in `lacpy-native-address-review-01`; selected loop
integer widths and64-bit address arithmetic match the prepared providers.
Nonempty M/N are strictly below the selected signed integer maximum, protecting
foreign loop terminals. Leading dimensions and aggregate workspace bytes are
checked. Pure count tests do not fabricate large arrays.

Queries inspect only metadata. Plans bind scalar, part, shape, both layouts,
strides and provider identity. Output uses explicit live scalar scratch; row
input uses a separate explicit packing region. Column input is direct. Packing
reads only the selected part, and only selected output cells publish. Input,
unselected output and padding remain unchanged. Reachable operand spans,
active workspace and report metadata must be disjoint. Empty dimensions make
no numeric access and no native call. Stale plans and exact undersized
workspace fail before numeric access or mutation.

The operation copies finite values, subnormals, signed zeros, infinities and
quiet NaNs without filtering or replacing them. It promises no NaN payload
normalization. Native INFO is absent on every path; successful completion
means ordinary native return, not a fabricated numerical diagnostic. Scratch
initialization does not inspect old caller output. It is not an omitted-write
detector: the native routine offers no such failure channel. The adapter makes
O(M*N) selection passes and reserves M*N output scalars, plus M*N input scalars
for row layout. It allocates, transfers and synchronizes nothing implicitly.

This is an implementation self-review, not independent owner approval. No
upstream algorithm is copied or patched and no dependency or floating-point
environment setting changes.

## Maintained tests and observation

Each scalar test covers all three parts and four layout pairs over four row
sizes and four column sizes, including empty, scalar and rectangular cases,
with repeated plan use. Bitwise comparisons cover selected copies, immutable
input, output complement and padding. Validation includes stale/changed-part
plans, invalid enums, exact one-byte-short active regions, aliases and both
integer count limits. Four real-provider threads have independent contexts,
operands, plans, workspace and reports. Serial calls audit allocation; the
existing global audit counters are excluded from concurrent calls.

The Linux observer uses initialized, aligned containing arrays and PROT_NONE.
An interception at the actual foreign entry restores caller-output access
before the real uninstrumented Fortran call. Each run requires192 intentional
forbidden output-read controls,24 legitimate row-input packing controls,
48 active calls and144 local empty contexts. Sixteen further controls protect
unselected numeric input cells in valid two-page containing arrays; selected
copying succeeds while those pages remain inaccessible through native return.
Metadata queries and stale/invalid workspace paths run with numeric pages
protected. Output starts at-5 and selected ordinary output must become2.
Final bytes alone are not used as proof of no reads. The observer covers the
protected regions and entry boundary, not instrumentation of provider code.

The first sanitizer observers timed out at the unchanged120-second limit.
A bounded20-second syscall trace (diagnostic exit124) confirmed that WSL's
piped crash collector delayed each deliberate SIGSEGV, despite RLIMIT_CORE=0.
The localized harness correction disables dumping only in calibration children
with PR_SET_DUMPABLE and checks that setup succeeds. Expected SIGSEGV and all
232 controls remain mandatory; no handler resumes an invalid instruction.
The parent/provider execution and timeout are unchanged. Original source,
trace and failures remain in `lacpy-sanitizer-timeout-diagnosis-01` and
`lacpy-observer-core-repair-01`.

## Completed execution and delivery

Raw records are under `master-continuation-20260910-01/`. Configured tests are
248–251 scalar,252 observation and253 maintained public example; IDs refer to
retained listings, not permanent catalogue IDs. `lacpy-debug-lp64-04`,
`lacpy-debug-ilp64-01` and both `lacpy-release-{abi}-01` pass6/6. Both
`lacpy-sanitizer-{abi}-01` retain5/6 with the observer timeout. All six
`lacpy-{debug,release,sanitizer}-{abi}-observer-final-03` repeats pass after the
child-only correction and direct platform-header include. No numerical source,
ordinary test, case or tolerance changed during that correction.

Both `lacpy-tsan-{abi}-01` pass4/4 real concurrent scalar tests, using the
existing setarch workaround. ASC/tests are instrumented; provider Fortran is
not. Normal/no-exception header probes792/793 pass in
`lacpy-debug-{abi}-headers-01`. `lacpy-documentation-01` passes strict Doxygen
with132 public headers,2286 members and zero warnings. Final strict product,
ordinary and consumer checks pass in `lacpy-style-02`; wrapper checks pass in
style01 and the final observer in style05. Earlier include/format findings
and their repairs remain preserved.

Both `install-lacpy-{abi}-01` pass all11 install/relocate/build/execute/export/
runtime and provider-free consumer-isolation commands. The maintained
`examples/matrix_copy` uses only public headers and `ASC::dense_lapack`, all
four scalars, all parts/layouts and plan reuse. The full package gate includes
a copied independent consumer; no historical evidence path or private header
is needed by the maintained example. Provider/runtime files are not bundled.

The early Debug LP64 attempt02 retains a missing-executable skip when the new
public target was registered during an incomplete target build; the runner
rejected it. Attempts03/04 built every selected target and pass. Attempt01's
earlier source snapshot is not substituted for final executed input identity.
Existing mathematical failures in PT, PPSVX and other families remain failures.
No reference row is promoted to verified by this integration.

Both `lacpy-integration-{abi}-01` runs pass all eight affected architecture,
header, documentation and full-package gates, using original configured test
commands and timeouts with fresh guarded scratch roots. The exact tracked-source
tree `dc86346df297d6ba06f393b3a8ecce3601bc5373` is frozen in
`lacpy-frozen-product-01`. Both `lacpy-fresh-{abi}-01` builds from that archive
pass9/9 ordinary/observer/public/header/package tests, zero skips. Full package
execution takes238.91 seconds LP64 and237.88 seconds ILP64, below the unchanged
300-second limit. `lacpy-executed-input-comparison-01` preserves the exact
ordinary-to-final-observer dependency comparison. Later review/state narrative
changes are separately compared and are not a new tested product tree.

Scoped validation in `lacpy-record-checks-01` passes:2113 required,
32 callable-unverified Reference,338 partial,1743 not started,0 verified,
56 reviewed contracts. Thus370 Reference rows are registered. The native20
mapping extension preserves original execution source/tree and all native
contracts/artifacts/routes; it does not attribute a later whole-tree run.

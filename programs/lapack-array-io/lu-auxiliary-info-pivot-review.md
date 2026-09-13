# Auxiliary LU INFO and GESVX output-pivot correction

The root imported the sealed auxiliary INFO02 change before checking and
applying dependent GESVX output-pivot01. Exactly sixteen native INFO variables
now start at the native signed minimum, which the existing translator rejects
without negating it. GESVX FACT=N/E fills only its n output-pivot entries with
the minimum after preflight; FACT=F input pivots and the real IWORK tail retain
their existing behavior. Missing/partial-width outputs are rejected before
ASC publishes deferred factors, pivots, solutions or growth statistics.
No public header, workspace size, numerical algorithm or provider pin changed.

The root read all four production deltas, both fault mechanisms and shared
INFO checks. The independent reviewer read every changed production file and
all eleven new test/support files. Its handoff
`p04-info-pivot-independent-review-01-i8do9md9/handoff.json`, SHA256
`59019f1515916aae35e6b83f3259f40373a5f875aea85483530ad656b624a656`,
reconciles 5,305 hashes and 830,096 main profiles with no blocking finding.
Original old-source regression failures remain ordinary failures. The fault
layers call real native routines with valid complete native destinations,
then withhold or partially publish outputs. They preserve original INFO and
actual computation, including supplied-factor reuse and singular/warning cases.

The two sealed handoffs and all108 named handoff artifacts were rehashed at
root import. `p04-lu-aux-root-import-01/import.json` records exact old/new
production identities and successful ordered prerequisite/patch commands.
Twenty new test registrations retain all old tests. Twenty partial Reference
mapping rows now bind the exact new production and test bytes; their full
contracts, modes and evidence remain pending. The denominator remains2113
required/270 partial Reference rows/20 separate native/zero fully verified.

Current root verification is pending. The composed external candidate rebuilds
all58 current Core/Dense/provider CPU sources, while final root differs only
in the corrected architecture inventory, two registration comments and mapping
artifacts. Root will run current numerical, preflight, installed-package and
source-policy checks and independent replays before recording a pass. Foreign
archives remain unsanitized. Thirty required numerical failures and all wider
P00–P11/platform/shared-provider/XBLAS requirements remain open.


## Current composed root checkpoint

Root two45/45 fresh Release lanes include38 affected numerical/fault/count controls,4 placement tests, package and2 policy tests. Four exact-input Debug/ASC-only sanitizer root replays38/38 reuse sealed current binaries. Fresh root production58 TUs per ABI; agent six58-TU lanes and24 fresh strict remain separately identified. All17 installed families per ABI pass. No full-project suite, foreign sanitizer, full mode/platform/shared-provider/XBLAS or required mathematical closure claim. Source-bound profile audit matches472272 main modes,321648 main native calls,47232 native warm calls and41472 reuse calls across six root executions. Format15/coverage/publicsurface pass. Initial build-record directory collision was rejected before compilation and preserved; corrected runner reused successful configures. Denominator2113/270 partial Reference/1843 not_started, native20 separate, zero fullyverified;30 required mathematical failures remain open.

Frozen product tree `b5168234598803a41158c7ecf613e73af1ceb0e9`, archive SHA256
`b2ed985dac1bbc3078ba38dfc547b91449e1bc19bcbb99b2d117fbf254f3c879`, mapping
`91defdbf15f2d7df6cbcb630ccc8b1e352ba80b5fe0e2cc0ae76b7bbefbb829c`. Completion audit
`p04-lu-aux-integration-01/completion-audit-01/audit.json`, SHA256
`9f7356bcd29dd8c25fc673624fddcdc1fc3085f496fcadaa5be9bea202bc6dc0`. Root package executes all17 installed
families perABI with the existing300s timeout. All prior failed-source
controls remain in the original INFO02/pivot01 handoffs; no old or unexecuted
configuration is relabeled current.

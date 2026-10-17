# Checked complex vector conjugation

The independent P09 CLACGV/ZLACGV leaf adds two Lacgv overloads in the optional
Dense LAPACK facet. Both exact source bodies and actual GNU11 LP64/global-ILP64
emitted declarations were reviewed against the pinned C headers. No new provider,
algorithm fallback or numerical source import is introduced.

## Complete spacing contract

The caller supplies N, a signed physical increment and a unit-contiguous checked
complex storage view. Its exact length is zero for N=0, otherwise
1+(N-1)*abs(increment). Positive spacing starts at the physical low address;
negative spacing starts at the high selected entry. The scalar types are the
existing audited `std::complex<float>`/`std::complex<double>` ABI specializations.

The pinned source does not exclude zero increment: it conjugates the same
physical entry N times. This mode is explicitly supported, with parity deciding
whether its imaginary sign changes. Ordinary positive/negative spacing and
unit/nonunit loop branches are retained. There is no workspace or query.

Native INTEGER bounds cover N+1, the negative starting cursor, and the final
positive nonunit cursor 1+N*INCX. Empty calls are local and accept an otherwise
immaterial signed increment. N=1 passes native increment one, safely accepting
any public increment because only one physical scalar is selected. This
normalization changes no numerical semantics. A private pure-count helper tests
both integer limits without passing fabricated large arrays to native code.

Storage is live host or admitted pinned-host memory. Reachable storage, including
gaps, must be disjoint from the live provider and report objects. Metadata aliases
reject before report reset; all other preflight failures reset the report and
preserve the entire numeric buffer. The selected entries are modified in place;
gaps and guard elements are never part of the native vector.

There is no INFO or return-status channel. Reports leave native INFO and argument
fields absent, distinguish local empty completion from actual provider entry,
and record normal-return completion. They do not claim to detect arbitrary
omitted or incorrect writes. A bounded typed-wrapper observation records normal
return without mutation as this explicit limitation, not as a detected defect.
Nonfinite components are valid conjugation operands and do not alone cause an
accuracy warning. No allocation, transfer, synchronization, floating-point
control-setting change or hidden cache occurs.

## Source and numerical prerequisites

Both ABIs pass 28,000 native cases, including guarded full-width integer inputs,
complex component representation, five lengths, seven increments and every pair
from a 20-value real-component set. Inputs include signed zero, subnormals,
minimum normal, ordinary signs, large finite endpoints, infinity and NaN.
The oracle toggles the imaginary sign bit while preserving the real component
and every unselected byte. Zero increment applies that operation N times.

The maintained mathematical suite additionally repeats each checked operation
to verify the conjugation involution. Native-fidelity results are separately
reported. Four concurrent workers share an immutable provider and use private
buffers/reports; serial calls use the existing allocation auditor. Validation
covers count/cursor limits, wrong physical shape/view spacing, inaccessible
placement, metadata overlap, empty paths and scalar-increment normalization.
Maintained emitted-signature ABI probes pass all 28,000 cases per ABI.

A Doxygen INPUT continuation/quoting error had omitted the earlier packed
condition/refinement, triangular-conversion and scalar-norm review pages.
Their historical header/member checks did not prove page inclusion. The
configuration now lists each path separately, and the maintained documentation
validator requires every explicitly listed programme input to appear as a page.
The new validator rejects the actual prior XML with all four missing pages while
retaining its original 164-header scope. This documentation repair changes no
numerical behavior or historical execution result.

Final Release/Debug static/shared LP64/global-ILP64 configurations pass all
80 selected processes, zero failures and skips. Four maintained translation
units and one relocated public consumer pass strict analysis. Four relocated
installed consumers each pass 4,480 component/length/spacing cases, including
invalid-count preservation checks, without private or source-tree headers.
Four standalone-header, ten package-manifest and three architecture checks pass.
The actual CI selector contains 1,829 tests and retains every previous entry,
adding ten conjugation processes and two header checks. This is local selection
evidence, not full-suite or hosted execution credit.

Source and four installed surfaces contain 165 public headers. Both ABI symbol
comparisons add two Lacgv overloads and remove no prior symbol. Doxygen checks
165/165 headers, 2,775 documented public members and all 25 explicitly listed
programme pages with zero warnings. Two contracts covering eight spacing modes
are registered as callable-unverified; no full Reference verification is granted. Deliberate-crash observations, sanitizer/TSan runs requiring address
randomization changes, remote/security operations and wider platform admission
remain deferred under the latest user instruction. They remain required gates;
no existing failure or test is removed and no Reference verification is granted.

The preceding LAS2 leaf remains deferred after a documented single-infinity
native failure; see [the blocker record](blockers.md#las2-single-infinity--failed-native-prerequisite-deferred).
All raw evidence is retained under the existing continuation root as
`completion-execution-01/vector-conjugation-*` and `scalar-svd-*` artifacts.

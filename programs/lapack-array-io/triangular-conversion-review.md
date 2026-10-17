# Checked triangular precision conversion

This independent P09 leaf adds DLAT2S and ZLAT2C to the existing precision
conversion header. Four new query/execute declarations cover both triangles
and four independent input/output layout combinations: sixteen mode cases.
The provider, existing LAG2 behavior and numerical contracts remain unchanged.
No provider source or third-party material is imported into ASC.

## Source and ABI boundary

The two exact required source definitions in pinned LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read. They call only the
existing SLAMCH and LSAME dependencies. They need no pivots, factor history,
XBLAS, native workspace or workspace query. Their actual signatures are
UPLO,N,A,LDA,SA,LDSA,INFO followed by the GNU size_t CHARACTER length. Both
routines are missing from the pinned C header. Private declarations are
checked against actual GNU 11.4 external prototype emissions in LP64 and
true global ILP64, removing only input-pointee const qualification. The
source bodies establish that those inputs remain immutable.

Both LAT2 bodies write INFO=1 on range overflow and **do not write INFO on
success**, including N=0. The checked adapter initializes the entire actual
native integer to zero and retains its value after return. Unlike LAG2, a
zero value is not proof of a native success write. Missing or partial-width
writes can be indistinguishable from legitimate values. Tests record that
limitation explicitly; integer ABI compatibility is established independently.
The raw prerequisite starts INFO at the selected integer maximum and verifies
its full-width preservation on successful conversions and full-width 1 on
range failures, with neighboring integer guards.

Initial native attempt01 did not compile because the provider C header lacked
the two declarations; no native test ran. The private emitted declarations
resolve that omission. Attempt02 compiles, links and passes 960 guarded cases
in each actual ABI: both scalar classes, N=0/1/2/5/13, both triangles, sixteen
input shifts, finite/range/NaN cases. It tests numerical conversion and complete
input/output padding guards without deliberate faults or process termination.

## Checked contract

Input and output are square, same-shaped matrices with independent padded
row/column layouts. UPLO includes the diagonal; complex diagonal imaginary
parts are significant, with no Hermitian interpretation. Only the selected
input triangle is read. Only the selected output triangle is published; the
other triangle and padding stay unchanged. The caller destination is never
numerically read. Every output is staged in N*N live destination scalars in
kScratch. Row input uses N*N live source scalars in kLayoutConversion; only
its selected triangle is packed. Column input is passed directly. Staging
is initialized with NaNs so unwritten numeric outputs stay visible, without
claiming to distinguish them from legitimate NaNs.

Queries inspect metadata only. Plans bind the provider, routine, scalar pair,
triangle, order and both layouts/strides. N+1 must fit the actual foreign
integer; foreign input leading dimensions fit the selected ABI. N*N,
reachable spans and simultaneous staging/packing bytes are checked before
conversion. Destination stride is ASC-owned because native output is compact.
Pure arithmetic boundary tests never fabricate large backing arrays.

All operand spans, workspace and live metadata objects must be disjoint and
CPU accessible. Metadata-object aliasing is checked before report reset;
ordinary preflight errors reset the report without numeric or workspace
mutation. Empty calls need no workspace and complete without numeric access,
native invocation or native INFO. Calls allocate, synchronize and transfer
nothing. Concurrent calls use independent mutable state and share the
immutable provider.

Finite narrowing follows the unchanged component casts, including rounding,
signed zero, underflow and information loss. INFO=1 returns a numerical
accuracy warning with all caller output unchanged. Other nonzero INFO values
are provider errors and also preserve output. After INFO=0, selected values
are published. Nonfinite components remain visible with a numerical accuracy
warning and complete publication validity. No values are clipped and no
floating-point environment setting is changed.

## Maintained checks and current evidence

The eleven maintained tests comprise two independent mathematical tests, two
native-fidelity tests, two concurrency tests, two report-fault tests, two
validation/count tests and one native ABI probe. Each ordinary scalar/mode
runs 640 cases over all eight triangle/layout combinations, five orders and
sixteen endpoint/rounding shifts. Range, infinity and NaN checks cover each
complex component separately. The unselected input triangle contains finite
values outside the destination range to expose unintended conversion.

Fault tests run 288 cases per scalar, including empty calls, actual native
baselines, all full-width INFO extremes, unchanged INFO, selected short-width
INFO, omitted output, NaN/Inf output and negative zero. Validation checks stale
plans, shape/options, source/destination layouts and strides, wrong-routine
plans, workspace sizes/alignment/placement, operand/workspace aliases, unsafe
report storage overlap and pure count/byte boundaries. Inputs, output,
padding and private workspace guards are checked. Four concurrent workers
perform repeated calls with private output/workspace/report and serial
independent mathematics, outside the serial allocation auditor.

Initial Release LP64 and global ILP64 runs both pass all eleven tests with
zero skips. The final local matrix adds nine existing LAG2 regression tests
(four ordinary, four fault and the public example) to each run. All eight
actual integer-ABI/static-shared/Release-Debug configurations pass 20/20:
160 selected processes, 160 passes, zero failures and zero skips. Shared
wrapped tests use the existing observation library built from the exact same
production PIC objects; unwrapped ABI and installed consumers retain the
production library. This is not evidence of instrumented provider internals.

All six production/test translation units pass strict analysis. Initial test
findings (explicit includes, optional access, enum width, switch defaults and
nested conditionals) were repaired without removing tests or changing their
expected outcomes. The installed consumer's initial strict findings on index
widening and a missing nodiscard annotation are also repaired. Final installed
attempt02 passes strict analysis and all four actual ABI/linkage configurations,
with 1,280 public-only cases per relocated consumer. Attempt01's passing
consumer and strict findings remain retained. Final native attempt03 binds the
maintained source and passes the two actual emitted-signature 960-case probes.

Four standalone header checks (normal/no-exceptions in both ABIs), ten package
manifest checks, three architecture checks, and five source/installed public
surface checks pass. Public header count stays 163; four new exported ASC
symbols appear in each ABI and none disappear. The actual CI selector lists
1,798 tests and its unchanged full required-set guard plus eleven new LAT2
requirements pass locally. This does not claim any hosted run or execution of
the entire selected suite. Source, provider and output hashes bind every record.


## Execution constraints and remaining admission

Under the latest user instruction, deliberate crash/termination experiments,
security-sensitive remote operations and remote delivery are deferred. The
packed whole-driver containment question remains recorded separately in
[blockers.md](blockers.md#packed-driver-containment-deferred--user-execution-constraint).
Sanitizer/TSan runs that require disabling process address randomization and
calibrated deliberate-fault observation experiments are deferred under the
same constraint. Existing tests remain registered and unchanged; deferred
execution is not a passing result or CTest skip. Local Release/Debug,
static/shared, ABI, validation and installed-consumer work continues.

Raw commands, source identities, failures and results use
`master-continuation-20260910-01/continuation-20260912-01/completion-execution-01/triangular-conversion-*`
under the existing external evidence root. Broader platform admission,
normalized Reference verification, owner/provenance decisions and every
other required programme gate remain separate and open.

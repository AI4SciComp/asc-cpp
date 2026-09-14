# Checked scalar sorting

This independent P09 leaf adds two SLASRT/DLASRT-backed Lasrt overloads and a
small increasing/decreasing order enum in the existing optional Dense LAPACK
facet. Both exact pinned source bodies, dependencies and actual GNU11 LP64/
global-ILP64 emitted signatures were reviewed. No provider, implementation copy,
hidden fallback or workspace subsystem is added.

## Native and checked contracts

The operand is a live host or admitted pinned-host real vector with view
increment one. It is sorted in place; backing padding is not part of the array.
Equal numerical values, including signed zeros, have no stable-order promise.
Infinities use the usual numerical order. NaNs have no specified position or
sorted-order guarantee; unchanged native NaN-containing output is visible with
an accuracy warning. No total ordering of NaNs is invented.

The actual insertion branch is `ENDD-START<=20`, so a segment of length21 still
uses insertion sort, despite the introductory source prose saying size<=20.
Tests cover lengths20/21/22 explicitly. Quicksort uses the midpoint sum
`START+ENDD`, endpoint increment and insertion loop terminals in native INTEGER.
Both sources document that their fixed32-entry stack limits N to about2^32.
The checked size bound is `min(native_integer_max/2, 2^32)`. Processing the
smaller partition first halves active lengths; insertion leaves of size<=21
keep the selected domain within29 simultaneously pending stack entries. A pure
count helper checks both ABI limits without manufacturing huge native arrays.

Storage, provider and report must be disjoint. Metadata aliasing rejects before
report reset; other preflight failures reset it and preserve every numeric
value. Empty operations are local, do not read numeric values and have no native
INFO. Every nonempty call initializes the full actual INFO integer to its maximum
and records the returned value. All nonzero INFO values are provider errors;
negative argument decoding protects the most-negative integer. The native path
is in place, so provider-error output remains mutated and is marked unusable.

INFO=0 output is scanned. A NaN produces numerical accuracy-warning status and
complete publication validity. Without NaNs, a numerical inversion produces a
provider error and unusable output. Ordered output succeeds. This scan cannot
independently prove input-multiset preservation for arbitrary injected provider
writes; an ordered-but-wrong fixture records that explicit limitation. There is
no workspace, query, allocation, transfer, synchronization, clipping or change
to floating-point control settings. Independent calls share an immutable
provider and keep mutable storage/reports separate.

## Prerequisites and maintained validation

Each actual ABI passes 5,120 native cases: 4,052 ordered mathematical cases and
1,068 NaN-containing bit-permutation observations. Both precisions, both orders,
ten lengths0/1/2/5/20/21/22/33/65/257, eight patterns and sixteen seeds are
covered. Patterns include sorted/reversed arrays, duplicate signed zero, repeated
values, signed finite endpoints/infinities, scattered NaNs, pivot-position NaNs
and organ-pipe sequences. The byte-multiset oracle sorts integer bit patterns,
so it never passes a non-strict NaN comparator to a standard sorting algorithm.
Every full-width integer and CHARACTER argument, INFO guard and surrounding
numeric element is checked.

The maintained suite separately reports mathematical behavior, native fidelity,
concurrency, report/output faults and count bounds. Typed wrappers first check
the native three-element baseline and hidden character length before injecting
omitted/full/partial-width INFO, legal/invalid argument reports, reversed output,
ordered-but-wrong values, NaNs and omitted numeric writes. No crash, memory
protection or provider-termination experiment is involved. Serial API execution
is allocation-audited; four concurrent workers have private mutable state.

Final Release/Debug static/shared LP64/global-ILP64 execution passes80/80
selected processes, with no failures or skips. Five maintained translation units
and the final public consumer pass strict analysis. Test-only corrections make
index widening, integer midpoint intent and optional access explicit; the
unknown-enumerator rejection remains tested with a narrow analyzer annotation
for a valid fixed-underlying-type value. No input pattern or numerical check is
removed. Both final emitted ABI probes pass5,120 cases.

Four relocated installed consumers each pass1,280 sorting scenarios and
unknown-enumerator preservation checks. Four standalone-header, ten
package-manifest and three architecture checks pass. Source plus four installed
surfaces contain166 public headers; both ABI symbol comparisons add two Lasrt
overloads and remove no symbol. The actual1,841-test CI selector retains all
previous entries and adds ten sorting processes plus two header checks; its
maintained guard passes. This is local selection evidence, not hosted/full-suite
execution. Doxygen covers166/166 headers,2,778 public members and all26 explicitly
listed programme pages with zero warnings. Two reviewed contracts/four order
modes are registered as callable-unverified.

Required deliberate-fault
memory observation, ASLR-changing instrumentation, remote/security and broader
platform admission remain deferred under the latest user constraint. No existing
test, required failure or provider identity is changed; normalized Reference
verification and full programme acceptance remain open. Raw records use the
`completion-execution-01/scalar-sort-*` paths under the established external
continuation evidence root.

# LAPACK execution evidence identities

The source inventory and coverage mapping remain schema1. Execution evidence
supports schema1 and schema2. This versioned extension changes how a record
selects its expected source identity; it does not change numerical requirements,
mode classes, provider pins, required ABIs, verification counts or owner gates.

## Schema1 compatibility

A schema1 evidence document has one `expected_identity` object. Every record's
`identity` must equal it exactly. Named identity catalogs, record selectors and
per-route selectors are rejected in this schema. Existing native20 records
retain their source commit/tree and accepted execution scope. A newer product
commit never becomes the source of an older execution merely by editing this
expectation.

## Schema2 explicit source expectations

A schema2 evidence document has a nonempty `expected_identities` object mapping
unique nonempty names to full identity objects. It has no `expected_identity`
default. Every execution record supplies an `identity_id` selecting one catalog
entry, and its full `identity` must equal that independently stated expectation.
Missing/unknown selectors and stale identities are rejected, even on records
not yet used for verification. Duplicate JSON keys remain invalid.

Each verified mapping route supplies a nonempty, duplicate-free
`execution_identity_ids` list of allowed catalog names. Every record used to
verify that route must select an identity from this list. Matching some other
catalog entry cannot transfer verification to a different source expectation.
Native and Reference routes retain separate allowed source identities and
separate covered cases. There is no implicit first-entry selection or fallback
to a record's self-declared identity.

Every expected identity retains implementation commit/tree, dirty-diff,
inventory, mapping and provider-lock hashes. The command-line validator binds
all expected inventory/mapping/provider-lock hashes to the actual supplied
files. Those common input hashes are computed once per invocation. Record
configuration, actual provider source/build/ABI, command status, positive
selected/executed/passed counts, absence of skips, artifact hashes, real test
results and every required mode/class remain checked as before.

## Migration and evidence use

A migration must update its generator, evidence document and affected mapping
route selectors together. Preserve the previous document and an exact change
record. Adding a selector does not authorize changing an old implementation
identity, covered-case assignment or execution outcome. Source identities come
from completed command records and retained source inputs. Reuse against a
later integration requires an explicit unchanged-input comparison for the
routine and its changed-input dependencies; different whole trees are not
silently treated as identical. Multiple allowed identities for a route require
review of each such comparison and the precise classes each execution covers.

The existing native20 mapping extensions remain historical evidence of their
own exact changes. A new versioned selector migration must describe any selector
changes honestly; it cannot label modified mapping rows byte-for-byte unchanged.
No new Reference verification is implied merely by migrating records or adding
passing partial evidence. Failed mathematical executions and insufficient
class/platform coverage remain failed or incomplete, with their raw artifacts
retained. Evidence is an auditable execution record, not an authenticity or
owner-approval certificate.

## Passing partial observations

Schema2 validates every record's results, artifacts, source and provider ABI,
including records not used to verify a route. Each assignment names an existing
routine, route, mode and required class. An incomplete implementation may use
`partial_evidence_ids` with explicit `execution_identity_ids`; its verified
`evidence_ids` remains empty. This adds no verification credit. Failed runs
remain in their original execution/disposition records and cannot enter the
passing evidence ledger by dropping their route references.

Newly normalized assignments explicitly state boolean `class_complete`.
`false` records a bounded observed fixture and never closes the required class,
even if somebody later attaches the record to a verified route. The original
native20 assignments retain their previously accepted complete-class meaning.
Neither status nor a profile total implies that all required mathematical
cases, installed use or platforms were exercised.

`tools/lapack/normalize_execution.py` reads a completed command record, the
configured selection, actual CTest JUnit and separately reviewed metadata. It
rejects zero, duplicate, missing and unexpected tests and preserves failures,
skips, exact configured IDs, executable commands and raw-input hashes. Its
successful exit means normalization completed; the resulting execution's exit
and counts determine whether it passed. A skipped entry is not counted as an
executed test. The tool never runs recorded commands or infers fixture coverage
from a test name. Raw input and output directories remain outside the source
tree; only reviewed compact records are maintained here.

`tools/lapack/migrate_execution_identities.py` prepares the schema1 migration
in an exclusive output directory. It preserves contract and route states,
attaches additional records only as partial evidence, and validates the result
before writing it. The [migration bridge](lapack-execution-identity-migration.json)
records exact prior file/record hashes and source expectation, plus every changed
route. Native20 selector changes are explicitly recorded; their original
execution commit/tree, covered cases and results are preserved. A later routine
extension must update all named mapping-index expectations together without
rewriting the underlying execution source identities.

The initial Reference use is the [LU evidence review](https://github.com/AI4SciComp/asc-cpp/blob/2814b1a21edda2f006b08fc1d80578513ba795b3/programs/lapack-array-io/lu-execution-evidence-review.md).

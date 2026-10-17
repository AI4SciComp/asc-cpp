# P00 source inventory review

Self-review performed 2026-09-07. This record freezes a source denominator;
it does not assert any callable ASC routine or numerical test coverage.

The authoritative HTTPS Git remote was Reference-LAPACK/lapack. The external
checkout has source commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`,
tree `7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, and annotated `v3.12.1`
tag object `5ebe92156143a341ab7b14bf76560d30093cfc54`. The tag peels to
the expected source commit. Its contents have no signature; HTTPS retrieval
and recorded object identities do not establish signed-release authentication.
The generator checks pristine Git state and independently hashes each input
as a Git blob against the pinned tree, including when index flags could hide
a modified file from ordinary status output. No upstream code is executed.

The generated denominator is 2,113 required external procedures: 336 drivers,
1,237 computational routines, 496 expert auxiliaries, and 44 deprecated
compatibility entries. The full source catalogue contains 3,551 qualified
procedure identities, 3,888 declaration occurrences, and 3,582 Fortran files.
Counts come from this exact source, not filename stems or LAPACKE's API list.

All documented external SRC auxiliary routines remain required conservatively.
An auxiliary category is not an exclusion. Deprecated entries and XBLAS
source-list membership remain visible and required. DMD's actual ISO Fortran
environment kinds are parsed from declarations. Mixed types, real outputs
for complex routines, and module-contained procedures remain distinct.
External alternate implementations share their numerical identity while
retaining each source path, signature, argument contracts and conditions.
INSTALL's externally linked machine/workspace query entries are expert
requirements. BLAS implementations and CBLAS wrappers are provider-internal
dependencies. Module-contained helpers cannot be mistaken for external ABI
entries. TESTING including MATGEN is excluded as upstream test support;
INSTALL's remaining platform/timing probes are build support. These reasons
are repeated on the corresponding machine-readable records.

Every required procedure has a source build-list reference. Raw Make/CMake
tokens and their originating lists/conditions are retained; suffix rules,
generated object names and dynamic build expressions are explicitly marked
when they do not resolve to a literal tracked source path. The generator does
not pretend to interpret a full build language. Documentation's unavailable
placeholders remain documentation-group facts rather than callable entries.
LAPACKE precision and `_work`/`_64` wrappers cross-check a single mathematical
identity. Its two source-unmatched declarations, `get_nancheck` and
`set_nancheck`, are C runtime controls in LAPACKE, not numerical operations.
Exported-library symbol cross-checking remains a P04/P09 gate.

Build-condition limitation: the generated per-row conditions include literal
source-list conditions and explicit XBLAS membership. Precision-selection
variables such as SLASRC and their conditional BUILD_SINGLE consumers are
retained in the pinned build-input files and source-list evidence, but their
transitive variable-use graph is not expanded by the generator. Consequently,
`optional_build_conditions` must not be treated as an exhaustive evaluated
CMake enablement formula. This does not change the source denominator; the
provider build/symbol gate must verify each selected configuration separately.

No routine has an unresolved source classification or required argument type.
This does not close ASC mode/alias/workspace contracts: the generator records
source argument directions and documented character-literal candidates;
binding owners must review actual legal options and their interactions.
The mapping validator must not treat those candidate literals as an exhaustive
checked mode specification.

Root LICENSE and LAPACKE/LICENSE were read directly. The root BSD-style text
contains an additional intellectual-property non-assurance clause, so the lock
uses a precise local license reference and records that observation. Provider
redistribution and its notice inventory still require actual owner review.
No upstream source implementation, test fixture, or license text was copied
into ASC production code by this inventory work. No MdeCpp material was used.
The source's CMake patch-version value of zero is recorded without changing
the authoritative v3.12.1 source identity.

Verification used Python's standard-library unittest runner: 17 tests executed,
17 passed, zero failed/skipped. Independent fixtures cover fixed/free form,
continuations, multiple/contained procedures, CPP alternatives, mixed and DMD
precision kinds, callback typing, quote parsing, build conditions, unavailable
documentation groups, conservative expert inclusion, unknown classifications,
dirty/wrong source rejection, direct blob integrity, and help/failure exits.
Generation followed by offline `--check` reproduced both files exactly.
Raw evidence is in the local external evidence index under
`inventory-tests.log`, `inventory-generate-final.log`, and
`inventory-check.log`.

The live [Google Python Style Guide](https://google.github.io/styleguide/pyguide.html)
was retrieved on 2026-09-07. Tooling uses safe JSON, checked subprocess lists,
explicit UTF-8, documented functions, no new development dependency, and no
runtime Python dependency for C++ consumers.

Frozen identities:

| Artifact | SHA-256 |
| --- | --- |
| Inventory | `1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7` |
| Source/provider lock | `e40cc81715470c6059bb6f742421585c308121a6f3157b425207f09b18585bb3` |
| Inventory generator | `2b8db8719bfffba3a15fe5bc24aa80647eff32331f3ea2202a8d366d10f6d02d` |

Regenerate using the generator command embedded in the inventory; add
`--lock-output docs/contracts/lapack-provider-lock.json` to reproduce the lock
and `--check` to fail on drift. The external source root is supplied explicitly.

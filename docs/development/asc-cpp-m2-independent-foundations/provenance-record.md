# Milestone 2 random provenance record

Status: Frozen source selection; implementation pending

## Approved algorithm source

| Field | Frozen value |
| --- | --- |
| Algorithm | Philox4x32-10 |
| Authors | John K. Salmon, Mark A. Moraes, Ron O. Dror, David E. Shaw |
| Work | *Parallel Random Numbers: As Easy as 1, 2, 3* |
| Venue/date | SC11, 2011 |
| Author-hosted artifact | <https://www.thesalmons.org/john/random123/papers/random123sc11.pdf> |
| Retrieval date | 2026-07-26 |
| Size | 280039 bytes |
| Pages | 12 |
| SHA-256 | `841f68114b052f436a818680c90d685ce796e492a448a1064e939adefbe596c6` |
| Approved material | mathematical counter/key model; generalized Feistel equations; Section 4.3 Philox construction, published multipliers and Weyl schedule; ten-round recommendation |

The owner approved Milestone 2 with no corrections after approving the
architecture package and ADR 0015. That approval closes capability gate
`PROV-03` only for this named primary paper and the exact project-owned mapping
frozen in the Milestone 2 contract.

## Clean-room derivation boundary

- The paper supplies mathematical and algorithmic requirements. The
  Milestone 2 contract fixes otherwise implementation-sensitive public lane,
  key-schedule, stream, subsequence, offset, and `Uniform01` mappings.
- The production engineer writes new C++ from the paper and frozen contract.
- The verification engineer independently writes new tests and expected words
  from the same paper and contract, after recording a contract-first test
  design and without reading production implementation.
- No paper prose, diagram, implementation code, or literal vector corpus is
  copied into the Apache-2.0 source distribution.
- MdeCpp at `f6294e9079262682ce63ae7ff2d8a643e658bf5d` remains GPLv3 behavioral and
  provenance evidence only. Its source, tests, constants-as-arranged,
  comments, generated data, and control structure are prohibited.
- Deleted asc-cpp random files at baseline
  `33b261ea33616a6395c4ad3b20646093103344f7` are also prohibited sources.
- Random123 implementation headers/tests and any other upstream implementation
  or vector corpus are not approved inputs for this milestone.

## Distribution-transform provenance

`Uniform01<float>` and `Uniform01<double>` are project-owned binary transforms
specified by ADR 0015 and frozen exactly in the Milestone 2 contract. They do
not reuse a standard-library distribution sequence or third-party
implementation.

## Notice and license disposition

ASCCpp remains Apache-2.0. This milestone implements mathematical requirements
from a cited scientific paper without copying paper text, figures, source code,
tests, or generated data. The citation and immutable artifact identity are
retained here for reproducibility and academic provenance. No third-party code
notice is added by this clean-room implementation.

Any later proposal to import or compare against implementation source, publish
additional engines, serialize state, or add Sobol code/data requires a separate
source, license, notice, mapping, and owner-approval record.

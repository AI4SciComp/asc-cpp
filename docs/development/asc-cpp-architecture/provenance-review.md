# MdeCpp provenance and reuse review

Status: Designer B provenance gate; Random decisions resolved by Issue 12

Reviewed MdeCpp commit: `f6294e9079262682ce63ae7ff2d8a643e658bf5d`

Reviewed asc-cpp baseline: `33b261ea33616a6395c4ad3b20646093103344f7`

Issue 12 Random destination baseline:
`bd73a5bc63524bfec8fae9b4d6446e0fcdf8e851`

## 1. Conclusion

MdeCpp is safe to use as a behavioral catalogue for the asc-cpp restart, but
the present evidence does **not** support copying its production code, tests,
generated files, or numerical tables into Apache-2.0 asc-cpp.

The conservative rules are:

- Reimplement approved behavior independently from a reviewed specification.
- Derive test expectations independently from mathematics or a named,
  approved primary source.
- If a permissively licensed upstream algorithm is selected, obtain it from a
  pinned authoritative upstream revision, not from the MdeCpp adaptation.
- Preserve the upstream copyright, license, attribution, version, and data
  lineage required by that source.
- Do not restore asc-cpp's intentionally deleted `THIRD_PARTY_NOTICES` as an
  incidental side effect. The lead/owner must approve a new notice inventory
  when a concrete dependency or derived source is accepted.

This is an engineering provenance assessment, not a legal opinion. A
rightsholder or project owner must decide any relicensing exception.

For Random, ADR 0020 and the machine-readable Random crosswalk supersede this
review's unresolved planning recommendations. They pin the approved
authoritative engine artifacts, select the separately licensed Joe/Kuo
direction data, and continue to reject every inspected MdeCpp Random
implementation, test, benchmark, and generated artifact.

## 2. Reproducible evidence boundary

### MdeCpp

- Remote: `git@github.com:escapetiger/MdeCpp.git`
- Branch: `main`
- Commit: `f6294e9079262682ce63ae7ff2d8a643e658bf5d`
- Tracked notice/license inventory: root `LICENSE` only
- Worktree exception: tracked `Makefile` is locally modified and was excluded
  from pristine evidence
- Ignored source-like roots: `/legacy/` and `/develop/`
- Ignored local artifacts: `/build/`, `/Testing/`, and local editor/agent
  settings
- Broken tracked artifact: `miniapps/vpfp/vpfp1d`, an absolute symlink into a
  local build tree

The tracked `CLAUDE.md` says MdeCpp is a C++ port of MdeMat mirrored in
`legacy/MdeMat`. `docs/physics_migration.md` also describes migrations from
that tree. Because `/legacy/` is ignored and not tracked at the reviewed
commit, it cannot establish original copyright, license, exact source
revision, or transformation history.

### asc-cpp

- Remote: `git@github.com:AI4SciComp/asc-cpp.git`
- Branch: `main`
- Commit: `33b261ea33616a6395c4ad3b20646093103344f7`
- Current retained root license: Apache License 2.0
- Current worktree: the prior source, tests, build files, contributor
  documents, and `THIRD_PARTY_NOTICES` are intentionally deleted

The deleted notice at the baseline commit is historical prior art only. It
listed several random-engine attributions, but it is not a current notice,
does not authorize reuse, and did not resolve Sobol or Lebedev data lineage.

## 3. License baseline

MdeCpp's root `LICENSE` is the GNU General Public License, version 3. Most
tracked source headers say “All rights reserved. See files LICENSE for
details,” but there are no SPDX expressions and no per-file exception or
Apache-compatible relicense grant.

For planning, this review therefore treats MdeCpp-authored production and test
source as GPLv3-covered. The repository does not state “or any later version,”
so `GPL-3.0-only` is the conservative metadata value, while acknowledging that
the project should clarify the intended SPDX expression.

GPLv3-covered MdeCpp source cannot simply be copied into an
Apache-2.0-only asc-cpp distribution while keeping that copied work under
Apache-2.0 alone. Acceptable paths are:

1. obtain an explicit compatible grant from every relevant rightsholder;
2. use a permissively licensed original upstream directly and preserve its
   conditions; or
3. independently implement behavior from non-copyrightable requirements,
   mathematical specifications, standards, or other approved sources.

The same restriction applies to MdeCpp tests. Rewriting names while retaining
the test's expressive structure and literal corpus is not a clean-room
process.

## 4. Inventory and determinations

| Source or artifact | Observed claim/lineage | Determination |
| --- | --- | --- |
| Most tracked `.h`, `.cc`, CMake, and test files | MdeCpp team/Yi Cai headers; root GPLv3 text | Behavior catalogue only; no copying into Apache asc-cpp |
| MdeMat-derived portions | `CLAUDE.md`, `docs/physics_migration.md`, and selected comments identify ignored `legacy/MdeMat` as source/reference | Exact source, copyright, license, and transformation history are not reproducible; source adaptation blocked |
| Top-level `CMakeLists.txt` | Line 9 says modified from MFEM's `CMakeLists.txt` | Do not copy; exact MFEM revision and retained BSD-3-Clause notice are missing |
| `random/generator.h` SplitMix64 | Names Sebastiano Vigna and asserts public-domain origin plus local MIT relicensing | Select and pin an authoritative upstream version; do not inherit the MdeCpp adaptation |
| `random/generator.h` PCG32 | Names Melissa O'Neill and embeds an Apache-2.0 notice | Upstream may be compatible, but select exact upstream revision and keep its notice; do not copy local GPL-covered adaptation |
| `random/generator.h` xoroshiro64*/128+ | Names David Blackman and Sebastiano Vigna and asserts public-domain origin plus local MIT relicensing | Select an exact upstream version and preserve public-domain dedication/attribution; do not copy local adaptation |
| `random/sobol.h` | Strongly resembles John Burkardt's MIT Sobol C++ implementation, including a distinctive dated “JVB” comment and large polynomial table | Local attribution/license omission; direct reuse blocked |
| `random/data/sobol.txt` | 1111-by-62-style direction assignments, no header or origin metadata | Provenance and license unresolved; reuse blocked |
| `random/data/sobol.bin` | Generated little-endian binary, no embedded version, hash, license, or attribution | Reuse blocked; runtime format is also not portable as read |
| `random/data/convert_sobol_to_binary.py` | Converts local text assignments to little-endian signed 64-bit values; docstring names a different input/output suffix | Converter is GPL-covered and does not establish data rights; reuse blocked |
| `integrate/lebedev_tables.h` | Says tables were generated from ignored `legacy/MdeMat/.../ld*.m` | Underlying data origin/license and generator are absent; reuse in `asc-xde` blocked |
| Eigen, MKL, CUDA, OpenMP, MPI, GoogleTest | Build-time/provider dependencies rather than vendored source in the inspected paths | Record exact versions and terms when enabled; keep provider headers private and do not imply that MdeCpp preapproval transfers |
| CGAL and VTK named in `CLAUDE.md` | Listed as preapproved MdeCpp dependencies but not established as active asc-cpp requirements | No asc-cpp approval or dependency follows from the MdeCpp guidance |
| MdeCpp tests | Project-authored tests under root license, often coupled to the monolithic target | Use only as behavioral prompts; independently design asc-cpp tests and expected results |

## 5. Detailed upstream findings

### 5.1 MFEM-derived build script

MdeCpp `CMakeLists.txt:9` says it was modified from MFEM's top-level
`CMakeLists.txt`, but it does not pin an MFEM commit or retain an MFEM notice
in the tracked notice inventory. MFEM's current repository publishes a
[BSD 3-Clause license](https://github.com/mfem/mfem/blob/master/LICENSE) that
requires source and binary redistributions to retain its notice and
conditions.

The exact historical MFEM revision used by MdeCpp is unknown. The clean
restart should use asc-cmake and new component-oriented build files, so there
is no need to resolve this lineage unless someone proposes copying the MdeCpp
build script.

### 5.2 SplitMix and xoroshiro

MdeCpp embeds algorithm bodies and comments derived from the Blackman/Vigna
family. The primary xoroshiro sources publish a broad public-domain dedication
and warranty disclaimer for
[xoroshiro64*](https://prng.di.unimi.it/xoroshiro64star.c) and
[xoroshiro128+](https://prng.di.unimi.it/xoroshiro128plus.c). The corresponding
[SplitMix64 source](https://prng.di.unimi.it/splitmix64.c) should be captured
and pinned if that version is selected.

The MdeCpp comments' phrase “Re-licensed under the MIT license for use within
mde library” is not a sufficient asc-cpp provenance record:

- it does not identify a locked upstream revision;
- there is no tracked MIT notice file for those local adaptations;
- local modifications are distributed in a repository whose root license is
  GPLv3;
- the exact delta from the upstream source has not been recorded.

Recommended path: decide whether these older engine variants are still desired,
then independently implement from the exact selected primary source and
publish authoritative fixed-seed vectors, attribution, version, and sequence
contract.

### 5.3 PCG32

MdeCpp `random/generator.h:88-168` names Melissa O'Neill and includes an
Apache-2.0 notice. The PCG project's
[official download page](https://www.pcg-random.org/download.html) publishes
the minimal PCG32 code and identifies it as Apache-2.0 licensed. Its
[minimal C API documentation](https://www.pcg-random.org/using-pcg-c-basic.html)
also defines state, stream selection, and deterministic reseeding behavior.

That permissive upstream is a plausible source for an Apache-2.0 asc-cpp
implementation, but approval still requires:

- a chosen PCG family member and version;
- a pinned upstream artifact or commit;
- the upstream copyright and Apache notice;
- independently verified initialization and output vectors;
- a decision on stream, jump/advance, serialization, and stability guarantees.

Do not use MdeCpp as the upstream artifact.

### 5.4 Sobol implementation

MdeCpp `random/sobol.h:76-81` retains a distinctive dated comment about
`ATMOST` and “JVB, 24 January 2006.” Its control structure and large
`poly[1111]` table also align with John Burkardt's C++ Sobol implementation.
This is strong evidence of derivation, not merely implementation of the same
abstract algorithm.

Burkardt's [Sobol project page](https://people.sc.fsu.edu/~jburkardt/cpp_src/sobol/sobol.html)
states that:

- the code computes the Sobol sequence by Bennett Fox;
- it adapts ACM TOMS Algorithms 647 and 659;
- the extension to 1111 dimensions follows Joe and Kuo, with extra C++ data
  supplied by Steffan Berridge;
- the C++ version is by John Burkardt; and
- the page is distributed under the MIT license.

The linked
[Burkardt C++ source](https://people.sc.fsu.edu/~jburkardt/cpp_src/sobol/sobol.cpp)
contains explicit MIT licensing, author, and reference blocks. MdeCpp's local
file replaces those with a generic MdeCpp header and does not retain the
upstream author/license block.

An MIT source can generally be included in a GPL work while preserving the
MIT conditions, but the absence of that attribution in MdeCpp is a provenance
defect. It does not erase the upstream condition or make the adapted source
safe to move into asc-cpp without reconstruction and review.

### 5.5 Sobol direction data and binary

The direction data is a separate blocker from the implementation:

- `random/data/sobol.txt` begins immediately with assignments and has no
  copyright, license, citation, version, or generation header.
- `random/data/convert_sobol_to_binary.py` claims to convert `sobol.inc` to
  `sobol.dat`, while actually reading `sobol.txt` and writing `sobol.bin`.
- The converter writes every value as little-endian signed 64-bit data.
- `random/sobol.cc:20-42` reads the binary directly into a native `int64_t`
  array from an absolute configured source-tree path.
- The runtime reader does not declare or validate endianness, format version,
  checksum, source version, or semantic dimensions beyond byte count.

Pinned evidence hashes:

| Artifact | SHA-256 |
| --- | --- |
| `random/data/sobol.txt` | `16722f0f9978c1d4f5f0cfc67228f5417f217f4fa30bc7666a03e10d52bd3e14` |
| `random/data/sobol.bin` | `836668fdbb3b72fabc0f0d07febfa06d9b022bed59b51d8eb5e5deed32a4b3cd` |
| `random/data/convert_sobol_to_binary.py` | `f43157752ad613be02b0e5c2271fcd801f0f5695f6f4b85b2d17cf5a5f8ceda2` |

These hashes identify the inspected files; they do not cure provenance.

Before a Sobol feature can proceed, the owner must select an authoritative
direction-number dataset, confirm redistribution rights, record its exact
version and citation, preserve notices, define the generated format, retain a
reproducible generator, and validate checksums. Until then, no local Sobol
table, binary, converter, or vector derived solely from them should enter
asc-cpp.

### 5.6 Lebedev table data

`integrate/lebedev_tables.h:31` says its 31 table sets were generated from
`legacy/MdeMat/+approx/+integrate/lebedev/ld*.m`. That input is ignored and
untracked. The reviewed commit contains neither the generator nor an upstream
Lebedev/Laikov source citation, license, version, or checksum.

The inspected `integrate/lebedev_tables.h` has SHA-256
`77b74a0057cf32971a804b253b83d592b95f1a60fd8bc98aef8120febeacddad`.
That identifies the reviewed output but does not establish the origin or
rights of its values.

This feature maps to `asc-xde`, not asc-cpp, but the same gate applies:

- do not copy the table or its literal values;
- locate an authoritative redistributable dataset;
- record original authors/citation, exact release, and license;
- retain a reproducible transformation script and manifest;
- verify symmetry, total weight, polynomial exactness, point counts, and
  checksums.

## 6. Source-derived versus clean-room classifications

### Prohibited under current evidence

- copying or mechanically translating MdeCpp production source;
- copying MdeCpp tests or preserving their expressive structure/literal
  corpus through superficial edits;
- importing the Sobol text, binary, converter, polynomial table, or locally
  derived reference vectors;
- importing the Lebedev table values;
- copying the MFEM-derived MdeCpp CMake file;
- treating ignored MdeMat/develop content or local build output as evidence;
- treating a historical deleted asc-cpp notice as current authorization.

### Allowed for architecture and specification

- cataloguing public behavior, invariants, edge cases, and observed defects;
- designing new interfaces that meet the approved six-module boundaries;
- deriving mathematical expected values independently;
- citing published algorithms and standards;
- comparing independently produced outputs with multiple approved
  implementations without copying code or fixtures;
- using MdeCpp failures and coupling as negative requirements.

### Potentially allowed after explicit approval

- direct use of an exact permissively licensed upstream source with its
  original notices and a recorded modification history;
- source-derived work under a new compatible rightsholder grant;
- generated data whose authoritative input, generator, license, citation,
  format, and checksums are all recorded.

## 7. Clean-room workflow for asc-cpp

For every behavior selected from `mdecpp-disposition.yaml`:

1. Record the MdeCpp behavior and why it is desired without transcribing
   implementation expression.
2. Write or approve a storage- and provider-aware asc-cpp contract.
3. Identify independent normative sources: mathematics, standards, primary
   algorithm publications, or approved upstream documentation.
4. Have implementation follow the approved contract and normative sources,
   not MdeCpp file structure or code.
5. Derive expected test values independently and record the derivation.
6. Run differential checks only as secondary evidence; do not import
   MdeCpp-generated fixtures.
7. Record author, reviewer, source citations, algorithm/data versions,
   licenses, notices, and hashes in the change.
8. Verify that component-isolation tests enforce the approved destination.

For a direct permissive-upstream adaptation, add:

- upstream URL and immutable revision/archive hash;
- original file path;
- upstream license and copyright text;
- local files derived from it;
- a concise modification log;
- required `NOTICE`/`THIRD_PARTY_NOTICES` entry;
- reference vectors tied to the exact algorithm version.

## 8. Required owner decisions

| ID | Decision | Blocks |
| --- | --- | --- |
| `PROV-01` | Clarify MdeCpp's intended SPDX expression and whether relevant rightsholders offer an Apache-compatible grant | Any direct MdeCpp source/test adaptation |
| `PROV-02` | Approve clean-room behavioral reimplementation as the default path | All six asc-cpp modules |
| `PROV-03` | **Resolved by Issue 12:** SplitMix64 2015, PCG32 minimal C 0.9 XSH-RR, xoroshiro64* 1.0, and xoroshiro128+ 1.0 are pinned by URL and SHA-256 | Issue 13 must preserve those identities and independently derive vectors |
| `PROV-04` | **Resolved for Random by Issue 12:** Issue 14 must retain the Joe/Kuo license in source and binary distributions and add `THIRD_PARTY_NOTICES` | Other accepted external source/data still needs a component-specific owner decision |
| `PROV-05` | **Resolved by Issue 12:** independently implement Joe/Kuo Sobol and use only the pinned BSD-style `new-joe-kuo-6.21201` input; reject all MdeCpp Sobol artifacts | Issue 14 must verify input, license, generator, generated, install, and compiled-table identities |
| `PROV-06` | Resolve Lebedev dataset lineage in `asc-xde` | Lebedev quadrature |
| `PROV-07` | Decide whether any MFEM-derived build ideas are needed; otherwise require new asc-cmake implementation | Build system |
| `PROV-08` | Approve exact optional provider versions and public/private header policy | CUDA/Eigen/MKL/MPI/OpenMP/provider facets |

## 9. Notice manifest required for future work

The restart should maintain a machine-reviewable provenance manifest with at
least:

- component and destination owner;
- local path;
- origin project and canonical URL;
- immutable upstream revision or release;
- original path and artifact hash;
- copyright holder;
- SPDX license expression;
- required notice text/location;
- derivation mode: original, source-derived, generated, or clean-room;
- modification summary;
- data citation/version/schema/endianness when applicable;
- test-vector source and derivation;
- approving reviewer and date.

No third-party code or data should merge merely because an algorithm is widely
known or a previous repository already contained it.

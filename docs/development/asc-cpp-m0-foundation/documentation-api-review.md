# Milestone 0 documentation and API review

Status: Complete; no release blocker

Date: 2026-07-27

Role: Independent documentation and API reviewer

## Scope and authority

This review used:

- the frozen Milestone 0 contract and ownership ledger;
- the approved implementation plan and Stage A architecture package;
- ADRs 0001–0018;
- the released ASCCMake 0.1.0 consumption record;
- the foundation implementation self-review; and
- the integrated root CMake, package configuration, and foundation tests.

Writes were limited to:

```text
README.md
CHANGELOG.md
CONTRIBUTING.md
SECURITY.md
docs/README.md
the exact 24 retained historical documents in the implementation plan
this review
```

No architecture-package, CMake/package/CI, test, production, contract,
ownership, sibling-repository, branch, or history change was made.

## Live documentation result

The five live entry documents now describe only the target-free Milestone 0
foundation.

### README

The README states:

- no production implementation, public header, API, library, provider,
  compatibility facade, product target, or release exists;
- the six modules, two random facets, and aggregate are approved future
  architecture, not current availability;
- there is no `array`, `linalg`, common backend, or seventh module;
- the package identity is unreleased `ASCCpp` 0.0.0;
- all component requests are negative and create no `ASC::*` target;
- validation binds released ASCCMake 0.1.0; and
- MdeCpp is clean-room behavioral evidence, not a source import.

### Changelog

The changelog has only an `Unreleased` Milestone 0 foundation entry. It does
not retain the deleted implementation's false `0.1.0` release history or
claim that a library has shipped.

### Contributor and security guidance

The contributor guide:

- removes the reference to deleted `AGENTS.md`;
- records the no-production/no-later-milestone boundary;
- uses the actual ASCCMake and standard-CMake ownership split;
- prohibits restored legacy paths, unapproved dependencies, and copied
  MdeCpp material; and
- distinguishes future C++20 policy from current target-free content.

The security policy truthfully reports that no runtime version is supported.
It gives private-reporting and secret-redaction guidance without inventing a
contact address, response deadline, or production support window.

### Documentation index

`docs/README.md` routes current readers to the Stage A package and Milestone 0
contract. It isolates the retained historical pages in a separately labeled
section.

## Package and API audit

The integrated implementation matches the documentation:

```text
project: ASCCpp
version: 0.0.0
languages: NONE
available components: none
exported ASC targets: none
```

Known future components are:

```text
core
utilities
expression
dense
sparse
random
random_dense
random_sparse
cpp
core_cuda
dense_cuda
sparse_cuda
random_cuda
random_dense_cuda
random_sparse_cuda
```

`ASCCppConfig.cmake.in` sets `ASCCpp_VERSION`,
`ASCCpp_KNOWN_COMPONENTS`, and empty `ASCCpp_AVAILABLE_COMPONENTS`.
No-component lookup requests unavailable `cpp`. Known, unknown, required,
optional-only, and quiet requests all remain negative and import no target, as
required by the frozen foundation contract.

No live document advertises a public C++ declaration, header, installed
target, numerical operation, provider, compatibility API, sanitizer result,
GPU result, ABI guarantee, or performance result.

## Historical document treatment

Exactly the 24 documents named by the implementation plan received the same
superseded-state warning. Each banner:

- identifies historical commit
  `33b261ea33616a6395c4ad3b20646093103344f7`;
- calls the implementation deleted and five-component;
- denies that its body describes the current M0 API/package; and
- links to the approved Stage A architecture.

Each file has exactly nine inserted lines. Removing lines 3–11 from every
bannered file produces a byte-for-byte match with its `HEAD` body. No
historical claim, spelling, table, code sample, link, or whitespace was
silently rewritten.

## Findings and resolutions

### DAPI-001 — live entry documentation described deleted production

Initial `README.md`, `CHANGELOG.md`, and `CONTRIBUTING.md` advertised the
deleted five-component implementation, its targets, dependencies, public
headers, providers, examples, and a `0.1.0` release.

Resolution: replaced all three with M0-only foundation documentation.

### DAPI-002 — required live foundation documents were absent

`SECURITY.md` and `docs/README.md` did not exist.

Resolution: added a no-release security boundary and an authoritative
documentation index without adding an API or support promise.

### DAPI-003 — intentional historical broken link

The preserved body of `docs/migration/inventory.md` links to deleted
`../../THIRD_PARTY_NOTICES`. Restoring that file is prohibited, and changing
the retained historical body would violate the frozen preservation contract.

Resolution: informational, accepted by the lead. The normalized banner makes
the page's historical status explicit. The link remains intentionally broken;
all live/new links pass. This is not a release blocker.

### DAPI-004 — no package/API mismatch found

Read-only review of the integrated CMake and package tests found no API,
component, target, version, dependency, or availability claim that conflicts
with the live documents.

Resolution: none required.

## Exact validation

### Fresh foundation/package validation

```sh
m0_docs_build=$(mktemp -d /tmp/asc-cpp-m0-doc-review.XXXXXX)
cmake -S . -B "$m0_docs_build/build" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build "$m0_docs_build/build" --parallel 4
ctest --test-dir "$m0_docs_build/build" \
  --output-on-failure --no-tests=error
```

Result at `/tmp/asc-cpp-m0-doc-review.NR6x2B`: **PASS**, 6/6 tests:

```text
3/3 architecture
3/3 package
0 failed
0 skipped
```

This includes dependency/capability manifests, no production targets/files,
build-tree package requests, installation/relocation with spaces, and
unchanged package registry.

### Historical body preservation

For each exact retained file:

```sh
diff -u <(git show "HEAD:$file") <(sed '3,11d' "$file")
```

Result: **PASS**, 24/24 byte-equivalent historical bodies.

```sh
git diff --numstat -- <24 retained files>
```

Result: **PASS**, exactly `9 0` for every file.

### Links

A local-path scan covered inline and reference-style Markdown links in all
five live files and all 24 retained files.

Result:

```text
live/new links: passed
historical banner links: 24/24 passed
intentional historical body link: 1 broken (DAPI-003)
remote HTTP links: not network-tested
```

### Fences, line width, and whitespace

```sh
rg -c '^```|^~~~' <each reviewed Markdown file>
awk 'length($0) > 80 { print FILENAME, FNR }' \
  README.md CHANGELOG.md CONTRIBUTING.md SECURITY.md docs/README.md
git diff --check -- <all reviewed files>
```

Results:

- **PASS** — balanced fences in every reviewed file;
- **PASS** — no line over 80 columns in live entry documents;
- **PASS** — no trailing-whitespace or patch whitespace errors.

Existing over-80 lines below historical banners were preserved rather than
rewritten.

## Compatibility and provider classification

Milestone 0 has no source, ABI, numerical, random-bit, provider, schema, or
runtime compatibility claim. It has only a package-negative foundation
contract.

No GPU provider exists. Configure, compile, runtime, and parity GPU evidence
are not applicable to this documentation role and are not claimed.

## Final review conclusion

The live documentation is target-free, matches the integrated `ASCCpp` 0.0.0
negative package behavior, and does not imply implementation or release. The
24 historical bodies remain intact behind normalized warnings. DAPI-003 is
the only local-link exception and is intentionally retained by contract.

No correction outside the assigned documentation scope is requested.

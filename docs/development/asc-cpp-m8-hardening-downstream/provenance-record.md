# Milestone 8 Provenance Record

Status: complete; clean-room boundary accepted

Date: 2026-07-28

## Authorship boundary

Milestone 8 hardening tools, verification fixtures, benchmark probes,
documentation, and integration are original project work derived from the
owner-approved architecture, ADRs, current Milestone 7 public/package
contracts, and independently frozen verification oracles.

No MdeCpp or deleted historical asc-cpp implementation, test, literal vector,
table, generated data, or distinctive source structure is copied or
mechanically translated. The unrelated modified MdeCpp `Makefile` remains
untouched.

## Prior M8 branch

Commit `d611aa876576ab949c2c213977f628d2844539ba` on the preserved stale M8
branch is project-owned prior evidence, not the approved predecessor. Its raw
Random CUDA pointer API conflicts with the owner's later M7 amendment.
Revision-2 roles may reuse only general hardening requirements or
project-native ideas after re-review against the current
`MutableMemoryView + word_count` surface and independently frozen tests.

No stale product source, API, ABI hash, CUDA symbol observation, checkpoint
claim, or validation result is adopted unchanged.

## External repositories and tools

- Released ASCCMake 0.1.0 is consumed exactly under its existing Apache-2.0
  package contract; no source is copied into asc-cpp.
- The real skeletal asc-xde repository at
  `abcb29b51f22f40afd7f174707b7ccf83c32d4bf` supplies only downstream
  repository identity. The asc-cpp-owned trial invents no asc-xde production
  API and writes nothing to that repository.
- Compiler, linker, CMake, CUDA, ELF, and sanitizer tools generate local
  observations only. Their output is not installed product source or vendored
  data.

## Dependency and notice result

No new third-party source, data, runtime dependency, license, or notice
obligation is introduced. Apache License 2.0 remains the asc-cpp license.
`THIRD_PARTY_NOTICES` remains intentionally absent because no approved import
requires it.

Terminal source, installed-package, dependency, and notice scans plus
independent review confirmed this record. No new third-party source, data,
runtime edge, license, or notice obligation was introduced.

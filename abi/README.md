# ASCCpp API and ABI observations

Public-header and component contracts describe ASCCpp `1.0.0`.
The three GCC 11/Clang 19 ELF digest files remain historical `0.9.0` evidence.
They are selected only for that project version. Other versions/toolchains
retain full ELF observations without comparing against an unmeasured digest.
These records do not promise ABI compatibility between different minor
versions, compilers, standard libraries, build modes, CUDA toolkits, or
platforms.

- `public-headers.sha256` is the normalized, byte-complete public-header
  baseline. It covers declarations, templates, inline definitions, macros, and
  documentation in every installed header.
- `header-owners.txt` assigns each public header to its one product target file
  set.
- `targets-and-components.txt` records target identities, target kinds,
  output/export names, static-definition macros, exact direct links, public
  links, provider class, components, and direct component dependencies.
- `linux-x86_64-*-shared.txt` files are local ELF observations created by
  `tools/hardening/InspectElfAbi.cmake`. They intentionally retain mangled
  names as the ABI key and include demangled names for review.

The tooling never updates these files. A baseline change is an explicit,
reviewed source change. Header hashes normalize CRLF and lone CR to LF before
hashing; all other content is exact.

Current shared libraries use `VERSION 1.0.0` and `SOVERSION 1.0`. The SONAME boundary
does not promise compatibility with another pre-1.0 minor; see ADR 0021.

See [ADR 0022](../docs/architecture/decisions/0022-v1-release-boundary.md).

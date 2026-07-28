# Milestone 8 API and ABI observations

These files are review baselines for the unreleased ASCCpp `0.9.0` candidate.
They are not a promise of ABI compatibility between different pre-1.0 minor
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

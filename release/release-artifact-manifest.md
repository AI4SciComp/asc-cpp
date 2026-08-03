# ASCCpp 0.9.0 artifact manifest

No release artifacts have been published. The tag-gated workflow will produce
immutable, versioned artifacts from the exact `v0.9.0` commit:

- deterministic source archive (`ASCCpp-0.9.0-source.tar.gz`);
- deterministic Linux x86-64 provider-free static and shared installed-package
  archives;
- deterministic strict Doxygen HTML/XML archive and a separate `ASCCpp.tag`;
- `SHA256SUMS` covering every downloadable artifact;
- SPDX SBOM;
- GitHub artifact attestations and workflow provenance where available; and
- validation logs needed to consume the downloaded draft artifacts.

Every source/package archive must contain `LICENSE`, `THIRD_PARTY_NOTICES`,
`data/random/LICENSE.joe-kuo`, the Joe--Kuo data, public headers, CMake package
metadata, and the applicable libraries.

The Gate B worktree dry run reproduced each installed archive byte-for-byte
from two assemblies under the recorded local GCC 11 toolchain. The installed
tree does not contain this release record, and its tar metadata uses the fixed
release epoch `2026-08-03T00:00:00+08:00`, so these local identities are not
tied to a worktree-report or release-commit timestamp:

- `ASCCpp-0.9.0-linux-x86_64-static.tar.gz`:
  `3c90c3b7ab1b269b0f285ae8488f37460f6369c12f793591bafcf0ed30c247d1`;
- `ASCCpp-0.9.0-linux-x86_64-shared.tar.gz`:
  `9a9397632e6fbe196f47cfd9bbf66e95af02cc625dbf2d88bfdba5bf91024798`.

The source, documentation, tag-file, and SBOM hashes are intentionally not
self-declared inside artifacts that contain this file. All Gate B hashes are
prepublication evidence carried by the externally generated and verified
`SHA256SUMS`. The final immutable values, including installed-package hashes,
must be generated from the exact approved `v0.9.0` commit; hosted binary hashes
may differ if the release assembler uses a different recorded toolchain.

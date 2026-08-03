# Release process

`main` is release-only, `develop` is integration, and `release/0.9.0` is the
sole 0.9.0 preparation branch. The original tree is preserved by the annotated
snapshot tag documented in [`release/release-plan.md`](../release/release-plan.md).

The release gate requires the exact provider-free compiler/platform/linkage
matrix, sanitizers, packages/relocation/isolation, contracts, strict Doxygen,
installed examples, static analysis, deterministic archives, checksums, SBOM,
and independent draft-artifact consumption. Unexpected CTest skips fail.

CUDA remains experimental unless a trusted real-NVIDIA runner supplies the
complete exact-commit evidence. Public publication additionally requires
public immutable ASCCMake acquisition, verified private vulnerability
reporting, and verified branch/tag protection.

The `Release draft` workflow accepts exactly `v<project-version>`, invokes the
complete CI and CodeQL workflows on the tag commit, uses least privilege and
full action SHAs, creates a draft prerelease, downloads it independently, and
never replaces an existing release. A manual dry run assembles and consumes
the same artifacts without creating GitHub state.

After final owner approval, the `Publish verified release` workflow runs in
the `release-publication` environment. The owner must configure that
environment with required reviewers before enabling public publication. The
workflow rechecks the tag, anonymous immutable ASCCMake download, draft state,
asset inventory, archive safety, and every checksum before changing only the
draft/prerelease flags. It never rebuilds or replaces an asset. Verified
private vulnerability reporting and branch/tag protection remain explicit
owner preconditions; workflow code does not infer those repository settings.

Exact phase results and artifact identities are maintained under
[`release/`](../release/).

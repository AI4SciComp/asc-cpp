# Governance

ASCCpp is maintained by the AI4SciComp project. Repository ownership is
recorded in [`.github/CODEOWNERS`](.github/CODEOWNERS).

## Decision making

Routine changes use review by a code owner for the affected area. Changes to
public APIs, numerical semantics, module dependencies, reproducibility,
provenance, ABI policy, supported platforms, or release process require an ADR
and approval from a release approver.

Technical decisions prefer reproducible evidence and consensus. If consensus
cannot be reached, the release approver records the decision and rationale in
an ADR. Conflicts of interest must be disclosed; an affected approver recuses
themself.

## Releases

`main` is release-only, `develop` is the integration branch, and release work
uses `release/<version>`. A release requires:

1. all supported jobs passing on the exact commit;
2. resolved review conversations and code-owner approval;
3. verified documentation, package relocation, checksums, SBOM, and
   provenance; and
4. explicit approval before merge, tagging, draft creation, and publication.

Tags and published artifacts are immutable. Security embargoes may temporarily
limit public discussion but do not permit silent history rewriting.

## Current release approver

The repository owner represented by `@escapetiger` is the release approver
until this file is amended through review.

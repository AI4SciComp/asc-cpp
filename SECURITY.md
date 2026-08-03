# Security policy

## Supported versions

| Version | Security support |
| --- | --- |
| `0.9.x` | Supported after public 0.9.0 publication |
| unreleased branches and older versions | No security support promise |

The 0.9.0 release must not be published until a private vulnerability-reporting
route is enabled and verified. No response or remediation deadline is promised.

## Reporting a vulnerability

Use the repository **Security** page at
<https://github.com/AI4SciComp/asc-cpp/security> and select private
vulnerability reporting when available. Do not disclose suspected
vulnerabilities in a public issue, discussion, or pull request. If the private
form is unavailable, publication remains blocked; contact the repository owner
`@escapetiger` privately through a previously established channel.

Include the affected version/commit, platform/toolchain/provider, smallest
reproducer, impact, and whether the report involves untrusted input, numerical
integrity, memory safety, credentials, or supply chain. Conduct reports use the
same private route.

## Trust boundaries

- `Status`/`Result` report recoverable validation/provider failures. Fatal
  contract checks are for violated caller invariants and are not a sandbox.
- Callers own input validation above documented scalar, shape, index, stride,
  capacity, and byte limits. Numerical correctness is not cryptographic
  integrity.
- Views do not own storage. Callers preserve owner, resource, context, stream,
  event, and workspace lifetimes through completion.
- Provider-free CPU execution requires accessible host memory and is
  synchronous. CUDA enqueue success is not completion; CUDA remains
  experimental in 0.9.0.
- Configuration sensitivity metadata controls rendering/redaction behavior but
  does not encrypt values.
- File and serialization helpers enforce documented byte/version boundaries;
  they do not authenticate untrusted files.

## Dependencies and provenance

The build-only `ASCCMake 0.1.0` identity and checksum are pinned in
[`docs/installation.md`](docs/installation.md). GitHub Actions are pinned by
full commit SHA. Third-party notices and Joe--Kuo data provenance are installed
with the package. Report any checksum, tag, action, archive, SBOM, or attestation
mismatch as a supply-chain concern.

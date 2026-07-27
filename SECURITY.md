# Security policy

## Supported versions

asc-cpp currently has no released library version. Milestone 1 is an
unreleased `0.1.0` candidate with the provider-free `ASC::core` target and
public core API. No release line has a production support or compatibility
window yet.

When releases begin, this file will identify supported release lines. Until
then, security-relevant foundation issues may still include build/package
integrity, CI permissions, dependency or provenance problems, path handling,
and accidental disclosure.

## Reporting a vulnerability

Do not disclose a suspected vulnerability in a public issue, pull request,
discussion, log, or test fixture.

Use the repository's private security-reporting channel when it is available.
If that channel is unavailable, contact the repository owner privately through
GitHub and include:

- the affected revision and environment;
- a concise impact assessment;
- reproduction steps or a minimal proof of concept;
- whether credentials, private repositories, or third-party material are
  involved; and
- any known mitigation.

Do not include live secrets. Redact tokens, credentials, private URLs, personal
data, and sensitive configuration values.

No response or resolution deadline is promised for this unreleased project.
Reports will be assessed according to impact, reproducibility, and the active
milestone scope. Coordinated disclosure timing must be agreed privately.

## Security boundaries

- Optional providers must remain opt-in and isolated from base components.
- Package configuration must not write the CMake user package registry.
- CI uses least privilege and pinned external actions.
- External source or data use must satisfy
  [ADR 0017](docs/development/asc-cpp-architecture/decisions/0017-third-party-provenance.md).
- MdeCpp production code and tests are not authorized for copying into
  Apache-2.0 asc-cpp.

This policy is an engineering process, not a warranty or legal advice.

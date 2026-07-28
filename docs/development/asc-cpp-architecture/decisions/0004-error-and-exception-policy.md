# ADR 0004: status/result public errors and no production exceptions

Status: Proposed at Architecture Checkpoint A

## Context

Scientific failures, invalid metadata, provider errors, and I/O errors must be
structured and release-stable. Historical behavior mixed assertions,
exceptions, aborts, mutable global policy, and provider macros.

## Decision

Public production APIs use:

- `Status` for success/failure without a value;
- `Result<T>` for value-or-failure, including move-only `T`;
- a stable ASC code plus optional provider name and signed native code;
- `[[nodiscard]]` on both transports.

Initial error domains cover argument, shape/index/overflow, state/lifetime,
allocation, memory/access/transfer, provider/capability, numerical,
configuration/parser, and I/O/encoding/version.

Public invalid input and operational failures return status before destination
mutation. Debug assertions diagnose internal assumptions; release-active fatal
contracts handle impossible programmer violations such as reading the value of
a failed result. Fatal contracts do not use mutable global handlers.

There is no public production exception API or exception translation layer.
Message text is diagnostic, redactable, and not ABI. `noexcept` is written only
when true, particularly for destruction and moves.

## Consequences

- Callers must inspect failures.
- Constructors that can fail become factories.
- Exception-based convenience can exist only in a separately reviewed adapter,
  not the six base modules.
- Provider error detail remains available without exposing provider enums.

## Verification

Compile nodiscard checks, build with exceptions disabled, fault-inject every
domain, verify unchanged outputs, provider-code preservation, secret redaction,
and release-build contract behavior.

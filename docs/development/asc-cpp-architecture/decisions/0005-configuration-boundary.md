# ADR 0005: core configuration model and utilities parsing

Status: Proposed at Architecture Checkpoint A

## Context

Configuration values are shared vocabulary, while argv/file syntax is a
utility. Historical MdeCpp configuration stored dense vectors/matrices and had
ambiguous numeric conversion, mutation, duplicates, and unknown-key behavior.

## Decision

Core owns a recursive, storage-independent configuration model:

```text
null, bool, signed/unsigned 64-bit integer, double, UTF-8 string,
list, string-keyed object
```

Core also owns schema type, required/default/deprecated state, constraints,
sensitivity, validation diagnostics, and effective-value source provenance.
Dense and sparse values are never alternatives.

Utilities owns command-line and approved local-file parsing, help, diagnostics,
and transactional merging. Precedence is:

```text
schema default < files in command order < command line
               < explicit programmatic override
```

Unknown keys and duplicate scalar keys at one precedence level are errors.
List append/repeat requires an explicit schema rule. Numeric conversions are
checked and never silently truncate. Failed parsing publishes no partial
configuration. Sensitive values are redacted.

Environment variables and response files are outside v1. Programmatic and
command-line input may ship before a local-file syntax. A local syntax requires
a separate narrow format or approved parser/dependency decision.

## Consequences

Utilities stays independent of dense, sparse, expression, and random. Core does
not parse argv or a concrete configuration language.

## Verification

Test type/conversion bounds, validation, unknown/duplicate keys, precedence,
origin, rollback, negative numbers, `--`, UTF-8/path/size limits, fuzzing of
any file parser, and redaction.

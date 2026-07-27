# ADR 0003: flat asc namespace and Google C++20 source policy

Status: Proposed at Architecture Checkpoint A

## Context

The owner requires flat `asc`, current Google style, C++20, self-contained
`.h` headers, and `.cc` compiled sources. Historical code used `asc::detail`,
large public implementation headers, and inconsistent guards.

## Decision

- All supported public declarations are directly in `namespace asc`.
- Module identity is conveyed by include paths and targets, not public nested
  namespaces.
- Every non-public namespace name contains `internal`, preferably a
  file-specific name such as `internal_dense_view`.
- Internal types never occur in public signatures.
- Strict C++20, `CXX_EXTENSIONS=OFF`; no C++23 API or C++20 modules.
- Public headers end in `.h`, are self-contained, include what they use, and
  use full-path guards such as `ASC_DENSE_VIEW_H_`.
- Ordinary compiled sources end in `.cc`. CUDA translation units may use `.cu`
  when required.
- Template definitions remain in their owning header or a deliberately named
  internal `.h`; no `-inl.h` or `*_impl.h` public convention.
- Current Google naming, ownership, include, formatting, and exception
  guidance applies unless another accepted ADR is more specific.
- Provider-neutral headers contain no optional SDK header/type.

## Consequences

Historical PascalCase accessor names and `asc::detail` receive no compatibility
promise. Public header parse cost and internal exposure require measurement.

## Verification

Compile every public header alone, scan guards/extensions/namespaces/includes,
run format/tidy/warnings, and instantiate representative templates across
multiple translation units.

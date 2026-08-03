# ADR 0017: Apache-2.0, clean-room MdeCpp use, and per-import provenance

Status: Proposed at Architecture Checkpoint A

## Context

asc-cpp retains Apache-2.0. MdeCpp is GPLv3 under current evidence, its
MdeMat source is ignored/unversioned, and some algorithm/table lineage is
unresolved. The historical asc-cpp notice is deleted.

## Decision

- Retain Apache License 2.0 for original asc-cpp work.
- Treat MdeCpp as behavior, defect, and test-category evidence only.
- Do not copy or mechanically translate MdeCpp production code or tests.
- Clean-room contracts, independent implementation, and independently derived
  expected values are the default.
- Direct source/data adaptation requires an exact authoritative upstream,
  immutable revision/hash, copyright, SPDX expression, notice, original/local
  paths, modification log, data schema/citation, test-vector derivation, and
  reviewer approval.
- Create a new `THIRD_PARTY_NOTICES` only when an accepted import requires it;
  do not restore the deleted file as authorization.
- Pin and record CI actions.
- Provider discovery does not approve provider licensing or redistribution.
- Sobol source/direction data is blocked. Lebedev data is separately blocked
  for asc-xde.
- The conservative MdeCpp metadata is `GPL-3.0-only` until its owner clarifies
  or grants compatible rights.

This is an engineering policy, not legal advice. Rightsholder/relicensing
questions go to the owner.

## Consequences

Historical algorithms/tests may require independent reimplementation effort.
Some features remain deferred even if a local copy works.

## Verification

Maintain a machine-reviewable disposition/provenance ledger, review every
external-source/data diff, verify license/notice distribution and hashes, scan
archives/installs, and block untracked/ignored/generated inputs without
lineage.

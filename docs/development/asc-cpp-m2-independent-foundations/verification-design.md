# Milestone 2 contract-first verification design

Status: frozen before production inspection

## Independent basis

This design was written before inspecting any Milestone 2 production header or
source. Its only technical inputs are:

- the frozen Milestone 2 contract, ownership ledger, and provenance record;
- approved ADRs 0001 through 0010, 0015, 0017, and 0018;
- the architecture verification strategy; and
- the approved Philox paper artifact with SHA-256
  `841f68114b052f436a818680c90d685ce796e492a448a1064e939adefbe596c6`.

The artifact independently matched the frozen size of 280039 bytes and page
count of 12. No MdeCpp source or test, deleted asc-cpp random source or test,
Random123 implementation or test, external vector corpus, or cumulative
Milestone 2 implementation was inspected.

## Utilities falsification plan

Command-line tests will cover:

- creation-time rejection of duplicate long names, short names, and
  destination paths;
- malformed ASCII names, leading dashes, invalid short-name widths, and
  destinations whose schema leaves are null, list, or object;
- long separated and attached values, exact short separated values, bare
  positionals, `--`, and value tokens beginning with `-`;
- bool positive, negative, explicit true/false, and short forms, plus rejection
  of long negation for non-bool destinations;
- locale-independent full-token numeric conversion at signed, unsigned, and
  floating boundaries without cross-type coercion;
- unknown options, malformed option tokens, short clusters, attached short
  values, missing values, invalid UTF-8, and duplicate destination spellings;
- schema defaults followed by command-line precedence, command-line origins
  with exact token indices, recursive schema validation, unknown paths,
  sensitive-value redaction, and failure without a published partial tree;
- deterministic caller-owned help, with no terminal output and no implicit
  `--help` behavior; and
- positional argument ordering and repeatable parsing with an owned option
  table.

Timer tests will exercise every empty, running, stopped, and reset transition.
They will check non-negative duration arithmetic, sample count, last interval,
average, accumulated elapsed time, restart after stop, rejected double start
and invalid stop, and reset while running. They will not use flaky minimum
sleep thresholds or infer wall-clock behavior.

## Expression falsification plan

An external type with no ASC base will participate solely through an explicit
adapter specialization. Tests will cover:

- concept acceptance for a complete adapter and compile rejection for absent
  or incomplete adapters and invalid pointwise operand combinations;
- exact rank and every-extent equality, rank-zero scalar expansion on either
  side, two-scalar rank zero, and rejection of all other broadcasting;
- negation, addition, subtraction, and multiplication scalar reads and result
  value types;
- lvalue non-owning capture, rvalue ownership, nested temporary ownership,
  stored-node lifetime, moved nodes, and the non-extension of an underlying
  non-owning view's storage;
- conservative alias propagation for each operand and opaque alias token;
- exact operation categories and sparsity effects, including
  scalar-expansion multiplication as value-dependent;
- zero allocation, zero scalar read, zero destination mutation, zero transfer,
  and zero execution dispatch during construction; and
- representative template instantiation in multiple translation units.

Tests will not introduce a storage owner, evaluator, reduction, materialized
result, provider, dense or sparse include, or generalized broadcasting.

## Independent random oracle

The oracle uses unsigned arithmetic only. For each of ten rounds it computes:

```text
p0 = 0xD2511F53 * c0
p1 = 0xCD9E8D57 * c2
next = [high(p1) xor c1 xor k0, low(p1),
        high(p0) xor c3 xor k1, low(p0)]
```

Between rounds it adds `0x9E3779B9` to `k0` and `0xBB67AE85`
to `k1`, modulo 2^32. Literal expected values below were calculated from that
oracle before production inspection:

| Case | Counter | Key | Expected result |
| --- | --- | --- | --- |
| zero | `00000000 00000000 00000000 00000000` | `00000000 00000000` | `6627E8D5 E169C58D BC57AC4C 9B00DBD8` |
| all-one | `FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF` | `FFFFFFFF FFFFFFFF` | `408F276D 41C83B0E A20BC7C6 6D5451FD` |
| asymmetric | `01234567 89ABCDEF FEDCBA98 76543210` | `13579BDF 2468ACE0` | `F3B36D22 1C1759F9 AD23A12E 11413B1C` |
| lane c0 | `00000001 00000000 00000000 00000000` | `00000000 00000000` | `F8E4CCA4 5CB200DB B1A574EB 097EFF67` |
| lane c1 | `00000000 00000001 00000000 00000000` | `00000000 00000000` | `6AD0C5EC EA236249 73A459F5 074944B3` |
| lane c2 | `00000000 00000000 00000001 00000000` | `00000000 00000000` | `844515E1 F08D6EAA 0F19C053 83F875F0` |
| lane c3 | `00000000 00000000 00000000 00000001` | `00000000 00000000` | `2DCE73E5 1348E23F FCF8E0EC A287AADB` |
| key k0 | `00000000 00000000 00000000 00000000` | `00000001 00000000` | `E3E80670 E50A0EBC 95F222C0 B615AA27` |
| key k1 | `00000000 00000000 00000000 00000000` | `00000000 00000001` | `FDDE3E0B FA7E58B6 3380EC46 D8D55C4F` |

For stream `0x0123456789ABCDEF` and subsequence
`0xFEDCBA9876543210`, the mapping oracle fixes these word results:

| Offset | Counter | Lane | Expected word |
| --- | --- | --- | --- |
| `0` | `00000000 00000000 76543210 FEDCBA98` | 0 | `AEF2ADF7` |
| `1` | `00000000 00000000 76543210 FEDCBA98` | 1 | `F69B5950` |
| `3` | `00000000 00000000 76543210 FEDCBA98` | 3 | `89B6573A` |
| `4` | `00000001 00000000 76543210 FEDCBA98` | 0 | `EC2AB39F` |
| `5` | `00000001 00000000 76543210 FEDCBA98` | 1 | `4671FD85` |
| `UINT64_MAX` | `FFFFFFFF 3FFFFFFF 76543210 FEDCBA98` | 3 | `4742B6C6` |

Transform tests compare exact floating object bits:

| Transform input | Expected object bits |
| --- | --- |
| float `00000000` | `00000000` |
| float `FFFFFFFF` | `3F7FFFFF` |
| float `80000000` | `3F000000` |
| float `12345678` | `3D91A2B0` |
| float `000001FF` | `33800000` |
| double `00000000 00000000` | `0000000000000000` |
| double `FFFFFFFF FFFFFFFF` | `3FEFFFFFFFFFFFFF` |
| double `80000000 00000000` | `3FE0000000000000` |
| double `12345678 9ABCDEF0` | `3FB23456789ABCD8` |
| double `00000000 FFFFFFFF` | `3DEFFFFF00000000` |

Random tests will also check public counter/key lane order, block purity and
`noexcept`, stream/key little-word order, subsequence/counter little-word
order, offset block and lane boundaries, and checked offset advance through
the last representable value without wrapping.

## Compile, architecture, and consumer plan

The compile layer will:

- compile each of the ten new public headers alone under strict C++20 and, on
  supported compilers, with exceptions disabled;
- compile representative expression nodes across multiple translation units;
- compile positive concept assertions and expected-failure negative cases;
- assert direct dependencies of utilities, expression, and random are exactly
  core, with expression remaining an interface target; and
- scan new headers and sources for sibling, storage, provider, implementation
  header, `detail`, `#pragma once`, and exception-policy violations.

Three isolated consumer programs will each use only its named installed target
and representative API. Lead-owned package fixtures must independently verify
build-tree, install-tree relocation with spaces, subproject use, static/shared
libraries, component closure to core, and absence of both sibling targets.

## Runtime and acceptance matrix

The local matrix requires Debug/static and Release/shared builds with warnings
as errors, focused ASan+UBSan runtime tests, all isolated consumers, formatting,
and whitespace checks. Hosted compiler and minimum-CMake rows remain CI
evidence unless locally available.

GPU evidence is **skipped** because the frozen milestone has no provider
target. No dedicated performance threshold is claimed; construction-side
allocation/read/dispatch counters provide the applicable performance sanity
evidence.

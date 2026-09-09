# Concrete owner decision: external Reference-LAPACK and derived metadata

Status: pending; implementation-plan authorization is not this approval.

The proposed decision is to approve the exact attribution/license addition
below for ASC’s source-derived inventory and contract metadata, while retaining
an externally supplied optional provider. No upstream algorithm source,
provider archive/shared library or Fortran runtime is proposed for bundling.

The complete review packet is retained outside source at
`../asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/owner-packet-01/`.
It contains both complete license texts, a concrete proposed
`THIRD_PARTY_NOTICES.proposed` preserving the existing notice, and `packet.json`
with the exact ASC material/generator hashes and alternatives.

| Material | Exact identity |
| --- | --- |
| Reference-LAPACK | 3.12.1, commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` |
| Source tree | `7217db728e4f7ee87dabf545a1a87a3d6cd30e9b` |
| Root LICENSE | SHA256 `978539245c405775bf347b1256c36df4daf8b69a64b099e94b1d53742201621e` |
| LAPACKE/LICENSE | SHA256 `871f415ebd2fd06c7ab8cc3c0016aa2d26b918622345116e2d0c740b688d4843` |
| Proposed complete notice | SHA256 `9ebeaf30a78e77f5fa4a4ba94ed66c68bdb92acd1dfdc8b1456e0c114ffbed41` |
| Packet manifest | SHA256 `6f5e8985d4c4d14c20c2cd44a5dabfbd28dc4720d92de2e52be0abb589f08fca` |

Authoritative complete texts: [root license](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/LICENSE)
and [LAPACKE license](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/LAPACKE/LICENSE).
The root license has an additional intellectual-property non-assurance clause;
the lock deliberately retains `LicenseRef-LAPACK-3.12.1-Root` rather than
silently relabeling it. The source checkout is pristine and both license hashes
were rechecked when assembling this packet. No local upstream patch exists.

ASC’s independently authored adapters/native algorithms are distinct from
source-derived declarations/argument documentation in
`docs/contracts/lapack-upstream-inventory.json` and the coverage/contracts that
consume those records. The packet binds these materials and their generator.
This review acknowledges existing draft development records; it does not
retroactively fabricate approval. ASC installation continues to distribute its
own libraries, headers and package metadata. Prepared LAPACK/LAPACKE/BLAS
prefixes and GCC runtimes remain external dependencies.

Decision requested: approve this exact notice and source-derived-metadata
inventory under the external-provider distribution model, or identify the
specific required amendment. Withholding approval keeps redistribution review
open while native, I/O, adapter and external-dependency development proceeds.
A future bundled provider, XBLAS import, upstream patch or dependency version
change requires its own concrete materials and review. This packet makes no
legal conclusion about those future distributions.

The approval requirement comes from
[ADR 0017](../../docs/architecture/decisions/0017-third-party-provenance.md):
“Direct source/data adaptation requires ... reviewer approval,” and the
[provenance review](../../docs/provenance/mdecpp-review.md): “The lead/owner must
approve a new notice inventory when a concrete dependency or derived source is
accepted.” The proposed notice has not been applied to the product.

# Milestone 3 Dense CPU provenance record

Status: Frozen before implementation

Date: 2026-07-28

Milestone 3 is original Apache-2.0 project work derived from the frozen
architecture, ADRs 0007--0011 and 0013, and ordinary mathematical definitions
of dense layouts, reductions, and BLAS-like operations.

No third-party source, generated data, table, literal vector corpus, or new
dependency is approved. MdeCpp, the deleted asc-cpp implementation/tests, and
the completed hardening branch's later implementation/tests are prohibited
production and verification inputs. Independent numerical expectations must
be calculated from the frozen mathematical contract rather than copied.

The operation names `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm`
describe conventional mathematical operations and do not authorize copying a
BLAS implementation. No third-party notice is added by this milestone.


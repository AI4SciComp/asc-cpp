# Milestone 6 Provenance Record

Status: Final clean-room and redistribution audit passed

Date: 2026-07-28

All Milestone 6 ASC source, tests, examples, benchmarks, and documentation are
project-owned work derived from the approved contract, current project APIs,
and official CMake, CUDA Runtime, and cuBLAS documentation.

No third-party source, test, vector, sample, benchmark framework, generated
code, or data is imported. The team does not inspect, copy, mechanically
translate, or differentially port MdeCpp/deleted asc-cpp CUDA wrappers,
provider implementation, GPU tests, literal outputs, macros, or prose.

CUDA Runtime and cuBLAS are optional system-supplied provider libraries, not
vendored artifacts. Provider discovery is not a redistribution or licensing
claim. Provider-neutral headers expose no NVIDIA SDK type.

Original asc-cpp work remains Apache-2.0. The final source/install audit
retains the repository `LICENSE` in the installed documentation and introduces
no vendored source, generated provider code, data file, bundled toolkit
binary, or additional license. Build-tree and relocated-install consumers use
the system CUDA Runtime and cuBLAS installations.

The production, verification, documentation/API, and portability roles each
confirmed the clean-room boundary in their assigned work. The final file and
dependency inventories contain no MdeCpp path, copied historical asc-cpp CUDA
path, third-party sample, or unapproved artifact.

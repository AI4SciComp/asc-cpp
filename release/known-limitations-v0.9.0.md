# ASCCpp 0.9.0 known limitations

- CPU Dense BLAS and Sparse BLAS are serial correctness-reference
  implementations, not optimized HPC providers.
- CUDA components are experimental and excluded from the supported matrix
  without exact-commit hosted real-NVIDIA evidence.
- Binary compatibility is promised only within compatible `0.9.x` patch
  releases; there is no cross-minor `0.x` ABI promise.
- Publication is blocked until `ASCCMake 0.1.0` is publicly and immutably
  obtainable at the recorded commit/checksum.
- Publication is also blocked until private vulnerability reporting and
  branch/tag protection are configured and verified on GitHub.
- Platform, compiler, sanitizer, and artifact claims are limited to jobs that
  pass on the exact release commit; Gate B local results are not a substitute
  for hosted release evidence.

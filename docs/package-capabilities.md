# ASCCpp 0.9.0 package capabilities

ASCCpp exports isolated CMake components. Consumers request only the component
closure they use; a provider-free request does not load CUDA metadata or locate
the CUDA toolkit.

## Provider-free components

| Component | Imported target | Direct ASCCpp dependencies |
| --- | --- | --- |
| `core` | `ASC::core` | none |
| `utilities` | `ASC::utilities` | `core` |
| `expression` | `ASC::expression` | `core` |
| `dense` | `ASC::dense` | `core`, `expression` |
| `sparse` | `ASC::sparse` | `core`, `expression` |
| `random` | `ASC::random` | `core` |
| `random_dense` | `ASC::random_dense` | `random`, `dense` |
| `random_sparse` | `ASC::random_sparse` | `random`, `sparse` |
| `cpp` | `ASC::cpp` | all provider-free components |

## Experimental CUDA components

`core_cuda`, `dense_cuda`, `sparse_cuda`, `random_cuda`,
`random_dense_cuda`, and `random_sparse_cuda` export matching `ASC::*` targets
only when ASCCpp was built with `ASC_CPP_ENABLE_CUDA=ON`. Required CUDA
components locate CUDA Toolkit 12; optional unavailable CUDA components set
their component `_FOUND` result without contaminating provider-free requests.

## Discovery

```cmake
find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS dense random)
target_link_libraries(app PRIVATE ASC::dense ASC::random)
```

With no component list, `cpp` is required. Unknown, unavailable, required,
optional, and repeated component lookup are tested. Metadata exposes
`ASCCpp_VERSION`, `ASCCpp_KNOWN_COMPONENTS`, and
`ASCCpp_AVAILABLE_COMPONENTS`.

## Package modes

The verification suite consumes:

- original and copied build-tree packages;
- static and shared installs;
- prefixes and build directories containing spaces;
- relocated installs after the original prefix is removed;
- isolated component consumers with package registries disabled; and
- repeated `find_package` calls.

Installed metadata must not contain source/build paths. Installs include public
headers, libraries, CMake targets/config/version files, Apache-2.0 license,
third-party notices, and Joe--Kuo data/license. See
[downstream integration](downstream-integration.md).

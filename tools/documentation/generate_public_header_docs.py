#!/usr/bin/env python3
"""Insert release documentation comments from Doxygen discovery XML.

The tool is intentionally one-shot for the 0.9.0 documentation baseline. It
only inserts comments, processes locations bottom-up, and refuses to touch a
header that already contains its marker.
"""

from __future__ import annotations

import argparse
import collections
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path


MARKER = "Generated public contract documentation baseline for ASCCpp 0.9.0."
INTERNAL = re.compile(r"(^|::)internal[^:]*($|::)|_internal($|::)")


@dataclass
class Declaration:
    kind: str
    name: str
    qualified: str
    return_type: str = ""
    parameters: list[tuple[str, str]] = field(default_factory=list)
    template_parameters: list[str] = field(default_factory=list)
    compound: str = ""


COMPOUND_BRIEFS = {
    "AliasToken": "Identifies storage for conservative expression-alias checks.",
    "Buffer": "Owns one allocation obtained from a MemoryResource.",
    "ByteSink": "Receives a synchronous sequence of bytes.",
    "ByteSource": "Provides a synchronous sequence of bytes.",
    "CommandLineOption": "Describes one schema-backed command-line option.",
    "CommandLineParseResult": "Contains validated configuration and positional arguments.",
    "CommandLineParser": "Parses command-line arguments into validated configuration.",
    "CompletionEvent": "Owns a backend completion token for ordered asynchronous work.",
    "CompressedSparseArray": "Owns canonical CSR or CSC sparse storage.",
    "CompressedSparseView": "Views canonical non-owning CSR or CSC sparse storage.",
    "Configuration": "Stores a validated configuration and per-path metadata.",
    "ConfigurationMetadata": "Records origin, sensitivity, and deprecation metadata.",
    "ConfigurationOrigin": "Identifies how and where a configuration value was supplied.",
    "ConfigurationSchema": "Defines recursive configuration type and validation constraints.",
    "ConfigurationValue": "Stores one validated configuration value variant.",
    "ConstMemoryView": "Describes a non-owning immutable byte span and memory space.",
    "CoordinateArray": "Owns canonical sparse coordinate/value storage.",
    "CoordinateBuilder": "Builds and canonicalizes sparse coordinate storage transactionally.",
    "CoordinateView": "Views canonical non-owning sparse coordinate/value storage.",
    "CudaCoordinateArray": "Owns an experimental CUDA coordinate-array generation result.",
    "CudaCsrArray": "Owns experimental CUDA CSR storage and its completion state.",
    "CudaCsrClone": "Owns an experimental asynchronous CUDA CSR clone result.",
    "CudaDenseUniform01Generation": "Owns an experimental CUDA dense random generation result.",
    "CudaIndexedVectorArray": "Owns an experimental CUDA indexed sparse vector.",
    "CudaIndexedVectorClone": "Owns an experimental asynchronous CUDA indexed-vector clone.",
    "CudaMemoryResource": "Allocates CUDA device or pinned-host memory for one device.",
    "CudaRandomWordGeneration": "Owns an experimental CUDA random-word generation result.",
    "CudaSparseUniform01Generation": "Owns an experimental CUDA sparse random generation result.",
    "CudaStridedVectorView": "Describes an experimental CUDA-accessible strided vector.",
    "CudaTriangularCsrArray": "Owns an experimental CUDA triangular CSR descriptor and storage.",
    "CudaTriangularCsrClone": "Owns an asynchronous CUDA triangular-CSR clone result.",
    "DenseArray": "Owns contiguous dense storage with validated extents and layout.",
    "DenseBlasBandMatrixView": "Views a BLAS general band-matrix encoding.",
    "DenseBlasMatrixView": "Views a BLAS matrix with explicit layout and leading dimension.",
    "DenseBlasPackedMatrixView": "Views a BLAS packed triangular/symmetric matrix encoding.",
    "DenseBlasTriangularBandView": "Views a BLAS triangular band-matrix encoding.",
    "DenseBlasVectorView": "Views a BLAS vector with a signed increment and backing span.",
    "DenseCudaContext": "Owns experimental cuBLAS state bound to an execution context.",
    "DenseLayout": "Maps rank-fixed logical coordinates to dense storage offsets.",
    "DenseView": "Views non-owning dense storage with explicit layout and memory space.",
    "Device": "Identifies one execution device.",
    "ExecutionContext": "Describes backend execution, accessibility, and native ordering state.",
    "ExpressionAliasMetadata": "Describes expression identity and conservative byte extent.",
    "Extents": "Stores validated rank-fixed static and dynamic extents.",
    "File": "Owns a synchronous readable or writable operating-system file handle.",
    "Generator": "Constrains stateful generators that return Result values.",
    "HostMemoryResource": "Allocates aligned host memory.",
    "LayoutStride": "Selects explicit-stride dense layout construction.",
    "MemoryResource": "Defines allocation, deallocation, and memory-space behavior.",
    "MutableMemoryView": "Describes a non-owning mutable byte span and memory space.",
    "NormalDistribution": "Transforms engine words into normal real samples.",
    "Pcg32": "Implements the versioned deterministic PCG32 engine.",
    "Pcg32State": "Stores a serializable PCG32 state and sequence version.",
    "Result": "Stores either one value or one non-OK Status.",
    "SobolSequence": "Advances a bounded-dimension deterministic Sobol sequence.",
    "SparseBlasIndexedVectorView": "Views canonical indexed sparse-vector storage.",
    "SparseBlasMatrixView": "Views a CSR matrix for Sparse BLAS operations.",
    "SparseBlasTriangularView": "Views a triangular CSR matrix and triangle/diagonal policy.",
    "SparseBlasVectorView": "Views a dense vector operand used by Sparse BLAS.",
    "SparseCudaContext": "Owns experimental cuSPARSE state bound to an execution context.",
    "SparseRandomStructureCandidate": "Stores a ranked candidate for sparse structure selection.",
    "SparseUniform01Generation": "Owns a generated sparse array and next deterministic offsets.",
    "SplitMix64": "Implements the versioned deterministic SplitMix64 engine.",
    "SplitMix64State": "Stores a serializable SplitMix64 state and sequence version.",
    "Status": "Carries a stable error category and optional diagnostic/provider details.",
    "Timer": "Accumulates monotonic wall-clock samples without internal synchronization.",
    "UniformIntegerDistribution": "Maps engine output uniformly to a validated integer interval.",
    "UniformRealDistribution": "Maps engine output uniformly to a validated half-open real interval.",
    "WritableExpressionAdapter": "Customizes writable shape, alias, access, and mutation behavior.",
    "ExpressionPlacementAdapter": "Customizes expression memory placement and alias metadata.",
    "ExpressionAdapter": "Customizes expression value, shape, access, and alias semantics.",
    "Xoroshiro128Plus": "Implements the versioned deterministic xoroshiro128+ engine.",
    "Xoroshiro128PlusState": "Stores a serializable xoroshiro128+ state and sequence version.",
    "Xoroshiro64Star": "Implements the versioned deterministic xoroshiro64* engine.",
    "Xoroshiro64StarState": "Stores a serializable xoroshiro64* state and sequence version.",
}

ENUM_BRIEFS = {
    "Backend": "Selects the execution backend.",
    "ConfigurationOriginKind": "Identifies configuration precedence provenance.",
    "ConfigurationValueType": "Identifies the active configuration value type.",
    "DenseBlasDiagonal": "Selects unit or explicit triangular diagonal handling.",
    "DenseBlasLayout": "Selects row-major or column-major BLAS storage.",
    "DenseBlasSide": "Selects the side on which a matrix operand is applied.",
    "DenseBlasTranspose": "Selects no-transpose, transpose, or conjugate-transpose behavior.",
    "DenseBlasTriangle": "Selects the stored matrix triangle.",
    "DuplicatePolicy": "Selects duplicate-coordinate rejection or summation.",
    "ErrorCode": "Defines stable machine-readable failure categories.",
    "ExplicitZeroPolicy": "Selects whether canonicalization retains explicit zeros.",
    "MemorySpace": "Identifies host, pinned-host, or device memory placement.",
    "SparseCompressedFormat": "Selects CSR or CSC compressed storage.",
    "TimerState": "Identifies whether a timer is stopped or running.",
}


def flatten(element: ET.Element | None) -> str:
    return "" if element is None else "".join(element.itertext()).strip()


def has_docs(element: ET.Element) -> bool:
    return bool(flatten(element.find("briefdescription")) or
                flatten(element.find("detaileddescription")))


def module_for(path: str) -> tuple[str, str]:
    if "/providers/" in path:
        return "asc_cuda", "CUDA provider"
    if "/core" in path:
        return "asc_core", "Core"
    if "/utilities" in path:
        return "asc_utilities", "Utilities"
    if "/expression" in path:
        return "asc_expression", "Expression"
    if "/dense/blas" in path:
        return "asc_dense_blas", "Dense BLAS"
    if "/dense" in path:
        return "asc_dense", "Dense"
    if "/sparse/blas" in path:
        return "asc_sparse_blas", "Sparse BLAS"
    if "/sparse" in path:
        return "asc_sparse", "Sparse"
    if "/random/engine" in path or "/random/seed" in path:
        return "asc_random_engines", "Random engine"
    if "/random/distribution" in path or "/random/generator" in path:
        return "asc_random_distributions", "Random distribution"
    if "/random/quasi" in path:
        return "asc_random_qmc", "quasi-random"
    if "/random/dense" in path or "/random/sparse" in path:
        return "asc_random_storage", "Random storage"
    return "asc_random", "Random"


def short_name(qualified: str) -> str:
    name = qualified.rsplit("::", 1)[-1]
    return name.split("<", 1)[0]


def brief_for(decl: Declaration, module: str) -> str:
    name = short_name(decl.qualified or decl.name)
    if decl.kind in {"class", "struct", "concept"}:
        return COMPOUND_BRIEFS.get(name, f"Defines the public {name} {decl.kind} contract.")
    if decl.kind == "enum":
        return ENUM_BRIEFS.get(name, f"Selects the public {name} policy.")
    if decl.kind == "typedef":
        return f"Defines the public {name} type used by this {module} contract."
    if decl.kind == "variable":
        words = name.replace("k", "", 1) if name.startswith("k") else name
        return f"Stores the {words.replace('_', ' ')} value for this contract."
    if decl.kind == "define":
        return f"Controls the public {name} declaration or contract behavior."

    lower = name.lower()
    if name.startswith("~"):
        return "Releases owned resources after required completion/lifetime conditions."
    if name == short_name(decl.compound):
        return f"Constructs a {name} with the documented ownership and validity state."
    if name.startswith("operator="):
        return "Replaces this object's state while preserving ownership invariants."
    if name == "operator()":
        return "Produces the next deterministic value according to the object's contract."
    if name.startswith("Create") or name.startswith("From"):
        return f"Validates inputs and creates the requested {module} object."
    if name.startswith("Convert"):
        return "Converts storage while preserving logical values and canonical invariants."
    if name.startswith("Validate"):
        return "Validates the documented shape, access, ownership, and provider contract."
    if name.startswith("Fill") or name.startswith("Generate"):
        return f"Generates deterministic {module} values into the requested destination."
    if name.startswith("Cuda"):
        return f"Enqueues the experimental CUDA {name[4:]} operation and returns completion."
    if lower.startswith(("set", "add")):
        return f"Updates {module} state after validating the requested value."
    if lower.startswith(("is", "has", "may", "can")) or decl.return_type == "bool":
        return f"Reports whether the documented {name} condition holds."
    if lower in {"data", "size", "space", "shape", "extents", "rank", "rows", "columns", "nnz", "values", "coordinates", "offset", "state", "status", "code", "message", "provider", "native_code", "type", "format", "alignment", "identity"} or lower.endswith(("_count", "_offset", "_minimum", "_maximum")):
        return f"Returns the object's {name.replace('_', ' ')} contract value."
    if any(token in lower for token in ("gemm", "gemv", "dot", "axpy", "swap", "copy", "scal", "nrm", "asum", "rot", "trm", "sym", "her", "ger", "spr", "syr")):
        return f"Computes the {name} operation defined by the {module} numerical contract."
    if name in {"ReadSome", "ReadExact", "ReadTextFile"}:
        return "Reads bytes synchronously with explicit end-of-file and I/O failures."
    if name in {"WriteSome", "WriteAll", "WriteTextFile"}:
        return "Writes bytes synchronously with explicit progress and I/O failures."
    if name in {"Start", "Stop", "Reset", "Wait", "Flush", "Close"}:
        return f"Performs the {name.lower()} state transition defined by this {module} object."
    return f"Performs the public {name} operation defined by the {module} contract."


def parameter_description(name: str) -> tuple[str, str]:
    normalized = name.lstrip("_")
    direction = "in"
    if normalized in {"output", "destination", "result", "workspace"}:
        direction = "in,out" if normalized == "workspace" else "out"
    descriptions = {
        "context": "Execution backend and accessibility/order contract.",
        "resource": "Allocator that must outlive storage allocated from it.",
        "destination_resource": "Allocator for the returned owning destination.",
        "source": "Input source, valid and accessible for the operation.",
        "input": "Input operand, valid and accessible for the operation.",
        "output": "Output operand mutated only as documented by the operation.",
        "destination": "Destination storage with the required size and accessibility.",
        "workspace": "Caller-owned workspace with the documented capacity and lifetime.",
        "alpha": "Scaling factor applied to the primary operation.",
        "beta": "Scaling factor applied to the prior output value.",
        "shape": "Logical extents; every extent must satisfy the documented bounds.",
        "extents": "Logical extents; every extent must satisfy the documented bounds.",
        "coordinate": "Logical coordinate within every corresponding extent.",
        "coordinates": "Canonical coordinate storage in logical ordering.",
        "values": "Value storage paired with the documented descriptor.",
        "value": "Value read or written by the operation.",
        "memory_space": "Placement of every referenced storage byte.",
        "operation": "Requested transpose/conjugation or matrix operation mode.",
        "transpose": "Requested transpose/conjugation mode.",
        "stream": "CUDA stream whose ordering and lifetime are caller controlled.",
        "seed": "Deterministic seed value.",
        "subsequence": "Deterministic independent subsequence identifier.",
        "offset": "Deterministic address offset within the selected sequence.",
        "lower": "Inclusive lower distribution bound.",
        "upper": "Exclusive upper distribution bound unless documented otherwise.",
        "path": "Filesystem path interpreted by the synchronous I/O operation.",
        "bytes": "Requested byte count.",
        "alignment": "Power-of-two allocation alignment.",
    }
    description = descriptions.get(
        normalized,
        f"The {normalized.replace('_', ' ')} value required by this contract.",
    )
    return direction, description


def details_for(group: str, module: str) -> list[str]:
    if group == "asc_cuda":
        return [
            "This API is experimental in 0.9.0. CUDA storage, context, stream,",
            "event, and workspace owners must remain alive until returned completion",
            "has been ordered or waited. Enqueue success does not imply completion.",
        ]
    if group in {"asc_dense_blas", "asc_sparse_blas"}:
        return [
            f"The exact layout, stride, aliasing, precision, and failure semantics are",
            f"defined by the {module} module contract. Provider-free CPU execution is",
            "synchronous and is a correctness reference, not a performance claim.",
        ]
    if group.startswith("asc_random"):
        return [
            "Reproducibility is defined by the documented engine, distribution,",
            "seed/subsequence/offset address mapping, and sequence-version boundary.",
        ]
    return [
        f"Ownership, lifetime, failure, memory-placement, aliasing, and concurrency",
        f"semantics follow the public {module} module contract.",
    ]


def render(declarations: list[Declaration], group: str, module: str) -> list[str]:
    primary = declarations[0]
    lines = ["/**", f" * @brief {brief_for(primary, module)}", " *"]
    for detail in details_for(group, module):
        lines.append(f" * {detail}")
    template_parameters: list[str] = []
    parameters: list[tuple[str, str]] = []
    return_type = ""
    for declaration in declarations:
        for parameter in declaration.template_parameters:
            if parameter and parameter not in template_parameters:
                template_parameters.append(parameter)
        for parameter in declaration.parameters:
            if parameter[0] and parameter not in parameters:
                parameters.append(parameter)
        return_type = return_type or declaration.return_type
    if template_parameters or parameters or return_type:
        lines.append(" *")
    for name in template_parameters:
        lines.append(
            f" * @tparam {name} Type or non-type argument satisfying the declaration's constraints."
        )
    for name, _type in parameters:
        direction, description = parameter_description(name)
        lines.append(f" * @param[{direction}] {name} {description}")
    function_kinds = {declaration.kind for declaration in declarations}
    function_name = primary.name
    if "function" in function_kinds and return_type and not function_name.startswith("~"):
        if "Result" in return_type:
            result = "The value on success, or a non-OK Status describing validation, access, allocation, provider, or numerical failure."
        elif "Status" in return_type:
            result = "OK on success; otherwise a stable failure category with optional diagnostics."
        elif return_type != "void":
            result = "The documented value; references and views do not extend owner lifetime."
        else:
            result = ""
        if result:
            lines.append(f" * @return {result}")
    lines.append(" * @ingroup " + group)
    lines.append(" */")
    return lines


def declaration_start(lines: list[str], line_number: int) -> int:
    index = max(0, line_number - 1)
    while index > 0:
        previous = lines[index - 1].strip()
        if not previous:
            break
        if previous.startswith(("#", "//", "/*", "*", "public:", "private:", "protected:")):
            break
        if previous.endswith((";", "{", "}", ":")):
            break
        index -= 1
    return index


def file_header(group: str, module: str) -> list[str]:
    return [
        "/**",
        " * @file",
        f" * @brief Public {module} declarations for ASCCpp 0.9.0.",
        " *",
        f" * {MARKER}",
        " * Every declaration below is governed by the module, ownership, failure,",
        " * memory-placement, numerical, concurrency, and package contracts linked",
        " * from the generated API reference.",
        f" * @ingroup {group}",
        " */",
        "",
    ]


def humanize_enum_value(name: str) -> str:
    words = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name.removeprefix("k"))
    explicit = {
        "Ok": "Successful operation with no failure.",
        "None": "No transformation or optional behavior.",
        "Serial": "Synchronous provider-free CPU execution.",
        "Cuda": "CUDA execution; experimental in ASCCpp 0.9.0.",
        "Hip": "HIP execution identifier; no 0.9.0 provider is implemented.",
        "Sycl": "SYCL execution identifier; no 0.9.0 provider is implemented.",
        "Host": "Ordinary host-accessible memory.",
        "Pinned Host": "Page-locked host memory suitable for CUDA transfers.",
        "Device": "Device-resident memory.",
        "Managed": "Managed memory; no provider-free allocation promise.",
        "Column Major": "Column-major matrix storage.",
        "Row Major": "Row-major matrix storage.",
        "Upper": "Upper triangular storage or operation.",
        "Lower": "Lower triangular storage or operation.",
        "Unit": "Implicit unit diagonal; stored diagonal values are ignored.",
        "Non Unit": "Explicit diagonal values are read.",
        "Transpose": "Transpose without conjugation.",
        "Conjugate Transpose": "Transpose with complex conjugation.",
        "Csr": "Compressed sparse row storage.",
        "Csc": "Compressed sparse column storage.",
        "Reject": "Reject duplicates or conflicting input.",
        "Sum": "Combine duplicate values by addition.",
        "Keep": "Retain explicit zero entries.",
        "Drop": "Remove explicit zero entries during canonicalization.",
    }
    return explicit.get(words, f"Selects {words.lower()} behavior.")


def document_enum_values(source: Path) -> int:
    changed = 0
    enum_start = re.compile(r"\benum\s+class\s+([A-Za-z_][A-Za-z0-9_]*)")
    value_line = re.compile(
        r"^(?P<indent>\s*)(?P<name>k[A-Za-z0-9_]+)(?P<rest>\s*(?:=\s*[^,]+)?\s*,?)\s*$"
    )
    for header in sorted((source / "include" / "asc").rglob("*.h")):
        lines = header.read_text(encoding="utf-8").splitlines()
        in_enum = False
        for index, line in enumerate(lines):
            if not in_enum and enum_start.search(line):
                in_enum = True
                continue
            if not in_enum:
                continue
            if "};" in line:
                in_enum = False
                continue
            match = value_line.match(line)
            if match and "///<" not in line:
                description = humanize_enum_value(match.group("name"))
                lines[index] = (
                    f"{match.group('indent')}{match.group('name')}"
                    f"{match.group('rest')}  ///< {description}"
                )
                changed += 1
        header.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return changed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--xml", type=Path, required=True)
    parser.add_argument("--enum-values-only", action="store_true")
    args = parser.parse_args()
    source = args.source.resolve()

    if args.enum_values_only:
        print(f"documented {document_enum_values(source)} enum values")
        return

    by_location: dict[tuple[str, int], list[Declaration]] = collections.defaultdict(list)
    for xml_path in args.xml.glob("*.xml"):
        if xml_path.name == "index.xml":
            continue
        compound = ET.parse(xml_path).getroot().find("compounddef")
        if compound is None:
            continue
        compound_name = flatten(compound.find("compoundname"))
        compound_kind = compound.get("kind", "")
        compound_location = compound.find("location")
        if (
            compound_kind in {"class", "struct", "union", "concept"}
            and compound.get("prot", "public") == "public"
            and compound_location is not None
            and not INTERNAL.search(compound_name)
            and not has_docs(compound)
        ):
            file_name = compound_location.get("file", "")
            if file_name.startswith("include/asc/"):
                by_location[(file_name, int(compound_location.get("line", "1")))].append(
                    Declaration(compound_kind, short_name(compound_name), compound_name)
                )

        for member in compound.findall(".//memberdef"):
            if member.get("prot", "public") != "public" or has_docs(member):
                continue
            qualified = flatten(member.find("qualifiedname")) or flatten(member.find("name"))
            if INTERNAL.search(qualified):
                continue
            location = member.find("location")
            if location is None:
                continue
            file_name = location.get("declfile", "") or location.get("file", "")
            line_text = location.get("declline", "") or location.get("line", "")
            if not file_name.startswith("include/asc/") or not line_text:
                continue
            params = []
            for param in member.findall("./param"):
                params.append((flatten(param.find("declname")), flatten(param.find("type"))))
            template_params = []
            for param in member.findall("./templateparamlist/param"):
                template_params.append(
                    flatten(param.find("declname")) or flatten(param.find("defname"))
                )
            by_location[(file_name, int(line_text))].append(
                Declaration(
                    member.get("kind", "member"),
                    flatten(member.find("name")),
                    qualified,
                    flatten(member.find("type")),
                    params,
                    template_params,
                    compound_name,
                )
            )

    headers = sorted((source / "include" / "asc").rglob("*.h"))
    inserted = 0
    for header in headers:
        relative = header.relative_to(source).as_posix()
        original = header.read_text(encoding="utf-8")
        if MARKER in original:
            raise SystemExit(f"refusing to regenerate marked header: {relative}")
        lines = original.splitlines()
        group, module = module_for(relative)
        locations = [
            (line, declarations)
            for (path, line), declarations in by_location.items()
            if path == relative
        ]
        for line_number, declarations in sorted(locations, reverse=True):
            start = declaration_start(lines, line_number)
            comment = render(declarations, group, module)
            indent = re.match(r"\s*", lines[start]).group(0)
            lines[start:start] = [indent + item if item else "" for item in comment]
            inserted += 1
        guard_index = next(
            (index + 1 for index, line in enumerate(lines[:10]) if line.startswith("#define ")),
            0,
        )
        lines[guard_index:guard_index] = [""] + file_header(group, module)
        header.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"documented {len(headers)} headers at {inserted} declaration locations")


if __name__ == "__main__":
    main()

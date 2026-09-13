#!/usr/bin/env python3
"""Prepare independent ASC text fixtures for the maintained file-close tests.

Writes only to an explicit fresh scratch directory. The 60 cases retain the
stabilization matrix: 12 scalar codes times Dense left/right, COO, CSR and CSC.
These are rank-two nonempty cleanup tests, not complete format acceptance.
"""

import argparse
import json
import pathlib

_SCALARS = (
    ("i8", "std::int8_t"),
    ("u8", "std::uint8_t"),
    ("i16", "std::int16_t"),
    ("u16", "std::uint16_t"),
    ("i32", "std::int32_t"),
    ("u32", "std::uint32_t"),
    ("i64", "std::int64_t"),
    ("u64", "std::uint64_t"),
    ("f32", "float"),
    ("f64", "double"),
    ("c64", "std::complex<float>"),
    ("c128", "std::complex<double>"),
)
_STORAGE = ("dense-left", "dense-right", "coo", "csr", "csc")
_OWNERS = {
    "dense": "asc::DenseArray<Element, Shape>",
    "coo": "asc::CoordinateArray<Element, Shape>",
    "csr": "asc::CsrArray<Element>",
    "csc": "asc::CscArray<Element>",
}


def _fixture(scalar: str, kind: str) -> str:
    values = ["2", "-0", "5", "-7"]
    if scalar.startswith("u"):
        values = ["2", "0", "5", "7"]
    elif scalar.startswith("c"):
        values = ["(2,1)", "(-0,-0)", "(5,-2)", "(-7,3)"]
    if kind == "dense":
        structure = "data\n"
    else:
        values = [values[0], values[1], values[3]]
        if kind == "coo":
            structure = "coordinates\n(0,0)\n(0,1)\n(1,1)\nvalues\n"
        elif kind == "csr":
            structure = "offsets\n0\n2\n3\nindices\n0\n1\n1\nvalues\n"
        else:
            structure = "offsets\n0\n1\n3\nindices\n0\n0\n1\nvalues\n"
    order = "dim0" if kind == "dense" else kind
    return (
        f"ASCARRAY 1\nkind {kind}\nscalar {scalar}\nrank 2\nshape 2 2\n"
        f"order {order}\ncount {len(values)}\n{structure}"
        + "\n".join(values)
        + "\nend\n"
    )


def prepare(destination: pathlib.Path) -> None:
    """Create all 60 case directories, rejecting an existing destination."""
    destination.mkdir(parents=True, exist_ok=False)
    names = []
    for scalar, cpp_type in _SCALARS:
        for storage in _STORAGE:
            name = f"{scalar}-{storage}"
            case = destination / name
            case.mkdir()
            kind = "dense" if storage.startswith("dense") else storage
            frame = _fixture(scalar, kind)
            literals = "\n".join(
                "    " + json.dumps(line + "\n") for line in frame.splitlines()
            )
            includes = ""
            if scalar.startswith(("i", "u")):
                includes += "#include <cstdint>\n"
            elif scalar.startswith("c"):
                includes += "#include <complex>\n"
            includes += "#include <string_view>\n\n"
            if kind in ("dense", "coo"):
                includes += '#include "asc/core/extents.h"\n'
            if kind == "dense":
                includes += (
                    '#include "asc/dense/array.h"\n'
                    '#include "asc/dense/layout.h"\n'
                )
            else:
                header = "coordinate" if kind == "coo" else "compressed"
                includes += f'#include "asc/sparse/{header}.h"\n'
            declarations = (
                (
                    "using Shape = asc::Extents<2, 2>;\n"
                    if kind in ("dense", "coo")
                    else ""
                )
                + f"using Element = {cpp_type};\n"
                f"using Owner = {_OWNERS[kind]};\n"
            )
            if kind == "dense":
                layout = "Right" if storage == "dense-right" else "Left"
                declarations += f"using Layout = asc::Layout{layout};\n"
            (case / "fixture.h").write_text(
                "#ifndef ASC_TESTS_FILE_CLOSE_FIXTURE_H_\n"
                "#define ASC_TESTS_FILE_CLOSE_FIXTURE_H_\n"
                + includes
                + "namespace asc_file_close_test {\n"
                + declarations
                + "constexpr std::string_view kFixture =\n"
                + literals
                + ";\n}  // namespace asc_file_close_test\n"
                "#endif  // ASC_TESTS_FILE_CLOSE_FIXTURE_H_\n",
                encoding="utf-8",
            )
            (case / "independent.asc").write_text(frame, encoding="ascii")
            names.append(name)
    (destination / "cases.cmake").write_text(
        "set(ASC_FILE_CLOSE_CASES " + " ".join(names) + ")\n",
        encoding="utf-8",
    )
    print(f"Prepared {len(names)} independent scalar/storage fixtures")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    prepare(arguments.output_dir.resolve())


if __name__ == "__main__":
    main()

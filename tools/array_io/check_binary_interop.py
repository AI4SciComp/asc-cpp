#!/usr/bin/env python3
"""Check both independent binary directions through an installed ASC consumer.

The original 60 profiles are shared by both directions, not 120 datasets.
Twenty additional real/complex profiles exercise quiet-NaN component payloads,
infinities and signed zeros. No signaling-NaN or text payload claim is made.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import pathlib
import subprocess

import prepare_file_close_cases
import reference_codec


def _special_values(frame: reference_codec.Frame) -> tuple[bytes, ...]:
    width = 4 if frame.scalar in ("f32", "c64") else 8
    components = 2 if frame.scalar.startswith("c") else 1
    patterns = (
        (0x7FC12345, 0x80000000, 0x7F800000, 0xFF800000,
         0xFFC54321, 0, 0x80000000, 0x7FC23456)
        if width == 4 else
        (0x7FF8123456789ABC, 0x8000000000000000,
         0x7FF0000000000000, 0xFFF0000000000000,
         0xFFF8ABCDEF012345, 0, 0x8000000000000000, 0x7FF823456789ABCD)
    )
    values = tuple(
        b"".join(patterns[i * components + j].to_bytes(width, "little")
                 for j in range(components))
        for i in range(4)
    )
    return values if frame.kind == "dense" else (values[0], values[1], values[3])


def _prepare(
    root: pathlib.Path, component: str
) -> dict[str, reference_codec.Frame]:
    fixtures = root / "fixtures"
    prepare_file_close_cases.prepare(fixtures)
    cases = {}
    for case in sorted(fixtures.iterdir()):
        is_dense = "dense" in case.name
        if not case.is_dir() or is_dense != (component == "dense"):
            continue
        text = (case / "independent.asc").read_bytes()
        frame = reference_codec.decode_text(text)
        cases[case.name] = frame
        if frame.scalar.startswith(("f", "c")):
            special = fixtures / (case.name + "-bits")
            special.mkdir()
            (special / "independent.asc").write_bytes(text)
            cases[special.name] = dataclasses.replace(
                frame, values=_special_values(frame)
            )
    for name, frame in cases.items():
        case = fixtures / name
        wire = reference_codec.encode_binary(frame)
        (case / "independent.ascb").write_bytes(wire)
        (case / "values.bits").write_bytes(b"".join(frame.values))
        corrupted = bytearray(wire)
        corrupted[-1] ^= 1
        for suffix, data in (("corrupt", corrupted), ("truncated", wire[:-1]),
                             ("trailing", wire + b"\x00")):
            (case / (suffix + ".ascb")).write_bytes(data)
    expected = 32 if component == "dense" else 48
    if len(cases) != expected:
        raise ValueError(f"Expected {expected} profiles, found {len(cases)}")
    return cases


def _observed(path: pathlib.Path, expected: reference_codec.Frame) -> None:
    observed = json.loads(path.read_text(encoding="ascii"))
    wanted = {"shape": list(expected.shape), "structure": list(expected.structure),
              "values": [value.hex() for value in expected.values]}
    if observed != wanted:
        raise ValueError(f"Public view changed shape/structure/value bits: {path}")


def check(executable: pathlib.Path, component: str, root: pathlib.Path) -> None:
    """Create fresh fixtures, execute real public APIs and check both directions."""
    root.mkdir(parents=True, exist_ok=False)
    cases = _prepare(root, component)
    output = root / "output"
    output.mkdir()
    subprocess.run(
        [str(executable), str(root / "fixtures"), str(output)], check=True
    )
    for name, expected in cases.items():
        wire = (output / (name + ".ascb")).read_bytes()
        if wire != reference_codec.encode_binary(expected):
            raise ValueError(f"ASC writer disagrees with independent bytes: {name}")
        if reference_codec.decode_binary(wire) != expected:
            raise ValueError(f"Independent decoder disagrees with frame: {name}")
        _observed(output / (name + ".json"), expected)
    print(
        f"{component}: {len(cases)} profiles passed in both directions; "
        "three malformed inputs per profile rejected with storage unchanged"
    )


def main() -> None:
    """Check one installed storage component in an explicit scratch directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=pathlib.Path, required=True)
    parser.add_argument("--component", choices=("dense", "sparse"), required=True)
    parser.add_argument("--work-dir", type=pathlib.Path, required=True)
    args = parser.parse_args()
    check(args.executable.resolve(), args.component, args.work_dir.resolve())


if __name__ == "__main__":
    main()

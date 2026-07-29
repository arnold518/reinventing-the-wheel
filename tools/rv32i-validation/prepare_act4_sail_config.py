#!/usr/bin/env python3
"""Narrow ACT4's pinned RV32 Sail template to CircuitSim's RV32I map."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

EXPECTED_TEMPLATE_SHA256 = (
    "bf53bf534947491057f1faacb972c8391db8ca4c807f90c3e31744776db7fa99"
)

DISABLED_EXTENSIONS = (
    "M",
    "A",
    "F",
    "D",
    "Zicntr",
    "Zicsr",
    "Zifencei",
    "Zihpm",
    "Zmmul",
    "Zaamo",
    "Zalrsc",
    "Zca",
    "Zcf",
    "Zcd",
)

RAM_REGIONS = """    "regions": [
      {
        "base": {"len": 64, "value": "0x0"},
        "size": {"len": 64, "value": "0x40000"},
        "attributes": {
          "mem_type": "MainMemory",
          "cacheable": true,
          "coherent": true,
          "executable": true,
          "readable": true,
          "writable": true,
          "read_idempotent": true,
          "write_idempotent": true,
          "misaligned_exceptions": {
            "load_store": {"Some": "AlignmentException"},
            "vector": {"Some": "AlignmentException"},
            "amo": "AccessFault"
          },
          "atomic_support": "AMONone",
          "misaligned_atomicity_granule_size_exp": 0,
          "vector_misaligned_atomicity_granule_size_exp": 0,
          "reservability": "RsrvNone",
          "supports_cbo_zero": false,
          "supports_pte_read": false,
          "supports_pte_write": false
        },
        "include_in_device_tree": true
      }
    ]"""


def disable_extension(text: str, name: str) -> str:
    pattern = re.compile(
        rf'("{re.escape(name)}"\s*:\s*\{{\s*'
        rf'"supported"\s*:\s*)true'
    )
    updated, count = pattern.subn(r"\1false", text)
    if count != 1:
        raise RuntimeError(
            f"Expected one enabled {name} entry in pinned Sail template, got {count}"
        )
    return updated


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = args.input.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != EXPECTED_TEMPLATE_SHA256:
        raise RuntimeError(
            "Pinned Sail template checksum changed: "
            f"expected {EXPECTED_TEMPLATE_SHA256}, got {digest}"
        )

    text = source.decode()
    for extension in DISABLED_EXTENSIONS:
        text = disable_extension(text, extension)

    text, count = re.subn(
        r'("fs_legal_states"\s*:\s*)"ExtContext_FourState"',
        r'\1"ExtContext_Off"',
        text,
    )
    if count != 1:
        raise RuntimeError(
            f"Expected one floating-point context setting, got {count}"
        )

    for device in ("clint", "simple_interrupt_generator"):
        pattern = re.compile(
            rf'("{device}"\s*:\s*\{{.*?'
            rf'"supported"\s*:\s*)true',
            re.DOTALL,
        )
        text, count = pattern.subn(r"\1false", text, count=1)
        if count != 1:
            raise RuntimeError(
                f"Expected one enabled {device}, got {count}"
            )

    region_pattern = re.compile(
        r'    "regions": \[.*?\n    \]\n  \},\n  "platform":',
        re.DOTALL,
    )
    text, count = region_pattern.subn(
        RAM_REGIONS + '\n  },\n  "platform":',
        text,
    )
    if count != 1:
        raise RuntimeError(
            f"Expected one memory-region list, got {count}"
        )

    dtb_pattern = re.compile(
        r'("dtb_address"\s*:\s*\{\s*'
        r'"len"\s*:\s*64,\s*'
        r'"value"\s*:\s*)"0x1000"'
    )
    text, count = dtb_pattern.subn(r'\1"0x3e000"', text)
    if count != 1:
        raise RuntimeError(
            f"Expected one DTB address, got {count}"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text)


if __name__ == "__main__":
    main()

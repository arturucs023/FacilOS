#!/usr/bin/env python3
"""Crea floppy 1.44MB: boot.bin + kernel.bin + padding."""
import pathlib, sys
base = pathlib.Path(__file__).parent.parent
boot = base / "build" / "boot.bin"
kern = base / "build" / "kernel.bin"
out = base / "build" / "floppy.img"
b = boot.read_bytes()
k = kern.read_bytes()
assert len(b) == 512, f"boot mide {len(b)}"
assert b[510]==0x55 and b[511]==0xAA, "sin firma 55AA"
assert len(k) < 63*512, f"kernel muy grande {len(k)}"
img = b + k
img += b"\x00" * (1474560 - len(img))
out.write_bytes(img)
print(f"OK floppy {len(img)} bytes, kernel {len(k)} bytes ({len(k)/512:.1f} sectores)")

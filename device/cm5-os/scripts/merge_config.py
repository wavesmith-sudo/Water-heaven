#!/usr/bin/env python3
"""Overlay a Buildroot config fragment onto a .config (later values win).

Usage: merge_config.py <buildroot>/.config <fragment>
Handles both `BR2_X=value` and `# BR2_X is not set` lines in the fragment.
"""
import sys
from pathlib import Path


def key_of(line):
    if line.startswith("BR2_") and "=" in line:
        return line.split("=", 1)[0]
    if line.startswith("# BR2_") and line.endswith(" is not set"):
        return line[2:-len(" is not set")]
    return None


config, fragment = map(Path, sys.argv[1:3])
new = [s.strip() for s in fragment.read_text().splitlines() if key_of(s.strip())]
keys = {key_of(s) for s in new}
lines = [s for s in config.read_text().splitlines() if key_of(s) not in keys]
config.write_text("\n".join(lines + new) + "\n")

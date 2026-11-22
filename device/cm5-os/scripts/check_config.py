#!/usr/bin/env python3
"""Report fragment symbols that Kconfig dropped or changed after `make olddefconfig`.

Usage: check_config.py <buildroot>/.config <fragment>
Exit 1 if any requested `=y`/string value is missing from the resolved .config. Kconfig
silently drops symbols whose dependencies are unmet, which is how the v0.3 fragment lost
EGLFS and Qt Quick without any error.
"""
import sys
from pathlib import Path


def parse(path):
    values = {}
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        if line.startswith("BR2_") and "=" in line:
            k, v = line.split("=", 1)
            values[k] = v
        elif line.startswith("# BR2_") and line.endswith(" is not set"):
            values[line[2:-len(" is not set")]] = "n"
    return values


def main():
    resolved, wanted = parse(sys.argv[1]), parse(sys.argv[2])
    bad = []
    for key, value in wanted.items():
        got = resolved.get(key, "n")
        # Strings containing variables are expanded differently; compare literally.
        if got != value:
            bad.append(f"{key}: wanted {value}, got {got}")
    for line in bad:
        print("DROPPED/CHANGED " + line)
    print(f"{len(wanted) - len(bad)}/{len(wanted)} fragment symbols applied")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

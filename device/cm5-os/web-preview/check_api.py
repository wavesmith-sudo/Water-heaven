#!/usr/bin/env python3
"""Fails if DemoJellyfinClient's QML-facing API drifts from JellyfinClient's, or if the
device QML imports a module that qml_imports.qml does not list.

The preview runs the device QML against the demo client, so a property or method the QML
uses must exist on both. Compares Q_PROPERTY declarations, Q_INVOKABLE method names and
signals between ../app/jellyfinclient.h and demojellyfin.h.
"""
import re
import sys
from pathlib import Path

here = Path(__file__).resolve().parent


def api(path):
    text = path.read_text()
    props = set(re.findall(r"Q_PROPERTY\((.*?)\)\s*$", text, re.M))
    methods = set(re.findall(r"Q_INVOKABLE\s+[\w:<>]+\s+(\w+)\s*\(", text))
    signals = re.search(r"signals:(.*?)(?:private|public|protected)", text, re.S)
    sigs = set(re.findall(r"void\s+(\w+)\s*\(", signals.group(1))) if signals else set()
    return {"Q_PROPERTY": props, "Q_INVOKABLE": methods, "signal": sigs}


real = api(here.parent / "app" / "jellyfinclient.h")
demo = api(here / "demojellyfin.h")
problems = [f"{kind} {name!r} is on JellyfinClient but not DemoJellyfinClient"
            for kind in real for name in sorted(real[kind] - demo[kind])]

# The wasm build links only the QML modules qml_imports.qml names (see that file).
def modules(paths):
    found = set()
    for path in paths:
        found |= set(re.findall(r"^import\s+([A-Za-z][\w.]*)", path.read_text(), re.M))
    return found


missing = modules((here.parent / "app" / "qml").glob("*.qml")) - modules([here / "qml_imports.qml"])
problems += [f"QML module {m!r} is imported by ../app/qml but missing from qml_imports.qml" for m in sorted(missing)]
for p in problems:
    print(p, file=sys.stderr)
print("preview API check:", "FAILED" if problems else "ok")
sys.exit(1 if problems else 0)

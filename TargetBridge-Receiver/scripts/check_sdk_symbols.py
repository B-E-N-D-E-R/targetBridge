#!/usr/bin/env python3
"""Fail if a Mach-O binary imports a symbol that an older macOS does not export.

Availability annotations in the current SDK are not enough: some functions
(for example CGPreflightListenEventAccess) are declared as available on 10.15
but only exported by the OS from a later release. dyld then refuses to launch
the binary on the older system ("Symbol not found").

This compares every non-weak symbol the binary imports, as reported by
`nm -m -u`, against the symbols exported by the text-based stubs (.tbd) of the
older SDK, e.g. MacOSX10.15.sdk.

Usage:
  check_sdk_symbols.py --sdk /path/to/MacOSX10.15.sdk BINARY [BINARY ...]
  check_sdk_symbols.py --sdk /path/to/MacOSX10.15.sdk --nm-output FILE
"""

import argparse
import pathlib
import re
import subprocess
import sys

# A flow list in a .tbd document, e.g. `symbols: [ _a, _b ]`; may span lines.
LIST_RE = re.compile(r"([a-z-]+):\s*\[([^\]]*)\]", re.S)
# One `nm -m -u` line: `(undefined) [weak] external _sym (from Library)`.
NM_RE = re.compile(r"\(undefined\)\s+(weak\s+)?external\s+(\S+)(?:\s+\((.*)\))?")

OBJC_PREFIXES = {
    "objc-classes": ("_OBJC_CLASS_$_", "_OBJC_METACLASS_$_"),
    "objc-eh-types": ("_OBJC_EHTYPE_$_",),
    "objc-ivars": ("_OBJC_IVAR_$_",),
}
PLAIN_KEYS = {"symbols", "weak-def-symbols", "thread-local-symbols"}


def sdk_symbols(sdk):
    exported = set()
    for tbd in pathlib.Path(sdk).rglob("*.tbd"):
        if not tbd.is_file():  # SDKs carry a few dangling compatibility symlinks
            continue
        text = tbd.read_text(errors="replace")
        for key, body in LIST_RE.findall(text):
            names = [n.strip().strip("'\"") for n in body.split(",")]
            names = [n for n in names if n]
            if key in PLAIN_KEYS:
                exported.update(names)
            elif key in OBJC_PREFIXES:
                for name in names:
                    # tbd v3 omits the leading underscore of class names.
                    name = name.lstrip("_")
                    for prefix in OBJC_PREFIXES[key]:
                        exported.add(prefix + name)
    return exported


def imported_symbols(nm_text):
    """Yields (symbol, library) for each strong import."""
    for line in nm_text.splitlines():
        match = NM_RE.search(line)
        if not match:
            continue
        weak, symbol, origin = match.groups()
        if weak:
            # Weak imports resolve to NULL when missing instead of aborting
            # the launch; the compiler's availability checks cover their use.
            continue
        if origin == "dynamically looked up":
            continue
        library = origin[len("from "):] if origin and origin.startswith("from ") else "?"
        yield symbol, library


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sdk", required=True, help="path to the older MacOSX SDK")
    parser.add_argument("--nm-output", help="read `nm -m -u` output from this file")
    parser.add_argument("binaries", nargs="*")
    args = parser.parse_args()

    exported = sdk_symbols(args.sdk)
    if len(exported) < 10000:
        sys.exit(f"Only {len(exported)} symbols found under {args.sdk}; is it an SDK?")

    sources = []
    if args.nm_output:
        sources.append((args.nm_output, pathlib.Path(args.nm_output).read_text()))
    for binary in args.binaries:
        nm = subprocess.run(["nm", "-m", "-u", binary], check=True,
                            capture_output=True, text=True).stdout
        sources.append((binary, nm))
    if not sources:
        parser.error("give a BINARY or --nm-output")

    failed = False
    for name, nm_text in sources:
        imports = sorted(set(imported_symbols(nm_text)))
        missing = [(s, lib) for s, lib in imports if s not in exported]
        print(f"{name}: {len(imports)} strong imports checked against {args.sdk}")
        for symbol, library in missing:
            print(f"  missing on the target macOS: {symbol} (from {library})")
        failed = failed or bool(missing)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

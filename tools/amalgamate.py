#!/usr/bin/env python3
"""Builds single_include/reflect/reflect.hpp from the headers in include/.

The single header is for people who want to copy one file into their project
or paste it into Compiler Explorer. It is generated; edit the headers in
include/reflect/ and run this script again.

    python tools/amalgamate.py           write the file
    python tools/amalgamate.py --check   fail if the file is out of date
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "include" / "reflect"
ENTRY = SOURCE / "reflect.hpp"
OUTPUT = ROOT / "single_include" / "reflect" / "reflect.hpp"

# Headers for code generator programs. They write files and are used from a
# build system, so they are not part of the single header.
GENERATOR_ONLY = ("codegen.hpp", "codegen/")

LOCAL_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"\s*$')
SYSTEM_INCLUDE = re.compile(r"^\s*#\s*include\s+<([^>]+)>\s*$")
PRAGMA_ONCE = re.compile(r"^\s*#\s*pragma\s+once\s*$")

BANNER = """\
// reflect: a readable API on top of C++26 std::meta.
// https://github.com/jaeckl/meta-reflection
//
// This file is generated from the headers in include/reflect/ by
// tools/amalgamate.py. Do not edit it by hand.

#pragma once
"""


def expand(path, seen, system_headers, body):
    """Appends `path` to `body`, after the local headers it includes.

    Every header is emitted once. The library headers have all their includes
    at the top, so "dependencies first, then the file" keeps the order right.
    """
    path = path.resolve()
    if path in seen:
        return
    seen.add(path)

    own = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if PRAGMA_ONCE.match(line):
            continue
        local = LOCAL_INCLUDE.match(line)
        if local:
            expand(path.parent / local.group(1), seen, system_headers, body)
            continue
        system = SYSTEM_INCLUDE.match(line)
        if system:
            system_headers.add(system.group(1))
            continue
        own.append(line)

    if path == ENTRY.resolve():
        return  # the umbrella header has no code of its own

    name = path.relative_to(SOURCE).as_posix()
    body.append(f"// ---- {name} " + "-" * max(4, 70 - len(name)))
    body.extend(own)


def squeeze(lines):
    """Collapses runs of blank lines into one."""
    out = []
    for line in lines:
        if line.strip() or (out and out[-1].strip()):
            out.append(line)
    while out and not out[-1].strip():
        out.pop()
    return out


def build():
    seen = set()
    system_headers = set()

    # config.hpp goes first: its check has to fire before <meta> is included.
    prelude = []
    expand(SOURCE / "config.hpp", seen, system_headers, prelude)
    if system_headers:
        sys.exit("config.hpp must not include anything")

    body = []
    expand(ENTRY, seen, system_headers, body)

    missing = sorted(
        name
        for p in SOURCE.rglob("*.hpp")
        if p.resolve() not in seen
        and not (name := p.relative_to(SOURCE).as_posix()).startswith(GENERATOR_ONLY)
    )
    if missing:
        sys.exit("not reachable from reflect.hpp: " + ", ".join(missing))

    out = BANNER.splitlines()
    out.append("")
    out.extend(prelude)
    out.append("")
    out.extend(f"#include <{header}>" for header in sorted(system_headers))
    out.append("")
    out.extend(body)
    return "\n".join(squeeze(out)) + "\n"


def main():
    text = build()
    if "--check" in sys.argv[1:]:
        current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
        if current != text:
            sys.exit(
                "single_include/reflect/reflect.hpp is out of date; "
                "run: python tools/amalgamate.py"
            )
        print("single header is up to date")
        return
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUTPUT.relative_to(ROOT).as_posix()} ({len(text.splitlines())} lines)")


if __name__ == "__main__":
    main()

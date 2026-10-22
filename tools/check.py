#!/usr/bin/env python3
"""Checks the module rules that the compiler does not.

- Every directory under src/ is a module with a row in MODULES below: its layer,
  dependencies and allowed third-party include prefixes.
- A module depends only on modules in lower layers.
- Includes respect module boundaries: public headers name no third-party header
  and only coldx modules the owner depends on; source reaches another module only
  through its public headers; samples use public headers only.
- No source file passes the line cap, and source is ASCII.

Standard library only. Run from anywhere: python tools/check.py
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LINE_CAP = 800
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".c", ".hlsl", ".hlsli", ".cmake", ".py", ".md"}
CHECKED_DIRS = ["include", "src", "samples", "tests", "tools", "cmake", "shaders/src", "shaders/include", "docs"]
CHECKED_FILES = ["README.md", "CHANGELOG.md", "CMakeLists.txt", "third_party/VERSIONS.md"]
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)

problems = []


def problem(path, message):
    rel = path.relative_to(ROOT) if isinstance(path, pathlib.Path) else path
    problems.append(f"{rel}: {message}")


def source_files(base):
    if not base.exists():
        return []
    return [p for p in base.rglob("*") if p.is_file() and p.suffix in SOURCE_SUFFIXES]


# Module -> (layer, the coldx modules it depends on, the third-party include prefixes it
# may use). Every directory under src/ is a module and has a row here.
MODULES = {
    "core": (0, [], ["SDL3", "stb", "dr_libs", "lz4"]),
    "content": (1, ["core"], ["SDL3", "stb"]),
    "protect": (1, [], []),
}


def read_modules():
    for directory in sorted(p for p in (ROOT / "src").iterdir() if p.is_dir()):
        if directory.name not in MODULES:
            problem(directory, "not in MODULES in tools/check.py")
    return {name: {"dir": ROOT / "src" / name, "layer": layer, "deps": deps, "third_party": third}
            for name, (layer, deps, third) in MODULES.items() if (ROOT / "src" / name).is_dir()}


def check_layers(modules):
    for name, m in modules.items():
        for dep in m["deps"]:
            if dep not in modules:
                problem(m["dir"], f"depends on unknown module '{dep}'")
            elif modules[dep]["layer"] >= m["layer"]:
                problem(m["dir"], f"layer {m['layer']} depends on '{dep}', layer {modules[dep]['layer']}")


def coldx_module_of(include):
    """'coldx/core/game.h' -> 'core'; umbrella 'coldx/coldx.h' -> 'core'; 'coldx/net.h' -> 'net'."""
    parts = include.split("/")
    if parts[0] != "coldx":
        return None
    if len(parts) == 2:
        stem = parts[1].rsplit(".", 1)[0]
        return "core" if stem == "coldx" else stem
    return parts[1]


def check_includes(path, allowed_modules, allowed_third_party, quoted_roots, owner):
    text = path.read_text(encoding="utf-8", errors="replace")
    for kind, include in INCLUDE_RE.findall(text):
        if kind == '"':
            if quoted_roots is None:
                problem(path, f'quoted include "{include}" not allowed here')
                continue
            # Resolve first: "../" can walk out of the allowed folders.
            found = [(base / include).resolve() for base in [path.parent] + quoted_roots
                     if (base / include).exists()]
            roots = [r.resolve() for r in quoted_roots]
            if not any(any(f.is_relative_to(r) for r in roots) for f in found):
                problem(path, f'"{include}" is outside {owner}; include another module by its public header')
            continue
        module = coldx_module_of(include)
        if module is not None:
            if module not in allowed_modules:
                problem(path, f"<{include}>: {owner} may not use coldx module '{module}'")
        elif "/" in include and include.split("/")[0] not in allowed_third_party:
            problem(path, f"<{include}>: third-party header not allowed here")


def check_boundaries(modules):
    for name, m in modules.items():
        visible = {name, *m["deps"]}
        for path in source_files(ROOT / "include" / "coldx" / name):
            check_includes(path, visible, [], None, f"public header of '{name}'")
        quoted = [ROOT / "src" / name, ROOT / "tests"] + ([ROOT / "shaders" / "compiled"] if name == "core" else [])
        for path in source_files(ROOT / "src" / name):
            if path.suffix in (".cmake", ".py"):
                continue
            check_includes(path, visible, m["third_party"], quoted, f"module '{name}'")
    for umbrella in (ROOT / "include" / "coldx").glob("*.h"):
        owner = coldx_module_of("coldx/" + umbrella.name)
        deps = {owner, *modules.get(owner, {}).get("deps", [])}
        check_includes(umbrella, deps, [], None, f"umbrella '{umbrella.name}'")
    every_module = set(modules)
    for path in source_files(ROOT / "samples"):
        check_includes(path, every_module, [], [path.parent], "its sample")


def check_files():
    paths = [ROOT / f for f in CHECKED_FILES]
    for d in CHECKED_DIRS:
        paths += source_files(ROOT / d)
    for path in paths:
        data = path.read_bytes()
        lines = data.count(b"\n")
        if lines > LINE_CAP:
            problem(path, f"{lines} lines, over the {LINE_CAP}-line cap; split it")
        if any(b > 127 for b in data):
            problem(path, "non-ASCII bytes")


SAMPLE_MARKER_RE = re.compile(r"<!-- sample: (\S+) -->\s*```\w*\n(.*?)```", re.S)


def check_embedded_samples():
    """A doc that embeds a sample under <!-- sample: path --> must match the file exactly,
    so the code a reader copies is the code that builds."""
    for doc in source_files(ROOT / "docs") + [ROOT / "README.md"]:
        text = doc.read_text(encoding="utf-8").replace("\r\n", "\n")
        for path, block in SAMPLE_MARKER_RE.findall(text):
            sample = ROOT / path
            if not sample.exists():
                problem(doc, f"embeds {path}, which does not exist")
            elif sample.read_text(encoding="utf-8").replace("\r\n", "\n") != block:
                problem(doc, f"the code block after its {path} marker differs from that file")


def main():
    modules = read_modules()
    check_layers(modules)
    check_boundaries(modules)
    check_files()
    check_embedded_samples()
    if problems:
        print("\n".join(problems))
        print(f"{len(problems)} problem(s)")
        return 1
    print(f"ok: {len(modules)} module(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Checks the rules in docs/ARCHITECTURE.md that the compiler does not.

- Every directory under src/ is a module with a MODULE.md giving its layer,
  dependencies, allowed third-party include prefixes and approximate size.
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
FIELD_RE = re.compile(r"^\*\*(.+?):\*\*\s*(.*)$", re.M)
# Size tolerance: 20%, but never tighter than this many lines.
SIZE_SLACK = 200

problems = []


def problem(path, message):
    rel = path.relative_to(ROOT) if isinstance(path, pathlib.Path) else path
    problems.append(f"{rel}: {message}")


def source_files(base):
    if not base.exists():
        return []
    return [p for p in base.rglob("*") if p.is_file() and p.suffix in SOURCE_SUFFIXES]


def count_lines(paths):
    return sum(len(p.read_text(encoding="utf-8", errors="replace").splitlines()) for p in paths)


def read_modules():
    modules = {}
    for directory in sorted(p for p in (ROOT / "src").iterdir() if p.is_dir()):
        doc = directory / "MODULE.md"
        if not doc.exists():
            problem(directory, "no MODULE.md")
            continue
        fields = dict(FIELD_RE.findall(doc.read_text(encoding="utf-8")))
        missing = [f for f in ("Layer", "Depends on", "Third-party", "Size") if f not in fields]
        if missing:
            problem(doc, "missing field(s): " + ", ".join(missing))
            continue

        def names(value):
            value = re.sub(r"\(.*?\)", "", value).replace("`", "")
            return [] if value.strip().lower() in ("", "none") else [v.strip() for v in value.split(",") if v.strip()]

        size = re.search(r"([\d.]+)\s*(k?)", fields["Size"])
        modules[directory.name] = {
            "doc": doc,
            "layer": int(fields["Layer"].strip()),
            "deps": names(fields["Depends on"]),
            "third_party": names(fields["Third-party"]),
            "size": float(size.group(1)) * (1000 if size.group(2) else 1) if size else None,
        }
    return modules


def check_layers(modules):
    for name, m in modules.items():
        for dep in m["deps"]:
            if dep not in modules:
                problem(m["doc"], f"depends on unknown module '{dep}'")
            elif modules[dep]["layer"] >= m["layer"]:
                problem(m["doc"], f"layer {m['layer']} depends on '{dep}', layer {modules[dep]['layer']}")


def check_sizes(modules):
    for name, m in modules.items():
        files = source_files(ROOT / "src" / name) + source_files(ROOT / "include" / "coldx" / name)
        actual = count_lines(files)
        if m["size"] is None:
            problem(m["doc"], "Size field has no number")
            continue
        if abs(actual - m["size"]) > max(SIZE_SLACK, 0.2 * actual):
            problem(m["doc"], f"Size says ~{int(m['size'])} lines, actual {actual}; update it")


def coldx_module_of(include):
    """'coldx/core/game.h' -> 'core'; umbrella 'coldx/coldx.h' -> 'core'; 'coldx/net.h' -> 'net'."""
    parts = include.split("/")
    if parts[0] != "coldx":
        return None
    if len(parts) == 2:
        stem = parts[1].rsplit(".", 1)[0]
        return "core" if stem == "coldx" else stem
    return parts[1]


# Headers CMake writes into the build folder, so they are not in the tree to resolve.
GENERATED_HEADERS = {"coldx_hlsli.h"}


def check_includes(path, allowed_modules, allowed_third_party, quoted_roots, owner):
    text = path.read_text(encoding="utf-8", errors="replace")
    for kind, include in INCLUDE_RE.findall(text):
        if kind == '"':
            if include in GENERATED_HEADERS and quoted_roots is not None:
                continue
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
            # Tests may use the example effects' compiled headers, as a game would.
            extra = [ROOT / "shaders" / "examples"] if "tests" in path.relative_to(ROOT).parts else []
            check_includes(path, visible, m["third_party"], quoted + extra, f"module '{name}'")
    for umbrella in (ROOT / "include" / "coldx").glob("*.h"):
        owner = coldx_module_of("coldx/" + umbrella.name)
        deps = {owner, *modules.get(owner, {}).get("deps", [])}
        check_includes(umbrella, deps, [], None, f"umbrella '{umbrella.name}'")
    every_module = set(modules)
    # A sample is a game: its own folder, coldx's public headers, and the example effects
    # coldx ships for games (shaders/examples).
    for path in source_files(ROOT / "samples"):
        check_includes(path, every_module, [], [path.parent, ROOT / "shaders" / "examples"], "its sample")


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
    check_sizes(modules)
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

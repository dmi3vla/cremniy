#!/usr/bin/env python3
"""Rebuild the trace prototype's real ELF, DWARF and objdump evidence.

Only the deliberately small fixture is parsed lexically. This is NOT a C++ AST
parser. The binaries, line mappings and revision diff come from external tools.
"""

import hashlib
import json
import os
import platform
import re
import subprocess
from difflib import SequenceMatcher
from pathlib import Path


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "artifacts"
COMPILER = os.environ.get("CXX", "c++")
FUNCTION = re.compile(r"\b(?:int)\s+(\w+)\s*\(([^)]*)\)\s*\{")
TOKEN = re.compile(r"[A-Za-z_]\w*|0x[0-9a-fA-F]+|\d+|==|!=|<=|>=|&&|\|\||\S")


def symbol_id(name, func):
    return f"demo.cpp::{name}{func['signature']}"


def run(*args, check=True):
    return subprocess.run(args, cwd=ROOT, text=True, capture_output=True, check=check)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def functions(source):
    result = {}
    for match in FUNCTION.finditer(source):
        name = match.group(1)
        start = match.start()
        open_brace = match.end() - 1
        depth = 1
        pos = open_brace + 1
        while depth and pos < len(source):
            depth += (source[pos] == "{") - (source[pos] == "}")
            pos += 1
        if depth:
            raise ValueError(f"unbalanced body: {name}")
        body = source[open_brace + 1:pos - 1]
        params = re.findall(r"\b[A-Za-z_]\w*\s+([A-Za-z_]\w*)\s*(?:,|$)", match.group(2))
        parameter_types = [re.sub(r"\s+[A-Za-z_]\w*$", "", part.strip())
                           for part in match.group(2).split(",") if part.strip()]
        locals_ = re.findall(r"\b(?:int|long|short|bool)\s+([A-Za-z_]\w*)\b", body)
        rename = {value: f"v{i}" for i, value in enumerate(params + locals_)}
        tokens = TOKEN.findall(body)
        normalized = [rename.get(token, token) for token in tokens]
        calls = re.findall(r"\b([A-Za-z_]\w*)\s*\(", body)
        constants = [token for token in tokens if re.fullmatch(r"0x[0-9a-fA-F]+|\d+", token)]
        branches = [token for token in tokens if token in {"if", "else", "?", "==", "!=", "<", ">", "<=", ">="}]
        writes = [target for target in re.findall(r"\b([A-Za-z_]\w*)\s*=(?!=)", body)
                  if target not in rename]
        result[name] = {
            "startLine": source.count("\n", 0, start) + 1,
            "endLine": source.count("\n", 0, pos) + 1,
            "source": source[start:pos],
            "body": body,
            "exact": hashlib.sha256("".join(tokens).encode()).hexdigest(),
            "normalized": normalized,
            "calls": calls,
            "constants": constants,
            "branches": branches,
            "types": ["int"] + parameter_types +
                     re.findall(r"\b(?:int|long|short|bool|uint\d+_t)\b", body),
            "globalWrites": writes,
            "signature": '(' + ','.join(parameter_types) + ')',
        }
    return result


def sections(binary):
    names = {}
    for line in run("readelf", "-WS", str(binary)).stdout.splitlines():
        match = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)", line)
        if match:
            names[match.group(1)] = match.group(2)
    return names


def symbols(binary):
    section_names = sections(binary)
    indices = {}
    for line in run("readelf", "--demangle", "-Ws", str(binary)).stdout.splitlines():
        fields = line.split(maxsplit=7)
        if len(fields) >= 8 and fields[3] == "FUNC":
            indices[fields[7].split("(")[0].split("@")[0]] = section_names.get(fields[6], "")
    result = {}
    for line in run("nm", "-S", "-C", "--defined-only", str(binary)).stdout.splitlines():
        match = re.match(r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([Tt])\s+(.+)$", line)
        if match:
            addr, size, _, name = match.groups()
            name = name.split("(")[0]
            result[name] = {"start": int(addr, 16), "end": int(addr, 16) + int(size, 16),
                            "section": indices.get(name, "")}
    return result


def disassembly(binary, output):
    raw = run("objdump", "-d", "-M", "intel", str(binary)).stdout
    output.write_text(raw)
    result = []
    section = ""
    for line in raw.splitlines():
        header = re.match(r"Disassembly of section (\S+):", line)
        if header:
            section = header.group(1)
            continue
        match = re.match(r"\s*([0-9a-fA-F]+):\s+((?:[0-9a-fA-F]{2}\s+)+)\s*(\S+)", line)
        if match:
            addr = int(match.group(1), 16)
            result.append({"section": section, "address": addr,
                           "size": len(match.group(2).split()), "mnemonic": match.group(3)})
    return result


def source_locations(binary, instructions):
    # addr2line is a DWARF lookup per real instruction; '?' is retained.
    addresses = [f"0x{item['address']:x}" for item in instructions]
    process = run("addr2line", "-e", str(binary), *addresses)
    locations = process.stdout.splitlines()
    if len(locations) != len(instructions):
        raise ValueError("addr2line output length mismatch")
    for item, location in zip(instructions, locations):
        path, sep, line = location.rpartition(":")
        try:
            number = int(line.split(" ")[0])
        except ValueError:
            number = 0
        try:
            relative = str(Path(path).resolve().relative_to(ROOT)) if path != "??" else ""
        except ValueError:
            relative = ""
        item["sourceFile"] = relative
        item["sourceLine"] = number if relative else 0


def build(revision, optimization, source_sha, funcs, debug=True):
    source = ROOT / revision / "demo.cpp"
    label = optimization if debug else optimization + "-nog"
    binary = OUT / f"demo-{revision}-{label}"
    flags = ["-std=c++17", f"-{optimization}"] + (["-g"] if debug else []) + ["-fno-pie", "-no-pie"]
    run(COMPILER, *flags, str(source), "-o", str(binary))
    instructions = disassembly(binary, OUT / f"demo-{revision}-{label}.objdump.txt")
    source_locations(binary, instructions)
    found = symbols(binary)
    binary_sha = sha(binary)
    disassembly_path = OUT / f"demo-{revision}-{label}.objdump.txt"
    build_id = f"{revision}-{label}-{binary_sha[:12]}"
    ranges = []
    source_links = []
    for name, func in funcs.items():
        symbol = found.get(name)
        if not symbol or symbol["start"] >= symbol["end"] or not symbol["section"]:
            ranges.append({"symbolId": symbol_id(name, func), "status": "optimized-away-or-unavailable"})
            continue
        belonging = [item for item in instructions if item["section"] == symbol["section"]
                     and symbol["start"] <= item["address"] < symbol["end"]]
        ranges.append({"symbolId": symbol_id(name, func), "status": "mapped",
                       "section": symbol["section"], "start": f"0x{symbol['start']:x}",
                       "end": f"0x{symbol['end']:x}",
                       "instructions": [f"0x{item['address']:x}" for item in belonging]})
        for item in belonging:
            if item["sourceLine"]:
                source_links.append({"kind": "source-instruction", "originKind": "tool",
                                     "origin": "DWARF/addr2line",
                                     "buildId": build_id, "revision": revision,
                                     "symbolId": symbol_id(name, func),
                                     "source": {"file": item["sourceFile"], "startLine": item["sourceLine"],
                                                "endLine": item["sourceLine"], "revision": revision,
                                                "sourceSha256": source_sha},
                                     "binary": {"section": item["section"], "start": f"0x{item['address']:x}",
                                                "end": f"0x{item['address'] + item['size']:x}",
                                                "buildId": build_id}})
    return {"id": build_id, "revision": revision, "optimization": optimization,
            "compiler": run(COMPILER, "--version").stdout.splitlines()[0],
            "flags": flags, "architecture": platform.machine(),
            "sourceSha256": source_sha, "binarySha256": binary_sha,
            "binary": str(binary.relative_to(ROOT)),
            "disassembly": str(disassembly_path.relative_to(ROOT)),
            "disassemblySha256": sha(disassembly_path),
            "hasDebugInfo": bool(run("readelf", "-S", str(binary)).stdout.find(".debug_info") >= 0),
            "ranges": ranges, "sourceLinks": source_links}


def main():
    OUT.mkdir(exist_ok=True)
    revisions = {}
    parsed = {}
    for revision in ("base", "head"):
        path = ROOT / revision / "demo.cpp"
        parsed[revision] = functions(path.read_text())
        revisions[revision] = {"source": f"{revision}/demo.cpp", "sha256": sha(path)}

    diff = run("git", "diff", "--no-index", "--", "base/demo.cpp", "head/demo.cpp", check=False)
    if diff.returncode != 1 or not diff.stdout.startswith("diff --git"):
        raise RuntimeError("expected a real Git diff between base and head")
    (ROOT / "base-head.diff").write_text(diff.stdout)

    all_names = sorted(set(parsed["base"]) | set(parsed["head"]))
    entries = []
    for revision in ("base", "head"):
        for name, func in parsed[revision].items():
            other = parsed["head" if revision == "base" else "base"].get(name)
            status = "added" if revision == "head" and not other else "deleted" if revision == "base" and not other else \
                     "modified" if other and func["source"] != other["source"] else "unchanged"
            entries.append({"id": symbol_id(name, func), "name": name, "revision": revision,
                            "path": f"{revision}/demo.cpp", "startLine": func["startLine"],
                            "endLine": func["endLine"], "gitState": status,
                            "exactHash": func["exact"], "normalizedTokens": func["normalized"]})

    # Fixture-scoped lexical comparison. Keep separate symbols even in a group.
    exemplars = {name: parsed["head"].get(name, parsed["base"].get(name)) for name in all_names}
    exact_sets = {}
    for name, func in exemplars.items():
        exact_sets.setdefault(func["exact"], []).append(name)
    exact_groups = {}
    for n, members in enumerate(sorted((sorted(v) for v in exact_sets.values() if len(v) > 1)), 1):
        for name in members:
            exact_groups[name] = f"EXACT-{n}"

    parent = {name: name for name in all_names}
    def root(name):
        while parent[name] != name:
            name = parent[name]
        return name
    for index, first in enumerate(all_names):
        for second in all_names[index + 1:]:
            left, right = exemplars[first], exemplars[second]
            if (left["calls"], left["constants"], left["branches"], left["types"]) != \
               (right["calls"], right["constants"], right["branches"], right["types"]):
                continue
            if SequenceMatcher(None, left["normalized"], right["normalized"]).ratio() >= 0.85:
                parent[root(second)] = root(first)
    components = {}
    for name in all_names:
        components.setdefault(root(name), []).append(name)
    candidate_groups = {}
    comparisons = {}
    for n, members in enumerate(sorted((sorted(v) for v in components.values() if len(v) > 1)), 1):
        anchor = exemplars[members[0]]
        for name in members:
            candidate_groups[name] = f"SIM-{n}"
            candidate = exemplars[name]
            ratio = SequenceMatcher(None, anchor["normalized"], candidate["normalized"]).ratio()
            differences = []
            if candidate["globalWrites"] != anchor["globalWrites"]:
                differences.append("extra write to volatile state" if candidate["globalWrites"] else "different memory write")
            comparisons[name] = f"normalized token similarity {ratio:.3f}; " + \
                                (", ".join(differences) if differences else "same token sequence after local/parameter rename")
    for entry in entries:
        name = entry["name"]
        if name in exact_groups: entry["exactGroup"] = exact_groups[name]
        if name in candidate_groups:
            entry["candidateGroup"] = candidate_groups[name]
            entry["comparison"] = comparisons[name]

    requirements = json.loads((ROOT / "requirements.json").read_text())
    manual = [("REQ-ONE", "scenario_one"), ("REQ-ONE", "shared_helper"),
              ("REQ-TWO", "scenario_two"), ("REQ-TWO", "shared_helper"),
              ("REQ-CALC", "calculate_alpha"), ("REQ-CALC", "calculate_beta"),
              ("REQ-CALC", "calculate_clone"), ("REQ-CALC", "calculate_with_state")]
    links = [{"kind": "requirement-symbol", "originKind": "human", "origin": "fixture",
              "requirementId": req,
              "symbolId": symbol_id(name, parsed["head"].get(name, parsed["base"].get(name)))}
             for req, name in manual]
    for req, name in manual:
        for revision in ("base", "head"):
            if name not in parsed[revision]:
                continue
            func = parsed[revision][name]
            links.append({"kind": "requirement-source", "originKind": "human", "origin": "fixture",
                          "requirementId": req, "symbolId": symbol_id(name, func),
                          "source": {"file": f"{revision}/demo.cpp", "revision": revision,
                                     "startLine": func["startLine"], "endLine": func["endLine"],
                                     "sourceSha256": revisions[revision]["sha256"]}})
    # The call graph is a reviewed fixture fact, not inferred from address order.
    calls = [("scenario_one", "calculate_alpha"), ("scenario_one", "shared_helper"),
             ("scenario_two", "calculate_beta"), ("scenario_two", "shared_helper")]
    links += [{"kind": "calls", "originKind": "human", "origin": "fixture",
               "from": symbol_id(a, parsed["head"][a]),
               "to": symbol_id(b, parsed["head"][b]), "revision": "head"} for a, b in calls]

    builds = [build(revision, opt, revisions[revision]["sha256"], parsed[revision])
              for revision in ("base", "head") for opt in ("O0", "O2")]
    builds.append(build("head", "O2", revisions["head"]["sha256"], parsed["head"], debug=False))
    manifest = {"schemaVersion": 1, "analysisMode": "limited lexical C++ fixture analysis, not full AST",
                "codemap": "trace.codemap.txt", "diff": "base-head.diff",
                "revisions": revisions, "requirements": requirements,
                "symbols": entries, "links": links, "builds": builds,
                "selectedBuildId": next(b["id"] for b in builds if b["revision"] == "head" and b["optimization"] == "O0")}
    (ROOT / "trace-manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    print("Wrote", ROOT / "trace-manifest.json")
    for item in builds:
        print(item["id"], len(item["sourceLinks"]), "DWARF instruction links")


if __name__ == "__main__":
    main()

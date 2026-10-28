#!/usr/bin/env python3
"""The deepest stack of a call chain, from GCC's -fcallgraph-info=su files.

Reads the .ci files GCC writes with -fcallgraph-info=su (one per
translation unit: a node per function with its frame, an edge per call),
and prints, for each root function, the call chain from it whose frames sum
highest. A function of another file is matched by name; a static function
is first looked up in the caller's file. A call to a function with no node
(the C library, the SDK) counts 0 bytes and is listed.

Fails when a chain passes --limit bytes, when a frame has no fixed size
(dynamic), when a chain recurses, or when a root is not found.

    tools/stack_depth.py --limit 1792 --root hitecJobStep build/**/*.ci
"""

import argparse
import re
import sys

NODE = re.compile(r'^node: \{ title: "([^"]*)" label: "([^"]*)"')
EDGE = re.compile(r'^edge: \{ sourcename: "([^"]*)" targetname: "([^"]*)"')
SIZE = re.compile(r'\\n(\d+) bytes \((\w+(?:,\w+)*)\)')


def short(name):
    """A function's name without its C++ return type, scope and parameters."""
    if "(" in name:
        name = name[:name.index("(")]
    return name.split()[-1].split("::")[-1] if name.split() else name


def load(paths):
    """Nodes {title: (name, file, bytes or None, qualifier)} and edges."""
    nodes, edges = {}, {}
    for path in paths:
        with open(path, encoding="utf-8") as f:
            for line in f:
                m = NODE.match(line)
                if m:
                    title, label = m.groups()
                    name = short(label.split("\\n")[0])
                    file = title.rsplit(":", 1)[0] if ":" in title else ""
                    s = SIZE.search(label)
                    if s or title not in nodes:
                        nodes[title] = (name, file, int(s.group(1)) if s else None,
                                        s.group(2) if s else "")
                    continue
                m = EDGE.match(line)
                if m:
                    edges.setdefault(m.group(1), []).append(m.group(2))
    return nodes, edges


def resolve(target, caller, nodes, by_name):
    """The node a call names: the title itself when it has a frame, else a
    function of that name with one, in the caller's file first."""
    if target in nodes and nodes[target][2] is not None:
        return target
    name = nodes[target][0] if target in nodes else target
    cands = by_name.get(name, [])
    caller_file = nodes[caller][1] if caller in nodes else ""
    for c in cands:
        if nodes[c][1] == caller_file:
            return c
    return cands[0] if len(cands) == 1 else target


def deepest(title, nodes, edges, by_name, path, memo, uncounted, errors):
    """(bytes, chain) of the deepest chain from @p title."""
    if title in memo:
        return memo[title]
    if title in path:
        errors.append("recursion: " + " -> ".join(nodes[t][0] for t in path + [title]))
        return 0, []
    name, _, size, qual = nodes.get(title, (title, "", None, ""))
    if size is None:
        uncounted.add(name)
        return 0, [name + " (0, no frame known)"]
    if qual != "static":
        errors.append(f"{name}: a frame of {size} bytes that is {qual}, not static")
    best, chain = 0, []
    for t in edges.get(title, []):
        r = resolve(t, title, nodes, by_name)
        b, c = deepest(r, nodes, edges, by_name, path + [title], memo, uncounted, errors)
        if b > best or not chain:
            best, chain = b, c
    memo[title] = (size + best, [f"{name} ({size})"] + chain)
    return memo[title]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--limit", type=int, required=True, help="bytes a chain may take")
    ap.add_argument("--root", action="append", required=True, help="a root function's name")
    ap.add_argument("ci", nargs="+", help=".ci files")
    a = ap.parse_args()

    nodes, edges = load(a.ci)
    by_name = {}
    for title, (name, _, size, _) in nodes.items():
        if size is not None:
            by_name.setdefault(name, []).append(title)

    errors, uncounted, worst = [], set(), 0
    for root in a.root:
        titles = by_name.get(root, [])
        if not titles:
            errors.append(f"root {root} not found")
            continue
        for t in titles:
            b, chain = deepest(t, nodes, edges, by_name, [], {}, uncounted, errors)
            worst = max(worst, b)
            print(f"{root}: {b} bytes")
            print("  " + "\n  -> ".join(chain))
    if uncounted:
        print("not counted, no frame known: " + ", ".join(sorted(uncounted)))
    print(f"deepest chain: {worst} bytes, limit {a.limit} bytes")
    if worst > a.limit:
        errors.append(f"the deepest chain takes {worst} bytes, over the limit of {a.limit}")
    for e in errors:
        print("error: " + e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Documentation checks for MiniRcBench.

Enforces what actually goes wrong, and nothing else:

  1. every function declared in a header carries a doc comment
  2. every @param names a parameter that exists in the signature it precedes
  3. @param coverage is all-or-nothing -- Doxygen warns on partial coverage
     regardless of WARN_NO_PARAMDOC, so documenting one argument out of six
     is worse than documenting none
  4. every @ref points at something that exists
  5. Doxygen commands are escaped when merely being talked about; writing
     "@mainpage" in prose makes Doxygen execute it, which silently produces a
     second main page
  6. backticks inside a doc comment are balanced, otherwise Doxygen hits the
     end of the block still waiting to close a code span
  7. a @ref is not immediately followed by a colon -- Doxygen's reference
     grammar includes ':' so that Class::member resolves, so "@ref foo.h: and
     then some prose" is read as a reference to "foo.h:" and does not
  8. every relative link in the running record (STATUS.md, CHANGELOG.md) and
     the design notes (docs/design/*.md) names a file that exists, and an
     anchor into a Markdown file names a heading that exists
  9. README.md and the wiki pages, which describe the screens as built, name
     no screen text, view or MENU path that menu structure B and the
     decisions that revise it removed (docs/design/menu-b.md,
     docs/design/bluejay-blheli-s.md) or a later design note replaced, and
     no image of a removed screen; nor does a fakeDumpFrame() call or
     tools/make_previews.py. CHANGELOG.md, STATUS.md and the design notes
     keep those names as records and are not checked
  10. the hardware models and CAD files are CC BY-NC-SA 4.0 and the rest is
     MIT: every .py and .step file under hardware/cad/ and hardware/mechanical/
     carries SPDX-License-Identifier: CC-BY-NC-SA-4.0 and not MIT;
     hardware/LICENSE-CAD.txt exists and holds the legal code unchanged
     (checked by SHA-256); a file elsewhere with an SPDX line names MIT.
     lib/rcbench/hardware/ is exempt: its files are unchanged copies of
     rcbench's, which have no SPDX line

Deliberately narrower than Doxygen's WARN_IF_UNDOCUMENTED, which also fires on
file-static internals. A comment on `s_scroll` saying "scroll position" is
noise; a public function with no documentation, or an @param that silently
stopped matching after a rename, is not.

Exit status is the number of problems, capped at 255: a process exit status
holds 8 bits, so an uncapped count of 256 would read as success. CI fails on
any non-zero status without extra plumbing.

Paths are resolved from the repository root, found from this file's own
location, so the result does not depend on the directory the script is run
from.
"""

import glob
import hashlib
import functools
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

HEADERS = sorted(glob.glob("src/*.h"))
SOURCES = sorted(glob.glob("src/*.cpp") + glob.glob("*.ino") + HEADERS)
LINKED_DOCS = sorted(glob.glob("STATUS.md") + glob.glob("CHANGELOG.md") +
                     glob.glob("docs/design/*.md"))
MANUAL_DOCS = sorted(glob.glob("README.md") + glob.glob("wiki/*.md"))

# Rule 10: the licence of the hardware models and CAD files.
CC_ID = "CC-BY-NC-SA-4.0"
CC_DIRS = ("hardware/cad", "hardware/mechanical")
CC_FILE = "hardware/LICENSE-CAD.txt"
CC_EXEMPT = ("lib/rcbench/hardware",)
CC_MARK = "Legal code\n----------\n\n"
# SHA-256 of the legal code as creativecommons.org publishes it
# (legalcode.txt of by-nc-sa 4.0).
CC_SHA256 = "e66c269d4819aaab34b49ef5220c4ddab6756f21bb5180761a4eb8561f2b7bbd"
# A tag opens its line after a comment marker; a STEP file carries it in the
# FILE_DESCRIPTION of its header, which has no comment marker.
SPDX_RE = re.compile(r"^[\s#/*;<!-]*SPDX-License-Identifier:\s*([A-Za-z0-9.+-]+)", re.M)
SPDX_STEP_RE = re.compile(r"SPDX-License-Identifier:\s*([A-Za-z0-9.+-]+)")
SKIP_DIRS = {".git", "build", "wiki"}

# Rule 9: what menu structure B and the decisions that revise it removed,
# each with the decision that removed it, of docs/design/menu-b.md unless
# named, and what a later design note replaced, with its decision.
# Case-sensitive regular expressions,
# matched line by line in README.md and wiki/*.md, code spans included.
REMOVED_TEXTS = [
    (r"PINS BUSY", "MENU's busy status (decision 15 f)"),
    (r"HOLD RESET|`RESET`", "SETTINGS' RESET, now HOLD DEFAULTS (decision 14)"),
    (r"\*\*HOLD\*\*", "the tester's HOLD, now LATCH (decision 15 a)"),
    (r"`MODE`", "the MODE chip, now PROG MODE (decision 15 g)"),
    (r"(\*\*|`)(ESC|KISS) PIN(\*\*|`)", "SIG PIN and AUX PIN (decision 10)"),
    (r"DS600|KBIT/S|DSHOT KBAUD", "the DShot rate, now DSHOT600 (decision 15 j)"),
    (r"COMPILED DEFAULTS IN USE", "NOTHING SAVED: DEFAULTS IN USE (decision 15 k)"),
    (r"ALLOW UNCONT|UNCONTROLLED (POS|TIME)|`REVERSION`",
     "the KST labels, now FAILSAFE and REVERSED (decision 15 h)"),
    (r"BACKUP IN MEMORY", "a backup not on the card, now BACKUP IN RAM (slice 2)"),
    (r"CHOOSE THE SERVO'S FAMILY|[Ff]amily view|Familien(ansicht|auswahl)",
     "HITEC SERVO's family view, now PROGRAMMER's entries (decision 19)"),
    (r"MUST BE POWERED NOW|POWER (THE SERVO UP|IT) BEFORE THE TAP",
     "the D series powered before the tap (decision 34)"),
    (r"[Uu]nplug page|Abzieh-?[Ss]eite", "the unplug page, now the leave page (owner decision 3)"),
    (r"LEAVING AFTER THE WRITE ENDS", "BACK's leave queue (decision 5)"),
    (r"MENU\s*(→|>|->|&gt;)\s*(KST|HITEC) SERVO|^\|\s*\*\*(KST|HITEC) SERVO\*\*\s*\|",
     "the KST SERVO and HITEC SERVO rows, now SERVO PROGRAMMER (owner decision 2)"),
    (r"titled `SERVO`", "SERVO's title, now SERVO TESTER (decision 13)"),
    (r"`NONE`, (the default, )?`GP2[89]`|SERVO SUPPLY[^|`]{0,12}`NONE`",
     "SERVO SUPPLY's NONE, now OWN (decisions 15 e and 22)"),
    (r"THROTTLE REL\b", "the latch's caption, now LATCHED (decision 15 a)"),
    (r"RELEASING DSHOT PIN|\b(RELEASING|TAKING|CHECKING) GP\d|"
     r"ESC AND KISS PIN|MOVE THE ESC PIN",
     "pin names without SIG (decision 10)"),
    (r"DSHOT RESTARTS ON GP\d+ AND READS AS|POWER THE SERVO OFF\.",
     "the unplug page's texts (owner decision 3)"),
    (r"SUPPLY SWITCHED ON GP", "the family view's supply line (decision 34)"),
    (r"BEEP THE ESC|BEEP-Zeile|BEEP flash",
     "MENU's BEEP THE ESC, now ESC COMMANDS (esc-commands.md, decision 1)"),
    (r"THROTTLE SCALE DIFFERS",
     "the tester's 3D note, now 0-50% REVERSED AT 2X (esc-commands.md, decision 9)"),
    (r"AM32 (CONFIG|WRITE|HEX)\b|\bFW \d+\.\d+",
     "AM32's titles and the strip's FW, now ESC CONFIG, ESC SETTINGS, ESC WRITE, "
     "ESC HEX and AM32 n.n (bluejay-blheli-s.md, call A)"),
    (r"ANSWERED: NOT READ YET",
     "slice 2's EFM8 status, now ANSWERED: READING. (bluejay-blheli-s.md, slice 3)"),
]
# Rule 9: image names of removed screens, hyphenated as in wiki/img/; a name
# that starts with one of these is refused, so kst-leave covers
# kst-leave-pending.
REMOVED_IMAGES = ["hitec-family", "hitec-unplug", "kst-leave", "servo-unplug",
                  "menu-pins-busy", "menu-beep"]


def collect_symbols():
    """Everything an @ref could legitimately point at."""
    symbols = set()
    for path in SOURCES:
        text = open(path, encoding="utf-8").read()
        base = path.split("/")[-1]
        symbols.update({path, base, base.replace(".ino", "")})
        symbols.update(re.findall(r"^\s*#define\s+(\w+)", text, re.M))
        symbols.update(re.findall(r"@defgroup\s+(\w+)", text))
        symbols.update(re.findall(r"@page\s+(\w+)", text))
        symbols.update(re.findall(r"^\s*enum\s+(?:class\s+)?(\w+)", text, re.M))
        symbols.update(re.findall(r"^\s*struct\s+(\w+)", text, re.M))
        symbols.update(re.findall(r"^\s*extern\s+[\w\s:*]+?(\w+)\s*[\[;]", text, re.M))
        symbols.update(
            re.findall(r"^\s*(?:static\s+)?(?:volatile\s+)?[\w:*<>]+\s+(\w+)\s*[=;(\[]",
                       text, re.M))
    return symbols


# An opening code fence: up to three spaces, then three or more backticks or
# tildes. A backtick fence may not have a backtick in its info string.
FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})(.*)$")


def strip_fences(text):
    """Drop fenced code blocks, ``` and ~~~ alike, indented at most 3 spaces.

    A block closes on a fence of the same character, at least as long as the
    opening one, with nothing after it. An unclosed block runs to the end.
    A fence nested in a list item (indented 4 spaces or more) and an
    indented code block are not recognised: links inside them are checked.
    No checked document uses either form.
    """
    out, fence = [], None
    for line in text.splitlines(keepends=True):
        m = FENCE.match(line.rstrip("\n"))
        if fence is None:
            if m and not (m.group(1)[0] == "`" and "`" in m.group(2)):
                fence = m.group(1)
            else:
                out.append(line)
        elif (m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence)
              and not m.group(2).strip()):
            fence = None
    return "".join(out)


def strip_code(text):
    """Drop fenced blocks and inline code spans; brackets there are not links."""
    return re.sub(r"`[^`\n]*`", "", strip_fences(text))


def heading_text(title):
    """A heading's rendered text: links become their label, markup is dropped.

    Emphasis delimiters (* and _ at the edge of a word) are markup, so
    '## _ital_' renders as 'ital'. An underscore inside a word, as in
    'snake_case', and anything inside a code span are literal text.
    """
    title = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", title)   # [label](url)
    title = re.sub(r"<[^>]+>", "", title)                         # inline HTML
    parts = re.split(r"(`[^`]*`)", title)
    for i in range(0, len(parts), 2):                             # outside code
        parts[i] = re.sub(r"(?<![\w\\])[*_]+|(?<!\\)[*_]+(?!\w)", "", parts[i])
    return "".join(parts).replace("`", "")


@functools.lru_cache(maxsize=None)
def heading_anchors(path):
    """The anchors GitHub generates for the headings of a Markdown file."""
    anchors, seen = set(), {}
    text = strip_fences(open(path, encoding="utf-8").read())
    for title in re.findall(r"^#{1,6}\s+(.*?)\s*#*\s*$", text, re.M):
        slug = re.sub(r"[^\w\- ]", "", heading_text(title).lower()).replace(" ", "-")
        n = seen.get(slug, 0)
        seen[slug] = n + 1
        anchors.add(slug if n == 0 else f"{slug}-{n}")
    return frozenset(anchors)


def check_links(path):
    """Rule 8: relative links in one Markdown file resolve."""
    problems = []
    # Code is not prose: a fenced block or an inline span may hold brackets.
    text = strip_code(open(path, encoding="utf-8").read())
    # Inline links: <angle-bracketed> targets may hold spaces and parentheses;
    # bare targets may hold one level of balanced parentheses. A title in
    # double quotes, single quotes or parentheses may follow.
    title = r"""(?:\s+(?:"[^"]*"|'[^']*'|\([^)]*\)))?"""
    targets = re.findall(r"\]\(\s*<([^>]+)>" + title + r"\s*\)", text)
    targets += re.findall(r"\]\(\s*((?:[^()\s<]|\([^()\s]*\))+)" + title + r"\s*\)", text)
    # Reference definitions; [^1]: is a footnote, not a link.
    targets += re.findall(r"^\s*\[(?!\^)[^\]]+\]:\s*<?([^\s>]+)>?", text, re.M)
    for target in targets:
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target):
            continue                      # https:, mailto: -- not ours to check
        file_part, _, anchor = target.partition("#")
        dest = os.path.normpath(os.path.join(os.path.dirname(path), file_part)) \
            if file_part else path
        if not os.path.exists(dest):
            problems.append(f"{path}: link '{target}' names a missing file")
        elif anchor and dest.endswith(".md") and anchor not in heading_anchors(dest):
            problems.append(f"{path}: link '{target}' names a missing heading")
    return problems


def removed_image(name):
    """The removed image @p name starts with, hyphenated, or None."""
    stem = name.replace("_", "-")
    for gone in REMOVED_IMAGES:
        if stem == gone or stem.startswith(gone + "-") or stem.startswith(gone + "."):
            return gone
    return None


def check_removed():
    """Rule 9: no removed screen text, view, MENU path or image comes back."""
    problems = []
    for path in MANUAL_DOCS:
        for n, line in enumerate(open(path, encoding="utf-8"), 1):
            for pattern, why in REMOVED_TEXTS:
                m = re.search(pattern, line)
                if m:
                    problems.append(f"{path}:{n}: '{m.group(0)}' names {why}")
            for name in re.findall(r"img/([\w.-]+)", line):
                gone = removed_image(name)
                if gone:
                    problems.append(f"{path}:{n}: img/{name} shows a removed screen ({gone})")
    shots = [(p, r'fakeDumpFrame\(\s*"shot_([\w.-]+)"') for p in sorted(glob.glob("test/*.cpp"))]
    shots.append(("tools/make_previews.py", r'"shot_([\w-]+)"'))
    for path, pattern in shots:
        if not os.path.exists(path):
            continue
        for n, line in enumerate(open(path, encoding="utf-8"), 1):
            for name in re.findall(pattern, line):
                gone = removed_image(name)
                if gone:
                    problems.append(f"{path}:{n}: shot_{name} renders a removed screen ({gone})")
    return problems


def spdx_id(path):
    """The SPDX licence id in the first 4096 bytes of @p path, or None."""
    with open(path, "rb") as f:
        text = f.read(4096).decode("utf-8", "replace")
    m = (SPDX_STEP_RE if path.endswith(".step") else SPDX_RE).search(text)
    return m.group(1) if m else None


def check_licences():
    """Rule 10: CC BY-NC-SA 4.0 on the models and CAD files, MIT elsewhere."""
    problems = []
    try:
        text = open(CC_FILE, encoding="utf-8").read()
    except OSError:
        return [f"{CC_FILE}: missing"]
    if spdx_id(CC_FILE) != CC_ID:
        problems.append(f"{CC_FILE}: no SPDX-License-Identifier: {CC_ID}")
    head, mark, legal = text.partition(CC_MARK)
    if not mark or hashlib.sha256(legal.encode()).hexdigest() != CC_SHA256:
        problems.append(f"{CC_FILE}: the legal code differs from CC BY-NC-SA 4.0")
    for d in CC_DIRS:
        if d + "/" not in head:
            problems.append(f"{CC_FILE}: the header does not name {d}/")
    covered = 0
    for root, dirs, files in os.walk("."):
        dirs[:] = sorted(x for x in dirs if x not in SKIP_DIRS or root != ".")
        rel_root = os.path.relpath(root, ".").replace(os.sep, "/")
        if rel_root.startswith(CC_EXEMPT):
            dirs[:] = []
            continue
        for name in sorted(files):
            path = f"{rel_root}/{name}" if rel_root != "." else name
            found = spdx_id(path)
            if path.startswith(tuple(d + "/" for d in CC_DIRS)):
                # .stp files and the Alibre files are exports; the
                # directory's licence covers them.
                if name.endswith((".py", ".step")):
                    covered += 1
                    if found != CC_ID:
                        problems.append(
                            f"{path}: SPDX-License-Identifier is "
                            f"{found or 'missing'}, expected {CC_ID}")
            elif path != CC_FILE and found is not None and found != "MIT":
                problems.append(
                    f"{path}: SPDX-License-Identifier {found} outside "
                    f"{', '.join(CC_DIRS)}; only MIT is expected")
    if not covered:
        problems.append("no .py or .step file found under " + ", ".join(CC_DIRS))
    return problems


def main():
    problems = []
    documented = 0
    symbols = collect_symbols()

    if not SOURCES:
        print(f"no sources found under {ROOT}/src")
        return 1

    for path in SOURCES:
        text = open(path, encoding="utf-8").read()

        # --- 2. @param names must match the signature that follows ---
        for m in re.finditer(r"/\*\*(.*?)\*/\s*\n([^\n]*)", text, re.S):
            names = []
            for group in re.findall(r"@param(?:\[[^\]]*\])?\s+([\w,]+)", m.group(1)):
                names.extend(group.split(","))
            if not names:
                continue
            documented += 1
            window = m.group(2) + text[m.end(2):m.end(2) + 300]
            sig = re.match(r".*?\((.*?)\)", window, re.S)
            if not sig:
                problems.append(f"{path}: @param block precedes no signature")
                continue
            actual = []
            for arg in sig.group(1).split(","):
                # Strip a default argument first. Without this the name is read
                # off the end of the whole clause, so `bool pressed = false`
                # reports its parameter as "false" -- and then demands that a
                # parameter called "false" be documented.
                arg = arg.split("=")[0]
                found = re.findall(r"(\w+)\s*$", arg.strip())
                if found and found[0] != "void":
                    actual.append(found[0])
            for name in names:
                if name not in actual:
                    problems.append(
                        f"{path}: @param '{name}' is not in ({', '.join(actual)})")

            # --- 3. all-or-nothing coverage ---
            undocumented = [a for a in actual if a not in names]
            if undocumented:
                problems.append(
                    f"{path}: partial @param coverage, missing "
                    f"{', '.join(undocumented)} (document all or none)")

        # --- 5. commands referred to in prose must be escaped ---
        for m in re.finditer(r"/\*\*(.*?)\*/", text, re.S):
            body = m.group(1)
            line = text[:m.start()].count("\n") + 1
            # A real @page/@mainpage is the first command in its block; any
            # later one is prose that Doxygen will execute.
            for cmd in ("mainpage", "page"):
                for hit in re.finditer(r"(?<!\\)@" + cmd + r"\b(.*)", body):
                    before = body[:hit.start()].strip(" *\n")
                    if before:
                        problems.append(
                            f"{path}:{line}: unescaped @{cmd} in prose "
                            f"(write \\@{cmd})")
                    elif cmd == "page" and not hit.group(1).strip():
                        problems.append(f"{path}:{line}: @page has no identifier")

            # --- 6. balanced backticks ---
            if body.count("`") % 2:
                problems.append(
                    f"{path}:{line}: odd number of backticks in a doc comment")

        # --- 4. @ref targets must exist ---
        for ref in re.findall(r"@ref\s+([\w./]+)", text):
            target = ref.rstrip(".")
            if target in symbols:
                continue
            if target.endswith((".h", ".md", ".ino", ".cpp")):
                continue
            problems.append(f"{path}: @ref '{target}' does not resolve")

        # --- 7. ...and must not run into a colon ---
        #
        # Rule 4 cannot see this. Its own pattern stops at the colon, so the
        # target looks fine and resolves; Doxygen's does not stop there, because
        # ':' is how Class::member is written. The result is a reference to
        # "pio_uart_rx.h:", which resolves to nothing -- and the only thing that
        # ever said so was Doxygen, in CI, after the commit.
        for m in re.finditer(r"@(?:ref|see)\s+([\w./]+):(?!:)", text):
            problems.append(
                f"{path}: @ref '{m.group(1)}:' runs into a colon; Doxygen reads "
                f"the colon as part of the name")

    # --- 1. public functions in headers need documentation ---
    for path in HEADERS:
        text = open(path, encoding="utf-8").read()
        for m in re.finditer(r"^((?:const\s+)?[\w:*]+(?:\s*\*)?)\s+(\w+)\s*\([^;]*\);",
                             text, re.M):
            before = text[:m.start()].rstrip()
            if not before.endswith("*/"):
                problems.append(f"{path}: {m.group(2)}() has no doc comment")

    # --- 8. relative links in the running record and design notes ---
    for path in LINKED_DOCS:
        problems.extend(check_links(path))

    # --- 9. removed screens stay out of the manual ---
    problems.extend(check_removed())

    # --- 10. licences of the hardware files ---
    problems.extend(check_licences())

    print(f"checked {len(SOURCES)} files, {documented} documented signatures, "
          f"links in {len(LINKED_DOCS)} documents, removed names in "
          f"{len(MANUAL_DOCS)} manual pages")
    if problems:
        print(f"\n{len(problems)} problem(s):")
        for p in problems:
            print(f"  {p}")
        return min(len(problems), 255)
    print("documentation checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

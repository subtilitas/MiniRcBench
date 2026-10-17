#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write THIRD_PARTY_NOTICES.txt for a firmware image, from the licence files.

    python3 tools/notices.py --sdk ~/pico-sdk --out dist/THIRD_PARTY_NOTICES.txt
    python3 tools/notices.py --check dist/THIRD_PARTY_NOTICES.txt
    python3 tools/notices.py --check dist/THIRD_PARTY_NOTICES.txt --sdk ~/pico-sdk

What is in the image comes from the image, not from a list kept here:

  - the DWARF (debugging format) compile units of the ELF (Executable and
    Linkable Format) file name every source file compiled into it, as the
    Firmware job of ci.yml reads them;
  - the linker map names every archive member and object file the toolchain
    contributes, which carry no debug information in the Arm GNU Toolchain:
    newlib, the C library, and the GCC (GNU Compiler Collection) runtime
    libraries.

Each of those is assigned to a component. A file no rule assigns fails the
run, so code with a licence of its own cannot reach the image unnoticed.

Each component's section names it, its version or commit as pinned in
CMakeLists.txt, release.yml or the Pico SDK (software development kit), the
files of it the image holds, the copyright and origin lines of those files,
and its licence text. The licence texts are copied from the files that ship
with the code: the fetched SD library in the build tree's _deps, the Pico SDK
checkout, and the license.txt of the Arm GNU Toolchain that compiled the image.
Nothing in this script restates a licence.

The commits are checked as well: the SDK checkout must be the commit
CMakeLists.txt pins and the version release.yml names, TinyUSB the commit
the SDK records for it, and the fetched SD library the commit CMakeLists.txt
pins.

--check reads a notices file back and fails when a section the file lists is
missing or carries no licence text. With --sdk, it also derives the component
list from the image and fails when the file lacks one. Every generating run
ends by checking the text it wrote, and by checking that the check rejects a
copy with one licence text emptied and a copy with one section removed.

Python 3 standard library only, and arm-none-eabi-readelf.
"""

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import textwrap

ROOT = pathlib.Path(__file__).resolve().parent.parent
RULE = "=" * 78
BEGIN = "----- licence text, copied from {} -----"
BEGIN_RE = re.compile(r"^----- licence text, copied from (.+) -----$")
END = "----- end of licence text -----"
MIN_LICENCE_LINES = 3


class NoticeError(Exception):
    """A notices file cannot be written or does not pass its check."""


def fail(msg):
    raise NoticeError(msg)


def run(cmd):
    try:
        return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        fail(f"{' '.join(map(str, cmd))}: {e}")


def git_head(path):
    return run(["git", "-C", str(path), "rev-parse", "HEAD"]).strip()


def read(path):
    try:
        return pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        fail(f"cannot read {path}: {e}")


# ------------------------------------------------------------------ pins

def pins():
    """Versions and commits as CMakeLists.txt and release.yml pin them."""
    cm = read(ROOT / "CMakeLists.txt")
    rel = read(ROOT / ".github/workflows/release.yml")
    out = {}
    m = re.search(r"set\(PICO_SDK_FETCH_FROM_GIT_TAG\s+([0-9a-f]{40})\)\s*#\s*(\S+)", cm)
    if not m:
        fail("CMakeLists.txt pins no Pico SDK commit")
    out["sdk_commit"], out["sdk_tag"] = m.group(1), m.group(2)
    m = re.search(r"FetchContent_Declare\(no_os_fatfs\s+GIT_REPOSITORY\s+(\S+)\s+"
                  r"GIT_TAG\s+([0-9a-f]{40})\s*#\s*(\S+)", cm)
    if not m:
        fail("CMakeLists.txt pins no commit for no_os_fatfs")
    out["sd_url"], out["sd_commit"], out["sd_tag"] = m.group(1), m.group(2), m.group(3)
    m = re.search(r'^\s*PICO_SDK_VERSION:\s*"([^"]+)"', rel, re.M)
    if not m:
        fail("release.yml names no PICO_SDK_VERSION")
    out["sdk_version"] = m.group(1)
    m = re.search(r"arm-none-eabi-gcc-action@\S+\s+with:\s+release:\s*\"([^\"]+)\"", rel)
    if not m:
        fail("release.yml names no toolchain release")
    out["toolchain"] = m.group(1)
    ver = read(ROOT / "src/version.h")
    parts = [re.search(rf"#define FW_VERSION_{p} +(\d+)", ver) for p in ("MAJOR", "MINOR", "PATCH")]
    if not all(parts):
        fail("src/version.h defines no complete version")
    out["fw_version"] = ".".join(p.group(1) for p in parts)
    return out


# ------------------------------------------------------------- inventory

def compile_units(readelf, elf):
    """Source paths of the image's DWARF compile units."""
    text = run([readelf, "--debug-dump=info", "--dwarf-depth=1", str(elf)])
    units, in_cu = set(), False
    for line in text.splitlines():
        if "DW_TAG_compile_unit" in line:
            in_cu = True
        elif in_cu and "DW_AT_name" in line:
            units.add(line.split(": ")[-1].strip())
            in_cu = False
    if not units:
        fail(f"{elf} has no DWARF compile units; build with debug information")
    return units


NOT_IN_IMAGE = re.compile(r"^\.(debug|comment|ARM\.attributes|stab|note|gnu)")


def map_inputs(map_path):
    """Linker inputs that place bytes in the image, from the linker map.

    Only the memory map counts: an input the linker loads and garbage-collects
    entirely is listed under the discarded sections, not there. Sections that
    hold only debug information or comments do not count either.
    """
    text = read(map_path)
    if "Linker script and memory map" not in text:
        fail(f"{map_path} has no memory map")
    inputs, out_sec = set(), ""
    for line in text.split("Linker script and memory map", 1)[1].splitlines():
        m = re.match(r"^(\.\S+)", line)
        if m:
            out_sec = m.group(1)
        if NOT_IN_IMAGE.match(out_sec):
            continue
        m = re.match(r"^\s+(?:\S+\s+)?0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(/\S+|\S+\.o\b\S*)$", line)
        if m and int(m.group(1), 16) > 0:
            inputs.add(m.group(2))
    if not inputs:
        fail(f"{map_path} places no input in the image")
    return inputs


NEWLIB_ARCHIVES = {"libc.a", "libg.a", "libm.a", "libnosys.a", "librdimon.a",
                   "libc_nano.a", "libg_nano.a", "libm_nano.a", "librdimon_nano.a"}
GCC_ARCHIVES = {"libgcc.a", "libstdc++.a", "libsupc++.a", "libstdc++_nano.a",
                "libsupc++_nano.a"}
GCC_OBJECTS = {"crti.o", "crtn.o", "crtbegin.o", "crtend.o"}
NEWLIB_OBJECTS = {"crt0.o"}


class Inventory:
    """What the image holds, per component key."""

    def __init__(self, build, sdk, elf, map_path, readelf):
        self.build = pathlib.Path(build).resolve()
        self.sdk = pathlib.Path(sdk).resolve()
        self.files = {}
        self.unassigned = []
        for cu in sorted(compile_units(readelf, elf)):
            key, rel = self.classify_source(cu)
            if key:
                self.files.setdefault(key, {})[rel] = cu
            else:
                self.unassigned.append(cu)
        for item in sorted(map_inputs(map_path)):
            if item.startswith("CMakeFiles/"):
                continue   # a compiled source: its compile unit is above
            m = re.match(r"^(.*\.a)\(([^)]+)\)$", item)
            name = os.path.basename(m.group(1) if m else item)
            if m:
                key = ("newlib" if name in NEWLIB_ARCHIVES
                       else "gcc" if name in GCC_ARCHIVES else None)
                shown = f"{name}({m.group(2)})"
            else:
                key = ("gcc" if name in GCC_OBJECTS
                       else "newlib" if name in NEWLIB_OBJECTS else None)
                shown = name
            if key:
                self.files.setdefault(key, {})[shown] = None
            else:
                self.unassigned.append(item)
        if self.unassigned:
            fail("code in the image that no component covers:\n  " +
                 "\n  ".join(self.unassigned))

    def classify_source(self, cu):
        p = pathlib.PurePosixPath(cu)
        if not p.is_absolute():
            # Toolchain libraries built with debug information name their
            # sources relative to their own build tree.
            if re.search(r"(^|/)(newlib|libgloss)/", cu):
                return "newlib", re.sub(r"^(\.\./)+", "", cu)
            if re.search(r"(^|/)(libgcc|libstdc\+\+-v3)/", cu):
                return "gcc", re.sub(r"^(\.\./)+", "", cu)
            return None, None
        p = pathlib.Path(os.path.normpath(cu))
        # (directory, component, the directory's path in the component's own
        # tree). build/sd_fatfs is the build's copy of the SD library's src/.
        rules = [
            (self.build / "sd_fatfs" / "ff15", "fatfs", "src/ff15"),
            (self.build / "sd_fatfs", "sdlib", "src"),
            (ROOT / "src", "own", "src"),
            (self.sdk / "lib" / "tinyusb", "tinyusb", ""),
            (self.sdk / "src" / "rp2_common" / "pico_printf", "printf",
             "src/rp2_common/pico_printf"),
            (self.sdk / "src" / "rp2_common" / "cmsis", "cmsis", "src/rp2_common/cmsis"),
            (self.sdk / "src", "sdk", "src"),
        ]
        # Any other submodule of the SDK (lib/mbedtls, lib/lwip, lib/btstack
        # and others) has a licence of its own, so it matches no rule.
        for base, key, shown in rules:
            if base in p.parents:
                return key, str(pathlib.PurePosixPath(shown) / p.relative_to(base))
        return None, None


# ------------------------------------------------------------ extraction

def toolchain_section(text, project):
    """One 'Contains code from project X' section of the toolchain license.txt."""
    lines = text.splitlines()
    start = next((i for i, l in enumerate(lines)
                  if l.startswith(f"Contains code from project {project}")), None)
    if start is None:
        fail(f"the toolchain licence file has no section for {project}")
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("Contains ")),
               len(lines))
    body = lines[start:end]
    while body and (not body[-1].strip() or set(body[-1].strip()) == {"-"}):
        body.pop()
    return "\n".join(body)


def leading_comment(path, until):
    """The comment at the top of a source file, up to the line matching @p until."""
    out = []
    for line in read(path).splitlines():
        if re.search(until, line):
            break
        if not line.lstrip().startswith(("//", "/*", "*")):
            break
        out.append(line)
    while out and not out[-1].strip("/ *"):
        out.pop()
    if not out:
        fail(f"{path} has no leading comment")
    return "\n".join(out)


def strip_comment(line):
    s = line.strip()
    s = re.sub(r"^(/\*+|\*+/|\*+|//+|/|#|;)\s?", "", s)
    s = re.sub(r"\s*(\*+/|/)$", "", s)
    return s.strip()


COPYRIGHT = re.compile(r"(?i)copyright\s*(\(c\)|©|\d{4})|^\\?author\s+\(c\)|^\(c\)\s+\w")
ORIGIN = re.compile(r"(?i)\b(derived from|borrows heavily from|based on code from|ported from)\b")


def notice_lines(path):
    """Copyright lines, and origin paragraphs, from the top 80 lines of a file."""
    lines = read(path).splitlines()[:80]
    out, i = [], 0
    while i < len(lines):
        s = strip_comment(lines[i])
        if COPYRIGHT.search(s) and "notice" not in s.lower():
            out.append(s)
        elif ORIGIN.search(s):
            para = [s]
            j = i + 1
            while j < len(lines) and strip_comment(lines[j]) and not lines[j].strip().startswith("#"):
                para.append(strip_comment(lines[j]))
                j += 1
            out.append(" ".join(para))
            i = j
            continue
        i += 1
    return out


# -------------------------------------------------------------- sections

NAMES = {
    "own": "MiniRcBench",
    "sdk": "Raspberry Pi Pico SDK",
    "printf": "printf by Marco Paland, the Pico SDK's pico_printf",
    "cmsis": "CMSIS (Common Microcontroller Software Interface Standard) by Arm, in the Pico SDK",
    "tinyusb": "TinyUSB (USB, Universal Serial Bus, device stack)",
    "sdlib": "no-OS-FatFS-SD-SDIO-SPI-RPi-Pico (SD card driver and FatFs glue)",
    "fatfs": "FatFs by ChaN (FAT file system)",
    "newlib": "newlib (C library) and libgloss",
    "gcc": "GCC runtime libraries: libgcc, libstdc++ (its language support part), start-up files",
}
"""Section titles, per component key."""


class Section:
    def __init__(self, key, version, source, licence, files, text_from, text,
                 notes=(), scan=False):
        self.key, self.name, self.version, self.source = key, NAMES[key], version, source
        self.licence, self.files, self.text_from, self.text = licence, files, text_from, text
        self.notes, self.scan = list(notes), scan

    def render(self, n):
        out = [RULE, f"{n}. {self.name}", RULE, ""]
        for label, value in (("Version", self.version), ("Source", self.source),
                             ("Licence", self.licence)):
            out.append(wrap(value, f"{label}:".ljust(10), " " * 10))
        out.append("")
        for note in self.notes:
            out += [wrap(note), ""]
        out.append(f"Files of it in the image ({len(self.files)}):")
        out += [f"  {f}" for f in sorted(self.files)]
        out.append("")
        if self.scan:
            found = []
            for f in sorted(self.files):
                for line in notice_lines(self.files[f]):
                    if line not in found:
                        found.append(line)
            if found:
                out.append("Copyright and origin lines in those files:")
                out += [wrap(line, "  ", "    ") for line in found]
                out.append("")
        out += [BEGIN.format(self.text_from), self.text.rstrip("\n"), END, ""]
        return "\n".join(out)


def wrap(text, first="", rest=""):
    return textwrap.fill(text, 78, initial_indent=first, subsequent_indent=rest,
                         break_long_words=False, break_on_hyphens=False)


def licence_file(base, *names):
    for name in names:
        if (base / name).is_file():
            return base / name
    fail(f"no licence file ({', '.join(names)}) in {base}")


def build_sections(inv, args, pin):
    sdk, build = inv.sdk, inv.build
    sd_src = build / "_deps" / "no_os_fatfs-src"
    tc_text = read(args.toolchain_licence)
    comment = run([args.readelf, "-p", ".comment", str(args.elf)])
    m = re.search(r"GCC: \((.*)\) (\S+)", comment)
    built_by = f"{m.group(1)}, GCC {m.group(2)}" if m else "unknown"

    # The commits the notices name are the commits that were compiled.
    sdk_head = git_head(sdk)
    if sdk_head != pin["sdk_commit"]:
        fail(f"the SDK checkout is {sdk_head}; CMakeLists.txt pins {pin['sdk_commit']}")
    v = read(sdk / "pico_sdk_version.cmake")
    sdk_ver = ".".join(re.search(rf"set\(PICO_SDK_VERSION_{p} (\d+)\)", v).group(1)
                       for p in ("MAJOR", "MINOR", "REVISION"))
    if sdk_ver != pin["sdk_version"]:
        fail(f"the SDK checkout is version {sdk_ver}; release.yml names {pin['sdk_version']}")
    tu = sdk / "lib" / "tinyusb"
    tu_head = git_head(tu)
    tu_pin = run(["git", "-C", str(sdk), "ls-tree", "HEAD", "lib/tinyusb"]).split()[2]
    if tu_head != tu_pin:
        fail(f"lib/tinyusb is {tu_head}; SDK {sdk_ver} records {tu_pin}")
    o = read(tu / "src" / "tusb_option.h")
    tu_ver = ".".join(re.search(rf"#define TUSB_VERSION_{p}\s+(\d+)", o).group(1)
                      for p in ("MAJOR", "MINOR", "REVISION"))
    sd_head = git_head(sd_src)
    if sd_head != pin["sd_commit"]:
        fail(f"the fetched SD library is {sd_head}; CMakeLists.txt pins {pin['sd_commit']}")
    ff = re.search(r"FatFs - Generic FAT Filesystem Module\s+(R[0-9.]+(?: w/patch\d+)?)",
                   read(sd_src / "src" / "ff15" / "source" / "ff.c"), re.I)
    ff_ver = ff.group(1) if ff else "unknown"

    notice = [p for p in sd_src.iterdir() if p.name.upper().startswith("NOTICE")]
    sd_notes = ["The library has no NOTICE file (Apache License 2.0, section 4(d))."]
    if notice:
        sd_notes = [f"NOTICE file, copied from the library's {notice[0].name}:", "",
                    read(notice[0]).rstrip()]

    sdk_short, sd_short = pin["sdk_commit"][:12], pin["sd_commit"][:12]
    s = []
    s.append(Section(
        "own", f"{pin['fw_version']} (src/version.h), commit {git_head(ROOT)[:12]}",
        "https://github.com/subtilitas/MiniRcBench", "MIT",
        inv.files.get("own", ()), "LICENSE", read(ROOT / "LICENSE")))
    s.append(Section(
        "sdk", f"{sdk_ver}, commit {sdk_short} (CMakeLists.txt, release.yml)",
        "https://github.com/raspberrypi/pico-sdk", "BSD-3-Clause",
        inv.files.get("sdk", ()), "pico-sdk LICENSE.TXT", read(sdk / "LICENSE.TXT"),
        scan=True))
    s.append(Section(
        "printf", f"as in Pico SDK {sdk_ver}, commit {sdk_short}",
        "https://github.com/raspberrypi/pico-sdk", "MIT",
        inv.files.get("printf", ()),
        "the leading comment of pico-sdk src/rp2_common/pico_printf/printf.c",
        leading_comment(sdk / "src/rp2_common/pico_printf/printf.c", r"\\brief")))
    s.append(Section(
        "cmsis", f"as in Pico SDK {sdk_ver}, commit {sdk_short}",
        "https://github.com/raspberrypi/pico-sdk", "Apache-2.0",
        inv.files.get("cmsis", ()), "pico-sdk src/rp2_common/cmsis/stub/CMSIS/LICENSE.txt",
        read(licence_file(sdk / "src/rp2_common/cmsis/stub/CMSIS", "LICENSE.txt")),
        notes=["The CMSIS core headers, whose inline functions the SDK compiles, are under "
               "the same licence."],
        scan=True))
    s.append(Section(
        "tinyusb", f"{tu_ver}, commit {tu_head[:12]}, the submodule of Pico SDK {sdk_ver}",
        "https://github.com/hathach/tinyusb", "MIT",
        inv.files.get("tinyusb", ()), "pico-sdk lib/tinyusb/LICENSE",
        read(licence_file(tu, "LICENSE")), scan=True))
    s.append(Section(
        "sdlib", f"{pin['sd_tag']}, commit {sd_short} (CMakeLists.txt)",
        pin["sd_url"].removesuffix(".git"), "Apache-2.0",
        inv.files.get("sdlib", ()), "no-OS-FatFS-SD-SDIO-SPI-RPi-Pico LICENSE",
        read(licence_file(sd_src, "LICENSE")), notes=sd_notes,
        scan=True))
    s.append(Section(
        "fatfs", f"{ff_ver}, as in no-OS-FatFS-SD-SDIO-SPI-RPi-Pico {pin['sd_tag']}, commit {sd_short}",
        "http://elm-chan.org/fsw/ff/", "FatFs licence (BSD-style, one condition)",
        inv.files.get("fatfs", ()), "no-OS-FatFS-SD-SDIO-SPI-RPi-Pico src/ff15/LICENSE.txt",
        read(licence_file(sd_src / "src" / "ff15", "LICENSE.txt")),
        scan=True))
    tc = f"Arm GNU Toolchain {pin['toolchain']} (release.yml); the image is built by {built_by}"
    s.append(Section(
        "newlib", tc,
        "https://sourceware.org/newlib/", "several permissive licences, listed in the text",
        inv.files.get("newlib", ()),
        "the NEWLIB and LIBGLOSS section of the toolchain's license.txt",
        toolchain_section(tc_text, "NEWLIB and LIBGLOSS")))
    s.append(Section(
        "gcc", tc, "https://gcc.gnu.org",
        "GPL-3.0-or-later WITH GCC-exception-3.1",
        inv.files.get("gcc", ()),
        "the GNU GCC section of the toolchain's license.txt",
        toolchain_section(tc_text, "GNU GCC"),
        notes=["The GCC Runtime Library Exception at the top of the text covers code from "
               "these libraries in a compiled program; the licences after it cover the "
               "rest of GCC, which is not in the image."]))
    present = [x for x in s if x.files]
    missing = [x.name for x in s if not x.files and x.key in ("own", "sdk")]
    if missing:
        fail("no file of " + ", ".join(missing) + " found in the image")
    return present, built_by


def render(sections, built_by, release):
    what = f"the MiniRcBench {release} release image" if release else "the MiniRcBench image"
    head = [
        "MiniRcBench: third-party notices",
        "",
        wrap(f"This file accompanies {what}, built by {built_by}. It lists every "
             "component whose code that image contains, with the licence text of each, "
             "copied from the licence files that ship with the code. tools/notices.py "
             "writes it from the image's debug information and linker map."),
        "",
        "Sections:",
    ]
    head += [f"  {i}. {x.name}" for i, x in enumerate(sections, 1)]
    head.append("")
    return "\n".join(head) + "\n" + "\n".join(x.render(i) for i, x in enumerate(sections, 1))


# ----------------------------------------------------------------- check

def parse(text):
    """Section names listed in the index, and per section its licence text."""
    lines = text.splitlines()
    try:
        i = lines.index("Sections:")
    except ValueError:
        fail("no section index")
    index = []
    for line in lines[i + 1:]:
        m = re.match(r"^  (\d+)\. (.+)$", line)
        if not m:
            break
        index.append(m.group(2))
    bodies, j = {}, 0
    while j < len(lines):
        if lines[j] == RULE and j + 2 < len(lines) and lines[j + 2] == RULE:
            name = re.sub(r"^\d+\. ", "", lines[j + 1])
            k, text_lines, inside = j + 3, None, False
            while k < len(lines) and lines[k] != RULE:
                if BEGIN_RE.match(lines[k]):
                    inside, text_lines = True, []
                elif lines[k] == END:
                    inside = False
                elif inside:
                    text_lines.append(lines[k])
                k += 1
            if inside:
                text_lines = None   # no end marker: truncated
            bodies[name] = text_lines
            j = k
        else:
            j += 1
    return index, bodies


def check(text, expected=None):
    index, bodies = parse(text)
    problems = []
    if not index:
        problems.append("the section index is empty")
    for name in index:
        body = bodies.get(name)
        if body is None:
            problems.append(f"section '{name}' is missing, or its licence text is not closed")
        elif sum(1 for l in body if l.strip()) < MIN_LICENCE_LINES:
            problems.append(f"section '{name}' has an empty licence text")
    for name in (expected or ()):
        if name not in index:
            problems.append(f"the image holds {name}, and the file has no section for it")
    return problems


def selftest(text):
    """The check must reject a copy with an emptied text and one with a section cut."""
    index, _ = parse(text)
    lines = text.splitlines()
    b = next(i for i, l in enumerate(lines) if BEGIN_RE.match(l))
    e = lines.index(END, b)
    emptied = "\n".join(lines[:b + 1] + lines[e:])
    s = lines.index(RULE, lines.index(RULE) + 3)   # the second section's rule
    t = next((i for i in range(s + 3, len(lines)) if lines[i] == RULE), len(lines))
    cut = "\n".join(lines[:s] + lines[t:])
    if not check(emptied) or not check(cut):
        fail("the check accepts a notices file with an empty or a missing section")
    return len(index)


# ------------------------------------------------------------------ main

def default_toolchain_licence(readelf):
    exe = shutil.which(readelf)
    if not exe:
        return None
    prefix = pathlib.Path(os.path.realpath(exe)).parent.parent
    return prefix / "license.txt"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--build", default=str(ROOT / "build"), help="CMake build tree")
    ap.add_argument("--sdk", default=os.environ.get("PICO_SDK_PATH"),
                    help="Pico SDK checkout the image was built with (default $PICO_SDK_PATH)")
    ap.add_argument("--elf", help="image ELF (default: the build's MiniRcBench-touch-lcd-2.8.elf)")
    ap.add_argument("--map", help="linker map (default: the ELF's .map beside it)")
    ap.add_argument("--readelf", default="arm-none-eabi-readelf")
    ap.add_argument("--toolchain-licence",
                    help="license.txt of the Arm GNU Toolchain (default: beside its bin/)")
    ap.add_argument("--release", help="release tag the file accompanies")
    ap.add_argument("--out", help="file to write")
    ap.add_argument("--check", metavar="FILE", help="check a notices file instead of writing one")
    args = ap.parse_args()
    args.elf = pathlib.Path(args.elf or pathlib.Path(args.build) / "MiniRcBench-touch-lcd-2.8.elf")
    args.map = pathlib.Path(args.map or f"{args.elf}.map")

    try:
        if args.check:
            expected = None
            if args.sdk:
                inv = Inventory(args.build, args.sdk, args.elf, args.map, args.readelf)
                expected = [NAMES[k] for k in NAMES if k in inv.files]
            problems = check(read(args.check), expected)
            if problems:
                fail(f"{args.check}:\n  " + "\n  ".join(problems))
            print(f"{args.check}: {len(parse(read(args.check))[0])} sections, each with its licence text")
            return 0

        if not args.sdk or not args.out:
            ap.error("writing needs --sdk (or PICO_SDK_PATH) and --out")
        if not args.toolchain_licence:
            args.toolchain_licence = default_toolchain_licence(args.readelf)
        if not args.toolchain_licence or not pathlib.Path(args.toolchain_licence).is_file():
            fail(f"no toolchain licence file at {args.toolchain_licence}; the Arm GNU "
                 "Toolchain keeps license.txt beside bin/, or pass --toolchain-licence")
        inv = Inventory(args.build, args.sdk, args.elf, args.map, args.readelf)
        sections, built_by = build_sections(inv, args, pins())
        text = render(sections, built_by, args.release)
        problems = check(text, [x.name for x in sections])
        if problems:
            fail("the generated text fails its own check:\n  " + "\n  ".join(problems))
        selftest(text)
        out = pathlib.Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="utf-8")
        print(f"{out}: {len(sections)} sections, {len(text.encode())} bytes")
        for i, x in enumerate(sections, 1):
            n = len(x.files)
            print(f"  {i}. {x.name}: {n} file{'s' * (n != 1)}, {x.licence}, from {x.text_from}")
        print("check: rejects an emptied licence text and a removed section")
        return 0
    except NoticeError as e:
        print(f"notices: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

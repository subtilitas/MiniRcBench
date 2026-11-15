#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-SA-4.0
"""Adapted from rcbench hardware/mechanical/gen_models.py, commit fb41a46a,
where it is MIT.

Write the STEP models of the mechanical parts from their specs.

The specs are hardware/mechanical/*.py other than this file, and the
specs LIB_SPECS names in lib/rcbench/hardware/mechanical/, which are
rcbench's files unchanged; each defines ``parts(colors)``, a dict of
file stem to solids as lib/rcbench/hardware/kicad/tools/step_gen.py's
``write`` takes them. The models go to
hardware/mechanical/models/<stem>.step.

Usage:
    python3 hardware/mechanical/gen_models.py           write the files
    python3 hardware/mechanical/gen_models.py --check   exit 1 when a file
                                                        differs from its
                                                        spec
"""

import importlib.util
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODELS = HERE / "models"
sys.path.insert(0, str(HERE.parent.parent / "lib" / "rcbench" / "hardware"
                       / "kicad" / "tools"))
import step_gen  # noqa: E402

LIB = HERE.parent.parent / "lib" / "rcbench" / "hardware" / "mechanical"
# rcbench's specs of the modules this bench is built with.
LIB_SPECS = ("ina219_cjmcu219", "boost_5v_en")
# The helpers a spec imports. Both directories have modules of these
# names, so each directory's specs load with only its own in sys.modules.
HELPERS = ("_solid", "_parts", "_font", "_board", "_print")


def _load(directory, names):
    """Load specs of one directory with that directory's helpers.

    Args:
        directory: The directory of the specs and their helpers.
        names: The specs' file names without ".py".

    Returns:
        The modules, in the order of ``names``.
    """
    for helper in HELPERS:
        sys.modules.pop(helper, None)
    sys.path.insert(0, str(directory))
    try:
        specs = []
        for name in names:
            spec = importlib.util.spec_from_file_location(
                name, directory / f"{name}.py")
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            specs.append(mod)
    finally:
        sys.path.remove(str(directory))
        for helper in HELPERS:
            sys.modules.pop(helper, None)
    return specs


def load_specs():
    """Load every spec of hardware/mechanical/ and the LIB_SPECS.

    Returns:
        The modules: this directory's sorted by file name, then
        rcbench's.
    """
    own = sorted(path.stem for path in HERE.glob("*.py")
                 if path.name != Path(__file__).name
                 and not path.name.startswith(("_", "check_")))
    return _load(HERE, own) + _load(LIB, LIB_SPECS)


def outputs():
    """Build every model in memory.

    Returns:
        A dict of path to (file text, solids).
    """
    out = {}
    for spec in load_specs():
        for stem, items in spec.parts(step_gen.COLORS).items():
            out[MODELS / f"{stem}.step"] = (
                step_gen.write(stem, items), items)
    return out


def main(argv):
    """Write or check every model.

    Args:
        argv: Command-line arguments without the program name.

    Returns:
        0 when every file is current or written, 1 for a stale file in check
        mode.
    """
    check = "--check" in argv
    stale = []
    for path, (text, _items) in outputs().items():
        old = path.read_text() if path.exists() else None
        if old == text:
            continue
        if check:
            stale.append(path.relative_to(HERE.parent.parent))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(HERE.parent.parent)}")
    for p in stale:
        print(f"STALE {p}: run hardware/mechanical/gen_models.py")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

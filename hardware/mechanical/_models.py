# SPDX-License-Identifier: CC-BY-NC-SA-4.0
"""Where each model of hardware/mechanical/models/ lies.

    models/pcb/      boards with their parts
    models/casing/   printed parts of the case
    models/parts/    bought parts, and the assembled bench

The folders are those of rcbench's hardware/mechanical/models/. Every
stem is in exactly one folder. gen_models.py asks ``path`` for a spec's
file; a stem that is not listed here is an error, so a new model is
sorted when it is added. renders/ has the same folders: the pictures of
a model, <stem>_<view>.png, lie in the model's folder.
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE / "models"
RENDERS = HERE / "renders"
FOLDERS = {
    "pcb": ("boost_5v_en", "ina219_cjmcu219", "rp2350_touch_lcd_2_8"),
    "casing": ("button", "case_shell_esp_rp_touch_lcd"),
    "parts": ("lipo_102540_1000mah", "minircbench"),
}
FOLDER = {stem: folder for folder, stems in FOLDERS.items()
          for stem in stems}
SUFFIXES = (".step", ".stp")


def path(stem, suffix=".step"):
    """The file of one model.

    Args:
        stem: The model's name without suffix.
        suffix: ".step" or ".stp".

    Returns:
        The path models/<folder>/<stem><suffix>.

    Raises:
        KeyError: The stem is in no folder of FOLDERS.
    """
    if stem not in FOLDER:
        raise KeyError(f"{stem} is in no folder of _models.FOLDERS")
    return ROOT / FOLDER[stem] / f"{stem}{suffix}"


def render_dir(directory, stem):
    """The folder a model's pictures go to; it exists afterwards.

    Args:
        directory: The renders' root.
        stem: The model's name.

    Returns:
        The path <directory>/<folder>.

    Raises:
        KeyError: The stem is in no folder of FOLDERS.
    """
    if stem not in FOLDER:
        raise KeyError(f"{stem} is in no folder of _models.FOLDERS")
    out = Path(directory) / FOLDER[stem]
    out.mkdir(parents=True, exist_ok=True)
    return out


def _stem(name):
    """The model a picture <stem>_<view>.png shows, or None."""
    fits = [stem for stem in FOLDER if name.startswith(stem + "_")]
    return max(fits, key=len) if fits else None


def strays():
    """The files under models/ and renders/ that lie elsewhere.

    Returns:
        The paths, sorted: a file beside the folders, a file in a folder
        its model is not listed for, or a file of another kind.
    """
    out = []
    for found in sorted(ROOT.rglob("*")):
        if found.is_dir() or found.name.startswith("."):
            continue
        if found.suffix not in SUFFIXES or found.parent != (
                ROOT / str(FOLDER.get(found.stem))):
            out.append(found)
    for found in sorted(RENDERS.rglob("*")):
        if found.is_dir():
            continue
        folder = FOLDER.get(_stem(found.stem))
        if found.suffix != ".png" or found.parent != RENDERS / str(folder):
            out.append(found)
    return out

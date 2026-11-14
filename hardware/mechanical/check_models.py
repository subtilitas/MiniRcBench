#!/usr/bin/env python3
"""Adapted from rcbench hardware/kicad/tools/step_check.py and
step_render.py, commit fb41a46a, MIT.

Read the written STEP models back with OpenCASCADE and check them.

For every file in hardware/mechanical/models/:

- it reads, and holds as many solids as its spec makes;
- every solid is a valid closed shell with a volume above 0;
- no two solids share volume (more than 1e-6 mm3);
- the bounding box is printed.

Usage:
    python3 hardware/mechanical/check_models.py
    xvfb-run -a python3 hardware/mechanical/check_models.py --render DIR

--render also draws every model to DIR/<stem>_<view>.png, 1600 x 1200,
from 4 sides: rear, front, rear straight on with y up, and the USB-C
end.

Exit status 1 when a check fails. Needs OCP (pip install cadquery-ocp);
--render needs an OpenGL context: an X display, or xvfb-run with Mesa.
"""

import argparse
import sys
from pathlib import Path

from OCP.Bnd import Bnd_Box
from OCP.BRepAlgoAPI import BRepAlgoAPI_Common
from OCP.BRepBndLib import BRepBndLib
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from OCP.IFSelect import IFSelect_RetDone
from OCP.Quantity import Quantity_Color, Quantity_TOC_RGB
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TCollection import TCollection_AsciiString, TCollection_ExtendedString
from OCP.TDocStd import TDocStd_Document
from OCP.TopAbs import TopAbs_SOLID
from OCP.TopExp import TopExp_Explorer
from OCP.XCAFDoc import XCAFDoc_ColorSurf, XCAFDoc_DocumentTool

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_models  # noqa: E402

SHARED = 1e-6           # mm3, the most two solids may share
VIEWS = {"rear": "V3d_XposYnegZneg", "front": "V3d_XnegYnegZpos",
         "rear_flat": "V3d_Zneg", "usb_end": "V3d_Yneg"}


def read(path):
    """Read one STEP file.

    Args:
        path: The file.

    Returns:
        A list of (solid, colour or None), in file order.

    Raises:
        RuntimeError: When the file does not read.
    """
    doc = TDocStd_Document(TCollection_ExtendedString("minircbench"))
    reader = STEPCAFControl_Reader()
    reader.SetColorMode(True)
    if (reader.ReadFile(str(path)) != IFSelect_RetDone
            or not reader.Transfer(doc)):
        raise RuntimeError(f"{path}: does not read")
    colors = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())
    found = []
    walk = TopExp_Explorer(reader.Reader().OneShape(), TopAbs_SOLID)
    while walk.More():
        col = Quantity_Color()
        ok = colors.GetColor(walk.Current(), XCAFDoc_ColorSurf, col)
        found.append((walk.Current(),
                      (col.Red(), col.Green(), col.Blue()) if ok else None))
        walk.Next()
    return found


def _volume(shape):
    props = GProp_GProps()
    BRepGProp.VolumeProperties_s(shape, props)
    return props.Mass()


def check(path, n_solids):
    """Check one model.

    Args:
        path: The STEP file.
        n_solids: The number of solids its spec makes.

    Returns:
        (errors, box): a list of error lines, and the bounding box as
        (x0, y0, z0, x1, y1, z1).
    """
    solids = [solid for solid, _colour in read(path)]
    errors, boxes = [], []
    if len(solids) != n_solids:
        errors.append(f"{len(solids)} solids, the spec makes {n_solids}")
    for i, solid in enumerate(solids):
        if not BRepCheck_Analyzer(solid).IsValid():
            errors.append(f"solid {i} is not valid")
        if _volume(solid) <= 0:
            errors.append(f"solid {i} has no volume")
        box = Bnd_Box()
        box.SetGap(0.0)
        BRepBndLib.AddOptimal_s(solid, box, False, False)
        lo, hi = box.CornerMin(), box.CornerMax()
        boxes.append((lo.X(), lo.Y(), lo.Z(), hi.X(), hi.Y(), hi.Z()))
    for i, a in enumerate(boxes):
        for j in range(i):
            b = boxes[j]
            if all(a[k] < b[k + 3] - 1e-4 and b[k] < a[k + 3] - 1e-4
                   for k in range(3)):
                both = BRepAlgoAPI_Common(solids[i], solids[j]).Shape()
                shared = _volume(both)
                if shared > SHARED:
                    errors.append(f"solids {j} and {i} share {shared:.4f}mm3")
    whole = tuple(f(b[k] for b in boxes) for f, ks in
                  ((min, range(3)), (max, range(3, 6))) for k in ks)
    return errors, whole


def render(path, out_dir, size=(1600, 1200)):
    """Draw one model from each of VIEWS.

    Args:
        path: The STEP file.
        out_dir: Where the PNGs go.
        size: Image width and height in pixels.

    Raises:
        RuntimeError: When an image cannot be written.
    """
    from OCP import V3d
    from OCP.AIS import AIS_InteractiveContext, AIS_Shape
    from OCP.Aspect import Aspect_DisplayConnection, Aspect_TypeOfLine
    from OCP.Graphic3d import (
        Graphic3d_BufferType,
        Graphic3d_MaterialAspect,
        Graphic3d_NameOfMaterial,
    )
    from OCP.Image import Image_AlienPixMap
    from OCP.OpenGl import OpenGl_GraphicDriver
    from OCP.Prs3d import Prs3d_LineAspect
    from OCP.Xw import Xw_Window

    def rgb(r, g, b):
        return Quantity_Color(r, g, b, Quantity_TOC_RGB)

    display = Aspect_DisplayConnection()
    driver = OpenGl_GraphicDriver(display, True)
    viewer = V3d.V3d_Viewer(driver)
    viewer.SetDefaultLights()
    viewer.SetLightOn()
    view = viewer.CreateView()
    window = Xw_Window(display, "check_models", 0, 0, *size)
    window.SetVirtual(True)
    view.SetWindow(window)
    view.SetBackgroundColor(rgb(1.0, 1.0, 1.0))
    view.ChangeRenderingParams().NbMsaaSamples = 8
    context = AIS_InteractiveContext(viewer)
    edge = Prs3d_LineAspect(rgb(0.1, 0.1, 0.1),
                            Aspect_TypeOfLine.Aspect_TOL_SOLID, 1.0)
    for solid, colour in read(path):
        shown = AIS_Shape(solid)
        shown.SetColor(rgb(*(colour or (1.0, 0.0, 1.0))))
        shown.SetMaterial(Graphic3d_MaterialAspect(
            Graphic3d_NameOfMaterial.Graphic3d_NOM_PLASTIC))
        shown.Attributes().SetFaceBoundaryDraw(True)
        shown.Attributes().SetFaceBoundaryAspect(edge)
        context.Display(shown, 1, -1, False)
    for name, side in VIEWS.items():
        view.SetProj(getattr(V3d.V3d_TypeOfOrientation, side))
        if name == "rear_flat":
            view.SetUp(0.0, 1.0, 0.0)
        view.FitAll(0.04, False)
        view.Redraw()
        image = Image_AlienPixMap()
        out = out_dir / f"{path.stem}_{name}.png"
        if not (view.ToPixMap(image, *size, Graphic3d_BufferType.Graphic3d_BT_RGB)
                and image.Save(TCollection_AsciiString(str(out)))):
            raise RuntimeError(f"{out}: not written")
        print(f"wrote {out}")


def main(argv):
    """Run from the command line.

    Args:
        argv: Command-line arguments without the program name.

    Returns:
        0 when every model passes, else 1.
    """
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", type=Path, metavar="DIR")
    args = ap.parse_args(argv)
    made = {}
    for spec in gen_models.load_specs():
        made.update(spec.parts(gen_models.step_gen.COLORS))
    bad = 0
    for stem, solids in sorted(made.items()):
        path = gen_models.MODELS / f"{stem}.step"
        errors, box = check(path, len(solids))
        size = " x ".join(f"{box[k + 3] - box[k]:.2f}" for k in range(3))
        print(f"{'FAIL' if errors else 'ok  '} {path.name}: {len(solids)} "
              f"solids, {size} mm")
        for line in errors:
            print(f"     {line}")
        bad += bool(errors)
        if args.render:
            args.render.mkdir(parents=True, exist_ok=True)
            render(path, args.render)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

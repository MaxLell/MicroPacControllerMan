"""Check the enclosure against the boards it has to hold.

    FreeCAD.AppImage freecadcmd verify_fit.py <directory with the enclosure STEPs>

Every solid of both board models is placed where the assembly puts it and intersected
with the base and the lid. Anything with a volume is a collision, and is printed with
the component's name so it can be found.

Two features of ST's models are drawings rather than geometry and would otherwise report
as collisions -- the display's flex tail is drawn unfolded and reaches far past the board,
and through-hole pins are drawn at full, unclipped length. Both are clipped away here
rather than skipped silently, so a real part that happens to be low or long still counts.
"""

import sys

import FreeCAD
import Import
import Part
from FreeCAD import Vector

CAD = "%s/../Cad" % (sys.argv[2] if len(sys.argv) > 2 else ".")
SHIELD_SHIFT = Vector(0.0, -16.80, 12.65)
PIN_TAIL_LIMIT = -3.52  # a soldered and clipped pin tail reaches about here
COLLISION_LIMIT_MM3 = 0.001


def prv_solids(in_path, in_shift):
    document = FreeCAD.newDocument(in_path.rsplit("/", 1)[-1])
    Import.insert(in_path, document.Name)
    document.recompute()
    out = []
    for obj in document.Objects:
        if obj.TypeId != "Part::Feature":
            continue
        shape = obj.Shape
        if shape.isNull() or not shape.Solids:
            continue
        placed = shape.copy()
        placed.Placement = obj.getGlobalPlacement()
        placed.translate(in_shift)
        out.append((obj.Label, placed))
    return out


def prv_load(in_path):
    shape = Part.Shape()
    shape.read(in_path)
    return shape


directory = sys.argv[2] if len(sys.argv) > 2 else "."
base = prv_load("%s/enclosure-base.step" % directory)
lid = prv_load("%s/enclosure-lid.step" % directory)

parts = prv_solids("%s/MB1841-U545REQ-D01.step" % CAD, Vector(0, 0, 0))
parts += prv_solids("%s/MB1642-TCXD022IB5-D01.step" % CAD, SHIELD_SHIFT)
print("checking %d component solids" % len(parts), flush=True)

# The board outline plus everything above a clipped pin tail: outside this the models are
# drawing the flex tail and the unclipped pins, not the hardware.
real = Part.makeBox(200, 200, 200, Vector(-60, -17.0, PIN_TAIL_LIMIT))

collisions = 0
for name, solid in parts:
    box = solid.BoundBox
    if box.ZMax < PIN_TAIL_LIMIT or box.YMax < -17.0:
        continue
    if box.ZMin < PIN_TAIL_LIMIT or box.YMin < -17.0:
        solid = solid.common(real)
        if solid.isNull() or solid.Volume < COLLISION_LIMIT_MM3:
            continue
    for part_name, part in (("base", base), ("lid", lid)):
        if not box.intersect(part.BoundBox):
            continue
        hit = solid.common(part)
        if not hit.isNull() and hit.Volume > COLLISION_LIMIT_MM3:
            collisions += 1
            hb = hit.BoundBox
            print(
                "COLLISION %-24s with %-4s %8.2f mm3 at x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f"
                % (name[:24], part_name, hit.Volume, hb.XMin, hb.XMax, hb.YMin, hb.YMax, hb.ZMin, hb.ZMax),
                flush=True,
            )

print("COLLISIONS %d" % collisions, flush=True)

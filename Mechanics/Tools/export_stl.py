"""Tessellate a board STEP file into an STL, for looking at rather than for printing.

Run it through FreeCAD's console binary:

    FreeCAD.AppImage freecadcmd export_stl.py ../Cad/MB1841-U545REQ-D01.step ../Stl/MB1841-U545REQ-D01.stl

An optional fourth argument is the linear deflection in millimetres -- how far a
triangle may stray from the true surface. It trades file size against roundness;
0.1 mm is the default and keeps a whole board under a few megabytes.

Only Part::Feature objects holding solids are meshed. The STEP hierarchy also
carries App::Part containers and datum lines, planes and points, and meshing
those would either fail or duplicate geometry. Each solid's shape is local to its
container, so getGlobalPlacement() is what puts it where it belongs -- without it
every component lands on the origin.
"""

import sys

import FreeCAD
import Import
import Mesh
import MeshPart
import Part

ANGULAR_DEFLECTION = 0.7
DEFAULT_LINEAR_DEFLECTION_MM = 0.1


def prv_collect_solids(in_document):
    solids = []
    for obj in in_document.Objects:
        if obj.TypeId != "Part::Feature":
            continue
        shape = obj.Shape
        if shape.isNull() or not shape.Solids:
            continue
        placed = shape.copy()
        placed.Placement = obj.getGlobalPlacement()
        solids.append(placed)
    return solids


# argv is ["FreeCAD", "<this script>", "<step file>", "<stl file>", "<deflection>"].
source_path, target_path = sys.argv[2], sys.argv[3]
linear_deflection = float(sys.argv[4]) if len(sys.argv) > 4 else DEFAULT_LINEAR_DEFLECTION_MM

document = FreeCAD.newDocument("export")
Import.insert(source_path, document.Name)
document.recompute()

solids = prv_collect_solids(document)
compound = Part.makeCompound(solids)
box = compound.BoundBox
print("%d solids, %.2f x %.2f x %.2f mm" % (len(solids), box.XLength, box.YLength, box.ZLength))

mesh = MeshPart.meshFromShape(
    Shape=compound,
    LinearDeflection=linear_deflection,
    AngularDeflection=ANGULAR_DEFLECTION,
    Relative=False,
)
print("%d facets at %.2f mm deflection" % (mesh.CountFacets, linear_deflection))
Mesh.Mesh(mesh).write(target_path)

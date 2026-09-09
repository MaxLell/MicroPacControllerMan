"""Report the geometry of a board STEP file, so enclosure dimensions can be read
off the vendor's own model instead of a caliper.

Run it through FreeCAD's console binary, not a system python:

    FreeCAD.AppImage freecadcmd measure_step.py bbox  ../Cad/MB1642-TCXD022IB5-D01.step
    FreeCAD.AppImage freecadcmd measure_step.py holes ../Cad/MB1841-U545REQ-D01.step

"bbox" lists every named part with its bounding box, largest first -- the part
names come from the STEP's PRODUCT entries, which ST fills with the schematic
designators (LCD1, B1, CN2, ...). "holes" lists the cylindrical faces of the bare
PCB solid, which is where the mounting holes are.

Both boards put their origin on the top face of the PCB, so a positive Z is the
height of a component above the board it sits on.
"""

import sys

import FreeCAD
import Import

HOLE_RADIUS_MM = (0.8, 3.5)
PART_LIMIT = 40


def prv_open(in_path):
    document = FreeCAD.newDocument("measure")
    Import.insert(in_path, document.Name)
    document.recompute()
    return document


def prv_report_bounding_boxes(in_document):
    rows = []
    for obj in in_document.Objects:
        shape = getattr(obj, "Shape", None)
        if shape is None or shape.isNull():
            continue
        box = shape.BoundBox
        rows.append((obj.Label, box.XMin, box.XMax, box.YMin, box.YMax, box.ZMin, box.ZMax, shape.Volume))

    print("parts with a shape: %d" % len(rows))
    print("%-32s %9s %9s %9s %9s %9s %9s" % ("label", "xmin", "xmax", "ymin", "ymax", "zmin", "zmax"))
    for row in sorted(rows, key=lambda row: -row[7])[:PART_LIMIT]:
        print("%-32s %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f" % row[:7])


def prv_report_holes(in_document):
    for obj in in_document.Objects:
        if obj.Label != "Board":
            continue
        shape = obj.Shape
        box = shape.BoundBox
        print("board %.2f x %.2f mm, %.2f mm thick" % (box.XLength, box.YLength, box.ZLength))

        already_seen = []
        for face in shape.Faces:
            surface = face.Surface
            if not hasattr(surface, "Radius"):
                continue
            radius = surface.Radius
            if not HOLE_RADIUS_MM[0] <= radius <= HOLE_RADIUS_MM[1]:
                continue
            centre = surface.Center
            key = (round(centre.x, 2), round(centre.y, 2), round(radius, 2))
            if key in already_seen:
                continue
            already_seen.append(key)
            print("hole x=%7.2f y=%7.2f diameter=%5.2f depth=%5.2f" % (centre.x, centre.y, 2 * radius, face.BoundBox.ZLength))


# argv is ["FreeCAD", "<this script>", "<mode>", "<step file>"] under freecadcmd.
mode, path = sys.argv[2], sys.argv[3]
document = prv_open(path)
if mode == "bbox":
    prv_report_bounding_boxes(document)
elif mode == "holes":
    prv_report_holes(document)
else:
    raise SystemExit("unknown mode %r -- expected 'bbox' or 'holes'" % mode)

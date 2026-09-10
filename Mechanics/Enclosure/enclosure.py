"""The 3D-printed enclosure: a base and a lid that snaps into it.

    FreeCAD.AppImage freecadcmd enclosure.py .

The argument is the directory to write into. Two parts come out, each as STEP (exact)
and STL (for the slicer).

Everything is in the Nucleo's own coordinates -- X and Y on its PCB outline, Z = 0 on
its top face -- so every number below can be checked against Mechanics/README.md.

The one number that is derived rather than measured is SHIELD_PCB_TOP. It follows from
the connectors, because the two board models are each referenced to their own PCB and
neither states the mated height. The display sits flush in the lid *because of* that
number: get it wrong and the display is recessed or proud by exactly the error.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(sys.argv[1])))

import FreeCAD
import Mesh
import MeshPart
import Part
from FreeCAD import Vector

import game_font

# ---------------------------------------------------------------- the boards

NUCLEO_X = (0.0, 70.00)
NUCLEO_Y = (0.0, 82.50)
NUCLEO_PCB_THICKNESS = 1.52
NUCLEO_HOLES = ((10.87, 54.57), (59.13, 53.30), (43.89, 2.50))
NUCLEO_HOLE_DIAMETER = 3.20

USB_X = (42.99, 52.19)
USB_Z = (-1.93, 3.25)

# The shield, already shifted into the Nucleo's coordinates (its own Y is 16.80 higher).
SHIELD_X = (-0.13, 70.23)
SHIELD_Y = (-16.93, 58.26)
SHIELD_PCB_TOP = 12.65
SHIELD_TALLEST_NEIGHBOUR = 1.40  # above the shield's PCB, ignoring display and joystick

DISPLAY_X = (15.67, 54.25)
DISPLAY_Y = (-8.91, 46.43)
DISPLAY_TOP = 15.15

JOYSTICK_CENTRE = (63.05, -6.51)
JOYSTICK_KEEPOUT = 10.76

# ------------------------------------------------------------- the enclosure

WALL = 2.4
FLOOR = 2.4
PRINT_CLEARANCE = 0.2  # what a Prusa Mini needs for one part to sit inside another

STANDOFF_HEIGHT = 4.0  # clears the soldered pin tails under the Nucleo
STANDOFF_DIAMETER = 6.0
PEG_HEIGHT = 3.0
PEG_DIAMETER = NUCLEO_HOLE_DIAMETER - 0.3

DISPLAY_GAP = 0.3  # around the display module in the lid
# The joystick's knob has to be able to tilt, and 0.32 mm around the keepout was not enough
# to do it in. 1.40 is close to the most this board allows: the knob's centre is 8.50 mm from
# the display window and 7.18 mm from the shield's own edge, so a symmetric opening wider than
# about 14.4 mm breaks out past the board, and one wider than this leaves under 1.7 mm of lid
# between the two openings -- a strip that cannot be made thicker either, because two 0.56 mm
# resistors sit under it.
JOYSTICK_GAP = 1.40  # around the joystick's keepout
JOYSTICK_CORNER_RADIUS = 3.0
JOYSTICK_CHAMFER = 0.5  # the top edge is bevelled outwards, where the tilting knob needs it
SHIELD_RIM_OVERLAP = 1.0  # how far the lid's full-thickness rim rests on the shield
POCKET_FLEX_MARGIN = 0.6  # the pocket runs this far past the shield's flex edge
OUTER_CORNER_RADIUS = 3.0
INNER_CORNER_RADIUS = 1.5

# The snap. The part that flexes is a horizontal tongue cut out of the base's wall, not a
# finger hanging off the lid, and that is a layer-direction decision rather than a strength
# one: printed lid-face-down, a hanging finger bends *across* its layers, which is the one
# direction PLA is weak in. A tongue in a wall printed floor-down bends within them.
#
# Peak strain in a rectangular cantilever is 1.5 * thickness * deflection / length^2. Here
# that is 0.56 %, about 20 MPa, against roughly 55 MPa for PLA along its layers. Note that
# thickness is in the numerator: making the spring beefier makes it break sooner.
TONGUE_LENGTH = 28.0
TONGUE_HEIGHT = 6.0
SLOT_WIDTH = 1.2
WALL_ABOVE_TONGUE = 2.0  # what the lid still rests on
NOSE_PROJECTION = 0.90  # inward from the wall's inner face
NOSE_WIDTH = 8.0
NOSE_LEAD_IN = 1.8  # the ramp the lid's nub rides in on
NOSE_INSET = 6.0  # from the tongue's free end, where the leverage is

NUB_GAP = 0.15  # the lid's nub to the wall
NUB_THICKNESS = 2.2
NUB_WIDTH = 12.0
NUB_BOTTOM = 2.0
SNAP_CLEARANCE = 3.0  # board edge to inner wall: enough for the lid's nub and its gap

# The front and back faces are chamfered at 45 degrees. Both print flat on the bed, so the
# chamfer rises off it at 45 degrees and needs no support either way round.
EDGE_CHAMFER = 1.5

# The legend on the front face, in the game's own font -- read out of the firmware by
# game_font.py rather than redrawn, so the two cannot drift apart. The font has no lower
# case, so "Pacman AI" is set as PACMAN AI. Sunk two 0.2 mm layers deep.
LEGEND_TEXT = "PACMAN AI"
LEGEND_PIXEL = 0.9  # mm per font pixel; the letters are seven pixels tall
LEGEND_DEPTH = 0.4

# Where the six snaps sit: (axis of the wall's normal, which side, centre along it). The
# one on the USB wall is off to the side because the socket's opening is in the middle.
SNAPS = (("x", 0, 8.0), ("x", 0, 58.0), ("x", 1, 8.0), ("x", 1, 58.0), ("y", 0, 35.0), ("y", 1, 20.0))

# ----------------------------------------------------------------- derived

FLOOR_TOP = -NUCLEO_PCB_THICKNESS - STANDOFF_HEIGHT
FLOOR_BOTTOM = FLOOR_TOP - FLOOR
WALL_TOP = SHIELD_PCB_TOP  # the lid's full-thickness underside rests here
LID_TOP = DISPLAY_TOP  # flush with the display glass -- this is the whole point
POCKET_FLOOR = SHIELD_PCB_TOP + SHIELD_TALLEST_NEIGHBOUR + 0.10

ASSEMBLY_X = (min(NUCLEO_X[0], SHIELD_X[0]), max(NUCLEO_X[1], SHIELD_X[1]))
ASSEMBLY_Y = (SHIELD_Y[0], NUCLEO_Y[1] + 0.85)  # the USB socket overhangs the board edge

INTERIOR_X = (ASSEMBLY_X[0] - SNAP_CLEARANCE, ASSEMBLY_X[1] + SNAP_CLEARANCE)
INTERIOR_Y = (ASSEMBLY_Y[0] - SNAP_CLEARANCE, ASSEMBLY_Y[1] + SNAP_CLEARANCE)
OUTER_X = (INTERIOR_X[0] - WALL, INTERIOR_X[1] + WALL)
OUTER_Y = (INTERIOR_Y[0] - WALL, INTERIOR_Y[1] + WALL)

POCKET_X = (SHIELD_X[0] + SHIELD_RIM_OVERLAP, SHIELD_X[1] - SHIELD_RIM_OVERLAP)
POCKET_Y = (SHIELD_Y[0] - POCKET_FLEX_MARGIN, SHIELD_Y[1] - SHIELD_RIM_OVERLAP)

ENGAGEMENT = NOSE_PROJECTION - NUB_GAP  # how far the nose actually overlaps the nub
RECESS = ENGAGEMENT + 0.05  # the nub steps in by this above its ledge, leaving the nose room
TONGUE_Z = (WALL_TOP - WALL_ABOVE_TONGUE - SLOT_WIDTH - TONGUE_HEIGHT, WALL_TOP - WALL_ABOVE_TONGUE - SLOT_WIDTH)
LEDGE_Z = TONGUE_Z[0] + (TONGUE_HEIGHT - NOSE_PROJECTION - NOSE_LEAD_IN) / 2.0

# Centred across the lid, and centred in the blank band the display leaves above itself.
LEGEND_CENTRE = (
    (OUTER_X[0] + OUTER_X[1]) / 2.0,
    ((DISPLAY_Y[1] + DISPLAY_GAP) + (OUTER_Y[1] - EDGE_CHAMFER)) / 2.0,
)


def prv_rounded_box(in_x, in_y, in_z, in_radius):
    box = Part.makeBox(
        in_x[1] - in_x[0], in_y[1] - in_y[0], in_z[1] - in_z[0], Vector(in_x[0], in_y[0], in_z[0])
    )
    if in_radius <= 0:
        return box
    upright = [e for e in box.Edges if abs(e.Vertexes[0].Point.z - e.Vertexes[-1].Point.z) > 1e-6]
    return box.makeFillet(in_radius, upright)


def prv_box(in_x, in_y, in_z):
    return prv_rounded_box(in_x, in_y, in_z, 0.0)


def prv_pillar(in_centre, in_diameter, in_z):
    return Part.makeCylinder(in_diameter / 2.0, in_z[1] - in_z[0], Vector(in_centre[0], in_centre[1], in_z[0]))


def prv_snap_frame(in_axis, in_side):
    """Where a wall is, and which way is out of the box through it."""
    interior = INTERIOR_X if in_axis == "x" else INTERIOR_Y
    outer = OUTER_X if in_axis == "x" else OUTER_Y
    inward = 1.0 if in_side == 0 else -1.0
    return interior[in_side], outer[in_side], inward


def prv_across(in_axis, in_face, in_inward, in_offsets, in_along, in_z_pairs):
    """A prism whose cross section is given as (offset inward from the wall face, z)."""
    points = []
    for offset, z in zip(in_offsets, in_z_pairs):
        depth = in_face + in_inward * offset
        points.append(Vector(depth, in_along[0], z) if in_axis == "x" else Vector(in_along[0], depth, z))
    face = Part.Face(Part.makePolygon(points + [points[0]]))
    span = in_along[1] - in_along[0]
    return face.extrude(Vector(0, span, 0) if in_axis == "x" else Vector(span, 0, 0))


def prv_slab(in_axis, in_face, in_inward, in_depths, in_along, in_z):
    """A box given as a depth range measured inward from the wall face."""
    low = in_face + in_inward * in_depths[0]
    high = in_face + in_inward * in_depths[1]
    depth = (min(low, high), max(low, high))
    return prv_box(depth, in_along, in_z) if in_axis == "x" else prv_box(in_along, depth, in_z)


def prv_snap_slots(in_axis, in_side, in_centre):
    """The cut that frees the tongue: under it, over it, and past its free end."""
    face, outside, inward = prv_snap_frame(in_axis, in_side)
    through = (-1.0, -(abs(outside - face) + 1.0))  # from inside the box out through the wall
    anchor, free = in_centre - TONGUE_LENGTH / 2.0, in_centre + TONGUE_LENGTH / 2.0
    return (
        prv_slab(in_axis, face, inward, through, (anchor, free + SLOT_WIDTH), (TONGUE_Z[0] - SLOT_WIDTH, TONGUE_Z[0]))
        .fuse(prv_slab(in_axis, face, inward, through, (anchor, free + SLOT_WIDTH), (TONGUE_Z[1], TONGUE_Z[1] + SLOT_WIDTH)))
        .fuse(
            prv_slab(
                in_axis, face, inward, through, (free, free + SLOT_WIDTH),
                (TONGUE_Z[0] - SLOT_WIDTH, TONGUE_Z[1] + SLOT_WIDTH),
            )
        )
    )


def prv_snap_nose(in_axis, in_side, in_centre):
    """The catch on the tongue's inner face: a 45 degree retention face, then a gentle ramp."""
    face, _, inward = prv_snap_frame(in_axis, in_side)
    at = in_centre + TONGUE_LENGTH / 2.0 - NOSE_INSET
    return prv_across(
        in_axis, face, inward,
        (0.0, NOSE_PROJECTION, 0.0),
        (at - NOSE_WIDTH / 2.0, at + NOSE_WIDTH / 2.0),
        (LEDGE_Z, LEDGE_Z + NOSE_PROJECTION, LEDGE_Z + NOSE_PROJECTION + NOSE_LEAD_IN),
    )


def prv_snap_nub(in_axis, in_side, in_centre):
    """The lid's half: rigid, and stepped so the nose has something to sit on."""
    face, _, inward = prv_snap_frame(in_axis, in_side)
    at = in_centre + TONGUE_LENGTH / 2.0 - NOSE_INSET
    along = (at - NUB_WIDTH / 2.0, at + NUB_WIDTH / 2.0)
    wide = (along[0] - 1.0, along[1] + 1.0)

    nub = prv_slab(in_axis, face, inward, (NUB_GAP, NUB_GAP + NUB_THICKNESS), along, (NUB_BOTTOM, WALL_TOP))
    nub = nub.cut(
        prv_across(
            in_axis, face, inward,
            (NUB_GAP, NUB_GAP + RECESS, NUB_GAP + RECESS, NUB_GAP),
            wide,
            (LEDGE_Z, LEDGE_Z + RECESS, WALL_TOP + 1.0, WALL_TOP + 1.0),
        )
    )
    return nub.cut(
        prv_across(
            in_axis, face, inward,
            (NUB_GAP, NUB_GAP + 1.0, NUB_GAP),
            wide,
            (NUB_BOTTOM, NUB_BOTTOM, NUB_BOTTOM + 1.0),
        )
    )


def prv_outline_edges(in_shape, in_z):
    """The edges of one flat face's outer boundary — the ones a chamfer should take, and
    not the openings cut into the same face."""
    box = in_shape.BoundBox
    picked = []
    for edge in in_shape.Edges:
        limits = edge.BoundBox
        if abs(limits.ZMin - in_z) > 1e-6 or abs(limits.ZMax - in_z) > 1e-6:
            continue
        if (
            limits.XMin <= box.XMin + 1e-6
            or limits.XMax >= box.XMax - 1e-6
            or limits.YMin <= box.YMin + 1e-6
            or limits.YMax >= box.YMax - 1e-6
        ):
            picked.append(edge)
    return picked


def prv_legend():
    """The letters as one compound of boxes, ready to be cut into the front face.

    Runs of ink along a row become one box each rather than one box per pixel: the result
    is identical and there are a tenth as many solids to subtract.
    """
    grid = game_font.render(LEGEND_TEXT)
    first_column, last_column, first_row, last_row = game_font.ink_extent(grid)
    left = LEGEND_CENTRE[0] - (last_column - first_column) * LEGEND_PIXEL / 2.0
    top = LEGEND_CENTRE[1] + (last_row - first_row) * LEGEND_PIXEL / 2.0

    boxes = []
    for row_index, cells in enumerate(grid):
        column = 0
        while column < len(cells):
            if not cells[column]:
                column += 1
                continue
            run = column
            while run < len(cells) and cells[run]:
                run += 1
            boxes.append(
                Part.makeBox(
                    (run - column) * LEGEND_PIXEL,
                    LEGEND_PIXEL,
                    LEGEND_DEPTH + 1.0,
                    Vector(
                        left + (column - first_column) * LEGEND_PIXEL,
                        top - (row_index - first_row + 1) * LEGEND_PIXEL,
                        LID_TOP - LEGEND_DEPTH,
                    ),
                )
            )
            column = run
    return Part.makeCompound(boxes)


def prv_build_base():
    shell = prv_rounded_box(OUTER_X, OUTER_Y, (FLOOR_BOTTOM, WALL_TOP), OUTER_CORNER_RADIUS)
    cavity = prv_rounded_box(INTERIOR_X, INTERIOR_Y, (FLOOR_TOP, WALL_TOP + 1.0), INNER_CORNER_RADIUS)
    base = shell.cut(cavity)

    for centre in NUCLEO_HOLES:
        base = base.fuse(prv_pillar(centre, STANDOFF_DIAMETER, (FLOOR_TOP, -NUCLEO_PCB_THICKNESS)))
        base = base.fuse(
            prv_pillar(centre, PEG_DIAMETER, (-NUCLEO_PCB_THICKNESS, -NUCLEO_PCB_THICKNESS + PEG_HEIGHT))
        )

    # The tongues are cut out of the wall, so releasing one is a fingernail from outside
    # rather than a paper clip through a hole.
    for snap in SNAPS:
        base = base.cut(prv_snap_slots(*snap))
    for snap in SNAPS:
        base = base.fuse(prv_snap_nose(*snap))

    # The USB plug's overmould is wider than the socket, so the opening is sized for the
    # plug rather than for the receptacle measured out of the model.
    usb_centre_x = (USB_X[0] + USB_X[1]) / 2.0
    usb_centre_z = (USB_Z[0] + USB_Z[1]) / 2.0
    base = base.cut(
        Part.makeBox(14.0, WALL + 2.0, 9.0, Vector(usb_centre_x - 7.0, INTERIOR_Y[1] - 1.0, usb_centre_z - 4.5))
    )
    return base.makeChamfer(EDGE_CHAMFER, prv_outline_edges(base, FLOOR_BOTTOM))


def prv_build_lid():
    lid = prv_rounded_box(OUTER_X, OUTER_Y, (WALL_TOP, LID_TOP), OUTER_CORNER_RADIUS)

    for snap in SNAPS:
        lid = lid.fuse(prv_snap_nub(*snap))

    # The lid is thin only where it has to be: over the shield there are 2.50 mm between
    # the PCB and the display's glass, and a 1.40 mm component in them. Everywhere else it
    # keeps its full 2.5 mm. The rim left standing rests on the shield's bare edge and is
    # what holds the board stack down -- on three sides only, because the display's flex
    # crosses the fourth on its way to the slot and stands 1.30 mm proud there.
    lid = lid.cut(prv_box(POCKET_X, POCKET_Y, (WALL_TOP, POCKET_FLOOR)))

    lid = lid.cut(
        prv_box(
            (DISPLAY_X[0] - DISPLAY_GAP, DISPLAY_X[1] + DISPLAY_GAP),
            (DISPLAY_Y[0] - DISPLAY_GAP, DISPLAY_Y[1] + DISPLAY_GAP),
            (WALL_TOP - 1.0, LID_TOP + 1.0),
        )
    )

    # The joystick's actuator is drawn simplified in ST's model, so the opening is sized to
    # the 10.76 mm keepout the model does state rather than to a cap diameter it does not.
    side = JOYSTICK_KEEPOUT + 2 * JOYSTICK_GAP
    span_x = (JOYSTICK_CENTRE[0] - side / 2.0, JOYSTICK_CENTRE[0] + side / 2.0)
    span_y = (JOYSTICK_CENTRE[1] - side / 2.0, JOYSTICK_CENTRE[1] + side / 2.0)
    lid = lid.cut(prv_rounded_box(span_x, span_y, (WALL_TOP - 1.0, LID_TOP + 1.0), JOYSTICK_CORNER_RADIUS))

    # Bevel its top edge outwards. A tilting knob leans furthest at the top, which is exactly
    # where the lid is in its way, and the bevel widens the opening there without touching the
    # 1.7 mm of lid left standing between it and the display window.
    rim = [
        edge
        for edge in lid.Edges
        if abs(edge.BoundBox.ZMin - LID_TOP) < 1e-6
        and abs(edge.BoundBox.ZMax - LID_TOP) < 1e-6
        and span_x[0] - 0.01 <= edge.BoundBox.XMin
        and edge.BoundBox.XMax <= span_x[1] + 0.01
        and span_y[0] - 0.01 <= edge.BoundBox.YMin
        and edge.BoundBox.YMax <= span_y[1] + 0.01
    ]
    lid = lid.makeChamfer(JOYSTICK_CHAMFER, rim)

    lid = lid.makeChamfer(EDGE_CHAMFER, prv_outline_edges(lid, LID_TOP))
    return lid.cut(prv_legend())


def prv_write(in_shape, in_directory, in_name):
    in_shape.exportStep("%s/%s.step" % (in_directory, in_name))
    mesh = MeshPart.meshFromShape(Shape=in_shape, LinearDeflection=0.05, AngularDeflection=0.5, Relative=False)
    Mesh.Mesh(mesh).write("%s/%s.stl" % (in_directory, in_name))
    box = in_shape.BoundBox
    print(
        "%-16s %6.2f x %6.2f x %6.2f mm   %9.1f mm3   solid=%s   %d facets"
        % (in_name, box.XLength, box.YLength, box.ZLength, in_shape.Volume, in_shape.isValid(), mesh.CountFacets)
    )


directory = sys.argv[2] if len(sys.argv) > 2 else "."
print("outer %.2f x %.2f x %.2f mm" % (OUTER_X[1] - OUTER_X[0], OUTER_Y[1] - OUTER_Y[0], LID_TOP - FLOOR_BOTTOM))
print("lid over the shield %.2f mm, elsewhere %.2f mm" % (LID_TOP - POCKET_FLOOR, LID_TOP - WALL_TOP))
print(
    "front and back chamfered %.1f mm at 45 deg; legend %r sunk %.1f mm"
    % (EDGE_CHAMFER, LEGEND_TEXT, LEGEND_DEPTH)
)
print(
    "joystick opening %.2f mm square, %.2f mm at the top face"
    % (JOYSTICK_KEEPOUT + 2 * JOYSTICK_GAP, JOYSTICK_KEEPOUT + 2 * JOYSTICK_GAP + 2 * JOYSTICK_CHAMFER)
)
print(
    "snap: %d tongues, %.2f mm engagement, peak strain %.2f %%"
    % (
        len(SNAPS),
        ENGAGEMENT,
        100.0 * 1.5 * WALL * ENGAGEMENT / (TONGUE_LENGTH - NOSE_INSET) ** 2,
    )
)
prv_write(prv_build_base(), directory, "enclosure-base")
prv_write(prv_build_lid(), directory, "enclosure-lid")

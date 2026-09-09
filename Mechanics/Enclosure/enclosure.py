"""The 3D-printed enclosure: a base and a lid, built from the boards' measured geometry.

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

import sys

import FreeCAD
import Mesh
import MeshPart
import Part
from FreeCAD import Vector

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
BOARD_GAP = 0.6  # board edge to inner wall
PRINT_CLEARANCE = 0.2  # what a Prusa Mini needs for one part to sit inside another

STANDOFF_HEIGHT = 4.0  # clears the soldered pin tails under the Nucleo
STANDOFF_DIAMETER = 6.0
PEG_HEIGHT = 3.0
PEG_DIAMETER = NUCLEO_HOLE_DIAMETER - 0.3

END_MARGIN = 7.0  # room at the two short ends for the screw bosses
BOSS_DIAMETER = 6.4
INSERT_DIAMETER = 4.0  # M3 heat-set insert
INSERT_DEPTH = 6.0
SCREW_CLEARANCE_DIAMETER = 3.4
SCREW_HEAD_DIAMETER = 6.0
SCREW_HEAD_DEPTH = 1.3
BOSS_X = (12.0, 58.0)

LIP_HEIGHT = 2.0
LIP_WIDTH = 1.6
DISPLAY_GAP = 0.3  # around the display module in the lid
JOYSTICK_GAP = 0.32  # around the joystick's keepout
JOYSTICK_CORNER_RADIUS = 2.0
SHIELD_RIM_OVERLAP = 1.0  # how far the lid's full-thickness rim rests on the shield
OUTER_CORNER_RADIUS = 3.0
INNER_CORNER_RADIUS = 1.5

# ----------------------------------------------------------------- derived

FLOOR_TOP = -NUCLEO_PCB_THICKNESS - STANDOFF_HEIGHT
FLOOR_BOTTOM = FLOOR_TOP - FLOOR
WALL_TOP = SHIELD_PCB_TOP  # the lid's full-thickness underside rests here
LID_TOP = DISPLAY_TOP  # flush with the display glass -- this is the whole point
POCKET_FLOOR = SHIELD_PCB_TOP + SHIELD_TALLEST_NEIGHBOUR + 0.10

ASSEMBLY_X = (min(NUCLEO_X[0], SHIELD_X[0]), max(NUCLEO_X[1], SHIELD_X[1]))
ASSEMBLY_Y = (SHIELD_Y[0], NUCLEO_Y[1] + 0.85)  # the USB socket overhangs the board edge

INTERIOR_X = (ASSEMBLY_X[0] - BOARD_GAP, ASSEMBLY_X[1] + BOARD_GAP)
INTERIOR_Y = (ASSEMBLY_Y[0] - BOARD_GAP - END_MARGIN, ASSEMBLY_Y[1] + BOARD_GAP + END_MARGIN)
OUTER_X = (INTERIOR_X[0] - WALL, INTERIOR_X[1] + WALL)
OUTER_Y = (INTERIOR_Y[0] - WALL, INTERIOR_Y[1] + WALL)

POCKET_X = (SHIELD_X[0] + SHIELD_RIM_OVERLAP, SHIELD_X[1] - SHIELD_RIM_OVERLAP)
POCKET_Y = (ASSEMBLY_Y[0] - BOARD_GAP, SHIELD_Y[1] - SHIELD_RIM_OVERLAP)

BOSS_Y = (
    (INTERIOR_Y[0] + ASSEMBLY_Y[0] - BOARD_GAP) / 2.0,
    (INTERIOR_Y[1] + ASSEMBLY_Y[1] + BOARD_GAP) / 2.0,
)
SCREW_POSITIONS = [(x, y) for x in BOSS_X for y in BOSS_Y]


def prv_rounded_box(in_x, in_y, in_z, in_radius):
    box = Part.makeBox(
        in_x[1] - in_x[0], in_y[1] - in_y[0], in_z[1] - in_z[0], Vector(in_x[0], in_y[0], in_z[0])
    )
    if in_radius <= 0:
        return box
    upright = [e for e in box.Edges if abs(e.Vertexes[0].Point.z - e.Vertexes[-1].Point.z) > 1e-6]
    return box.makeFillet(in_radius, upright)


def prv_pillar(in_centre, in_diameter, in_z):
    return Part.makeCylinder(in_diameter / 2.0, in_z[1] - in_z[0], Vector(in_centre[0], in_centre[1], in_z[0]))


def prv_build_base():
    shell = prv_rounded_box(OUTER_X, OUTER_Y, (FLOOR_BOTTOM, WALL_TOP), OUTER_CORNER_RADIUS)
    cavity = prv_rounded_box(INTERIOR_X, INTERIOR_Y, (FLOOR_TOP, WALL_TOP + 1.0), INNER_CORNER_RADIUS)
    base = shell.cut(cavity)

    for centre in NUCLEO_HOLES:
        base = base.fuse(prv_pillar(centre, STANDOFF_DIAMETER, (FLOOR_TOP, -NUCLEO_PCB_THICKNESS)))
        base = base.fuse(
            prv_pillar(centre, PEG_DIAMETER, (-NUCLEO_PCB_THICKNESS, -NUCLEO_PCB_THICKNESS + PEG_HEIGHT))
        )

    for centre in SCREW_POSITIONS:
        base = base.fuse(prv_pillar(centre, BOSS_DIAMETER, (FLOOR_TOP, WALL_TOP)))
    for centre in SCREW_POSITIONS:
        base = base.cut(prv_pillar(centre, INSERT_DIAMETER, (WALL_TOP - INSERT_DEPTH, WALL_TOP + 1.0)))

    # The USB plug's overmould is wider than the socket, so the opening is sized for the
    # plug rather than for the receptacle measured out of the model.
    usb_centre_x = (USB_X[0] + USB_X[1]) / 2.0
    usb_centre_z = (USB_Z[0] + USB_Z[1]) / 2.0
    opening = Part.makeBox(
        14.0, WALL + 2.0, 9.0, Vector(usb_centre_x - 7.0, INTERIOR_Y[1] - 1.0, usb_centre_z - 4.5)
    )
    return base.cut(opening)


def prv_build_lid():
    plate = prv_rounded_box(OUTER_X, OUTER_Y, (WALL_TOP, LID_TOP), OUTER_CORNER_RADIUS)

    lip_outer = (INTERIOR_X[0] + PRINT_CLEARANCE, INTERIOR_X[1] - PRINT_CLEARANCE)
    lip_outer_y = (INTERIOR_Y[0] + PRINT_CLEARANCE, INTERIOR_Y[1] - PRINT_CLEARANCE)
    lip = prv_rounded_box(lip_outer, lip_outer_y, (WALL_TOP - LIP_HEIGHT, WALL_TOP), INNER_CORNER_RADIUS)
    lip = lip.cut(
        prv_rounded_box(
            (lip_outer[0] + LIP_WIDTH, lip_outer[1] - LIP_WIDTH),
            (lip_outer_y[0] + LIP_WIDTH, lip_outer_y[1] - LIP_WIDTH),
            (WALL_TOP - LIP_HEIGHT - 1.0, WALL_TOP + 1.0),
            INNER_CORNER_RADIUS,
        )
    )
    lip = lip.cut(
        Part.makeBox(
            INTERIOR_X[1] - INTERIOR_X[0] + 2.0,
            (ASSEMBLY_Y[1] + BOARD_GAP) - (ASSEMBLY_Y[0] - BOARD_GAP),
            LIP_HEIGHT + 2.0,
            Vector(INTERIOR_X[0] - 1.0, ASSEMBLY_Y[0] - BOARD_GAP, WALL_TOP - LIP_HEIGHT - 1.0),
        )
    )
    lid = plate.fuse(lip)

    # The lid is thin only where it has to be: over the shield there are 2.50 mm between
    # the PCB and the display's glass, and a 1.40 mm component in them. Everywhere else it
    # keeps its full 2.5 mm. The rim left standing rests on the shield's bare edge and is
    # what holds the board stack down -- on three sides only, because the display's flex
    # crosses the fourth on its way to the slot and stands 1.30 mm proud there.
    lid = lid.cut(
        Part.makeBox(
            POCKET_X[1] - POCKET_X[0],
            POCKET_Y[1] - POCKET_Y[0],
            POCKET_FLOOR - WALL_TOP,
            Vector(POCKET_X[0], POCKET_Y[0], WALL_TOP),
        )
    )

    lid = lid.cut(
        Part.makeBox(
            (DISPLAY_X[1] - DISPLAY_X[0]) + 2 * DISPLAY_GAP,
            (DISPLAY_Y[1] - DISPLAY_Y[0]) + 2 * DISPLAY_GAP,
            LID_TOP - WALL_TOP + 2.0,
            Vector(DISPLAY_X[0] - DISPLAY_GAP, DISPLAY_Y[0] - DISPLAY_GAP, WALL_TOP - 1.0),
        )
    )

    # The joystick's actuator is drawn simplified in ST's model, so the opening is sized to
    # the 10.76 mm keepout the model does state rather than to a cap diameter it does not.
    side = JOYSTICK_KEEPOUT + 2 * JOYSTICK_GAP
    window = prv_rounded_box(
        (JOYSTICK_CENTRE[0] - side / 2.0, JOYSTICK_CENTRE[0] + side / 2.0),
        (JOYSTICK_CENTRE[1] - side / 2.0, JOYSTICK_CENTRE[1] + side / 2.0),
        (WALL_TOP - 1.0, LID_TOP + 1.0),
        JOYSTICK_CORNER_RADIUS,
    )
    lid = lid.cut(window)

    for centre in SCREW_POSITIONS:
        lid = lid.cut(prv_pillar(centre, BOSS_DIAMETER + 2 * PRINT_CLEARANCE, (WALL_TOP - LIP_HEIGHT - 1.0, WALL_TOP)))
        lid = lid.cut(prv_pillar(centre, SCREW_CLEARANCE_DIAMETER, (WALL_TOP - 1.0, LID_TOP + 1.0)))
        lid = lid.cut(
            Part.makeCone(
                SCREW_CLEARANCE_DIAMETER / 2.0,
                SCREW_HEAD_DIAMETER / 2.0,
                SCREW_HEAD_DEPTH,
                Vector(centre[0], centre[1], LID_TOP - SCREW_HEAD_DEPTH),
            )
        )
    return lid


def prv_write(in_shape, in_directory, in_name):
    step_path = "%s/%s.step" % (in_directory, in_name)
    stl_path = "%s/%s.stl" % (in_directory, in_name)
    in_shape.exportStep(step_path)
    mesh = MeshPart.meshFromShape(Shape=in_shape, LinearDeflection=0.05, AngularDeflection=0.5, Relative=False)
    Mesh.Mesh(mesh).write(stl_path)
    box = in_shape.BoundBox
    print(
        "%-16s %6.2f x %6.2f x %6.2f mm   %9.1f mm3   solid=%s   %d facets"
        % (in_name, box.XLength, box.YLength, box.ZLength, in_shape.Volume, in_shape.isValid(), mesh.CountFacets)
    )


directory = sys.argv[2] if len(sys.argv) > 2 else "."
print("outer %.2f x %.2f x %.2f mm" % (OUTER_X[1] - OUTER_X[0], OUTER_Y[1] - OUTER_Y[0], LID_TOP - FLOOR_BOTTOM))
print("lid over the shield %.2f mm, elsewhere %.2f mm" % (LID_TOP - POCKET_FLOOR, LID_TOP - WALL_TOP))
prv_write(prv_build_base(), directory, "enclosure-base")
prv_write(prv_build_lid(), directory, "enclosure-lid")

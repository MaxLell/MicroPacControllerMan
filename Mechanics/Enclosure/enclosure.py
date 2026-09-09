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
PRINT_CLEARANCE = 0.2  # what a Prusa Mini needs for one part to sit inside another

STANDOFF_HEIGHT = 4.0  # clears the soldered pin tails under the Nucleo
STANDOFF_DIAMETER = 6.0
PEG_HEIGHT = 3.0
PEG_DIAMETER = NUCLEO_HOLE_DIAMETER - 0.3

DISPLAY_GAP = 0.3  # around the display module in the lid
JOYSTICK_GAP = 0.32  # around the joystick's keepout
JOYSTICK_CORNER_RADIUS = 2.0
SHIELD_RIM_OVERLAP = 1.0  # how far the lid's full-thickness rim rests on the shield
POCKET_FLEX_MARGIN = 0.6  # the pocket runs this far past the shield's flex edge
OUTER_CORNER_RADIUS = 3.0
INNER_CORNER_RADIUS = 1.5

# The snap. A cantilever hook on the lid springs into a window in the base's wall.
# In PLA the arm's peak strain is what matters, and for a rectangular cantilever it is
# 1.5 * thickness * deflection / length^2. With these numbers that is 1.2 %, comfortably
# under the ~2 % where PLA stops coming back. Lengthening the arm is the cheap way to
# lower it; thickening it is the expensive way to raise it.
ARM_LENGTH = 14.0
ARM_THICKNESS = 1.6
ARM_WIDTH = 8.0
ARM_GAP = 0.15  # arm's back to the wall, so it can slide
LUG_PROJECTION = 0.95  # of which ARM_GAP is taken up reaching the wall face
LUG_LEAD_IN = 1.2  # the ramp the lug rides in on
WINDOW_WIDTH = ARM_WIDTH + 1.0
WINDOW_SLACK = 0.3  # under the lug, so the lid seats on the wall and not on the window
SNAP_CLEARANCE = 3.0  # board edge to inner wall: arm, its gap, and room to flex

# Where the six snaps sit: (axis of the wall's normal, which side, position along it).
SNAPS = (("x", 0, 8.0), ("x", 0, 58.0), ("x", 1, 8.0), ("x", 1, 58.0), ("y", 0, 35.0), ("y", 1, 35.0))

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

ARM_BOTTOM = WALL_TOP - ARM_LENGTH
LUG_TOP = ARM_BOTTOM + LUG_LEAD_IN + LUG_PROJECTION


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


def prv_snap_geometry(in_axis, in_side, in_along):
    """The arm and lug that hang off the lid, and the window they snap into.

    The wall's inner face is the reference. `outward` is the direction the lug projects,
    which is away from the box's middle.
    """
    interior = INTERIOR_X if in_axis == "x" else INTERIOR_Y
    outer = OUTER_X if in_axis == "x" else OUTER_Y
    face = interior[in_side]
    outward = -1.0 if in_side == 0 else 1.0

    back = face - outward * ARM_GAP  # the arm's outward face
    front = back - outward * ARM_THICKNESS
    across = (in_along - ARM_WIDTH / 2.0, in_along + ARM_WIDTH / 2.0)
    window_across = (in_along - WINDOW_WIDTH / 2.0, in_along + WINDOW_WIDTH / 2.0)
    through = (face - outward * 1.0, outer[in_side] + outward * 1.0)  # the full wall, and past it

    def spanning(in_low_high):
        return (min(in_low_high), max(in_low_high))

    if in_axis == "x":
        arm = prv_box(spanning((front, back)), across, (ARM_BOTTOM, WALL_TOP))
        window = prv_box(spanning(through), window_across, (ARM_BOTTOM - WINDOW_SLACK, LUG_TOP))
        profile = [
            Vector(back, across[0], ARM_BOTTOM),
            Vector(back + outward * LUG_PROJECTION, across[0], ARM_BOTTOM + LUG_LEAD_IN),
            Vector(back, across[0], LUG_TOP),
        ]
        sweep = Vector(0, ARM_WIDTH, 0)
    else:
        arm = prv_box(across, spanning((front, back)), (ARM_BOTTOM, WALL_TOP))
        window = prv_box(window_across, spanning(through), (ARM_BOTTOM - WINDOW_SLACK, LUG_TOP))
        profile = [
            Vector(across[0], back, ARM_BOTTOM),
            Vector(across[0], back + outward * LUG_PROJECTION, ARM_BOTTOM + LUG_LEAD_IN),
            Vector(across[0], back, LUG_TOP),
        ]
        sweep = Vector(ARM_WIDTH, 0, 0)

    lug = Part.Face(Part.makePolygon(profile + [profile[0]])).extrude(sweep)
    return arm.fuse(lug), window


def prv_build_base():
    shell = prv_rounded_box(OUTER_X, OUTER_Y, (FLOOR_BOTTOM, WALL_TOP), OUTER_CORNER_RADIUS)
    cavity = prv_rounded_box(INTERIOR_X, INTERIOR_Y, (FLOOR_TOP, WALL_TOP + 1.0), INNER_CORNER_RADIUS)
    base = shell.cut(cavity)

    for centre in NUCLEO_HOLES:
        base = base.fuse(prv_pillar(centre, STANDOFF_DIAMETER, (FLOOR_TOP, -NUCLEO_PCB_THICKNESS)))
        base = base.fuse(
            prv_pillar(centre, PEG_DIAMETER, (-NUCLEO_PCB_THICKNESS, -NUCLEO_PCB_THICKNESS + PEG_HEIGHT))
        )

    # The windows are through holes on purpose: the engagement is visible, and a snap that
    # cannot be released is a box that has to be broken open.
    for snap in SNAPS:
        base = base.cut(prv_snap_geometry(*snap)[1])

    # The USB plug's overmould is wider than the socket, so the opening is sized for the
    # plug rather than for the receptacle measured out of the model.
    usb_centre_x = (USB_X[0] + USB_X[1]) / 2.0
    usb_centre_z = (USB_Z[0] + USB_Z[1]) / 2.0
    return base.cut(
        Part.makeBox(14.0, WALL + 2.0, 9.0, Vector(usb_centre_x - 7.0, INTERIOR_Y[1] - 1.0, usb_centre_z - 4.5))
    )


def prv_build_lid():
    lid = prv_rounded_box(OUTER_X, OUTER_Y, (WALL_TOP, LID_TOP), OUTER_CORNER_RADIUS)

    for snap in SNAPS:
        lid = lid.fuse(prv_snap_geometry(*snap)[0])

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
    return lid.cut(
        prv_rounded_box(
            (JOYSTICK_CENTRE[0] - side / 2.0, JOYSTICK_CENTRE[0] + side / 2.0),
            (JOYSTICK_CENTRE[1] - side / 2.0, JOYSTICK_CENTRE[1] + side / 2.0),
            (WALL_TOP - 1.0, LID_TOP + 1.0),
            JOYSTICK_CORNER_RADIUS,
        )
    )


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
    "snap: %d arms, %.2f mm engagement, peak strain %.2f %%"
    % (len(SNAPS), LUG_PROJECTION - ARM_GAP, 100.0 * 1.5 * ARM_THICKNESS * LUG_PROJECTION / ARM_LENGTH ** 2)
)
prv_write(prv_build_base(), directory, "enclosure-base")
prv_write(prv_build_lid(), directory, "enclosure-lid")

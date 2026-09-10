"""The game's own font, read out of the firmware rather than redrawn.

`sprite_set.c` keeps its art as ASCII, one character per pixel, so the letters the panel
draws can be parsed straight out of the source. That is the point: a legend cut into the
lid in a lookalike font would drift from the game the first time either changed, and this
one cannot. It is the 1980 tile ROM's font, decoded offline into that file.

Every glyph is 8 x 8 with `1` for ink and `2` for the background the panel paints behind
it. The letter itself occupies seven columns and seven rows -- column 0 and row 7 are the
spacing the font builds in -- which is why rendering a string is just concatenation.

The font is **upper case, digits and a space** (plus the one hyphen that was drawn rather
than decoded, for `PAC-MAN`). Anything else raises.
"""

import os
import re

SOURCE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "Firmware", "App", "sprite_set", "sprite_set.c"
)
INK = "1"
GLYPH_SIZE = 8

SPELLED_OUT = {" ": "space", "-": "hyphen"}


def prv_array_name(in_character):
    if in_character in SPELLED_OUT:
        return SPELLED_OUT[in_character]
    if in_character.isdigit():
        return "digit_" + in_character
    return in_character.lower()


def prv_read_source():
    with open(SOURCE_PATH) as handle:
        return handle.read()


def get_glyph(in_character, in_source=None):
    """The eight rows of one glyph, exactly as the firmware holds them."""
    source = prv_read_source() if in_source is None else in_source
    found = re.search(
        r"static const char\* const g_glyph_%s\[\]\s*=\s*\{(.*?)\};" % re.escape(prv_array_name(in_character)),
        source,
        re.S,
    )
    if found is None:
        raise KeyError("the game's font has no glyph for %r" % in_character)
    rows = re.findall(r'"([^"]*)"', found.group(1))
    if len(rows) != GLYPH_SIZE or any(len(row) != GLYPH_SIZE for row in rows):
        raise ValueError("glyph %r is not %d x %d" % (in_character, GLYPH_SIZE, GLYPH_SIZE))
    return rows


def render(in_text):
    """The string as a grid of booleans, row 0 at the top. Ink is True."""
    source = prv_read_source()
    glyphs = [get_glyph(character, source) for character in in_text.upper()]
    return [[cell == INK for glyph in glyphs for cell in glyph[row]] for row in range(GLYPH_SIZE)]


def ink_extent(in_grid):
    """The box the ink actually fills, so a legend can be centred on its letters rather
    than on the blank column every glyph carries."""
    marked = [(row, column) for row, cells in enumerate(in_grid) for column, on in enumerate(cells) if on]
    if not marked:
        raise ValueError("nothing to draw")
    rows = [row for row, _ in marked]
    columns = [column for _, column in marked]
    return min(columns), max(columns) + 1, min(rows), max(rows) + 1

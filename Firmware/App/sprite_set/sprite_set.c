#include "sprite_set.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "sprite.h"

/* ==========================================================================
 * sprite_set - private
 * ========================================================================= */

/* The art: **the arcade's own sprites**, not a likeness of them.
 *
 * One character per pixel: '.' shows what is behind, '1' the body colour, '2' and '3' the
 * two detail colours a palette supplies. `clang-format off` keeps the formatter from
 * reflowing a picture into a paragraph, exactly as the maze in `playfield.c` does.
 *
 * These were drawn by hand once and it showed. They are now decoded from the 1980 sprite
 * ROM, whose 16 x 16 sprites are stored as four 8 x 4 blocks at two bits per pixel, in a
 * scrambled order and counter-rotated because the cabinet's monitor is on its side. The
 * decode was done offline; the tables below are the source now, and reading a pixel out
 * of them costs nothing at run time.
 *
 * Two things come from the hardware rather than the ROM, and are done here instead. The
 * cabinet mirrors sprites in x and y, so the ROM holds only the east- and south-facing
 * figures and the west and north ones below are their mirrors. And an eye that looks the
 * wrong way is the one mistake that is invisible in a table and obvious on a panel, so a
 * unit test checks the mirrors against their originals rather than trusting the paste.
 *
 * The colour *numbering* is ours: the ROM calls the ghost body 3 and the eye white 1,
 * which would mean rewriting five palettes. The decode renumbered instead, so the body is
 * always '1' — the same convention every other drawing here uses. */
/* clang-format off */
static const char* const g_pacman_closed[] = {
    "................",
    ".....11111......",
    "...111111111....",
    "..11111111111...",
    "..11111111111...",
    ".1111111111111..",
    ".1111111111111..",
    ".1111111111111..",
    ".1111111111111..",
    ".1111111111111..",
    "..11111111111...",
    "..11111111111...",
    "...111111111....",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_pacman_wide_east[] = {
    "................",
    ".....11111......",
    "...1111111......",
    "..1111111.......",
    "..111111........",
    ".111111.........",
    ".11111..........",
    ".1111...........",
    ".11111..........",
    ".111111.........",
    "..111111........",
    "..1111111.......",
    "...1111111......",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_pacman_wide_west[] = {
    "................",
    ".....11111......",
    ".....1111111....",
    "......1111111...",
    ".......111111...",
    "........111111..",
    ".........11111..",
    "..........1111..",
    ".........11111..",
    "........111111..",
    ".......111111...",
    "......1111111...",
    ".....1111111....",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_pacman_wide_south[] = {
    "................",
    ".....11111......",
    "...111111111....",
    "..11111111111...",
    "..11111111111...",
    ".111111.111111..",
    ".11111...11111..",
    ".1111.....1111..",
    ".111.......111..",
    ".11.........11..",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
};

static const char* const g_pacman_wide_north[] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    ".11.........11..",
    ".111.......111..",
    ".1111.....1111..",
    ".11111...11111..",
    ".111111.111111..",
    "..11111111111...",
    "..11111111111...",
    "...111111111....",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_pacman_half_east[] = {
    "................",
    ".....11111......",
    "...111111111....",
    "..11111111111...",
    "..11111111111...",
    ".1111111111.....",
    ".1111111........",
    ".1111...........",
    ".1111111........",
    ".1111111111.....",
    "..11111111111...",
    "..11111111111...",
    "...111111111....",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_pacman_half_west[] = {
    "................",
    ".....11111......",
    "...111111111....",
    "..11111111111...",
    "..11111111111...",
    "....1111111111..",
    ".......1111111..",
    "..........1111..",
    ".......1111111..",
    "....1111111111..",
    "..11111111111...",
    "..11111111111...",
    "...111111111....",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_pacman_half_south[] = {
    "................",
    ".....11111......",
    "...111111111....",
    "..11111111111...",
    "..11111111111...",
    ".1111111111111..",
    ".111111.111111..",
    ".111111.111111..",
    ".11111...11111..",
    ".11111...11111..",
    "..1111...1111...",
    "..111.....111...",
    "...11.....11....",
    "................",
    "................",
    "................",
};

static const char* const g_pacman_half_north[] = {
    "................",
    "................",
    "...11.....11....",
    "..111.....111...",
    "..1111...1111...",
    ".11111...11111..",
    ".11111...11111..",
    ".111111.111111..",
    ".111111.111111..",
    ".1111111111111..",
    "..11111111111...",
    "..11111111111...",
    "...111111111....",
    ".....11111......",
    "................",
    "................",
};

static const char* const g_ghost_east_a[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..111221111221..",
    "..112222112222..",
    "..112233112233..",
    ".11122331122331.",
    ".11112211112211.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11.111..111.11.",
    ".1...11..11...1.",
    "................",
};

static const char* const g_ghost_east_b[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..111221111221..",
    "..112222112222..",
    "..112233112233..",
    ".11122331122331.",
    ".11112211112211.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".1111.1111.1111.",
    "..11...11...11..",
    "................",
};

static const char* const g_ghost_south_a[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..111111111111..",
    "..112211112211..",
    "..122221122221..",
    ".11222211222211.",
    ".11233211233211.",
    ".11133111133111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11.111..111.11.",
    ".1...11..11...1.",
    "................",
};

static const char* const g_ghost_south_b[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..111111111111..",
    "..112211112211..",
    "..122221122221..",
    ".11222211222211.",
    ".11233211233211.",
    ".11133111133111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".1111.1111.1111.",
    "..11...11...11..",
    "................",
};

static const char* const g_ghost_west_a[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..122111122111..",
    "..222211222211..",
    "..332211332211..",
    ".13322113322111.",
    ".11221111221111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11.111..111.11.",
    ".1...11..11...1.",
    "................",
};

static const char* const g_ghost_west_b[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..122111122111..",
    "..222211222211..",
    "..332211332211..",
    ".13322113322111.",
    ".11221111221111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".1111.1111.1111.",
    "..11...11...11..",
    "................",
};

static const char* const g_ghost_north_a[] = {
    "................",
    "......1111......",
    "....33111133....",
    "...2332112332...",
    "..122221122221..",
    "..122221122221..",
    "..112211112211..",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11.111..111.11.",
    ".1...11..11...1.",
    "................",
};

static const char* const g_ghost_north_b[] = {
    "................",
    "......1111......",
    "....33111133....",
    "...2332112332...",
    "..122221122221..",
    "..122221122221..",
    "..112211112211..",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11111111111111.",
    ".1111.1111.1111.",
    "..11...11...11..",
    "................",
};

static const char* const g_ghost_frightened_a[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..111111111111..",
    "..111111111111..",
    "..111221122111..",
    ".11112211221111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11221122112211.",
    ".12112211221121.",
    ".11111111111111.",
    ".11.111..111.11.",
    ".1...11..11...1.",
    "................",
};

static const char* const g_ghost_frightened_b[] = {
    "................",
    "......1111......",
    "....11111111....",
    "...1111111111...",
    "..111111111111..",
    "..111111111111..",
    "..111221122111..",
    ".11112211221111.",
    ".11111111111111.",
    ".11111111111111.",
    ".11221122112211.",
    ".12112211221121.",
    ".11111111111111.",
    ".1111.1111.1111.",
    "..11...11...11..",
    "................",
};

/* The outer frame: a **6-pixel band** with a 1-pixel margin either side, which is the weight a
 * two-cell wall inside the maze already renders at (two 3-pixel block lines meeting).
 *
 * **Not the arcade's**, which is 2 pixels — the one place in this file that is ours rather than
 * the ROM's, asked for by the owner and recorded as [DEC-031]. The ROM's reason for 2 is
 * geometric: the frame is one cell thick where an inner wall is two, so 2 pixels is what leaves
 * room for a corner's diagonal step. At 6 there is none, which is why the corners below are solid
 * right angles — a consequence that was put to the owner and accepted.
 *
 * All sixteen are **generated from the geometry**, not drawn: four edges, four corners, eight
 * tees. Hand-drawing sixteen symmetric tiles is how a maze ends up with 94 wrong cells, which
 * this project has already done once. The tees carry the 3-pixel branch that an inner wall's own
 * line arrives on, so a wall meeting the frame lands exactly on it. */

/* The font, and a blank the size of an actor.
 *
 * Also the arcade's, from the same tile ROM as the walls. Its glyph tiles sit at their own
 * ASCII codes — '0' at 0x30, 'A' at 0x41 — which is why #sprite_set_get_glyph can be
 * arithmetic rather than a table.
 *
 * The whole alphabet is here although the HUD spells only two words with it. Thirty-seven
 * 8 x 8 tiles is under 4 kB of a 512 kB part, and the screens FR-001, FR-002 and FR-023
 * still want are all text. */
static const char* const g_glyph_space[] = {
    "22222222",
    "22222222",
    "22222222",
    "22222222",
    "22222222",
    "22222222",
    "22222222",
    "22222222",
};

static const char* const g_glyph_digit_0[] = {
    "22211122",
    "22122112",
    "21122211",
    "21122211",
    "21122211",
    "22112212",
    "22211122",
    "22222222",
};

static const char* const g_glyph_digit_1[] = {
    "22221122",
    "22211122",
    "22221122",
    "22221122",
    "22221122",
    "22221122",
    "22111111",
    "22222222",
};

static const char* const g_glyph_digit_2[] = {
    "22111112",
    "21122211",
    "22222111",
    "22211112",
    "22111122",
    "21112222",
    "21111111",
    "22222222",
};

static const char* const g_glyph_digit_3[] = {
    "22111111",
    "22222112",
    "22221122",
    "22211112",
    "22222211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_digit_4[] = {
    "22221112",
    "22211112",
    "22112112",
    "21122112",
    "21111111",
    "22222112",
    "22222112",
    "22222222",
};

static const char* const g_glyph_digit_5[] = {
    "21111112",
    "21122222",
    "21111112",
    "22222211",
    "22222211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_digit_6[] = {
    "22211112",
    "22112222",
    "21122222",
    "21111112",
    "21122211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_digit_7[] = {
    "21111111",
    "21122211",
    "22222112",
    "22221122",
    "22211222",
    "22211222",
    "22211222",
    "22222222",
};

static const char* const g_glyph_digit_8[] = {
    "22111122",
    "21122212",
    "21112212",
    "22111122",
    "21221111",
    "21222211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_digit_9[] = {
    "22111112",
    "21122211",
    "21122211",
    "22111111",
    "22222211",
    "22222112",
    "22111122",
    "22222222",
};

static const char* const g_glyph_a[] = {
    "22211122",
    "22112112",
    "21122211",
    "21122211",
    "21111111",
    "21122211",
    "21122211",
    "22222222",
};

static const char* const g_glyph_b[] = {
    "21111112",
    "21122211",
    "21122211",
    "21111112",
    "21122211",
    "21122211",
    "21111112",
    "22222222",
};

static const char* const g_glyph_c[] = {
    "22211112",
    "22112211",
    "21122222",
    "21122222",
    "21122222",
    "22112211",
    "22211112",
    "22222222",
};

static const char* const g_glyph_d[] = {
    "21111122",
    "21122112",
    "21122211",
    "21122211",
    "21122211",
    "21122112",
    "21111122",
    "22222222",
};

static const char* const g_glyph_e[] = {
    "22111111",
    "22112222",
    "22112222",
    "22111112",
    "22112222",
    "22112222",
    "22111111",
    "22222222",
};

static const char* const g_glyph_f[] = {
    "21111111",
    "21122222",
    "21122222",
    "21111112",
    "21122222",
    "21122222",
    "21122222",
    "22222222",
};

static const char* const g_glyph_g[] = {
    "22211111",
    "22112222",
    "21122222",
    "21122111",
    "21122211",
    "22112211",
    "22211111",
    "22222222",
};

static const char* const g_glyph_h[] = {
    "21122211",
    "21122211",
    "21122211",
    "21111111",
    "21122211",
    "21122211",
    "21122211",
    "22222222",
};

static const char* const g_glyph_i[] = {
    "22111111",
    "22221122",
    "22221122",
    "22221122",
    "22221122",
    "22221122",
    "22111111",
    "22222222",
};

static const char* const g_glyph_j[] = {
    "22222211",
    "22222211",
    "22222211",
    "22222211",
    "22222211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_k[] = {
    "21122211",
    "21122112",
    "21121122",
    "21111222",
    "21111122",
    "21121112",
    "21122111",
    "22222222",
};

static const char* const g_glyph_l[] = {
    "22112222",
    "22112222",
    "22112222",
    "22112222",
    "22112222",
    "22112222",
    "22111111",
    "22222222",
};

static const char* const g_glyph_m[] = {
    "21122211",
    "21112111",
    "21111111",
    "21111111",
    "21121211",
    "21122211",
    "21122211",
    "22222222",
};

static const char* const g_glyph_n[] = {
    "21122211",
    "21112211",
    "21111211",
    "21111111",
    "21121111",
    "21122111",
    "21122211",
    "22222222",
};

static const char* const g_glyph_o[] = {
    "22111112",
    "21122211",
    "21122211",
    "21122211",
    "21122211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_p[] = {
    "21111112",
    "21122211",
    "21122211",
    "21122211",
    "21111112",
    "21122222",
    "21122222",
    "22222222",
};

static const char* const g_glyph_q[] = {
    "22111112",
    "21122211",
    "21122211",
    "21122211",
    "21121111",
    "21122112",
    "22111121",
    "22222222",
};

static const char* const g_glyph_r[] = {
    "21111112",
    "21122211",
    "21122211",
    "21122111",
    "21111122",
    "21121112",
    "21122111",
    "22222222",
};

static const char* const g_glyph_s[] = {
    "22111122",
    "21122112",
    "21122222",
    "22111112",
    "22222211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_t[] = {
    "22111111",
    "22221122",
    "22221122",
    "22221122",
    "22221122",
    "22221122",
    "22221122",
    "22222222",
};

static const char* const g_glyph_u[] = {
    "21122211",
    "21122211",
    "21122211",
    "21122211",
    "21122211",
    "21122211",
    "22111112",
    "22222222",
};

static const char* const g_glyph_v[] = {
    "21122211",
    "21122211",
    "21122211",
    "21112111",
    "22111112",
    "22211122",
    "22221222",
    "22222222",
};

static const char* const g_glyph_w[] = {
    "21122211",
    "21122211",
    "21121211",
    "21111111",
    "21111111",
    "21112111",
    "21122211",
    "22222222",
};

static const char* const g_glyph_x[] = {
    "21122211",
    "21112111",
    "22111112",
    "22211122",
    "22111112",
    "21112111",
    "21122211",
    "22222222",
};

static const char* const g_glyph_y[] = {
    "22112211",
    "22112211",
    "22112211",
    "22211112",
    "22221122",
    "22221122",
    "22221122",
    "22222222",
};

static const char* const g_glyph_z[] = {
    "21111111",
    "22222111",
    "22221112",
    "22211122",
    "22111222",
    "21112222",
    "21111111",
    "22222222",
};

static const char* const g_actor_blank[] = {
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
    "2222222222222222",
};

/* Ours, not the arcade's: it draws its maze as thin double lines, and this project fills
 * the whole cell instead. A solid tile plus a palette is therefore both a wall and an
 * empty cell, which is what keeps a field cell to one display-list item. */
static const char* const g_tile_solid[] = {
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
};

/* These two are the arcade's, from the tile ROM: a 2 x 2 dot and the energiser blob. */
static const char* const g_tile_pellet[] = {
    "22222222",
    "22222222",
    "22222222",
    "22211222",
    "22211222",
    "22222222",
    "22222222",
    "22222222",
};

static const char* const g_tile_power_pellet[] = {
    "22111122",
    "21111112",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "21111112",
    "22111122",
};
/* clang-format on */

/* An actor spans two cells and a field tile is one, exactly as the arcade does it. */
#define SPRITE_SET_ACTOR_SIZE (16U)
#define SPRITE_SET_TILE_SIZE  (8U)

static const sprite_t g_sprites[SPRITE_SET_ID_COUNT] = {
    [SPRITE_SET_PACMAN_CLOSED] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_closed},
    [SPRITE_SET_PACMAN_HALF_EAST] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_half_east},
    [SPRITE_SET_PACMAN_HALF_NORTH] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_half_north},
    [SPRITE_SET_PACMAN_HALF_WEST] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_half_west},
    [SPRITE_SET_PACMAN_HALF_SOUTH] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_half_south},
    [SPRITE_SET_PACMAN_WIDE_EAST] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_wide_east},
    [SPRITE_SET_PACMAN_WIDE_NORTH] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_wide_north},
    [SPRITE_SET_PACMAN_WIDE_WEST] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_wide_west},
    [SPRITE_SET_PACMAN_WIDE_SOUTH] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_pacman_wide_south},
    [SPRITE_SET_GHOST_EAST_A] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_east_a},
    [SPRITE_SET_GHOST_EAST_B] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_east_b},
    [SPRITE_SET_GHOST_NORTH_A] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_north_a},
    [SPRITE_SET_GHOST_NORTH_B] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_north_b},
    [SPRITE_SET_GHOST_WEST_A] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_west_a},
    [SPRITE_SET_GHOST_WEST_B] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_west_b},
    [SPRITE_SET_GHOST_SOUTH_A] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_south_a},
    [SPRITE_SET_GHOST_SOUTH_B] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_south_b},
    [SPRITE_SET_GHOST_FRIGHTENED_A] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_frightened_a},
    [SPRITE_SET_GHOST_FRIGHTENED_B] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_ghost_frightened_b},

    /* The field. A tile is exactly one item in a display list, so an empty cell, a wall
     * and a pellet all have to be full-tile drawings — a pellet sprite that left its
     * surroundings transparent would need the tile cleared first, and that would be two
     * items for every dot on the screen. */
    [SPRITE_SET_TILE] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_tile_solid},
    [SPRITE_SET_TILE_PELLET] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_tile_pellet},
    [SPRITE_SET_TILE_POWER_PELLET] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_tile_power_pellet},
    [SPRITE_SET_ACTOR_BLANK] = {SPRITE_SET_ACTOR_SIZE, SPRITE_SET_ACTOR_SIZE, g_actor_blank},

    [SPRITE_SET_GLYPH_SPACE] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_space},
    [SPRITE_SET_GLYPH_DIGIT_0] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_0},
    [SPRITE_SET_GLYPH_DIGIT_1] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_1},
    [SPRITE_SET_GLYPH_DIGIT_2] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_2},
    [SPRITE_SET_GLYPH_DIGIT_3] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_3},
    [SPRITE_SET_GLYPH_DIGIT_4] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_4},
    [SPRITE_SET_GLYPH_DIGIT_5] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_5},
    [SPRITE_SET_GLYPH_DIGIT_6] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_6},
    [SPRITE_SET_GLYPH_DIGIT_7] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_7},
    [SPRITE_SET_GLYPH_DIGIT_8] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_8},
    [SPRITE_SET_GLYPH_DIGIT_9] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_digit_9},
    [SPRITE_SET_GLYPH_A] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_a},
    [SPRITE_SET_GLYPH_B] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_b},
    [SPRITE_SET_GLYPH_C] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_c},
    [SPRITE_SET_GLYPH_D] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_d},
    [SPRITE_SET_GLYPH_E] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_e},
    [SPRITE_SET_GLYPH_F] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_f},
    [SPRITE_SET_GLYPH_G] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_g},
    [SPRITE_SET_GLYPH_H] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_h},
    [SPRITE_SET_GLYPH_I] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_i},
    [SPRITE_SET_GLYPH_J] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_j},
    [SPRITE_SET_GLYPH_K] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_k},
    [SPRITE_SET_GLYPH_L] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_l},
    [SPRITE_SET_GLYPH_M] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_m},
    [SPRITE_SET_GLYPH_N] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_n},
    [SPRITE_SET_GLYPH_O] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_o},
    [SPRITE_SET_GLYPH_P] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_p},
    [SPRITE_SET_GLYPH_Q] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_q},
    [SPRITE_SET_GLYPH_R] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_r},
    [SPRITE_SET_GLYPH_S] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_s},
    [SPRITE_SET_GLYPH_T] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_t},
    [SPRITE_SET_GLYPH_U] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_u},
    [SPRITE_SET_GLYPH_V] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_v},
    [SPRITE_SET_GLYPH_W] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_w},
    [SPRITE_SET_GLYPH_X] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_x},
    [SPRITE_SET_GLYPH_Y] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_y},
    [SPRITE_SET_GLYPH_Z] = {SPRITE_SET_TILE_SIZE, SPRITE_SET_TILE_SIZE, g_glyph_z},

};

/* One drawing, five palettes. Index 0 is never read — it is the transparent character's
 * slot, kept so the array index matches the digit in the art. */
static const sprite_palette_t g_palettes[SPRITE_SET_PALETTE_COUNT] = {
    [SPRITE_SET_PALETTE_PACMAN] = {{0U, FRAMEBUFFER_COLOR_YELLOW, 0U, 0U}},
    [SPRITE_SET_PALETTE_BLINKY] = {{0U, FRAMEBUFFER_RGB(255U, 0U, 0U), FRAMEBUFFER_COLOR_WHITE,
                                    FRAMEBUFFER_RGB(0U, 0U, 160U)}},
    [SPRITE_SET_PALETTE_PINKY] = {{0U, FRAMEBUFFER_RGB(255U, 184U, 255U), FRAMEBUFFER_COLOR_WHITE,
                                   FRAMEBUFFER_RGB(0U, 0U, 160U)}},
    [SPRITE_SET_PALETTE_INKY] = {{0U, FRAMEBUFFER_RGB(0U, 255U, 255U), FRAMEBUFFER_COLOR_WHITE,
                                  FRAMEBUFFER_RGB(0U, 0U, 160U)}},
    [SPRITE_SET_PALETTE_CLYDE] = {{0U, FRAMEBUFFER_RGB(255U, 184U, 82U), FRAMEBUFFER_COLOR_WHITE,
                                   FRAMEBUFFER_RGB(0U, 0U, 160U)}},

    /* The one the four ghosts share when a power pellet is eaten: a dark blue body and a
     * white face, so which ghost it is stops mattering — which is the point. */
    [SPRITE_SET_PALETTE_FRIGHTENED] = {{0U, FRAMEBUFFER_RGB(33U, 33U, 255U), FRAMEBUFFER_COLOR_WHITE, 0U}},

    /* The other half of the flash the game asks for as the window runs out (§10.9): the
     * same shape gone white, so the warning reads at a glance without changing the
     * silhouette that says "edible". */
    [SPRITE_SET_PALETTE_FRIGHTENED_FLASH] = {{0U, FRAMEBUFFER_COLOR_WHITE, FRAMEBUFFER_RGB(255U, 0U, 0U), 0U}},

    /* The field palettes. Index 2 is the tile's own background, which is black
     * everywhere — a pellet drawing carries its surroundings so that one item paints a
     * whole cell. */
    [SPRITE_SET_PALETTE_EMPTY] = {{0U, FRAMEBUFFER_COLOR_BLACK, FRAMEBUFFER_COLOR_BLACK, 0U}},
    [SPRITE_SET_PALETTE_WALL] = {{0U, FRAMEBUFFER_RGB(33U, 33U, 222U), FRAMEBUFFER_COLOR_BLACK, 0U}},

    /* The ghost house gate. The arcade draws it a pale pink so it reads as a way through
     * rather than as another wall, which matters because it is the one wall a ghost may
     * cross. */
    [SPRITE_SET_PALETTE_DOOR] = {{0U, FRAMEBUFFER_RGB(255U, 184U, 222U), FRAMEBUFFER_COLOR_BLACK, 0U}},

    /* Words, in the arcade's white on black. Like the field tiles, a glyph carries its own
     * background so one item paints a whole cell. */
    [SPRITE_SET_PALETTE_TEXT] = {{0U, FRAMEBUFFER_COLOR_WHITE, FRAMEBUFFER_COLOR_BLACK, 0U}},
    [SPRITE_SET_PALETTE_PELLET] = {{0U, FRAMEBUFFER_RGB(255U, 184U, 151U), FRAMEBUFFER_COLOR_BLACK, 0U}},
};

/* ==========================================================================
 * sprite_set - public
 * ========================================================================= */

const sprite_t* sprite_set_get(sprite_set_id_e in_id)
{
    ASSERT(in_id < SPRITE_SET_ID_COUNT);

    return &g_sprites[in_id];
}

sprite_set_id_e sprite_set_get_glyph(char in_character)
{
    if ((in_character >= '0') && (in_character <= '9'))
    {
        return (sprite_set_id_e)(SPRITE_SET_GLYPH_DIGIT_0 + (in_character - '0'));
    }

    if ((in_character >= 'A') && (in_character <= 'Z'))
    {
        return (sprite_set_id_e)(SPRITE_SET_GLYPH_A + (in_character - 'A'));
    }

    ASSERT(in_character == ' ');

    return SPRITE_SET_GLYPH_SPACE;
}

const sprite_palette_t* sprite_set_get_palette(sprite_set_palette_e in_palette)
{
    ASSERT(in_palette < SPRITE_SET_PALETTE_COUNT);

    return &g_palettes[in_palette];
}

/* The four directions in the order the frame enums run, so a direction plus a frame's
 * base id is the id itself and neither lookup needs a switch per animation frame. */
static uint8_t prv_get_direction_index(direction_e in_direction)
{
    switch (in_direction)
    {
        case DIRECTION_NORTH: return 1U;

        case DIRECTION_WEST: return 2U;

        case DIRECTION_SOUTH: return 3U;

        default:
            /* Including DIRECTION_NONE: an entity that has not moved yet has to look
             * somewhere, and east is where the arcade points them. */
            return 0U;
    }
}

sprite_set_id_e sprite_set_get_ghost_sprite(direction_e in_direction, cell_progress_t in_progress)
{
    /* Two frames of skirt, half a cell each. The arcade runs this off a frame counter;
     * clocking it off the step instead means a ghost slowed by the tunnel waves more
     * slowly too, which is the tell that it is crawling. */
    const uint8_t frame = (in_progress < SPRITE_SET_SKIRT_SWAPS_AT) ? 0U : 1U;

    return (sprite_set_id_e)(SPRITE_SET_GHOST_EAST_A + (prv_get_direction_index(in_direction) * 2U) + frame);
}

sprite_set_id_e sprite_set_get_frightened_sprite(cell_progress_t in_progress)
{
    return (in_progress < SPRITE_SET_SKIRT_SWAPS_AT) ? SPRITE_SET_GHOST_FRIGHTENED_A : SPRITE_SET_GHOST_FRIGHTENED_B;
}

/* Pacman's mouth is the animation, so the frame depends on both where he faces and how
 * far through his step he is: shut, half, wide, half — the arcade's own cycle, once per
 * cell. */
sprite_set_id_e sprite_set_get_pacman_sprite(direction_e in_direction, cell_progress_t in_progress)
{
    const uint8_t index = prv_get_direction_index(in_direction);

    if (in_progress < SPRITE_SET_MOUTH_HALF_AT)
    {
        return SPRITE_SET_PACMAN_CLOSED;
    }

    if ((in_progress >= SPRITE_SET_MOUTH_WIDE_AT) && (in_progress < SPRITE_SET_MOUTH_CLOSES_AT))
    {
        return (sprite_set_id_e)(SPRITE_SET_PACMAN_WIDE_EAST + index);
    }

    return (sprite_set_id_e)(SPRITE_SET_PACMAN_HALF_EAST + index);
}

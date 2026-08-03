#include "render.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "display.h"
#include "framebuffer.h"
#include "msg.h"
#include "sprite.h"
#include "sprite_set.h"

/* ==========================================================================
 * render - private
 * ========================================================================= */

/* One actor's worth of covered pixels, plus where they came from. */
typedef struct
{
    framebuffer_color_t pixels[RENDER_SPRITE_MAX_SIZE][RENDER_SPRITE_MAX_SIZE];
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
    bool is_saved;
} render_save_t;

/* 153,600 bytes, 60 % of SRAM. There is exactly one, which is the constraint the whole
 * render path is shaped around. */
static framebuffer_t g_framebuffer;

static render_save_t g_saves[MSG_ACTOR_COUNT];
static uint8_t g_save_count;

static int16_t prv_min(int16_t in_first, int16_t in_second)
{
    return (in_first < in_second) ? in_first : in_second;
}

static int16_t prv_max(int16_t in_first, int16_t in_second)
{
    return (in_first > in_second) ? in_first : in_second;
}

/* Send a rectangle, clipped to the panel. An actor entering through a tunnel hangs off
 * an edge, and the display port refuses a rectangle that crosses one. */
static void prv_present_clipped(int16_t in_x, int16_t in_y, int16_t in_width, int16_t in_height)
{
    const int16_t left = prv_max(in_x, 0);
    const int16_t top = prv_max(in_y, 0);
    const int16_t right = prv_min((int16_t)(in_x + in_width), (int16_t)FRAMEBUFFER_WIDTH);
    const int16_t bottom = prv_min((int16_t)(in_y + in_height), (int16_t)FRAMEBUFFER_HEIGHT);

    if ((right <= left) || (bottom <= top))
    {
        return;
    }

    display_present_region(&g_framebuffer, left, top, (int16_t)(right - left), (int16_t)(bottom - top));
}

static void prv_restore(render_save_t* const inout_save)
{
    if (!inout_save->is_saved)
    {
        return;
    }

    for (uint8_t row = 0U; row < inout_save->height; ++row)
    {
        for (uint8_t column = 0U; column < inout_save->width; ++column)
        {
            framebuffer_set_pixel(&g_framebuffer, (int16_t)(inout_save->x + column), (int16_t)(inout_save->y + row),
                                  inout_save->pixels[row][column]);
        }
    }
}

static void prv_save(render_save_t* const inout_save, int16_t in_x, int16_t in_y, const sprite_t* const in_sprite)
{
    ASSERT(in_sprite->width <= RENDER_SPRITE_MAX_SIZE);
    ASSERT(in_sprite->height <= RENDER_SPRITE_MAX_SIZE);

    inout_save->x = in_x;
    inout_save->y = in_y;
    inout_save->width = in_sprite->width;
    inout_save->height = in_sprite->height;
    inout_save->is_saved = true;

    for (uint8_t row = 0U; row < in_sprite->height; ++row)
    {
        for (uint8_t column = 0U; column < in_sprite->width; ++column)
        {
            /* Reading outside the buffer yields a defined colour, so a sprite hanging off
             * an edge saves and restores harmlessly. */
            inout_save->pixels[row][column] =
                framebuffer_get_pixel(&g_framebuffer, (int16_t)(in_x + column), (int16_t)(in_y + row));
        }
    }
}

static void prv_draw_item(const msg_display_item_t* const in_item)
{
    sprite_draw(&g_framebuffer, sprite_set_get((sprite_set_id_e)in_item->sprite),
                sprite_set_get_palette((sprite_set_palette_e)in_item->palette), in_item->x, in_item->y);
}

/* ==========================================================================
 * render - public
 * ========================================================================= */

void render_init(void)
{
    memset(g_saves, 0, sizeof(g_saves));
    g_save_count = 0U;

    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);

    display_init();
    display_present(&g_framebuffer);
}

void render_draw(const msg_display_list_t* in_list)
{
    uint8_t actor_index = 0U;

    ASSERT(in_list != NULL);

    /* 1. Undo last frame's actors, so what they covered is on screen again.
     *
     * **In reverse.** Two actors can stand on the same cell — four ghosts share three pen
     * cells at the start of every level — and the second one to be drawn saved pixels
     * that already contained the first. Unwinding in the order they were drawn would put
     * that copy back as if it were background and leave a ghost of a ghost on the panel.
     * Last drawn, first restored. */
    for (uint8_t index = g_save_count; index > 0U; --index)
    {
        prv_restore(&g_saves[index - 1U]);
    }

    /* 2. Apply the field changes. After the restore, on purpose: a cell whose pellet has
     *    been eaten must win over the pixels that were saved while it still had one. */
    for (uint8_t index = 0U; index < in_list->count; ++index)
    {
        if (in_list->items[index].kind == (uint8_t)DISPLAY_ITEM_BACKGROUND)
        {
            prv_draw_item(&in_list->items[index]);
            prv_present_clipped(in_list->items[index].x, in_list->items[index].y, RENDER_SPRITE_MAX_SIZE,
                                RENDER_SPRITE_MAX_SIZE);
        }
    }

    /* 3. and 4. Save what each actor is about to cover, draw it, and send the rectangle
     *    spanning where it was and where it is — one transfer per actor rather than two,
     *    because the two overlap while it is moving a couple of pixels a frame. */
    for (uint8_t index = 0U; index < in_list->count; ++index)
    {
        const msg_display_item_t* const item = &in_list->items[index];
        const sprite_t* sprite;
        render_save_t* save;
        int16_t previous_x;
        int16_t previous_y;

        if (item->kind != (uint8_t)DISPLAY_ITEM_ACTOR)
        {
            continue;
        }

        ASSERT(actor_index < MSG_ACTOR_COUNT);

        save = &g_saves[actor_index];
        sprite = sprite_set_get((sprite_set_id_e)item->sprite);
        previous_x = save->is_saved ? save->x : item->x;
        previous_y = save->is_saved ? save->y : item->y;

        prv_save(save, item->x, item->y, sprite);
        prv_draw_item(item);

        prv_present_clipped(prv_min(previous_x, item->x), prv_min(previous_y, item->y),
                            (int16_t)(prv_max(previous_x, item->x) - prv_min(previous_x, item->x) + sprite->width),
                            (int16_t)(prv_max(previous_y, item->y) - prv_min(previous_y, item->y) + sprite->height));

        ++actor_index;
    }

    g_save_count = actor_index;
}

const framebuffer_t* render_get_framebuffer(void)
{
    return &g_framebuffer;
}

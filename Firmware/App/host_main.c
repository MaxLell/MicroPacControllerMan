/*
 * host_main.c
 *
 * The host application's entry point — the SDL side of CON-103 / FR-104, and the
 * counterpart to `app_main.c`. It exists so the Model and the View can be played, and
 * therefore judged, without a board.
 *
 * There is no `host_main.h`: nothing calls into this file, the C runtime calls `main()`.
 *
 * What this file is allowed to do that no other App module may: talk to SDL. This is the
 * host's platform adapter, the way the CubeMX `main()` is the target's — everything below
 * it is the same code that runs on the STM32. SDL never sees the game and the game never
 * sees SDL; the two meet at a #framebuffer_t.
 *
 * The picture is not re-derived for the window. The game draws through the ordinary
 * `display_present()` port, and this file blits back what the driver actually received
 * (`display_host_get_last_frame()`), scaled up. So the window shows the exact 1-bpp buffer
 * the Sharp panel would be clocked with — a layout bug looks the same in both places, which
 * is the only reason developing the View on the host is worth anything.
 *
 * The loop is shaped like the target's future super-loop rather than like an SDL game loop:
 * a `Services/sw_timer` re-armed from its own callback paces the frames, and the game is
 * advanced by a fixed slice. Fixed rather than measured on purpose — a run then plays out
 * identically whatever the host was doing at the time, which is what makes a bug seen here
 * reproducible.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "custom_assert.h"
#include "display.h"
#include "display_host.h"
#include "framebuffer.h"
#include "game.h"
#include "game_view.h"
#include "playfield.h"
#include "sw_timer.h"
#include "systick_bsp.h"

/* ==========================================================================
 * host_main - private
 * ========================================================================= */

/*! \brief Milliseconds per frame — ~60 Hz, and the slice the game is advanced by. */
#define FRAME_PERIOD_MS  (16U)

/*! \brief Window magnification. 128x128 is unreadably small on a desktop monitor. */
#define WINDOW_SCALE     (5)

#define WINDOW_TITLE     "MicroPacControllerMan (host)"

/* The Sharp panel is dark ink on a pale background; the window says the same. */
#define PIXEL_BACKGROUND (0xFFEFEFE7U)
#define PIXEL_INK        (0xFF101010U)

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
} host_window_t;

static game_t g_game;
static framebuffer_t g_framebuffer;
static host_window_t g_window;
static sw_timer_t g_frame_timer;
static bool g_is_frame_due = false;
static bool g_is_running = true;
static game_state_e g_reported_state = GAME_STATE_IDLE;
static uint8_t g_reported_level = 0U;

/* Re-arms itself, which is how a one-shot timer becomes the frame clock (see sw_timer.h). */
static void prv_on_frame_due(void)
{
    g_is_frame_due = true;
}

static bool prv_open_window(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        (void)fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());

        return false;
    }

    g_window.window =
        SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, FRAMEBUFFER_WIDTH * WINDOW_SCALE,
                         FRAMEBUFFER_HEIGHT * WINDOW_SCALE, SDL_WINDOW_SHOWN);

    if (g_window.window == NULL)
    {
        (void)fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());

        return false;
    }

    /* No renderer flags: whatever SDL has. Insisting on an accelerated one costs nothing
     * on a desktop and makes the application refuse to start under the dummy video driver,
     * which is exactly how it is run without a display server. */
    g_window.renderer = SDL_CreateRenderer(g_window.window, -1, 0U);

    if (g_window.renderer == NULL)
    {
        (void)fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());

        return false;
    }

    /* One texel per panel pixel; the renderer does the magnification, so nothing here has
     * to know about the scale factor. */
    g_window.texture = SDL_CreateTexture(g_window.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    if (g_window.texture == NULL)
    {
        (void)fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

static void prv_close_window(void)
{
    if (g_window.texture != NULL)
    {
        SDL_DestroyTexture(g_window.texture);
    }

    if (g_window.renderer != NULL)
    {
        SDL_DestroyRenderer(g_window.renderer);
    }

    if (g_window.window != NULL)
    {
        SDL_DestroyWindow(g_window.window);
    }

    SDL_Quit();
}

/* Blit the frame the display driver was actually handed, one panel pixel per texel. */
static void prv_present_window(void)
{
    const framebuffer_t* const frame = display_host_get_last_frame();
    uint32_t pixels[FRAMEBUFFER_HEIGHT][FRAMEBUFFER_WIDTH];

    for (int16_t y = 0; y < FRAMEBUFFER_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < FRAMEBUFFER_WIDTH; ++x)
        {
            pixels[y][x] =
                (framebuffer_get_pixel(frame, x, y) == FRAMEBUFFER_COLOR_BLACK) ? PIXEL_INK : PIXEL_BACKGROUND;
        }
    }

    (void)SDL_UpdateTexture(g_window.texture, NULL, pixels, (int)sizeof(pixels[0]));
    (void)SDL_RenderClear(g_window.renderer);
    (void)SDL_RenderCopy(g_window.renderer, g_window.texture, NULL, NULL);
    SDL_RenderPresent(g_window.renderer);
}

/* The keyboard stands in for the touchpad: the same four directions and the same one
 * button, so the host plays the game the device plays (FR-003/FR-004). */
static void prv_handle_key(SDL_Keycode in_key)
{
    switch (in_key)
    {
        case SDLK_UP:
        case SDLK_w: game_set_direction(&g_game, DIRECTION_NORTH); break;

        case SDLK_DOWN:
        case SDLK_s: game_set_direction(&g_game, DIRECTION_SOUTH); break;

        case SDLK_LEFT:
        case SDLK_a: game_set_direction(&g_game, DIRECTION_WEST); break;

        case SDLK_RIGHT:
        case SDLK_d: game_set_direction(&g_game, DIRECTION_EAST); break;

        case SDLK_SPACE:
        case SDLK_RETURN:
            /* The button of FR-003: starts a run, and starts the next one once this has
             * ended. Ignored mid-run, so a stray press cannot restart a good game. */
            if (game_get_state(&g_game) != GAME_STATE_RUNNING)
            {
                game_start(&g_game);
            }
            break;

        case SDLK_ESCAPE:
        case SDLK_q: g_is_running = false; break;

        default:
            /* Not a key the game knows. */
            break;
    }
}

static void prv_poll_input(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
        {
            g_is_running = false;
        }
        else if (event.type == SDL_KEYDOWN)
        {
            prv_handle_key(event.key.keysym.sym);
        }
        else
        {
            /* Nothing the game reacts to. */
        }
    }
}

/* The console commentary. On the target this is what the System module and the NVM will
 * hear over the bus; here it is the only way to see that a level really did change. */
static void prv_report_progress(void)
{
    const game_state_e state = game_get_state(&g_game);
    const uint8_t level = game_get_level(&g_game);

    if ((state == g_reported_state) && (level == g_reported_level))
    {
        return;
    }

    g_reported_state = state;
    g_reported_level = level;

    switch (state)
    {
        case GAME_STATE_RUNNING:
            (void)printf("level %u — %u lives, %u points\n", level, game_get_lives(&g_game), game_get_score(&g_game));
            break;

        case GAME_STATE_OVER:
            (void)printf("game over on level %u with %u points. Space to play again.\n", level,
                         game_get_score(&g_game));
            break;

        case GAME_STATE_WON:
            (void)printf("all %u levels cleared with %u points. Space to play again.\n",
                         (unsigned)PLAYFIELD_LEVEL_COUNT, game_get_score(&g_game));
            break;

        default: break;
    }
}

static void prv_run_frame(void)
{
    game_tick(&g_game, FRAME_PERIOD_MS);

    game_view_draw(&g_framebuffer, game_get_snapshot(&g_game));

    display_present(&g_framebuffer);
    display_service();

    prv_present_window();
    prv_report_progress();
}

/* ==========================================================================
 * host_main - entry point
 * ========================================================================= */

int main(int in_argument_count, char** in_arguments)
{
    uint32_t frame_limit = 0U;

    /* `--frames <n>` runs n frames and exits, which is what makes VT-INT-008 automatic:
     * with SDL_VIDEODRIVER=dummy there is no display server to need, and the exit code
     * plus the driver's own present count say whether a frame really was rendered. Zero
     * means run until the player quits, which is the normal case. */
    if ((in_argument_count == 3) && (strcmp(in_arguments[1], "--frames") == 0))
    {
        frame_limit = (uint32_t)strtoul(in_arguments[2], NULL, 10);
    }

    systick_bsp_init();
    sw_timer_init();
    display_init();

    memset(&g_game, 0, sizeof(g_game));
    game_init(&g_game);
    game_start(&g_game);

    if (!prv_open_window())
    {
        prv_close_window();

        return 1;
    }

    (void)printf("%s — arrows or WASD to steer, space to (re)start, escape to quit.\n", WINDOW_TITLE);

    sw_timer_create(&g_frame_timer);
    sw_timer_start(&g_frame_timer, FRAME_PERIOD_MS, prv_on_frame_due);

    while (g_is_running)
    {
        prv_poll_input();
        sw_timer_process();

        if (!g_is_frame_due)
        {
            /* Hand the core back until the frame is due. The target's super-loop has real
             * work to do here instead. */
            SDL_Delay(1U);

            continue;
        }

        g_is_frame_due = false;
        sw_timer_start(&g_frame_timer, FRAME_PERIOD_MS, prv_on_frame_due);

        prv_run_frame();

        if ((frame_limit != 0U) && (display_host_get_present_count() >= frame_limit))
        {
            (void)printf("rendered %u frames\n", display_host_get_present_count());

            g_is_running = false;
        }
    }

    prv_close_window();

    return 0;
}

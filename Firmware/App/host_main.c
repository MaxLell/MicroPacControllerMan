/*
 * host_main.c
 *
 * The host application's entry point — the SDL side of CON-103 / FR-104, and the
 * counterpart to `app_main.c`. It exists so the game can be played, and therefore
 * judged, without a board.
 *
 * There is no `host_main.h`: nothing calls into this file, the C runtime calls `main()`.
 *
 * What this file may do that no other App module may: talk to SDL. It is the host's
 * platform adapter, the way the CubeMX `main()` is the target's — everything below it is
 * the same code that runs on the STM32. SDL never sees the game and the game never sees
 * SDL.
 *
 * **The picture is not re-derived for the window.** The game draws through the ordinary
 * display port, and this file blits back what the *driver was actually handed*
 * (`display_host_get_last_frame()`). So the window shows exactly the pixels the panel
 * would be clocked with, including the partial updates — a layout bug looks the same in
 * both places, which is the only reason developing the view on the host is worth
 * anything.
 *
 * The loop is shaped like the target's future super-loop rather than like an SDL game
 * loop: a `Services/sw_timer` re-armed from its own callback paces the frames, and the
 * game is advanced by a **fixed** slice. Fixed rather than measured on purpose — a run
 * then plays out identically whatever the host was doing at the time, which is what
 * makes a bug seen here reproducible.
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
#include "msg.h"
#include "playfield.h"
#include "render.h"
#include "sw_timer.h"
#include "systick_bsp.h"

/* ==========================================================================
 * host_main - private
 * ========================================================================= */

#define WINDOW_TITLE    "MicroPacControllerMan"

/* One panel pixel per two window pixels: 240 x 320 is small on a desktop, and doubling
 * keeps the sprites' pixel grid honest instead of smoothing it away. */
#define WINDOW_SCALE    (2)

/* 60 FPS, the rate NFR-002 asks for. The slice handed to the game is this same number,
 * so the simulation advances by exactly one frame per frame. */
#define FRAME_PERIOD_MS (16U)

#define RED_MASK_5      (0xF800U)
#define GREEN_MASK_6    (0x07E0U)
#define BLUE_MASK_5     (0x001FU)

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
} host_window_t;

static host_window_t g_window;
static game_t g_game;
static game_view_t g_view;
static sw_timer_t g_frame_timer;

static bool g_is_running = true;
static bool g_is_frame_due = false;
static game_state_e g_reported_state = GAME_STATE_IDLE;
static uint8_t g_reported_level = 0U;

static void prv_on_frame_due(void)
{
    g_is_frame_due = true;

    sw_timer_start(&g_frame_timer, FRAME_PERIOD_MS, prv_on_frame_due);
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

    g_window.renderer = SDL_CreateRenderer(g_window.window, -1, 0U);

    if (g_window.renderer == NULL)
    {
        (void)fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());

        return false;
    }

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

/* RGB565 as the panel takes it, widened to the 8 bits per channel a desktop wants. The
 * low bits are replicated rather than zeroed, so full-scale stays full-scale — otherwise
 * white would arrive as 0xF8FCF8 and every screenshot would be subtly wrong. */
static uint32_t prv_to_argb(framebuffer_color_t in_colour)
{
    const uint32_t red = (uint32_t)((in_colour & RED_MASK_5) >> 11);
    const uint32_t green = (uint32_t)((in_colour & GREEN_MASK_6) >> 5);
    const uint32_t blue = (uint32_t)(in_colour & BLUE_MASK_5);

    return 0xFF000000U | (((red << 3) | (red >> 2)) << 16) | (((green << 2) | (green >> 4)) << 8)
           | ((blue << 3) | (blue >> 2));
}

/* Blit the frame the display driver was actually handed, one panel pixel per texel. */
static void prv_present_window(void)
{
    static uint32_t pixels[FRAMEBUFFER_HEIGHT][FRAMEBUFFER_WIDTH];
    const framebuffer_t* const frame = display_host_get_last_frame();

    for (int16_t y = 0; y < FRAMEBUFFER_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < FRAMEBUFFER_WIDTH; ++x)
        {
            pixels[y][x] = prv_to_argb(framebuffer_get_pixel(frame, x, y));
        }
    }

    (void)SDL_UpdateTexture(g_window.texture, NULL, pixels, (int)sizeof(pixels[0]));
    (void)SDL_RenderClear(g_window.renderer);
    (void)SDL_RenderCopy(g_window.renderer, g_window.texture, NULL, NULL);
    SDL_RenderPresent(g_window.renderer);
}

/* The keyboard stands in for the joystick: the same four directions and the same one
 * button, so the host plays the game the device plays (FR-003 / FR-004). */
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
                         (unsigned)DIFFICULTY_FINAL_LEVEL, game_get_score(&g_game));
            break;

        default: break;
    }
}

/* One frame, through exactly the path the target will use: the game advances, the view
 * turns its state into pixels, and Render decides what to transfer. */
static void prv_run_frame(void)
{
    msg_game_state_t state;
    msg_display_list_t list;

    game_tick(&g_game, FRAME_PERIOD_MS);

    game_get_state_message(&g_game, &state);
    game_view_set_state(&g_view, &state);

    /* A level change hands the whole field over across several lists; an ordinary frame
     * is one. Both are drained here so a frame is never left half-drawn. */
    do
    {
        if (game_view_get_display_list(&g_view, &list))
        {
            render_draw(&list);
        }
    } while (game_view_is_field_pending(&g_view));

    display_service();

    prv_present_window();
    prv_report_progress();
}

/* ==========================================================================
 * host_main - entry point
 * ========================================================================= */

int main(int in_argument_count, char** in_arguments)
{
    (void)in_argument_count;
    (void)in_arguments;

    systick_bsp_init();
    sw_timer_init();
    render_init();

    game_init(&g_game);
    game_view_init(&g_view);

    if (!prv_open_window())
    {
        prv_close_window();

        return EXIT_FAILURE;
    }

    (void)printf("%s — arrows or WASD to move, space to start, escape to quit.\n", WINDOW_TITLE);

    sw_timer_create(&g_frame_timer);
    sw_timer_start(&g_frame_timer, FRAME_PERIOD_MS, prv_on_frame_due);

    while (g_is_running)
    {
        sw_timer_process();
        prv_poll_input();

        if (g_is_frame_due)
        {
            g_is_frame_due = false;

            prv_run_frame();
        }
        else
        {
            /* Nothing due yet. Giving the millisecond back keeps a developer's fan quiet
             * without changing what the game sees, because the slice is fixed. */
            SDL_Delay(1U);
        }
    }

    prv_close_window();

    return EXIT_SUCCESS;
}

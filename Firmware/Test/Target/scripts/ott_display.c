#include "ott_display.h"

#include "button.h"
#include "display.h"
#include "gfx.h"
#include "systick.h"
#include "uart.h"

#define W DISPLAY_WIDTH
#define H DISPLAY_HEIGHT
#define BLACK DISPLAY_BLACK
#define SCENE_MS (1500U)
#define DISPLAY_MAX_MS (120000U)

/* Hold the current frame for `ms`, servicing VCOM ~1 Hz. Returns 1 if the USER
 * button was pressed during the hold (to advance/finish early), else 0. */
static int prv_hold(uint32_t ms)
{
    uint32_t start = millis();
    uint32_t last_vcom = start;
    while ((millis() - start) < ms) {
        if (button_pressed()) {
            return 1;
        }
        if ((millis() - last_vcom) >= 1000U) {
            last_vcom = millis();
            display_vcom_tick();
        }
        delay_ms(10);
    }
    return 0;
}

static void scene_lines(void)
{
    gfx_fill(DISPLAY_WHITE);
    for (int x = 0; x < W; x += 8) {
        gfx_line(0, 0, x, H - 1, BLACK);
    }
    for (int y = 0; y < H; y += 8) {
        gfx_line(W - 1, 0, 0, y, BLACK);
    }
}

static void scene_rects(void)
{
    gfx_fill(DISPLAY_WHITE);
    for (int i = 0; i < W / 2; i += 6) {
        gfx_rect(i, i, W - 2 * i, H - 2 * i, BLACK);
    }
}

static void scene_fill_rects(void)
{
    gfx_fill(DISPLAY_WHITE);
    for (int i = 0; i < W / 2; i += 12) {
        gfx_fill_rect(i, i, W - 2 * i, H - 2 * i, (i / 12) & 1 ? DISPLAY_WHITE : BLACK);
    }
}

static void scene_circles(void)
{
    gfx_fill(DISPLAY_WHITE);
    for (int r = 6; r < W; r += 8) {
        gfx_circle(W / 2, H / 2, r, BLACK);
    }
}

static void scene_fill_circles(void)
{
    gfx_fill(DISPLAY_WHITE);
    for (int y = 16; y < H; y += 32) {
        for (int x = 16; x < W; x += 32) {
            gfx_fill_circle(x, y, 12, BLACK);
        }
    }
}

static void scene_triangles(void)
{
    gfx_fill(DISPLAY_WHITE);
    for (int i = 0; i < H / 2; i += 8) {
        gfx_triangle(W / 2, i, i, H - 1 - i, W - 1 - i, H - 1 - i, BLACK);
    }
}

static void scene_composite(void)
{
    gfx_fill(DISPLAY_WHITE);
    gfx_rect(0, 0, W, H, BLACK);
    gfx_fill_rect(8, 8, 40, 40, BLACK);
    gfx_circle(96, 28, 20, BLACK);
    gfx_fill_circle(96, 28, 10, BLACK);
    gfx_line(8, 120, 120, 60, BLACK);
    gfx_fill_triangle(20, 118, 64, 70, 108, 118, BLACK);
}

int ott_display_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0;
    return 1;
}

int ott_display_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;
    (void)reason;
    (void)reason_size;

    button_init();
    display_init();

    uart_write("Display test: geometric patterns on the LCD Mono Click.\r\n");
    uart_write("Watch the panel cycle lines/rects/circles/triangles, then hold.\r\n");
    uart_write("Press USER button (B1) to finish.\r\n");

    void (*scenes[])(void) = {scene_lines,   scene_rects,        scene_fill_rects, scene_circles,
                              scene_fill_circles, scene_triangles, scene_composite};
    unsigned n = sizeof(scenes) / sizeof(scenes[0]);

    uint32_t start = millis();
    for (unsigned i = 0; i < n; i++) {
        scenes[i]();
        display_flush();
        if (prv_hold(SCENE_MS)) {
            return 1; /* early finish */
        }
    }

    /* Hold the composite until the operator confirms (or the safety cap). */
    scene_composite();
    display_flush();
    while ((millis() - start) < DISPLAY_MAX_MS) {
        if (prv_hold(1000)) {
            break;
        }
    }
    return 1;
}

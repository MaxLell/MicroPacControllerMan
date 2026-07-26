#include "ott_dispdiag.h"

#include "button.h"
#include "display.h"
#include "gfx.h"
#include "systick.h"
#include "uart.h"

/* Logic-analyzer hookup (LCD Mono Click, mikroBUS slot 1 -> NUCLEO-G431RB):
 *   SCK  = PA5   (Arduino D13)   SPI clock
 *   MOSI = PA7   (Arduino D11)   serial data (SI)
 *   CS   = PB6                   chip select, ACTIVE-HIGH
 *   DISP = PA6   (Arduino D12)   panel on/off, HIGH = on
 *   EXTCOMIN = PB10              external VCOM clock (PWM line)
 * SPI decoder settings: 8-bit, MSB-first? NO -> LSB-first, mode 0 (CPOL=0,
 * CPHA=0), enable line ACTIVE-HIGH, clock ~0.66 MHz. */

#define STATIC_HOLD_MS (8000U)  /* phase 1: time to meter DC levels        */
#define BURST_COUNT    (10U)    /* phase 2: number of 2-byte clear bursts  */
#define BURST_GAP_MS   (800U)   /* phase 2: quiet gap between bursts        */
#define FRAME_HOLD_MS  (1500U)  /* phase 3: hold each full frame            */
#define DIAG_MAX_MS    (180000U)/* safety cap so the OTT always terminates  */

/* Hold for `ms`, servicing software+external VCOM ~1 Hz. Returns 1 if the USER
 * button was pressed during the hold (to finish early), else 0. */
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

int ott_dispdiag_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0;
    return 1;
}

int ott_dispdiag_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;
    (void)reason;
    (void)reason_size;

    button_init();
    display_init(); /* DISP high, CS + EXTCOMIN low, panel cleared to white */

    uart_write("\r\n=== LCD Mono Click SPI diagnosis (dispdiag) ===\r\n");
    uart_write("Probe: SCK=PA5, MOSI=PA7, CS=PB6(active-HIGH), DISP=PA6, EXTCOMIN=PB10.\r\n");
    uart_write("Decoder: 8-bit, LSB-first, mode 0 (CPOL=0/CPHA=0), enable ACTIVE-HIGH, ~0.66 MHz.\r\n");
    uart_write("Press USER button (B1) at any time to finish.\r\n\r\n");

    uint32_t start = millis();

    /* -- Phase 1: static levels -------------------------------------------- */
    uart_write("[1] STATIC LEVELS for 8 s. No SPI activity. Meter now:\r\n");
    uart_write("    PA6/DISP should read ~3.3 V (panel enabled).\r\n");
    uart_write("    PB6/CS, PB10/EXTCOMIN, PA5/SCK, PA7/MOSI should read ~0 V (idle).\r\n");
    if (prv_hold(STATIC_HOLD_MS)) {
        goto done;
    }

    /* -- Phase 2: short, decodable bursts ---------------------------------- */
    uart_write("\r\n[2] SHORT BURSTS: 10x a 2-byte all-clear command.\r\n");
    uart_write("    Trigger the LA on CS(PB6) rising edge; each burst is CS-HIGH,\r\n");
    uart_write("    two bytes on SCK/MOSI (0x04 or 0x06 = M2 clear, M1/VCOM toggles),\r\n");
    uart_write("    then CS-LOW. Use these to confirm SPI mode and LSB bit order.\r\n");
    for (unsigned i = 0; i < BURST_COUNT; i++) {
        display_all_clear(); /* one CS-framed [cmd][0x00] transaction */
        uart_write("    -> clear burst sent\r\n");
        if (prv_hold(BURST_GAP_MS)) {
            goto done;
        }
    }

    /* -- Phase 3: full-frame black/white (bulk data + visual test) ---------- */
    uart_write("\r\n[3] FULL FRAMES: alternating all-BLACK / all-WHITE flushes.\r\n");
    uart_write("    Each flush = CS-HIGH, ~2.3 kB on SCK/MOSI, CS-LOW.\r\n");
    uart_write("    Panel should visibly toggle solid black <-> blank. If nothing\r\n");
    uart_write("    changes but SCK/MOSI clock correctly, suspect DISP/JP1/contrast.\r\n");
    while ((millis() - start) < DIAG_MAX_MS) {
        gfx_fill(DISPLAY_BLACK);
        display_flush();
        uart_write("    -> BLACK frame flushed\r\n");
        if (prv_hold(FRAME_HOLD_MS)) {
            goto done;
        }

        gfx_fill(DISPLAY_WHITE);
        display_flush();
        uart_write("    -> WHITE frame flushed\r\n");
        if (prv_hold(FRAME_HOLD_MS)) {
            goto done;
        }
    }

done:
    uart_write("\r\ndispdiag finished.\r\n");
    return 1;
}

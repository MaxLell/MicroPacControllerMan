/*
 * console.h
 *
 * The serial console's transport, and the only module above the BSP that names a
 * UART. Everything built on the console — the CLI, the OTT core, the test
 * scenarios — talks to this interface instead, so moving the console to another
 * transport means replacing this module and nothing else.
 *
 * Output goes through `cli_print()` — that is what a caller should reach for. What
 * remains here is what the command line cannot express: the character sink it
 * writes into, the character source it is fed from, and a flush before a reset.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>

/* ==========================================================================
 * console - public API
 * ========================================================================= */

/*! \brief Bring the console transport up. Must be called before any other
 *         function of this module. */
void console_init(void);

/*! \brief Send one character, blocking until it has been handed to the hardware.
 *
 * \param[in]       in_character: character to send
 */
void console_write_character(char in_character);

/*! \brief Take one received character if one is waiting.
 *
 * Never blocks, so it can be polled from the main loop.
 *
 * \param[out]      out_character: receives the character, must not be `NULL`
 * \return          `true` when a character was taken, `false` when none was ready
 */
bool console_read_character(char* out_character);

/*! \brief Take whatever the UART has received into the console's own buffer.
 *
 * **Call this from the 1 ms tick, not from the main loop.** The receive register holds
 * exactly one character and has no FIFO, so anything arriving while the loop is busy
 * elsewhere is lost — and once the game is running, the loop spends milliseconds at a time
 * inside a frame. Sampling on the tick decouples the two: a command typed during a frame
 * waits in the buffer instead of falling on the floor.
 *
 * It follows that characters still have to arrive more than a millisecond apart. That is
 * true of anything typed and of `run_ott.py`, which paces itself at 2 ms for this very
 * reason; it is not true of a line pasted into a terminal at full speed. Fixing that
 * properly needs the UART receive interrupt, which CubeMX does not currently enable
 * (RF-016).
 */
void console_poll_receive(void);

/*! \brief Block until everything written has left the transmitter.
 *
 * Needed before a software reset: without it the last line is still sitting in
 * the peripheral when the core restarts, and is lost.
 */
void console_flush(void);

#endif /* CONSOLE_H */

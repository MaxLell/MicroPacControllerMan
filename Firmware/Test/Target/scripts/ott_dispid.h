/*
 * ott_dispid.h
 *
 * dispid OTT (board bring-up): resets the display controller and reads its
 * identification registers, so the firmware learns three things at once before any
 * driver exists.
 *
 *  - Whether the display answers at all, which is the only available check that the
 *    SPI and control pins are the ones the pin map claims.
 *  - Which chip-select polarity works. UM2750 says "SPI chip select active high" in
 *    five places, including for a NOR flash whose pin is named CS# — so the document
 *    contradicts itself and cannot be trusted here. This test tries both.
 *  - What the controller reports as its identity, which either backs up the ST7789V
 *    read off the board's sticker or contradicts it.
 *
 * Automatic: nobody has to look at the panel, because nothing is drawn. Takes no
 * arguments, so the scenario table carries `NULL`.
 */

#ifndef OTT_DISPID_H
#define OTT_DISPID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_dispid - public API
 * ========================================================================= */

bool ott_dispid_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_DISPID_H */

/*
 * display_host.h
 *
 * Extra API of the host implementation of the display port, for whoever renders or
 * inspects what was presented. Not available on the target — only display_host.c
 * defines these.
 */

#ifndef DISPLAY_HOST_H
#define DISPLAY_HOST_H

#include <stdbool.h>
#include <stdint.h>

#include "framebuffer.h"

/* ==========================================================================
 * display_host - public API
 * ========================================================================= */

/*! \brief Borrow the most recently presented frame.
 *
 * All white until the first #display_present.
 *
 * \return          Pointer to the stored frame, never `NULL`
 */
const framebuffer_t* display_host_get_last_frame(void);

/*! \brief Number of #display_present calls since #display_init. */
uint32_t display_host_get_present_count(void);

/*! \brief Number of #display_service calls since #display_init. */
uint32_t display_host_get_service_count(void);

/*! \brief Whether the display is currently enabled. */
bool display_host_is_enabled(void);

#endif /* DISPLAY_HOST_H */

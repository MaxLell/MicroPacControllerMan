/*
 * retain_ram.h
 *
 * Fixed-size RAM buffer that survives a software reset. This module owns the
 * memory only — what the bytes mean is entirely up to the caller.
 */

#ifndef RETAIN_RAM_H
#define RETAIN_RAM_H

#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * retain_ram - public API
 * ========================================================================= */

#define RETAIN_RAM_BUFFER_SIZE (64U)

/*! \brief Copy a buffer into the retained memory.
 *
 * \param[in]       in_buffer: source bytes, must not be `NULL`
 * \param[in]       in_buffer_size: must equal #RETAIN_RAM_BUFFER_SIZE
 */
void retained_ram_write(const uint8_t* const in_buffer, size_t in_buffer_size);

/*! \brief Copy the retained memory into a buffer.
 *
 * The contents are undefined after a power-on or brown-out reset, so the caller
 * must be able to recognise garbage (magic word, checksum, ...).
 *
 * \param[out]      out_buffer: receives the bytes, must not be `NULL`
 * \param[in]       in_buffer_size: must equal #RETAIN_RAM_BUFFER_SIZE
 */
void retained_ram_read(uint8_t* const out_buffer, size_t in_buffer_size);

#endif /* RETAIN_RAM_H */

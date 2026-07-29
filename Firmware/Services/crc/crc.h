/*
 * crc.h
 *
 * Cyclic redundancy check over a byte range. Detects accidental corruption —
 * a partially written buffer, a decayed retained-RAM region — and is not a
 * defence against deliberate tampering.
 */

#ifndef CRC_H
#define CRC_H

#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * crc - public API
 * ========================================================================= */

/*! \brief CRC-32/ISO-HDLC over a byte range.
 *
 * The variant used by zip, gzip and Ethernet: reflected, polynomial
 * `0xEDB88320`, initial value all-ones, final XOR all-ones. Computed bitwise so
 * the module costs no lookup table.
 *
 * \param[in]       in_data: bytes to cover, must not be `NULL` unless
 *                      `in_size` is `0`
 * \param[in]       in_size: number of bytes
 * \return          The checksum. `0` for an empty range.
 */
uint32_t crc_32(const uint8_t* in_data, size_t in_size);

#endif /* CRC_H */

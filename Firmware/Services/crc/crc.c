#include "crc.h"

#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"

/* ==========================================================================
 * crc - private
 * ========================================================================= */

#define CRC_32_POLYNOMIAL_REFLECTED (0xEDB88320U)
#define CRC_32_INITIAL_VALUE (0xFFFFFFFFU)
#define CRC_32_BITS_PER_BYTE (8U)
#define CRC_32_LOWEST_BIT_MASK (1U)

/* ==========================================================================
 * crc - public
 * ========================================================================= */

uint32_t crc_32(const uint8_t* in_data, size_t in_size)
{
    uint32_t crc = CRC_32_INITIAL_VALUE;

    ASSERT((in_data != NULL) || (in_size == 0U));

    for (size_t index = 0U; index < in_size; ++index)
    {
        crc ^= (uint32_t)in_data[index];

        for (uint8_t bit = 0U; bit < CRC_32_BITS_PER_BYTE; ++bit)
        {
            if ((crc & CRC_32_LOWEST_BIT_MASK) != 0U)
            {
                crc = (crc >> 1U) ^ CRC_32_POLYNOMIAL_REFLECTED;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return ~crc;
}

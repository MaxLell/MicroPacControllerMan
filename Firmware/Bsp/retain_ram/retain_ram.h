#ifndef RETAIN_RAM_H
#define RETAIN_RAM_H

#include <stdint.h>

/*
 * Retained RAM: a small object placed in the linker's `.noinit` section, which
 * the startup code does NOT zero, so its contents survive a software reset
 * (they are only lost on a cold/power-on or brown-out reset). Used to carry an
 * OTT test request across the reset that the OTT mechanism performs (doc 09).
 */

#define OTT_ARG_MAX 32U

typedef struct {
    uint32_t magic;              /* set to OTT_MAGIC when a request is valid   */
    uint32_t checksum;           /* over test_id + data_size + data[]          */
    uint32_t test_id;            /* 0 = none; otherwise scenario index + 1     */
    uint32_t data_size;          /* valid bytes in data[]                      */
    uint8_t  data[OTT_ARG_MAX];  /* opaque per-test parameter blob             */
} ott_spec_t;

/* Pointer to the single retained OTT request (lives in `.noinit`). */
ott_spec_t* retain_ott_spec(void);

#endif /* RETAIN_RAM_H */

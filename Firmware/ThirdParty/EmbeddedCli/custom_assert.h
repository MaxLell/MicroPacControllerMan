#ifndef CUSTOM_ASSERT_H
#define CUSTOM_ASSERT_H

/*
 * Firmware shim for EmbeddedCli's assert hook. On a failed assertion (a
 * programming error — null pointer, corrupted canary, ...) we halt at a
 * debugger breakpoint, matching the project's fatal-error handling (FR-111).
 */
#define ASSERT(expr)                          \
    do {                                      \
        if (!(expr)) {                        \
            __asm volatile("bkpt #0");        \
        }                                     \
    } while (0)

#endif /* CUSTOM_ASSERT_H */

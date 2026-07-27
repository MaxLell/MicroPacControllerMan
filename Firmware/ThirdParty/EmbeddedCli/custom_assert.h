#ifndef CUSTOM_ASSERT_H
#define CUSTOM_ASSERT_H

/*
 * Fatal-error handling for programming mistakes (FR-111). A failed assertion is a
 * bug, not a runtime condition, so it stops the program instead of returning an
 * error — callers are not expected to handle it.
 *
 * On the ARM target it breaks to the debugger; with none attached the BKPT
 * escalates to a HardFault, which halts the board — the intended outcome for a
 * detected bug. On the host it aborts, so a failing build or test run says so
 * loudly rather than executing an illegal instruction.
 *
 * Release-build behaviour is still undecided — see RF-011 in the refactoring
 * backlog. The coding standard wants assertions disabled in Release; there is no
 * Release build yet, so nothing depends on it.
 */

#if defined(__arm__)
#define ASSERT_FAIL() __asm volatile("bkpt #0")
#else /* defined(__arm__) */
#include <stdlib.h>
#define ASSERT_FAIL() abort()
#endif /* !defined(__arm__) */

#define ASSERT(expr)       \
    do                     \
    {                      \
        if (!(expr))       \
        {                  \
            ASSERT_FAIL(); \
        }                  \
    } while (0)

#endif /* CUSTOM_ASSERT_H */

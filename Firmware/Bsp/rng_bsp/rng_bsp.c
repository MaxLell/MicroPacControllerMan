/*
 * Target implementation of the random source: the STM32U545's RNG peripheral. See rng_bsp.h.
 *
 * **This is the project's second direct register access**, and the coding standard asks for the
 * reason (NFR-102, "HAL over registers"). The reason is that the HAL's RNG driver is *not compiled*:
 * `HAL_RNG_MODULE_ENABLED` is commented out in the CubeMX export's `stm32u5xx_hal_conf.h`, and
 * `stm32u5xx_hal_rng.c` is not in the build. Turning it on means editing generated code — and a
 * CubeMX regeneration silently discards exactly that, which this project has already been bitten by
 * twice and documents as two edits that must be re-applied by hand
 * ([DEC-012](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)). A third such edit buys the
 * three register writes below, which is a bad trade: what is here cannot be dropped by a
 * regeneration, because it is our file.
 *
 * The other reason is that there is very little to get wrong. An RNG is a clock, an enable bit, and
 * a "ready" flag; the HAL's driver around that is a handle, a lock and a state machine this module
 * does not need. The two things that *are* easy to get wrong — which clock feeds it, and noticing
 * when it says its entropy is bad — are both handled explicitly below, and the RCC part still goes
 * through the HAL's own macros.
 */
#include "rng_bsp.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32u5xx_hal.h"

/* ==========================================================================
 * rng_bsp - private
 * ========================================================================= */

/* Loops spent waiting, rather than a time: this runs before `systick_bsp` is guaranteed to be
 * counting, and a spin count is exactly what the coding standard forbids for a *delay*. This is not
 * a delay — nothing is being timed, the loop is a bound on a peripheral that either answers or is
 * broken. At 160 MHz the figure is a fraction of a millisecond, and the RNG produces a word every
 * few dozen cycles of its own 48 MHz clock. */
#define RNG_BSP_SPIN_LIMIT (100000U)

static bool g_is_available;

/* Wait for a bit in a register, and say whether it arrived. */
static bool prv_wait_for(const volatile uint32_t* in_register, uint32_t in_mask)
{
    for (uint32_t spin = 0U; spin < RNG_BSP_SPIN_LIMIT; ++spin)
    {
        if ((*in_register & in_mask) != 0U)
        {
            return true;
        }
    }

    return false;
}

/* ==========================================================================
 * rng_bsp - public
 * ========================================================================= */

bool rng_bsp_init(void)
{
    g_is_available = false;

    /* The RNG runs from its own 48 MHz kernel clock, not from the system clock, and this part's
     * source for that is HSI48. The CubeMX configuration already computes `RNGFreq_Value =
     * 48000000`, but nothing had switched HSI48 *on*, because no peripheral was using it — so this
     * is the step that would otherwise leave the peripheral enabled and permanently not ready. */
    __HAL_RCC_HSI48_ENABLE();

    if (!prv_wait_for(&RCC->CR, RCC_CR_HSI48RDY))
    {
        return false;
    }

    /* Selected explicitly rather than left at its reset value: a reset default is a fact about
     * silicon that a later clock-tree change in CubeMX could quietly move out from under this. */
    MODIFY_REG(RCC->CCIPR2, RCC_CCIPR2_RNGSEL, 0U);

    __HAL_RCC_RNG_CLK_ENABLE();

    RNG->CR |= RNG_CR_RNGEN;

    /* The first word is waited for here rather than at the first call, so "did the generator come
     * up" is answered once, at start-up, where a caller can report it — instead of turning into a
     * game whose timings mysteriously never vary. */
    if (!prv_wait_for(&RNG->SR, RNG_SR_DRDY))
    {
        return false;
    }

    /* A seed or clock error means the entropy is not trustworthy. Better to report the generator as
     * unavailable and play nominal timings than to pace a game from a number the silicon has
     * flagged. */
    if ((RNG->SR & (RNG_SR_SECS | RNG_SR_CECS)) != 0U)
    {
        return false;
    }

    (void)RNG->DR;

    g_is_available = true;

    return true;
}

bool rng_bsp_is_available(void)
{
    return g_is_available;
}

void rng_bsp_seed(uint32_t in_seed)
{
    /* Nothing to seed: this is an entropy source, not a sequence. Taken rather than refused,
     * because the caller is the same code that runs on the host and it has no business knowing
     * which platform it is on. */
    (void)in_seed;
}

uint32_t rng_bsp_get_u32(void)
{
    if (!g_is_available)
    {
        return 0U;
    }

    if (!prv_wait_for(&RNG->SR, RNG_SR_DRDY))
    {
        return 0U;
    }

    /* Reading DR clears DRDY, which is what asks for the next word. */
    return RNG->DR;
}

uint32_t rng_bsp_get_below(uint32_t in_span)
{
    if (in_span == 0U)
    {
        return 0U;
    }

    return rng_bsp_get_u32() % in_span;
}

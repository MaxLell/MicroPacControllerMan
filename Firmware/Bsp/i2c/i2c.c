#include "i2c.h"

#include "stm32g4xx.h"

/* Bounded spin count — generous vs. one I2C byte at 100 kHz (~90 us) on a 16 MHz
 * core, but small enough that a dead bus is reported in well under a second. */
#define I2C_SPIN_TIMEOUT (200000U)

/* Standard-mode (100 kHz) timing for I2CCLK = PCLK1 = 16 MHz (HSI), from the
 * RM0440 timing-example table: PRESC=3, SCLDEL=4, SDADEL=2, SCLH=0x0F, SCLL=0x13. */
#define I2C_TIMINGR_100K_16MHZ (0x30420F13U)

void i2c_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;

    /* PB8/PB9 -> AF (MODER=10), open-drain, internal pull-ups. */
    GPIOB->MODER &= ~(GPIO_MODER_MODE8_Msk | GPIO_MODER_MODE9_Msk);
    GPIOB->MODER |= (0x2U << GPIO_MODER_MODE8_Pos) | (0x2U << GPIO_MODER_MODE9_Pos);
    GPIOB->OTYPER |= GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9;
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8_Msk | GPIO_PUPDR_PUPD9_Msk);
    GPIOB->PUPDR |= (0x1U << GPIO_PUPDR_PUPD8_Pos) | (0x1U << GPIO_PUPDR_PUPD9_Pos);
    /* AF4 = I2C1 on PB8/PB9. */
    GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL8_Msk | GPIO_AFRH_AFSEL9_Msk);
    GPIOB->AFR[1] |= (4U << GPIO_AFRH_AFSEL8_Pos) | (4U << GPIO_AFRH_AFSEL9_Pos);

    I2C1->CR1 &= ~I2C_CR1_PE; /* configure with the peripheral disabled */
    I2C1->TIMINGR = I2C_TIMINGR_100K_16MHZ;
    I2C1->CR1 |= I2C_CR1_PE;
}

/* Spin on an ISR flag; abort early on a NACK. */
static int prv_wait_flag(uint32_t mask)
{
    uint32_t spins = I2C_SPIN_TIMEOUT;
    while (!(I2C1->ISR & mask)) {
        if (I2C1->ISR & I2C_ISR_NACKF) {
            I2C1->ICR = I2C_ICR_NACKCF;
            return I2C_ERR_NACK;
        }
        if (--spins == 0U) {
            return I2C_ERR_TIMEOUT;
        }
    }
    return I2C_OK;
}

/* Clear any leftover STOP/NACK flags from a prior (possibly aborted) transfer. */
static void prv_clear_flags(void) { I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF; }

int i2c_write(uint8_t addr7, const uint8_t* data, size_t len)
{
    prv_clear_flags();
    I2C1->CR2 = ((uint32_t)addr7 << 1) | ((uint32_t)len << I2C_CR2_NBYTES_Pos) |
                I2C_CR2_AUTOEND | I2C_CR2_START;

    for (size_t i = 0; i < len; i++) {
        int rc = prv_wait_flag(I2C_ISR_TXIS);
        if (rc != I2C_OK) {
            return rc;
        }
        I2C1->TXDR = data[i];
    }
    int rc = prv_wait_flag(I2C_ISR_STOPF);
    if (rc != I2C_OK) {
        return rc;
    }
    /* A NACK on the final byte sets NACKF and (via AUTOEND) STOPF together, so the
     * STOPF wait above would otherwise mask it — check explicitly. */
    if (I2C1->ISR & I2C_ISR_NACKF) {
        I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
        return I2C_ERR_NACK;
    }
    I2C1->ICR = I2C_ICR_STOPCF;
    return I2C_OK;
}

int i2c_read(uint8_t addr7, uint8_t* data, size_t len)
{
    prv_clear_flags();
    I2C1->CR2 = ((uint32_t)addr7 << 1) | I2C_CR2_RD_WRN |
                ((uint32_t)len << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND | I2C_CR2_START;

    for (size_t i = 0; i < len; i++) {
        int rc = prv_wait_flag(I2C_ISR_RXNE);
        if (rc != I2C_OK) {
            return rc;
        }
        data[i] = (uint8_t)I2C1->RXDR;
    }
    int rc = prv_wait_flag(I2C_ISR_STOPF);
    if (rc != I2C_OK) {
        return rc;
    }
    I2C1->ICR = I2C_ICR_STOPCF;
    return I2C_OK;
}

int i2c_write_read(uint8_t addr7, const uint8_t* wr, size_t wlen, uint8_t* rd, size_t rlen)
{
    /* Phase 1: write the register pointer, no STOP (AUTOEND=0) so we can repeated-START. */
    prv_clear_flags();
    I2C1->CR2 = ((uint32_t)addr7 << 1) | ((uint32_t)wlen << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;

    for (size_t i = 0; i < wlen; i++) {
        int rc = prv_wait_flag(I2C_ISR_TXIS);
        if (rc != I2C_OK) {
            return rc;
        }
        I2C1->TXDR = wr[i];
    }
    int rc = prv_wait_flag(I2C_ISR_TC);
    if (rc != I2C_OK) {
        return rc;
    }

    /* Phase 2: repeated-START read, AUTOEND generates the STOP. */
    return i2c_read(addr7, rd, rlen);
}

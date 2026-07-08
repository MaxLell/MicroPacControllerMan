#include "spi.h"

#include "stm32g4xx.h"

void spi_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* PA5 (SCK) and PA7 (MOSI) -> alternate function (MODER = 10). PA6 is left to
     * the display driver as a plain GPIO output (DISP), so it is not touched here. */
    GPIOA->MODER &= ~(GPIO_MODER_MODE5_Msk | GPIO_MODER_MODE7_Msk);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODE5_Pos) | (0x2U << GPIO_MODER_MODE7_Pos);
    /* High speed for a clean ~1 MHz edge. */
    GPIOA->OSPEEDR |= (0x3U << GPIO_OSPEEDR_OSPEED5_Pos) | (0x3U << GPIO_OSPEEDR_OSPEED7_Pos);
    /* AF5 = SPI1 on PA5/PA7. */
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL5_Msk | GPIO_AFRL_AFSEL7_Msk);
    GPIOA->AFR[0] |= (5U << GPIO_AFRL_AFSEL5_Pos) | (5U << GPIO_AFRL_AFSEL7_Pos);

    /* Master, BR = fPCLK2/16 = 1 MHz (PCLK2 = 16 MHz), CPOL=0/CPHA=0 (mode 0),
     * LSB-first (LS013B7DH03 sends address/data LSB-first), software NSS high. */
    SPI1->CR1 = 0U;
    SPI1->CR1 = SPI_CR1_MSTR | (0x3U << SPI_CR1_BR_Pos) | SPI_CR1_LSBFIRST | SPI_CR1_SSM |
                SPI_CR1_SSI;
    /* 8-bit data size, RXNE on 1/4 (byte) so single-byte writes don't stall. */
    SPI1->CR2 = (0x7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi_write(const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while (!(SPI1->SR & SPI_SR_TXE)) {
        }
        *(volatile uint8_t*)&SPI1->DR = data[i]; /* 8-bit write into the FIFO */
    }
    /* Wait for the last byte to fully shift out before the caller drops CS. */
    while (SPI1->SR & SPI_SR_BSY) {
    }
    /* Drain the RX FIFO so a stale OVR does not linger (write-only use). */
    (void)SPI1->DR;
    (void)SPI1->SR;
}

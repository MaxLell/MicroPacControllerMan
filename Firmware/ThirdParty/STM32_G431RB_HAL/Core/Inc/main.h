/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define USER_BUTTON_Pin GPIO_PIN_13
#define USER_BUTTON_GPIO_Port GPIOC
#define TOUCHPAD_RESET_Pin GPIO_PIN_4
#define TOUCHPAD_RESET_GPIO_Port GPIOA
/* Slot-1 control pins for the MikroE Click Shield for Nucleo-64 (MIKROE-5193),
 * which mates with the ST-Morpho headers — NOT the Arduino header. Slot-1 SPI is
 * SCK=PB3 / MOSI=PB5 / MISO=PB4 (see spi.c), CS1=PB12, PWM1=PC8. Verified against
 * the shield schematic v1.01 + UM2505 and confirmed on hardware (R-001 closed). */
#define DISPLAY_DISP_Pin GPIO_PIN_4      /* PB4 = mikroBUS MISO line = panel DISP */
#define DISPLAY_DISP_GPIO_Port GPIOB
#define DISPLAY_EXTCOMIN_Pin GPIO_PIN_8  /* PC8 = mikroBUS PWM1 = EXTCOMIN         */
#define DISPLAY_EXTCOMIN_GPIO_Port GPIOC
#define DISPLAY_CS_Pin GPIO_PIN_12       /* PB12 = mikroBUS CS1, active HIGH       */
#define DISPLAY_CS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

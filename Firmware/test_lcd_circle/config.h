#ifndef CONFIG_H
#define CONFIG_H

#include "stm32g0xx_ll_gpio.h"
#include <sys/_types.h>

#define EVER    ;;

#define PIN_PSU_ENABLE		11

#define DISP_CS_GPIO_Port   GPIOB
#define DISP_CS_Pin         LL_GPIO_PIN_4

#define DISP_DC_GPIO_Port   GPIOB
#define DISP_DC_Pin         LL_GPIO_PIN_5

#define DISP_RES_GPIO_Port  GPIOB
#define DISP_RES_Pin        LL_GPIO_PIN_6


#define true 1
#define false 0

typedef unsigned int bool;
typedef unsigned int uint;

#endif

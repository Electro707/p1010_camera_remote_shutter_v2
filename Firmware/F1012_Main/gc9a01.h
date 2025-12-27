#ifndef _GC9A01_H_
#define _GC9A01_H_

#include <stdint.h>
#include "stm32g0xx.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_spi.h"
#include "config.h"
#include "graphics.h"

/********** Function Declaration **********/
void gc9a01Init(void);
void gc9a01SetDrawWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void gc9a01_send_color(uint32_t rgbParsed);
void gc9a01_send_color_noWait(uint32_t rgbParsed);
void gc9a01_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void gc9a01DrawInit(void);
void gc9a01DrawEnd(void);

void gc9a01_point(uint16_t x, uint16_t y, uint32_t color);
void gc9a01_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color);
void gc9a01_vert_line(uint16_t x, uint16_t y0, uint16_t y1, uint32_t color);
void gc9a01_draw_bit_canvas(uint8_t *canvas, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint32_t color);

void gc9a01_send_single_cmd_data(uint8_t command, uint8_t data);

/********** Defines for Commands and Whatnot **********/
#define DISP_CMD_INTER_REG_EN2 0xEF
#define DISP_CMD_INTER_REG_EN1 0xFE
#define DISP_CMD_DISPLAY_FUNCTION_CONTROL 0xB6
#define DISP_CMD_MEMORY_ACCESS_CONTROL 0x36
#define DISP_CMD_PIXEL_FORMAT_SET 0x3A

/********** Macros **********/
// #pragma GCC push_options
// #pragma GCC optimize ("O0")

#define DISP_CS_0		LL_GPIO_ResetOutputPin(DISP_CS_GPIO_Port, DISP_CS_Pin)
#define DISP_CS_1		LL_GPIO_SetOutputPin(DISP_CS_GPIO_Port, DISP_CS_Pin)

#define DISP_DC_0		LL_GPIO_ResetOutputPin(DISP_DC_GPIO_Port, DISP_DC_Pin)
#define DISP_DC_1		LL_GPIO_SetOutputPin(DISP_DC_GPIO_Port, DISP_DC_Pin)

#define DISP_RST_0		LL_GPIO_ResetOutputPin(DISP_RES_GPIO_Port, DISP_RES_Pin)
#define DISP_RST_1		LL_GPIO_SetOutputPin(DISP_RES_GPIO_Port, DISP_RES_Pin)

#define SPI_Write_Byte(__DATA) LL_SPI_TransmitData8(SPI2, __DATA); while( (SPI2->SR & (1<<7)) != 0)

// #pragma GCC pop_options

extern const uint32_t gc9a01_color_white;
extern const uint32_t gc9a01_color_black;
extern const uint32_t gc9a01_color_red;
extern const uint32_t gc9a01_color_green;
extern const uint32_t gc9a01_color_blue;
extern const uint32_t gc9a01_color_cyan;
extern const uint32_t gc9a01_color_orange;
extern const uint32_t gc9a01_color_purple;


#endif /* INC_GC9A01_H_ */

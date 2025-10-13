#ifndef _GC9A01_H_
#define _GC9A01_H_

#include <stdint.h>
#include "stm32g0xx.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_spi.h"
#include "config.h"

typedef enum{
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2,
}gc9a01_align_e;

typedef struct{
	uint16_t x0;
	uint16_t y0;
	uint16_t x1;
	uint16_t y1;
}boundingBox_t;

/********** Function Declaration **********/
void gc9a01_init(void);
void gc9a01_fill_screen(uint32_t color);
void gc9a01_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void gc9a01_draw_rect(boundingBox_t *textBox, uint32_t color);
void gc9a01_print_text_sma(const char *text, uint16_t x, uint16_t y, uint16_t color, uint16_t bgColor, gc9a01_align_e alignMode, boundingBox_t *textBox);
void gc9a01_print_text_med(const char *text, uint16_t x, uint16_t y, uint16_t color, uint16_t bgColor, gc9a01_align_e alignMode, boundingBox_t *textBox);
void gc9a01_print_text_big(const char *text, uint16_t x, uint16_t y, uint16_t color, uint16_t bgColor, gc9a01_align_e alignMode, boundingBox_t *textBox);
void gc9a01_point(uint16_t x, uint16_t y, uint32_t color);
void gc9a01_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color);
void gc9a01_draw_bit_canvas(uint8_t *canvas, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint32_t color);
void gc9a01_vert_line(uint16_t x, uint16_t y0, uint16_t y1, uint32_t color);


void gc9a01_send_color(uint32_t rgbParsed);
void _gc9a01_send_single_cmd_data(uint8_t command, uint8_t data);

/********** Defines for Commands and Whatnot **********/
#define DISP_CMD_INTER_REG_EN2 0xEF
#define DISP_CMD_INTER_REG_EN1 0xFE
#define DISP_CMD_DISPLAY_FUNCTION_CONTROL 0xB6
#define DISP_CMD_MEMORY_ACCESS_CONTROL 0x36
#define DISP_CMD_PIXEL_FORMAT_SET 0x3A

typedef enum{
	TEXT_SIZE_SMALL, 	// 16 pixels high
	TEXT_SIZE_MEDIUM,	// 24 pixels high
	TEXT_SIZE_LARGE,	// 32 pixels high
}textSize_e;

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

extern const uint32_t gc9a01_color_white;
extern const uint32_t gc9a01_color_black;
extern const uint32_t gc9a01_color_red;
extern const uint32_t gc9a01_color_green;
extern const uint32_t gc9a01_color_blue;
extern const uint32_t gc9a01_color_cyan;
extern const uint32_t gc9a01_color_orange;
extern const uint32_t gc9a01_color_purple;

// #pragma GCC pop_options


#endif /* INC_GC9A01_H_ */

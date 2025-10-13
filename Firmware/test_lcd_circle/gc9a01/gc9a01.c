/*
 * GC9A01.c
 *
 *  Created on: Jan 6, 2021
 *      Author: electro
 */

#include <stdlib.h>
#include <string.h>
#include "font.h"
#include "gc9a01.h"
#include "stm32g0xx_ll_utils.h"

void _gc9a01_send_command(uint8_t command);
void _gc9a01_send_single_cmd_data(uint8_t command, uint8_t data);
void _gc9a01_send_cmd_and_data(uint8_t command, uint8_t *data, int len);
void _gc9a01_reset(void);

const uint32_t gc9a01_color_white = 0xFFFF;
const uint32_t gc9a01_color_black = 0x0000;
const uint32_t gc9a01_color_red = 0xF800;
const uint32_t gc9a01_color_green = 0x07E0;
const uint32_t gc9a01_color_blue = 0x001F;
const uint32_t gc9a01_color_cyan = 0x07FF;
const uint32_t gc9a01_color_orange = 0xFFE0;
const uint32_t gc9a01_color_purple = 0xF81F;


void gc9a01_send_color(uint32_t rgbParsed){
	// SPI_Write_Byte((rgbParsed >> 16) & 0xFF);
	SPI_Write_Byte((rgbParsed >> 8) & 0xFF);
	SPI_Write_Byte(rgbParsed & 0xFF);
}

uint32_t gc9a01_full_rgb_conv(uint8_t r, uint8_t g, uint8_t b){
	r <<= 2;
	g <<= 2;
	b <<= 2;

	return r << 16 | g << 8 | b;
}

void _gc9a01_send_command(uint8_t command){
	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(command);

//	while(!LL_SPI_IsActiveFlag_BSY(SPI1));
	DISP_CS_1;
}

void _gc9a01_send_single_cmd_data(uint8_t command, uint8_t data){
	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(command);
	DISP_DC_1;
	SPI_Write_Byte(data);
	DISP_CS_1;
}

void _gc9a01_send_cmd_and_data(uint8_t command, uint8_t *data, int len){
	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(command);
	DISP_DC_1;
	for(int i=0;i<len;i++){
		SPI_Write_Byte(data[i]);
	}
	DISP_CS_1;
}

void _gc9a01_reset(void){
	DISP_RST_1;
	LL_mDelay(20);
	DISP_RST_0;
	LL_mDelay(50);
	DISP_RST_1;
	LL_mDelay(200);
}

void gc9a01_draw_rect(boundingBox_t *textBox, uint32_t color){
	uint16_t w = textBox->x1-textBox->x0;
	uint16_t h = textBox->y1-textBox->y0;
	uint32_t len = w * h;
	gc9a01_set_addr_window(textBox->x0, textBox->y0, textBox->x1-1, textBox->y1-1);


	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	for(uint32_t i=0;i<len;i++){
		gc9a01_send_color(color);
	}
	DISP_CS_1;
}

void gc9a01_point(uint16_t x, uint16_t y, uint32_t color){
	gc9a01_set_addr_window(x, y, x, y);
	
	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	
	for(uint32_t i=0;i<1;i++){
		gc9a01_send_color(color);
	}

	DISP_CS_1;
	// adding this below works for 18-bit per pixel, but not a delay or extra SPI writes...WTF???
	// gc9a01_set_addr_window(x, y, x, y);
}

/**
 * @brief This function draws a 2d image, where each bit represents an off/on pixel
 * 
 * @param canvas The canvas array, of size (w/8)*h. Each bit is a pixel in the X direction.
 *               So for a 128x64 display, the array would be [16*64]
 * @param x0 The start x pos of the canvas
 * @param y0 The start y pos of the canvas
 * @param w The width of the canvas
 * @param h The height of the canvas
 * @param color The color to draw if a pixel is set
 */
void gc9a01_draw_bit_canvas(uint8_t *canvas, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint32_t color){
	uint8_t currBitColor;

	gc9a01_set_addr_window(x0, y0, x0+w-1, y0+h-1);
	
	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	for(uint32_t y=0;y<h;y++){
		for(uint32_t x=0;x<w;x++){
			currBitColor = canvas[(y*(w/8)) + (x / 8)];
			currBitColor &= 1 << (x & 0b111);
			if(currBitColor == 0){
				gc9a01_send_color(0);
			}
			else {
				gc9a01_send_color(color);
			}
		}
	}
	DISP_CS_1;
}

void gc9a01_vert_line(uint16_t x, uint16_t y0, uint16_t y1, uint32_t color){
	gc9a01_set_addr_window(x, y0, x, y1-1);

	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	for(int i=0;i<y1-y0;i++){
		gc9a01_send_color(color);
	}
	DISP_CS_1;
}

void gc9a01_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color){
	// impletemented from:
	// 		https://en.wikipedia.org/wiki/Bresenham's_line_algorithm#All_cases
	//		https://zingl.github.io/Bresenham.pdf
	int16_t dx = abs(x1-x0);
	int16_t dy = -abs(y1-y0);
	int16_t sx = x0 < x1 ? 1 : -1;
	int16_t sy = y0 < y1 ? 1 : -1;
	int16_t error = dx + dy;

	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	while(1){
		gc9a01_point(x0, y0, color);
		if((2*error) >= dy){
			if(x0 == x1){break;}
			error += dy;
			x0 += sx;
		}
		if((2*error) <= dx){
			if(y0 == y1){break;}
			error += dx;
			y0 += sy;
		}

	}
	DISP_CS_1;
}

void gc9a01_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
	uint8_t dataarray[4];
	dataarray[0] = x0 >> 8;
	dataarray[1] = x0 & 0xFF;
	dataarray[2] = x1 >> 8;
	dataarray[3] = x1 & 0xFF;
	_gc9a01_send_cmd_and_data(0x2A,dataarray,4);
	dataarray[0] = y0 >> 8;
	dataarray[1] = y0 & 0xFF;
	dataarray[2] = y1 >> 8;
	dataarray[3] = y1 & 0xFF;
	_gc9a01_send_cmd_and_data(0x2B,dataarray,4);
}

void gc9a01_fill_screen(uint32_t color){
	gc9a01_set_addr_window(0, 0, 240-1, 240-1);

	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	for(int i=0;i<(240*240);i++){
		gc9a01_send_color(color);
	}

	DISP_CS_1;
}

void gc9a01_print_text(const char *text,
					   uint16_t x, uint16_t y,
					   uint16_t color, uint16_t bgColor,
					   gc9a01_align_e alignMode, boundingBox_t *textBox,
					   uint8_t fontWidth, uint8_t fontHeight, const uint32_t *fontLut){
	char currText;
	uint32_t toSend;

	uint16_t width = fontWidth*strlen(text);
	
	if(alignMode == ALIGN_CENTER){
		x -= width >> 1;		// width / 2
	}
	else if(alignMode == ALIGN_RIGHT){
		x -= width;
	}

	if(textBox){
		textBox->x0 = x;
		textBox->y0 = y;
		textBox->x1 = x+width;
		textBox->y1 = y+fontHeight;
	}

	// X and Y are flipped as we are flipping it for writting, due to how LUT is structured
	gc9a01_set_addr_window(y, x, y+fontHeight-1, x+width-1);
	_gc9a01_send_single_cmd_data(DISP_CMD_MEMORY_ACCESS_CONTROL, 0xA8);

	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	while(*text){
		currText = *text++;
		currText -= 0x20;

		for(int c=0;c<fontWidth;c++){
			for(int r=0;r<fontHeight;r++){
				toSend = fontLut[c + currText*fontWidth];
				toSend &= (1 << r);
				if(toSend != 0){
					toSend = color;
				} else {
					toSend = bgColor;
				}
				gc9a01_send_color(toSend);
			}
		}
	}
	DISP_CS_1;

	// revert to original access control method
	_gc9a01_send_single_cmd_data(DISP_CMD_MEMORY_ACCESS_CONTROL, 0x88);
}

void gc9a01_print_text_big(const char *text,
					   uint16_t x, uint16_t y,
					   uint16_t color, uint16_t bgColor,
					   gc9a01_align_e alignMode, boundingBox_t *textBox){
	gc9a01_print_text(text, x, y, color, bgColor, alignMode, textBox, 16, 32, spleenFont32);
}
void gc9a01_print_text_med(const char *text, 
					   uint16_t x, uint16_t y,
					   uint16_t color, uint16_t bgColor,
					   gc9a01_align_e alignMode, boundingBox_t *textBox){	
	gc9a01_print_text(text, x, y, color, bgColor, alignMode, textBox, 12, 24, spleenFont24);
}
void gc9a01_print_text_sma(const char *text, 
					   uint16_t x, uint16_t y,
					   uint16_t color, uint16_t bgColor,
					   gc9a01_align_e alignMode, boundingBox_t *textBox){	
	gc9a01_print_text(text, x, y, color, bgColor, alignMode, textBox, 8, 16, spleenFont16);
}

void gc9a01_init(void){
	_gc9a01_reset();

	_gc9a01_send_command(DISP_CMD_INTER_REG_EN2);
	_gc9a01_send_single_cmd_data(0xEB, 0x14);

	_gc9a01_send_command(DISP_CMD_INTER_REG_EN1); _gc9a01_send_command(DISP_CMD_INTER_REG_EN2);
	_gc9a01_send_single_cmd_data(0xEB, 0x14);

	_gc9a01_send_single_cmd_data(0x84, 0x40);
	_gc9a01_send_single_cmd_data(0x85, 0xFF);

	_gc9a01_send_single_cmd_data(0x86, 0xFF);
	_gc9a01_send_single_cmd_data(0x87, 0xFF);
	_gc9a01_send_single_cmd_data(0x88, 0x0A);
	_gc9a01_send_single_cmd_data(0x89, 0x21);
	_gc9a01_send_single_cmd_data(0x8A, 0x00);
	_gc9a01_send_single_cmd_data(0x8B, 0x80);
	_gc9a01_send_single_cmd_data(0x8C, 0x01);
	_gc9a01_send_single_cmd_data(0x8D, 0x01);
	_gc9a01_send_single_cmd_data(0x8E, 0xFF);
	_gc9a01_send_single_cmd_data(0x8F, 0xFF);

	_gc9a01_send_cmd_and_data(0x90, (uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4);

	_gc9a01_send_single_cmd_data(0xBD, 0x06);

	_gc9a01_send_single_cmd_data(0xBC, 0x00);

	_gc9a01_send_cmd_and_data(0xFF, (uint8_t[]){0x60, 0x01, 0x04}, 3);

	_gc9a01_send_single_cmd_data(0xC3, 0x13);
	_gc9a01_send_single_cmd_data(0xC4, 0x13);

	_gc9a01_send_single_cmd_data(0xC9, 0x22);

	_gc9a01_send_single_cmd_data(0xBE, 0x11);

	_gc9a01_send_cmd_and_data(0xE1, (uint8_t[]){0x10, 0x0E}, 2);

	_gc9a01_send_cmd_and_data(0xDF, (uint8_t[]){0x21, 0x0c, 0x02}, 3);

	_gc9a01_send_cmd_and_data(0xF0, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);

    _gc9a01_send_cmd_and_data(0xF1, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);

    _gc9a01_send_cmd_and_data(0xF2, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    _gc9a01_send_cmd_and_data(0xF3, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);
    _gc9a01_send_cmd_and_data(0xED, (uint8_t[]){0x1B, 0x0B}, 2);
    _gc9a01_send_single_cmd_data(0xAE, 0x77);

    _gc9a01_send_single_cmd_data(0xCD, 0x63);

    _gc9a01_send_cmd_and_data(0x70, (uint8_t[]){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9);
    _gc9a01_send_single_cmd_data(0xE8, 0x34);

    _gc9a01_send_cmd_and_data(0x62, (uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12);
    _gc9a01_send_cmd_and_data(0x63, (uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12);
    _gc9a01_send_cmd_and_data(0x64, (uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7);
    _gc9a01_send_cmd_and_data(0x66, (uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10);
    _gc9a01_send_cmd_and_data(0x67, (uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10);
    _gc9a01_send_cmd_and_data(0x74, (uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7);
    _gc9a01_send_cmd_and_data(0x98, (uint8_t[]){0x3e, 0x07}, 2);
    _gc9a01_send_command(0x35);
    _gc9a01_send_command(0x21);

	_gc9a01_send_single_cmd_data(DISP_CMD_PIXEL_FORMAT_SET, 0x05);

	_gc9a01_send_cmd_and_data(DISP_CMD_DISPLAY_FUNCTION_CONTROL, (uint8_t[]){0x00, 0x00}, 2);
	_gc9a01_send_single_cmd_data(DISP_CMD_MEMORY_ACCESS_CONTROL, 0x88);

    _gc9a01_send_command(0x11);    //Exit Sleep
    LL_mDelay(120);
    _gc9a01_send_command(0x29);    //Display on
}

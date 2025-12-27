/* Graphics helper files
 * 
 * This file contains non-display specific graphics helpers, such as drawing to a canvas before the display draws on top
 */
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "config.h"

typedef enum{
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2,
}alignment_e;

typedef enum{
	FONT_SIZE_SMALL,
	FONT_SIZE_MED,
	FONT_SIZE_HUGE,
}fontSize_e;

typedef struct{
	uint16 x0;
	uint16 y0;
	uint16 x1;
	uint16 y1;
}boundingBox_t;

void uiFillScreenBg(void);
void uiPrintText(const char *text, uint32 x, uint32 y, boundingBox_t *textBox);
void uiDrawRectFill(void);
void uiDrawRectOutline(void);

void uiDrawCanvas(const uint8_t *canvas, uint16_t w, uint16_t h);

void uiDrawButton(const char *text, float prog);

/**
 * Draws a slider. The starting point and size are used from the rectangle settings
 */
void uiDrawSlider(float prog);

void uiSetFontSize(fontSize_e size);
void uiSetBackground(uint32 color);
void uiSetForeground(uint32 color);
void uiSetAlignment(alignment_e alignMode);
void uiInvertBgFg(void);

void uiSetDrawPos(uint x, uint y);
void uiSetRectangleEndPos(uint x1, uint y1);
void uiSetRectangleSize(uint w, uint h);
void uiSetRectangleThickness(uint thickness);

void canvas_fill(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h);
void canvasFillWithInvert(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h);
void canvas_draw_rect(uint8_t *canvas, uint16_t canvasW, boundingBox_t *textBox, uint32_t thickness);

void canvas_print_text( uint8_t *canvasBuff, uint16_t canvasW,
                        const char *text,
					    uint16_t x0, uint16_t y0);



#endif
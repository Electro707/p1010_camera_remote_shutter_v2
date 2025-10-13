/* Graphics helper files
 * 
 * This file contains non-display specific graphics helpers, such as drawing to a canvas before the display draws on top
 */
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

typedef enum{
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2,
}alignment_e;

typedef struct{
	uint16_t x0;
	uint16_t y0;
	uint16_t x1;
	uint16_t y1;
}boundingBox_t;

void canvas_fill(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h);
void canvasFillWithInvert(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h);
void canvas_draw_rect(uint8_t *canvas, uint16_t canvasW, boundingBox_t *textBox, uint32_t thickness);

void canvas_print_text( uint8_t *canvasBuff, uint16_t canvasW,
                        const char *text,
					    uint16_t x0, uint16_t y0, 
					    alignment_e alignMode,
					    uint8_t fontWidth, uint8_t fontHeight, const uint32_t *fontLut);



#endif
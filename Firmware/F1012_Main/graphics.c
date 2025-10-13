#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "graphics.h"
#include "config.h"

void canvas_fill(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h){
    uint16_t x1 = x0 + w;
    uint16_t y1 = y0 + h;

    for(uint32_t y=y0; y<y1; y++){
		for(uint32_t x=x0; x<x1; x++){
            canvasBuff[(y*(canvasW>>3)) + (x >> 3)] |= 1 << (x & 0b111);
		}
	}
}

// todo: have background color be set externally through dedicated function
void canvas_draw_rect(uint8_t *canvas, uint16_t canvasW, boundingBox_t *textBox, uint32_t thickness){
	canvas_fill(canvas, canvasW, textBox->x0, textBox->y0, textBox->x1-textBox->x0, thickness);             // top
	canvas_fill(canvas, canvasW, textBox->x0, textBox->y0, thickness, textBox->y1-textBox->y0);             // left
	canvas_fill(canvas, canvasW, textBox->x1-thickness, textBox->y0, thickness, textBox->y1-textBox->y0);   // right
	canvas_fill(canvas, canvasW, textBox->x0, textBox->y1-thickness, textBox->x1-textBox->x0, thickness);   // bottom
}

// function to fill in a rectangle while inverting existing colors if need-be
void canvasFillWithInvert(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h){
    uint x1 = x0 + w;
    uint y1 = y0 + h;
    uint canvasIdx;

    for(uint y=y0; y<y1; y++){
		for(uint x=x0; x<x1; x++){
            canvasIdx = (y*(canvasW>>3)) + (x >> 3);
            if((canvasBuff[canvasIdx] & (1 << (x & 0b111)))){
                canvasBuff[canvasIdx] &= ~(1 << (x & 0b111));
            }
            else {
                canvasBuff[canvasIdx] |= 1 << (x & 0b111);
            }
            
		}
	}
}

// a copy of gc9a01_print_text(), but instead writes to a bit-field canvas buffer
void canvas_print_text( uint8_t *canvasBuff, uint16_t canvasW,
                        const char *text,
					    uint16_t x0, uint16_t y0,
					    alignment_e alignMode,
					    uint8_t fontWidth, uint8_t fontHeight, const uint32_t *fontLut){
	char currText;
	uint toSend;
	uint canvasIdx;
	uint16_t width = fontWidth*strlen(text);
	
	if(alignMode == ALIGN_CENTER){
		x0 -= width >> 1;		// width / 2
	}
	else if(alignMode == ALIGN_RIGHT){
		x0 -= width;
	}

	uint x = x0;

	while(*text){
		currText = *text++;
		currText -= 0x20;

		for(int c=0;c<fontWidth;c++){
			for(int r=0;r<fontHeight;r++){
				toSend = fontLut[c + currText*fontWidth];
				toSend &= (1 << r);
				if(toSend){
					canvasIdx = ((r+y0)*(canvasW>>3)) + (x >> 3);
					canvasBuff[canvasIdx] |= 1 << (x & 0b111);
				}
			}
			x++;
		}
	}
}


void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h){
	int16_t canvasIdx;

	for(int y=0;y<h;y++){
		canvasIdx = (y*canvasW) + x / 8;
		canvasBuff[canvasIdx] |= 1 << (x & 0b111);
	}
}

/**
 * draws a canvas
 * buffer is [Y][X/8], and is bit filled
 */
void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
	// https://en.wikipedia.org/wiki/Bresenham's_line_algorithm#All_cases
	int16_t dx = abs(x1-x0);
	int16_t dy = -abs(y1-y0);
	int16_t sx = x0 < x1 ? 1 : -1;
	int16_t sy = y0 < y1 ? 1 : -1;
	int16_t error = dx + dy;

	int16_t canvasIdx;

    while(1){
        canvasIdx = (y0*canvasW) + x0 / 8;
        if(canvasIdx < 0){
            canvasIdx = 0;
        }
        canvasBuff[canvasIdx] |= 1 << (x0 & 0b111);

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
}

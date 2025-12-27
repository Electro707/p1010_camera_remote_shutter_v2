#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "graphics.h"
#include "gc9a01.h"
#include "config.h"
#include "font.h"

// local config/settings for drawing
struct{
	uint32 fg;
	uint32 bg;
	uint32 x0;
	uint32 y0;
}d;

struct{
	uint32 width;
	uint32 height;
	const uint32 *lut;
	alignment_e align;
}fontConfig;

struct{
	uint32 w;
	uint32 h;
	uint32 size;			// number of pixels
	uint32 thickness;
}rectangleConfig;

/********** UI settings functions **********/

void uiSetFontSize(fontSize_e size){
	switch(size){
		case FONT_SIZE_SMALL:
			fontConfig.width = 8;
			fontConfig.height = 16;
			fontConfig.lut = spleenFont16;
			break;
		case FONT_SIZE_MED:
			fontConfig.width = 12;
			fontConfig.height = 24;
			fontConfig.lut = spleenFont24;
			break;
		case FONT_SIZE_HUGE:
			fontConfig.width = 16;
			fontConfig.height = 32;
			fontConfig.lut = spleenFont32;
			break;
		default:
			break;
	}
}

void uiSetBackground(uint32 color){
	d.bg = color;
}

void uiSetForeground(uint32 color){
	d.fg = color;
}

void uiSetAlignment(alignment_e alignMode){
	fontConfig.align = alignMode;
}

void uiInvertBgFg(void){
	uint tmp = d.fg;
	d.fg = d.bg;
	d.bg = tmp;
}

void uiSetDrawPos(uint x, uint y){
	d.x0 = x;
	d.y0 = y;
}

void uiSetRectangleEndPos(uint x1, uint y1){
	rectangleConfig.w = x1 - d.x0;
	rectangleConfig.h = y1 - d.y0;
	rectangleConfig.size = (rectangleConfig.w) * (rectangleConfig.h);
}

void uiSetRectangleSize(uint w, uint h){
	rectangleConfig.w = w;
	rectangleConfig.h = h;
	rectangleConfig.size = (rectangleConfig.w) * (rectangleConfig.h);
}

void uiSetRectangleThickness(uint thickness){
	rectangleConfig.thickness = thickness;
}

/********** Direct LCD drawing functions **********/

void uiFillScreenBg(void){
	gc9a01SetDrawWindow(0, 0, UI_WIDTH, UI_HEIGHT);

	gc9a01DrawInit();
	for(int i=0;i<(UI_WIDTH*UI_HEIGHT);i++){
		gc9a01_send_color(d.bg);
	}
	gc9a01DrawEnd();
}

void uiDrawRectFill(void){
	gc9a01SetDrawWindow(d.x0,
						d.y0,
						d.x0 + rectangleConfig.w,
						d.y0 + rectangleConfig.h);
	gc9a01DrawInit();
	for(uint32_t i=0;i<rectangleConfig.size;i++){
		gc9a01_send_color(d.fg);
	}
	gc9a01DrawEnd();
}

void uiDrawRectOutline(void){
	uint32 i;
	uint32 thickness = rectangleConfig.thickness;
	// top half
	gc9a01SetDrawWindow(d.x0,
						d.y0,
						d.x0 + rectangleConfig.w,
						d.y0 + thickness);
	gc9a01DrawInit();
	for(i=0;i<(rectangleConfig.w * thickness);i++){
		gc9a01_send_color(d.fg);
	}
	gc9a01DrawEnd();

	// left half
	gc9a01SetDrawWindow(d.x0,
						d.y0,
						d.x0 + thickness,
						d.y0 + rectangleConfig.h);
	gc9a01DrawInit();
	for(i=0;i<(rectangleConfig.h * thickness);i++){
		gc9a01_send_color(d.fg);
	}
	gc9a01DrawEnd();

	// right half
	gc9a01SetDrawWindow(d.x0 + rectangleConfig.w - thickness,
						d.y0,
						d.x0 + rectangleConfig.w,
						rectangleConfig.h + d.y0);
	gc9a01DrawInit();
	for(i=0;i<(rectangleConfig.h * thickness);i++){
		gc9a01_send_color(d.fg);
	}
	gc9a01DrawEnd();

	// bottom half
	gc9a01SetDrawWindow(d.x0,
						d.y0 + rectangleConfig.h - thickness,
						d.x0 + rectangleConfig.w,
						d.y0 + rectangleConfig.h);
	gc9a01DrawInit();
	for(i=0;i<(rectangleConfig.w * thickness);i++){
		gc9a01_send_color(d.fg);
	}
	gc9a01DrawEnd();
}

void uiDrawSlider(float prog){
	uint32 progI = (uint32)(prog * (rectangleConfig.w - 2*rectangleConfig.thickness));
	progI += rectangleConfig.thickness;

	gc9a01SetDrawWindow(d.x0,
						d.y0,
						d.x0 + rectangleConfig.w,
						d.y0 + rectangleConfig.h);
	gc9a01DrawInit();
	
	for(uint32_t y=0;y<rectangleConfig.h;y++){
		for(uint32_t x=0;x<rectangleConfig.w;x++){
			if(x < progI){
				gc9a01_send_color(d.fg);
			}
			else if(x >= (rectangleConfig.w - rectangleConfig.thickness)){
				gc9a01_send_color(d.fg);
			}
			else if(y < rectangleConfig.thickness){
				gc9a01_send_color(d.fg);
			}
			else if(y >= (rectangleConfig.h - rectangleConfig.thickness)){
				gc9a01_send_color(d.fg);
			}
			else{
				gc9a01_send_color(d.bg);
			}
			
		}
	}
	gc9a01DrawEnd();
}

void uiDrawButton(const char *text, float prog){
	boundingBox_t textBox;

	uint32 sizeByte = rectangleConfig.size / 8;
	uint8_t buttonCanvas[sizeByte];

	memset(buttonCanvas, 0, sizeByte);

	const uint32 canvasW = rectangleConfig.w;
	const uint32 canvasH = rectangleConfig.h;
	const uint32 borderW = 2;
	
	// start button, background
	textBox.x0 = 0;
	textBox.x1 = canvasW;
	textBox.y0 = 0;
	textBox.y1 = canvasH;

	uiSetAlignment(ALIGN_CENTER);

	canvas_draw_rect(buttonCanvas, canvasW, &textBox, borderW);

	if(text){
		uint32 textPosX = canvasW / 2;
		uint32 textPosY = (canvasH - fontConfig.height) / 2;
		canvas_print_text(buttonCanvas, canvasW, text, textPosX, textPosY);
	}

	if(prog){
		if(prog > 1.0){
			prog = 1.0;
		}
		uint32 w = (uint32)((canvasW-2*borderW) * prog);
		canvasFillWithInvert(buttonCanvas, canvasW, borderW, borderW, w, canvasH-2*borderW);
	}

	uiDrawCanvas(buttonCanvas, canvasW, canvasH);
}

void uiPrintText(const char *text, uint32 x, uint32 y, boundingBox_t *textBox){
	char currText;
	uint32_t toSend;

	uint width = fontConfig.width*strlen(text);
	
	if(fontConfig.align == ALIGN_CENTER){
		x -= width >> 1;		// width / 2
	}
	else if(fontConfig.align == ALIGN_RIGHT){
		x -= width;
	}

	if(textBox){
		textBox->x0 = x;
		textBox->y0 = y;
		textBox->x1 = x+width;
		textBox->y1 = y+fontConfig.height;
	}

	// X and Y are flipped as we are flipping it for writing, due to how LUT is structured
	gc9a01SetDrawWindow(y, x, y+fontConfig.height, x+width);
	gc9a01_send_single_cmd_data(DISP_CMD_MEMORY_ACCESS_CONTROL, 0xA8);

	gc9a01DrawInit();
	while(*text){
		currText = *text++;
		currText -= 0x20;

		for(int c=0;c<fontConfig.width;c++){
			for(int r=0;r<fontConfig.height;r++){
				toSend = fontConfig.lut[c + currText*fontConfig.width];
				toSend &= (1 << r);
				if(toSend != 0){
					toSend = d.fg;
				} else {
					toSend = d.bg;
				}
				gc9a01_send_color(toSend);
			}
		}
	}
	gc9a01DrawEnd();
	// revert to original access control method
	gc9a01_send_single_cmd_data(DISP_CMD_MEMORY_ACCESS_CONTROL, 0x88);
}

/**
 * @brief This function draws a 2d image, where each bit represents an off/on pixel
 * 
 * @param canvas The canvas array, of size (w/8)*h. Each bit is a pixel in the X direction.
 *               So for a 128x64 display, the array would be [16*64]
 * @param w The width of the canvas
 * @param h The height of the canvas
 */
void uiDrawCanvas(const uint8_t *canvas, uint16_t w, uint16_t h){
	uint8_t currBitColor;
	uint pixelCnt = w * h;

	gc9a01SetDrawWindow(d.x0,
						d.y0,
						d.x0+w,
						d.y0+h);
	
	gc9a01DrawInit();
	for(uint p=0;p<pixelCnt;p++){
		currBitColor = canvas[p >> 3];
		currBitColor &= 1 << (p & 0b111);
		if(currBitColor){
			gc9a01_send_color_noWait(d.fg);
		}
		else {
			gc9a01_send_color_noWait(d.bg);
		}
	}
	while( (SPI2->SR & (1<<7)) != 0);
	gc9a01DrawEnd();
}

/********** Canvas related functions, they manipulate some canvas that is later drawn on screen **********/

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
					    uint16_t x0, uint16_t y0){
	char currText;
	uint toSend;
	uint canvasIdx;
	uint16_t width = fontConfig.width*strlen(text);
	
	if(fontConfig.align == ALIGN_CENTER){
		x0 -= width >> 1;		// width / 2
	}
	else if(fontConfig.align == ALIGN_RIGHT){
		x0 -= width;
	}

	uint x = x0;

	while(*text){
		currText = *text++;
		currText -= 0x20;

		for(int c=0;c<fontConfig.width;c++){
			for(int r=0;r<fontConfig.height;r++){
				toSend = fontConfig.lut[c + currText*fontConfig.width];
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
		canvasIdx = (y*canvasW) + (x >> 3);
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
        canvasIdx = (y0*canvasW) + (x0 >> 3);
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

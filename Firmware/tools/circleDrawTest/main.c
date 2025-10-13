#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define APP_W (240*1)
#define APP_H (240*1)

typedef uint32_t u32;
typedef unsigned int uint;


void drawLine(u32 x0, u32 y0, u32 x1, u32 y1, u32 color);

static uint32_t fb[APP_W * APP_H]; // 32-bit ARGB8888

static inline void putPixel(int x, int y, uint32_t c) {
	if ((unsigned)x < APP_W && (unsigned)y < APP_H) {
		fb[y * APP_W + x] = c;
	}
}

void drawLine(u32 x0, u32 y0, u32 x1, u32 y1, u32 color){
	// https://en.wikipedia.org/wiki/Bresenham's_line_algorithm#All_cases
	int16_t dx = abs(x1-x0);
	int16_t dy = -abs(y1-y0);
	int16_t sx = x0 < x1 ? 1 : -1;
	int16_t sy = y0 < y1 ? 1 : -1;
	int16_t error = dx + dy;

	 while(1){
		putPixel(x0, y0, color);

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

uint lastX, lastY;
void drawCircle(float angle){
	const uint radius = 25;
	const uint color = 0xFF00FF;
	uint tmp;
	angle *= (M_PI/180.);

	int centerX = (APP_W/2);
	int centerY = (APP_H/2);

	int xMax = ((APP_W/2))*cos(angle);
	int yMax = ((APP_H/2))*sin(angle);
	int xMin = ((APP_W/2)-radius)*cos(angle);
	int yMin = ((APP_H/2)-radius)*sin(angle);

	// drawLine(120, 120, xMax+120, 120-yMax, 0xFFFFFF);

	float edgeDefM = 1;
	int yDir = 1;
	int xDir = 1;


	int xMinFor = xMin;
	int yMinFor = yMin;
	int xMaxFor = xMax;
	int yMaxFor = yMax;

	if(xMax < xMin){
		tmp = xMinFor; xMinFor = xMaxFor; xMaxFor = tmp;
		edgeDefM *= -1;
		// printf("%d->%d\t%d->%d\n", xMin, xMinFor, xMax, xMaxFor);
	}
	if(yMaxFor < yMinFor){
		tmp = yMinFor; yMinFor = yMaxFor; yMaxFor = tmp;
		// edgeDefM *= -1;
	}
	float dx = (xMax - xMin);
	float dy = (yMax - yMin);

	for(int y=yMinFor;y<yMaxFor;y++){
		for(int x=xMinFor;x<xMaxFor;x++){
			float edgeDef = (x-xMin)*dy - (y-yMin)*dx;
			// int edgeDef = (xMax-x)*dy - (yMax-y)*dx;
			// printf("%d\n", edgeDef);
			// edgeDef *= edgeDefM;
			if(abs(dy) < 7 || abs(dx) < 7){
				putPixel(x+centerX, centerY-y, 0xFF0000);
				continue;
			}
			if(edgeDef <= 0){
				putPixel(x+centerX, centerY-y, color);
			}else{
				// putPixel(x+centerX, centerY-y, 0xFF0000);
			}

		}
	}

}


int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("SDL init failed: %s", SDL_GetError());
		return 1;
	}

	SDL_Window *win = SDL_CreateWindow("fb", APP_W, APP_H, 0);
	if (!win) {
		SDL_Log("CreateWindow failed: %s", SDL_GetError());
		return 1;
	}

	SDL_Surface *surf = SDL_GetWindowSurface(win);

	// drawLine(0, 0, 100, 150, 0x00FFFF);

	lastX = (APP_H/2);
	lastY = (APP_H/2);

	// Simple event loop to keep window open
	SDL_Event e;
	int quit = 0;
	float angle = 90;
	while (!quit) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT)
				quit = 1;
		}

		// SDL_memset(fb, 0, APP_W * APP_H * sizeof(uint32_t));

		drawCircle(angle);

		// Copy fb[] into window surface
		SDL_memcpy(surf->pixels, fb, APP_W * APP_H * sizeof(uint32_t));
		SDL_UpdateWindowSurface(win);

		angle -= 1;
		if(angle > 360){
			angle -= 360;
		}
		if(angle < 0){
			angle += 360;
		}

		SDL_Delay(50);
	}

	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}

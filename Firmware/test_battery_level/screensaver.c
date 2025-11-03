#include "config.h"
#include "gc9a01.h"
#include <math.h>
#include <string.h>

typedef  uint32_t u32;

typedef struct{
	u32 rows;
	u32 cols;
} matrixSize_s;

#define CANVAS_WIDTH    240
#define CANVAS_HEIGHT   240
#define CANVAS_WIDTH_BYTE (CANVAS_WIDTH/8)

uint8_t canvas[CANVAS_HEIGHT*CANVAS_WIDTH_BYTE];         // width = 240, 240/8=30 for rgb
float newPos[32*4];
float txMatrix[4*4];
float rot = 0.0;

void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);

void matrixMult(float *srcA, float *srcB, float *dst, matrixSize_s sizeA, matrixSize_s sizeB);

/* defines for object */
float objectVertex[32*4] = {
	2.000000, 1.000000, -0.750000, 1.000000,
	2.000000, -1.000000, -0.750000, 1.000000,
	2.000000, 1.000000, 0.750000, 1.000000,
	2.000000, -1.000000, 0.750000, 1.000000,
	-2.000000, 1.000000, -0.750000, 1.000000,
	-2.000000, -1.000000, -0.750000, 1.000000,
	-2.000000, 1.000000, 0.750000, 1.000000,
	-2.000000, -1.000000, 0.750000, 1.000000,
	0.000000, 0.750000, -1.500000, 1.000000,
	0.000000, 0.750000, -0.750000, 1.000000,
	0.530330, 0.530330, -1.500000, 1.000000,
	0.530330, 0.530330, -0.750000, 1.000000,
	0.750000, 0.000000, -1.500000, 1.000000,
	0.750000, -0.000000, -0.750000, 1.000000,
	0.530330, -0.530330, -1.500000, 1.000000,
	0.530330, -0.530330, -0.750000, 1.000000,
	0.000000, -0.750000, -1.500000, 1.000000,
	0.000000, -0.750000, -0.750000, 1.000000,
	-0.530330, -0.530330, -1.500000, 1.000000,
	-0.530330, -0.530330, -0.750000, 1.000000,
	-0.750000, 0.000000, -1.500000, 1.000000,
	-0.750000, -0.000000, -0.750000, 1.000000,
	-0.530330, 0.530330, -1.500000, 1.000000,
	-0.530330, 0.530330, -0.750000, 1.000000,
	-1.600000, 1.000000, 0.300000, 1.000000,
	-1.600000, 1.300000, 0.300000, 1.000000,
	-1.600000, 1.000000, -0.300000, 1.000000,
	-1.600000, 1.300000, -0.300000, 1.000000,
	-1.000000, 1.000000, 0.300000, 1.000000,
	-1.000000, 1.300000, 0.300000, 1.000000,
	-1.000000, 1.000000, -0.300000, 1.000000,
	-1.000000, 1.300000, -0.300000, 1.000000,
};
uint objectEdge[48*2] =     {
	0, 4,	4, 6,	6, 2,	2, 0,
	3, 2,	6, 7,	7, 3,	4, 5,
	5, 7,	5, 1,	1, 3,	1, 0,
	8, 9,	9, 11,	11, 10,	10, 8,
	11, 13,	13, 12,	12, 10,	13, 15,
	15, 14,	14, 12,	15, 17,	17, 16,
	16, 14,	17, 19,	19, 18,	18, 16,
	19, 21,	21, 20,	20, 18,	9, 23,
	23, 21,	23, 22,	22, 20,	8, 22,
	24, 25,	25, 27,	27, 26,	26, 24,
	27, 31,	31, 30,	30, 26,	31, 29,
	29, 28,	28, 30,	29, 25,	24, 28,
};

uint16_t scaleVectorToDraw(float pos){
	return (uint16_t)((pos * 40) + 120);
}

void createTransformMatrix(float rotX, float rotY, float rotZ, float *resultMatrix){
	float tmpA[4*4];
	float tmpB[4*4];

	rotX = rotX * (M_PI/180);
	rotY = rotY * (M_PI/180);
	rotZ = rotZ * (M_PI/180);

	float rotationMatrixX[4*4] = {
		1, 0,         0,          0,
		0, cos(rotX), -sin(rotX), 0,
		0, sin(rotX), cos(rotX),  0,
		0, 0,         0,          1,
	};
	float rotationMatrixY[4*4] = {
		cos(rotY),  0, sin(rotY), 0,
		0,          1, 0,         0,
		-sin(rotY), 0, cos(rotY), 0,
		0,          0, 0,         1,
	};
	float rotationMatrixZ[4*4] = {
		cos(rotZ), -sin(rotZ), 0, 0,
		sin(rotZ), cos(rotZ),  0, 0,
		0,         0,          1, 0,
		0,         0,          0, 1,
	};

	float inverseMatrix[4*4] = {
		1, 0, 0, 0,
		0, -1,  0, 0,
		0,     0,          1, 0,
		0,         0,          0, 1,
	};

	matrixMult(rotationMatrixX, rotationMatrixY, tmpA, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpA, rotationMatrixY, tmpB, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpB, rotationMatrixZ, tmpA, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpA, inverseMatrix, resultMatrix, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
}

void matrixMult(float *srcA, float *srcB, float *dst, matrixSize_s sizeA, matrixSize_s sizeB){
	float sum;

	// assert(sizeA.cols == sizeB.rows);

	u32 m = sizeA.rows;
	u32 p = sizeB.cols;
	u32 n = sizeA.cols;

	// memset(dst, 0, m * p * sizeof(float));

	for(int i = 0; i < m; i++){
		for(int j = 0; j < p; j++){
			sum = 0;
			for(int k = 0; k < n; k++){
				sum += srcA[k+(i*sizeA.cols)] * srcB[j+(k*sizeB.cols)];
			}
			dst[(i*sizeB.cols) + j] = sum;
		}
	}
}

void drawObj(float *obj, uint *faceDef, uint objEdgeLen){
	float *pos;
	float *posNext;

	for(int i=0;i<objEdgeLen;i++){
		pos = &obj[4*(faceDef[i*2])];
		posNext = &obj[4*(faceDef[(i*2)+1])];

		line_draw_abstract(canvas, CANVAS_WIDTH_BYTE,
				scaleVectorToDraw(pos[0]),
				scaleVectorToDraw(pos[1]),
				scaleVectorToDraw(posNext[0]),
				scaleVectorToDraw(posNext[1]));
	}
}

void serviceScreenSaver(void){
    memset(canvas, 0, CANVAS_HEIGHT*CANVAS_WIDTH_BYTE);

    createTransformMatrix(-20., rot, rot*0.5, txMatrix);
    matrixMult(objectVertex, txMatrix, newPos, (matrixSize_s){32, 4}, (matrixSize_s){4, 4});
    drawObj(newPos, objectEdge, 48);

    gc9a01_draw_bit_canvas(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, gc9a01_color_white);

    rot += 1;
    if(rot > 360){
        rot = 0;
    }
}
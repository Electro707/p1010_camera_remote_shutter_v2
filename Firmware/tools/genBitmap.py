#!/bin/python3
"""
    Tool to generate some icons, right now just battery ones
"""
from PIL import Image, ImageDraw
import math

def getVariableName(varName, imgW, imgH):
    # as we are only storing the bits of an image
    sorageSize = math.ceil((imgW * imgH) / 8)

    toPrint = f"const uint8_t {varName}[{sorageSize}]"
    return toPrint

def genIconImage(varName, imageFile):
    img = Image.open(imageFile, 'r')
    img = img.convert("1")

    imgW, imgH = img.size

    imgCName = getVariableName(varName, imgW, imgH)

    toPrint = f"#define IMG_W_{varName} {imgW}\n"
    toPrint += f"#define IMG_H_{varName} {imgH}\n"
    toPrint += f"extern {imgCName};\n\n"
    toPrint += f"{imgCName} = {{\n\t"

    colorDat = 0
    bitIdx = 0
    for p in range(imgW * imgH):
        pixelIdx = (p % imgW), (p // imgW)

        color = img.getpixel(pixelIdx)
        if color != 0:
            colorDat |= (1 << bitIdx)

        bitIdx += 1

        if bitIdx >= 8:
            toPrint += f"0x{colorDat:02x},"
            colorDat = 0
            bitIdx = 0
            if ((p+1) % 32) == 0:
                toPrint += '\n\t'

    if bitIdx != 0:
        toPrint += f"0x{colorDat:02x},"

    toPrint += '\n\t'

    toPrint = toPrint[:-3]
    toPrint += "};\n\n"

    return toPrint


print(genIconImage("batteryBase", "battIcon/battery.png"))
print(genIconImage("batteryCharge", "battIcon/batteryCharge.png"))

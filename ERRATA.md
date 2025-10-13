# Purpose
This document describes the hardware erratas and any potential fix using said hardware.

# E1011 Rev 2: Driver Board
## Errata 2: PA11 and PA9 Flipping
Due to how the schematic showcased pins PA9 and PA11, I assumed what I was connecting `ROT_B` to was PA9, while in reality it was PA11 unless the pin is remapped in software, which leads the original PA11 to not be usable, which had `3V3_EN_MCU` connected to it

### Fix
Flip PA9 and PA11 connections as follows:
![Bodge Wire](.misc/2020_0809_014633_027.JPG)

# E1011 Rev 1: Driver Board

## Errata 1: Crystal Enable
The inverting and non-inverting inputs of the op-amp were flipped

### Fix
None other than time-consuming bodge wiring

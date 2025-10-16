# P1010 - Camera Remote Shutter Project V2
#### Rev 2

This is an open-source camera remote shutter.

This was the second revision of the [original project](https://github.com/Electro707/camera_remote_shutter), but is now treated as a seperate project due to scope creep.

Currently working on Rev 2, which is smaller and fixed some issues with V1

# Directory Structure
- `CAD`: FreeCAD enclosure and models
    - C1013: Bottom half of enclosure
    - C1014: Top half of enclosure
- `Firmware`: Firmware. Currently contains the main and misc test firmwares
- `PCB`: KiCAD PCB Files
    - E1011: PCB
- `Release`: Locked in "released" files, such as Schematics, Gerbers, etc
- `SimTest`: A simulation I did in KiCAD for the trans-impedance amplifier

## Erratas
All hardware erratas and potential work-arounds are documented in [ERRATA.md](ERRATA.md)

## Firmware
The firmware, located in `Firmware/F1012_Main`, is the firmware for this project.

The firwmare requires [stm32-cmake by ObKo](https://github.com/ObKo/stm32-cmake) to be installed somewhere in your system, as well as [stm32's G0's firmware package](https://github.com/STMicroelectronics/STM32CubeG0).
The paths `CMAKE_TOOLCHAIN_FILE` and `STM32_CUBE_G0_PATH` must be set to the path of the installed dependencies above, respectivally.
Eventually I plan on having them be easily configured, but need to figure out the best way of approaching that.

It can be build with cmake as follows in the firmware directory:
```bash
mkdir -p build; cd build
cmake ../
make
```

The firmware can then be flashed with the custom command
```bash
make flash
```

### Setup
This project uses [stm32-cmake](https://github.com/ObKo/stm32-cmake) in the build process, so you must set it up and change the `CMAKE_TOOLCHAIN_FILE` variable in each firmware's CMAKE file. You must also download [STM32G0's MCU Package](https://github.com/STMicroelectronics/STM32CubeG0) and set the `STM32_CUBE_G0_PATH` variable to the download location.
Optionally if you have stm32cubeide installed, setup `STM32_TOOLCHAIN_PATH` in the CMAKE file, or remove it

### Compiling
To build any of the test or main applications, go into that directory and run the following:
```bash
mkdir -p build
cd build
cmake ../
make
```

### Programming
To upload the program, run the following stlink command
```bash
st-flash write <file_to_flash>.bin 0x08000000
```

Where the <file_to_flash> is the bin file inside of the build folder. `0x08000000` is the starting flash address for the STM32G0 family.

## KiCAD 3D Models
The 3D models for some components in the directory `PCB/3d_model/` are not included due to licensing reasons. You can grab the step files yourself and put it in that directory from the manufacturer. The models are
- CUI_DEVICES_SJ-2509N.step
- 615006138421_Download_STP_615006138421_rev1.stp
- c-1-1478035-0-b-3d.stp
- pec12r-4025f-n0024.stp

# License
This project is licensed under GPLv3. See [LICENSE.md](LICENSE.md) for details.

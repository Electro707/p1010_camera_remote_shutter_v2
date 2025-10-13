# uses https://github.com/ObKo/stm32-cmake

cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE "/home/electro/STM32Cube/Repository/stm32-cmake/cmake/stm32_gcc.cmake")

project(LcdTestCircle C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(STM32_CUBE_G0_PATH "/home/electro/STM32Cube/Repository/STM32CubeG0")
set(STM32_TOOLCHAIN_PATH "/opt/stm32cubeide/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.linux64_1.1.0.202410170702/tools/bin/")

find_package(CMSIS COMPONENTS STM32G071K REQUIRED)
find_package(HAL COMPONENTS STM32G071K REQUIRED)

add_executable(LcdTestCircle main.c)

target_include_directories(LcdTestCircle PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/gc9a01
    ${CMAKE_SOURCE_DIR}/lvgl
)


target_sources(LcdTestCircle PRIVATE
    syscalls.c
    main.c
    gc9a01/gc9a01.c
)

# stm32_add_linker_script(CameraRemoteShutterV2 PRIVATE STM32G071K8TX_FLASH.ld)

target_link_libraries(LcdTestCircle PRIVATE
    HAL::STM32::G0::LL_RCC
    HAL::STM32::G0::LL_UTILS
    HAL::STM32::G0::LL_GPIO
    CMSIS::STM32::G071KB
    STM32::NoSys
    )

target_compile_options(LcdTestCircle PRIVATE 
    -O2 -Wall
)

target_link_options(LcdTestCircle PRIVATE 
    LINKER:-Map=foo.map
)

stm32_print_size_of_target(LcdTestCircle)
stm32_generate_binary_file(LcdTestCircle)

add_definitions(-fdiagnostics-color=always)

# uses https://github.com/ObKo/stm32-cmake

cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE "/home/electro/STM32Cube/Repository/stm32-cmake/cmake/stm32_gcc.cmake")

project(CameraRemoteShutterV2 C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(STM32_CUBE_G0_PATH "/home/electro/STM32Cube/Repository/STM32CubeG0")

find_package(CMSIS COMPONENTS STM32G071K REQUIRED)
find_package(HAL COMPONENTS STM32G071K REQUIRED)

add_executable(CameraRemoteShutterV2 main.c)

target_include_directories(CameraRemoteShutterV2 PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/gc9a01
    ${CMAKE_SOURCE_DIR}/lvgl
)

target_sources(CameraRemoteShutterV2 PRIVATE
    syscalls.c
    main.c
    gc9a01/gc9a01.c
    graphics.c
    font.c
    screensaver.c
)

# stm32_add_linker_script(CameraRemoteShutterV2 PRIVATE STM32G071K8TX_FLASH.ld)

target_link_libraries(CameraRemoteShutterV2 PRIVATE
    HAL::STM32::G0::LL_RCC
    HAL::STM32::G0::LL_UTILS
    HAL::STM32::G0::LL_GPIO
    CMSIS::STM32::G071KB
    STM32::NoSys
    )

target_compile_options(CameraRemoteShutterV2 PRIVATE 
    -O2 -Wall
)

target_link_options(CameraRemoteShutterV2 PRIVATE 
    LINKER:-Map=foo.map
)

stm32_print_size_of_target(CameraRemoteShutterV2)
stm32_generate_binary_file(CameraRemoteShutterV2)

add_definitions(-fdiagnostics-color=always)

stm32_get_memory_info(CHIP STM32G071K8 FLASH ORIGIN FLASH_ORIGIN)

add_custom_target(flash
  COMMAND st-flash write "${CMAKE_CURRENT_BINARY_DIR}/CameraRemoteShutterV2.bin" ${FLASH_ORIGIN}
  COMMENT "Flashing ${FW_BIN} to ${FLASH_ADDR} with st-flash"
  VERBATIM
)
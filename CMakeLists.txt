cmake_minimum_required(VERSION 3.13)

set(PROJECT pico-cec)
set(BOARD "pico_sdk") # Configure TinyUSB use pico-sdk board configuration
set(PICO_BOARD "seeed_xiao_rp2350" CACHE STRING "Raspberry Pi Pico board type.")
set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tinyusb" CACHE PATH "TinyUSB source tree.")

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/boards)

# Include pico-sdk and tinyusb (must be before project)
include(pico-sdk/pico_sdk_init.cmake)
include(${PICO_TINYUSB_PATH}/hw/bsp/family_support.cmake)

# TinyUSB 0.21's RP2040-family BSP supplies a default FreeRTOS stack-overflow
# hook. Keep this project's LED diagnostic hook as the application hook and
# give the unused TinyUSB default a private symbol to avoid a duplicate.
set_source_files_properties(
  ${PICO_TINYUSB_PATH}/hw/bsp/rp2040/family.c
  PROPERTIES COMPILE_DEFINITIONS
  vApplicationStackOverflowHook=tinyusb_vApplicationStackOverflowHook)

project(${PROJECT}
  DESCRIPTION "HDMI CEC to USB HID keyboard adapter."
  LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Include FreeRTOS kernel
if(PICO_PLATFORM STREQUAL "rp2350-arm-s")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ)
else()
message(FATAL_ERROR "Unsupported Pico platform: ${PICO_PLATFORM}; this project targets the XIAO RP2350.")
endif()

# Include CRC library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/crc)

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall -Werror)

set(TCLI_SOURCE_DIR ${PROJECT_SOURCE_DIR}/tcli)

add_library(tcli STATIC
  ${TCLI_SOURCE_DIR}/source/tcli.c
  ${TCLI_SOURCE_DIR}/source/tclie.c)

target_include_directories(tcli PUBLIC
  ${TCLI_SOURCE_DIR}/include)

target_compile_definitions(tcli PUBLIC
  TCLI_COMPLETE=1
  TCLIE_ENABLE_USERS=0
  TCLIE_PATTERN_MATCH=1)

target_compile_options(tcli PRIVATE
  -Wno-stringop-truncation)

add_executable(${PROJECT}
  src/blink.c
  src/cec-config.c
  src/cec-frame.c
  src/cec-log.c
  src/cec-task.c
  src/cec-user.c
  src/ddc.c
  src/freertos_hook.c
  src/main.c
  src/nvs.c
  src/usb-cdc.c
  src/usb_descriptors.c
  src/usb_hid.c
  src/util.c
  src/ws2812.c
  src/ws2812.pio)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

pico_generate_pio_header(${PROJECT} ${PROJECT_SOURCE_DIR}/src/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_include_directories(${PROJECT} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/tusb
  ${CMAKE_CURRENT_BINARY_DIR}/generated)

set(CEC_PIN "3" CACHE STRING "GPIO pin for HDMI CEC.")
set(PICO_CEC_VERSION "unknown" CACHE STRING "Pico-CEC version string.")
set(KEYMAP_DEFAULT "KODI" CACHE STRING "Default keymap, specify KODI or MISTER.")
set(CEC_OSD_NAME "Pico-CEC" CACHE STRING "OSD name for the CEC device (max 14 characters).")

set_source_files_properties(src/cec-frame.c PROPERTIES COMPILE_DEFINITIONS
  "CEC_PIN=${CEC_PIN}")

set_source_files_properties(src/usb-cdc.c PROPERTIES COMPILE_DEFINITIONS
  "PICO_CEC_VERSION=\"${PICO_CEC_VERSION}\"")

set_source_files_properties(src/nvs.c PROPERTIES COMPILE_DEFINITIONS
  "CEC_OSD_NAME=\"${CEC_OSD_NAME}\"")

# Undefine TinyUSB built-in OS, redefined in our tusb_config.h
target_compile_options(${PROJECT} PRIVATE
  -UCFG_TUSB_OS
  -DKEYMAP_DEFAULT_${KEYMAP_DEFAULT}=1)

target_link_libraries(${PROJECT}
  crc
  pico_stdlib
  pico_unique_id
  hardware_i2c
  hardware_pio
  tinyusb_device
  tinyusb_board
  FreeRTOS-Kernel
  tcli)

pico_add_extra_outputs(${PROJECT})
pico_set_binary_type(${PROJECT} copy_to_ram)
pico_set_linker_script(${PROJECT} ${PROJECT_SOURCE_DIR}/src/memmap_ram_nvs_${PICO_PLATFORM}.ld)

# disable stdio and uart
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 0)

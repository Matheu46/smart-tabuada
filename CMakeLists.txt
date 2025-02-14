# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(smart-tabuada C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(smart-tabuada smart-tabuada.c inc/ssd1306_i2c.c)

pico_set_program_name(smart-tabuada "smart-tabuada")
pico_set_program_version(smart-tabuada "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(smart-tabuada 0)
pico_enable_stdio_usb(smart-tabuada 1)

# Add the standard include files to the build
target_include_directories(smart-tabuada PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(smart-tabuada 
        pico_stdlib
        hardware_i2c
        hardware_adc
        pico_lwip_http
        pico_cyw43_arch_lwip_threadsafe_background 
        )

pico_add_extra_outputs(smart-tabuada)

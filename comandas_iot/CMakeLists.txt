# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

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

project(comandas_iot C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add the main executable
add_executable(comandas_iot
        comandas_iot.c
        led_matrix.c
        inc/ssd1306_i2c.c

        )

pico_generate_pio_header(comandas_iot ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

# Include directories
target_include_directories(comandas_iot PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts

        )

# Link libraries
target_link_libraries(comandas_iot
        pico_cyw43_arch_lwip_threadsafe_background
        hardware_pio
        hardware_pwm
        hardware_i2c
        pico_stdlib
        )

# Configure the IP address for the access point
pico_configure_ip4_address(comandas_iot PRIVATE
        CYW43_DEFAULT_IP_AP_ADDRESS 192.168.4.1
        )

# Add extra outputs (e.g., .uf2, .hex, etc.)
pico_add_extra_outputs(comandas_iot)

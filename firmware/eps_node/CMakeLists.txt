cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "Set PICO_SDK_PATH to your pico-sdk checkout before configuring this firmware")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(eps_node C CXX ASM)
pico_sdk_init()

add_executable(eps_node
    main.cpp
    display_status.cpp
    ina219.cpp
    ssd1306.cpp
    ../common/eps_app.cpp
    ../common/protocol.cpp
)

target_include_directories(eps_node PRIVATE
    ../common
)

target_link_libraries(eps_node
    pico_stdlib
    hardware_i2c
)

pico_enable_stdio_usb(eps_node 1)
pico_enable_stdio_uart(eps_node 0)
pico_add_extra_outputs(eps_node)

add_executable(eps_node_measured_load
    main.cpp
    display_status.cpp
    ina219.cpp
    ssd1306.cpp
    ../common/eps_app.cpp
    ../common/protocol.cpp
)

target_include_directories(eps_node_measured_load PRIVATE
    ../common
)

target_compile_definitions(eps_node_measured_load PRIVATE
    EPS_LOAD_ACTIVE_LOW=1
)

target_link_libraries(eps_node_measured_load
    pico_stdlib
    hardware_i2c
)

pico_enable_stdio_usb(eps_node_measured_load 1)
pico_enable_stdio_uart(eps_node_measured_load 0)
pico_add_extra_outputs(eps_node_measured_load)

add_executable(eps_node_uart
    main_uart.cpp
    display_status.cpp
    ina219.cpp
    ssd1306.cpp
    ../common/eps_app.cpp
    ../common/protocol.cpp
)

target_include_directories(eps_node_uart PRIVATE
    ../common
)

target_link_libraries(eps_node_uart
    pico_stdlib
    hardware_uart
    hardware_i2c
)

pico_enable_stdio_usb(eps_node_uart 0)
pico_enable_stdio_uart(eps_node_uart 0)
pico_add_extra_outputs(eps_node_uart)

add_executable(eps_node_uart_measured_load
    main_uart.cpp
    display_status.cpp
    ina219.cpp
    ssd1306.cpp
    ../common/eps_app.cpp
    ../common/protocol.cpp
)

target_include_directories(eps_node_uart_measured_load PRIVATE
    ../common
)

target_compile_definitions(eps_node_uart_measured_load PRIVATE
    EPS_LOAD_ACTIVE_LOW=1
)

target_link_libraries(eps_node_uart_measured_load
    pico_stdlib
    hardware_uart
    hardware_i2c
)

pico_enable_stdio_usb(eps_node_uart_measured_load 0)
pico_enable_stdio_uart(eps_node_uart_measured_load 0)
pico_add_extra_outputs(eps_node_uart_measured_load)

add_executable(obc_bridge
    ../obc_bridge/main.cpp
    ../common/protocol.cpp
)

target_include_directories(obc_bridge PRIVATE
    ../common
)

target_link_libraries(obc_bridge
    pico_stdlib
    hardware_uart
)

pico_enable_stdio_usb(obc_bridge 1)
pico_enable_stdio_uart(obc_bridge 0)
pico_add_extra_outputs(obc_bridge)

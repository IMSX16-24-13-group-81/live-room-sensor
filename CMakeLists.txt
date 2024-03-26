cmake_minimum_required(VERSION 3.24)
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/lib/pico-sdk")
include(pico_sdk_import.cmake)

project(live-room-sensor C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w)
pico_sdk_init()

add_executable(live-room-sensor)
pico_add_extra_outputs(live-room-sensor)

target_include_directories(live-room-sensor PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/include
)

target_sources(live-room-sensor PRIVATE
        src/main.c
)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
)

target_link_libraries(live-room-sensor
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background
        hardware_pwm
        hardware_pio
)
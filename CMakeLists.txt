cmake_minimum_required(VERSION 3.5)
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/lib/pico-sdk")
include(pico_sdk_import.cmake)

project(live-room-sensor C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(FIRMWARE_VERSION "0.3.0")

if (DEFINED ENV{WIFI_SSID} AND (NOT WIFI_SSID))
    set(WIFI_SSID $ENV{WIFI_SSID})
    message("Using WIFI_SSID from environment ('${WIFI_SSID}')")
else ()
    message(FATAL_ERROR "WIFI_SSID must be set in the environment")
endif ()

if (DEFINED ENV{WIFI_PASSWORD} AND (NOT WIFI_PASSWORD))
    set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})
    message("Using WIFI_PASSWORD from environment")
else ()
    message(FATAL_ERROR "WIFI_PASSWORD must be set in the environment")
endif ()

if (DEFINED ENV{BLUETOOTH_AUTH_TOKEN} AND (NOT BLUETOOTH_AUTH_TOKEN))
    set(BLUETOOTH_AUTH_TOKEN $ENV{BLUETOOTH_AUTH_TOKEN})
    message("Using BLUETOOTH_AUTH_TOKEN from environment")
else ()
    message(FATAL_ERROR "BLUETOOTH_AUTH_TOKEN must be set in the environment")
endif ()

if (DEFINED ENV{REPORT_API_KEY} AND (NOT REPORT_API_KEY))
    set(REPORT_API_KEY $ENV{REPORT_API_KEY})
    message("Using REPORT_API_KEY from environment")
else ()
    message(FATAL_ERROR "REPORT_API_KEY must be set in the environment")
endif ()

if (DEFINED ENV{REPORTING_SERVER} AND (NOT REPORTING_SERVER))
    set(REPORTING_SERVER $ENV{REPORTING_SERVER})
    message("Using REPORTING_SERVER from environment ('${REPORTING_SERVER}')")
else ()
    message(FATAL_ERROR "REPORTING_SERVER must be set in the environment")
endif ()

if (DEFINED ENV{REPORTING_PATH} AND (NOT REPORTING_PATH))
    set(REPORTING_PATH $ENV{REPORTING_PATH})
    message("Using REPORTING_ENDPOINT from environment ('${REPORTING_PATH}')")
else ()
    message(FATAL_ERROR "REPORTING_ENDPOINT must be set in the environment")
endif ()

set(PICO_BOARD pico_w)
pico_sdk_init()

add_executable(live-room-sensor)
pico_add_extra_outputs(live-room-sensor)

if (DEFINED ENV{USE_NEW_MINEW_RADAR} AND (NOT USE_NEW_MINEW_RADAR))
    target_compile_definitions(live-room-sensor PRIVATE
            USE_NEW_MINEW_RADAR
    )
    message("USE_NEW_MINEW_RADAR is defined in the environment. Using radar driver for minewsemi")
else ()
    message("USE_NEW_MINEW_RADAR is not defined in the environment. Using radar driver for micradar")
endif ()

target_include_directories(live-room-sensor PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/include
)

target_compile_definitions(live-room-sensor PRIVATE
        FIRMWARE_VERSION="${FIRMWARE_VERSION}"
        WIFI_SSID="${WIFI_SSID}"
        WIFI_PASSWORD="${WIFI_PASSWORD}"
        REPORT_API_KEY="${REPORT_API_KEY}"
        BLUETOOTH_AUTH_TOKEN="${BLUETOOTH_AUTH_TOKEN}"
        REPORTING_PATH="${REPORTING_PATH}"
        REPORTING_SERVER="${REPORTING_SERVER}"
)

target_sources(live-room-sensor PRIVATE
        src/main.c
        src/pir_sensor.c
        src/micradar.c
        src/minewsemi_radar.c
        src/sensor_controller.c
        src/reset.c
        src/reporting.c
        src/https.c
        src/bluetooth_spp.c
        src/multi_printf.c
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
        pico_lwip_mbedtls
        pico_mbedtls
        hardware_resets
        pico_btstack_cyw43
        pico_btstack_classic
)
